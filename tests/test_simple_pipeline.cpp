/*
 * Copyright (c) 2026 BUGS! Video Editor contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// Saves real files through the real engine and inspects the results with ffprobe. Needs ffmpeg,
// ffprobe and melt-7 (or melt) on the PATH, and a display for MLT's Qt module (xvfb-run is fine).

#include "collage.h"
#include "simpleedit.h"
#include "simplemedia.h"

#include <Mlt.h>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTransform>
#include <QtTest/QtTest>

#include <deque>

using namespace Simple;

namespace {

QByteArray sha1(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(&file);
    return hash.result();
}

bool run(const QString &program, const QStringList &args, QByteArray *output = nullptr)
{
    QProcess process;
    process.setProcessChannelMode(output ? QProcess::SeparateChannels : QProcess::MergedChannels);
    process.start(program, args);
    if (!process.waitForFinished(300000))
        return false;
    if (output)
        *output = process.readAllStandardOutput();
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

bool ffmpeg(const QStringList &args)
{
    return run(QStringLiteral("ffmpeg"),
               QStringList{QStringLiteral("-hide_banner"),
                           QStringLiteral("-loglevel"),
                           QStringLiteral("error"),
                           QStringLiteral("-y")}
                   + args);
}

struct Probe
{
    QString videoCodec;
    QString audioCodec;
    QString pixelFormat;
    QString colorTransfer;
    int width{0};
    int height{0};
    double duration{0};
    int videoFrames{0};
    int rotation{0};
    bool faststart{false};
};

Probe probe(const QString &path)
{
    QByteArray json;
    run(QStringLiteral("ffprobe"),
        {QStringLiteral("-v"),
         QStringLiteral("error"),
         QStringLiteral("-count_frames"),
         QStringLiteral("-show_streams"),
         QStringLiteral("-show_format"),
         QStringLiteral("-of"),
         QStringLiteral("json"),
         path},
        &json);
    Probe result;
    const auto root = QJsonDocument::fromJson(json).object();
    result.duration
        = root[QStringLiteral("format")].toObject()[QStringLiteral("duration")].toString().toDouble();
    for (const auto &value : root[QStringLiteral("streams")].toArray()) {
        const auto stream = value.toObject();
        if (stream[QStringLiteral("codec_type")].toString() == QLatin1String("video")) {
            result.videoCodec = stream[QStringLiteral("codec_name")].toString();
            result.pixelFormat = stream[QStringLiteral("pix_fmt")].toString();
            result.colorTransfer = stream[QStringLiteral("color_transfer")].toString();
            result.width = stream[QStringLiteral("width")].toInt();
            result.height = stream[QStringLiteral("height")].toInt();
            result.videoFrames = stream[QStringLiteral("nb_read_frames")].toString().toInt();
            for (const auto &side : stream[QStringLiteral("side_data_list")].toArray())
                result.rotation = side.toObject()[QStringLiteral("rotation")].toInt();
        } else if (stream[QStringLiteral("codec_type")].toString() == QLatin1String("audio")) {
            result.audioCodec = stream[QStringLiteral("codec_name")].toString();
        }
    }
    // Fast start puts the index before the video data, so sharing and streaming work.
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        const QByteArray head = file.read(4 * 1024 * 1024);
        const int moov = head.indexOf("moov");
        const int mdat = head.indexOf("mdat");
        result.faststart = moov >= 0 && (mdat < 0 || moov < mdat);
    }
    return result;
}

QImage frameFromFile(const QString &path, double seconds, const QString &scratch)
{
    const QString out = scratch + QStringLiteral("/frame.png");
    QFile::remove(out);
    ffmpeg({QStringLiteral("-ss"),
            QString::number(seconds, 'f', 3),
            QStringLiteral("-i"),
            path,
            QStringLiteral("-frames:v"),
            QStringLiteral("1"),
            QStringLiteral("-update"),
            QStringLiteral("1"),
            out});
    return QImage(out).convertToFormat(QImage::Format_RGB32);
}

// Average difference per colour channel, from 0 (identical) to 255.
double difference(const QImage &a, const QImage &b)
{
    if (a.size() != b.size() || a.isNull())
        return 255.0;
    const QImage x = a.convertToFormat(QImage::Format_RGB32);
    const QImage y = b.convertToFormat(QImage::Format_RGB32);
    qint64 total = 0;
    qint64 count = 0;
    for (int row = 0; row < x.height(); row += 3) {
        const auto *p = reinterpret_cast<const QRgb *>(x.constScanLine(row));
        const auto *q = reinterpret_cast<const QRgb *>(y.constScanLine(row));
        for (int col = 0; col < x.width(); col += 3) {
            total += qAbs(qRed(p[col]) - qRed(q[col])) + qAbs(qGreen(p[col]) - qGreen(q[col]))
                     + qAbs(qBlue(p[col]) - qBlue(q[col]));
            count += 3;
        }
    }
    return double(total) / count;
}

} // namespace

class TestSimplePipeline : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    QString m_landscape;
    QString m_portrait;
    QString m_vfr;
    QString m_hdr;
    QString m_old;
    QString m_jpg;
    QString m_png;
    QString m_unicode;
    QHash<QString, QByteArray> m_hashes;

    QString path(const QString &name) const { return m_dir.filePath(name); }

    QString save(const Edit &edit, const QString &name)
    {
        const QString target = path(name);
        CompileOptions options;
        options.captionImage = writeCaptionImage(edit);
        options.target = target;
        const QString xmlPath = target + QStringLiteral(".mlt");
        QFile xml(xmlPath);
        if (!xml.open(QIODevice::WriteOnly))
            return QString();
        xml.write(compileXml(edit, options).toUtf8());
        xml.close();
        QString melt = QStandardPaths::findExecutable(QStringLiteral("melt-7"));
        if (melt.isEmpty())
            melt = QStandardPaths::findExecutable(QStringLiteral("melt"));
        QProcess process;
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.start(melt,
                      {QStringLiteral("-progress2"),
                       QStringLiteral("-abort"),
                       QStringLiteral("xml:") + QUrl::toPercentEncoding(xmlPath)});
        process.waitForFinished(300000);
        return QFile::exists(target) ? target : QString();
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(Mlt::Factory::init());
        const QString interBold = QFINDTESTDATA("../src/qml/views/simple/fonts/Inter-Bold.ttf");
        if (!interBold.isEmpty()) {
            const QDir fonts = QFileInfo(interBold).dir();
            for (const auto &file : fonts.entryList({QStringLiteral("*.ttf")}))
                QFontDatabase::addApplicationFont(fonts.filePath(file));
        }

        // Tagged BT.709 like phone videos. Untagged HD is ambiguous and decoders guess differently.
        const QString tone = QStringLiteral("sine=frequency=440:duration=%1");
        m_landscape = path(QStringLiteral("landscape.mp4"));
        QVERIFY(ffmpeg({"-f",
                        "lavfi",
                        "-i",
                        "testsrc2=size=1280x720:rate=30:duration=10",
                        "-f",
                        "lavfi",
                        "-i",
                        tone.arg(10),
                        "-c:v",
                        "libx264",
                        "-preset",
                        "ultrafast",
                        "-pix_fmt",
                        "yuv420p",
                        "-colorspace",
                        "bt709",
                        "-color_primaries",
                        "bt709",
                        "-color_trc",
                        "bt709",
                        "-c:a",
                        "aac",
                        m_landscape}));

        // Like an iPhone: HEVC stored sideways with a rotation tag.
        const QString rawPortrait = path(QStringLiteral("raw.mov"));
        QVERIFY(ffmpeg({"-f",
                        "lavfi",
                        "-i",
                        "testsrc2=size=1280x720:rate=30:duration=5",
                        "-f",
                        "lavfi",
                        "-i",
                        tone.arg(5),
                        "-c:v",
                        "libx265",
                        "-preset",
                        "ultrafast",
                        "-x265-params",
                        "log-level=error",
                        "-tag:v",
                        "hvc1",
                        "-pix_fmt",
                        "yuv420p",
                        "-colorspace",
                        "bt709",
                        "-color_primaries",
                        "bt709",
                        "-color_trc",
                        "bt709",
                        "-c:a",
                        "aac",
                        rawPortrait}));
        m_portrait = path(QStringLiteral("iphone portrait.mov"));
        QVERIFY(ffmpeg({"-display_rotation", "90", "-i", rawPortrait, "-c", "copy", m_portrait}));

        // Like WhatsApp: a variable frame rate.
        m_vfr = path(QStringLiteral("whatsapp.mp4"));
        QVERIFY(ffmpeg({"-f",
                        "lavfi",
                        "-i",
                        "testsrc2=size=848x480:rate=30:duration=6",
                        "-f",
                        "lavfi",
                        "-i",
                        tone.arg(6),
                        "-vf",
                        "setpts='if(lt(N,60),PTS,PTS*1.5)'",
                        "-fps_mode",
                        "vfr",
                        "-c:v",
                        "libx264",
                        "-preset",
                        "ultrafast",
                        "-pix_fmt",
                        "yuv420p",
                        "-colorspace",
                        "bt709",
                        "-color_primaries",
                        "bt709",
                        "-color_trc",
                        "bt709",
                        "-c:a",
                        "aac",
                        m_vfr}));

        // Like an iPhone in its default HDR mode.
        m_hdr = path(QStringLiteral("hdr.mov"));
        QVERIFY(
            ffmpeg({"-f",
                    "lavfi",
                    "-i",
                    "testsrc2=size=1280x720:rate=30:duration=3",
                    "-f",
                    "lavfi",
                    "-i",
                    tone.arg(3),
                    "-vf",
                    "format=yuv420p10le",
                    "-c:v",
                    "libx265",
                    "-preset",
                    "ultrafast",
                    "-x265-params",
                    "log-level=error:colorprim=bt2020:transfer=arib-std-b67:colormatrix=bt2020nc",
                    "-tag:v",
                    "hvc1",
                    "-c:a",
                    "aac",
                    m_hdr}));

        // An older format: non-square pixels, no audio.
        m_old = path(QStringLiteral("old.mpg"));
        QVERIFY(ffmpeg({"-f",
                        "lavfi",
                        "-i",
                        "testsrc2=size=720x576:rate=25:duration=4",
                        "-c:v",
                        "mpeg2video",
                        "-q:v",
                        "4",
                        "-aspect",
                        "16:9",
                        "-an",
                        m_old}));

        m_jpg = path(QStringLiteral("photo.jpg"));
        QVERIFY(ffmpeg(
            {"-f", "lavfi", "-i", "testsrc2=size=2000x1500:rate=1", "-frames:v", "1", m_jpg}));
        m_png = path(QStringLiteral("photo.png"));
        QVERIFY(ffmpeg({"-f",
                        "lavfi",
                        "-i",
                        "testsrc2=size=1200x800:rate=1",
                        "-frames:v",
                        "1",
                        "-update",
                        "1",
                        m_png}));

        m_unicode = path(QStringLiteral("סרט 100% חדש.mp4"));
        QVERIFY(QFile::copy(m_landscape, m_unicode));

        for (const auto &file :
             {m_landscape, m_portrait, m_vfr, m_hdr, m_old, m_jpg, m_png, m_unicode})
            m_hashes.insert(file, sha1(file));
    }

    void cleanupTestCase()
    {
        // The core promise: after everything above and below, every original is byte-identical.
        for (auto it = m_hashes.cbegin(); it != m_hashes.cend(); ++it)
            QCOMPARE(sha1(it.key()), it.value());
        Mlt::Factory::close();
    }

    void readsFacts()
    {
        Edit edit;
        QVERIFY(openMedia(m_landscape, &edit));
        QCOMPARE(edit.sourceSize, QSize(1280, 720));
        QCOMPARE(edit.frames, 300);
        QCOMPARE(edit.fpsNum, 30);
        QVERIFY(edit.hasAudio);

        QVERIFY(openMedia(m_portrait, &edit));
        QCOMPARE(edit.sourceSize, QSize(720, 1280));
        QVERIFY(edit.baseRotation == 90 || edit.baseRotation == 270);

        QVERIFY(openMedia(m_hdr, &edit));
        QVERIFY(edit.isHdr());

        QVERIFY(openMedia(m_old, &edit));
        QCOMPARE(edit.sourceSize.height(), 576);
        QVERIFY(qAbs(double(edit.sourceSize.width()) / 576 - 16.0 / 9.0) < 0.02);
        QCOMPARE(edit.fpsNum, 25);

        QVERIFY(openMedia(m_jpg, &edit));
        QVERIFY(edit.isPhoto);
        QCOMPARE(edit.sourceSize, QSize(2000, 1500));

        QVERIFY(!openMedia(path(QStringLiteral("does not exist.mp4")), &edit));
    }

    // The main job from the brief: trim the ends, cut the middle, caption, Finish.
    void trimCutCaptionFinish()
    {
        Edit edit;
        QVERIFY(openMedia(m_landscape, &edit));
        QVERIFY(edit.setTrim(30, 270));
        QVERIFY(edit.addCut(Range{100, 160}));
        edit.captionName = QStringLiteral("Bike Show 2026");
        edit.captionSecond = QStringLiteral("Lakewood, NJ");
        const QString out = save(edit, QStringLiteral("main.mp4"));
        QVERIFY(!out.isEmpty());
        const Probe p = probe(out);
        QCOMPARE(p.videoCodec, QStringLiteral("h264"));
        QCOMPARE(p.audioCodec, QStringLiteral("aac"));
        QCOMPARE(p.pixelFormat, QStringLiteral("yuv420p"));
        QCOMPARE(QSize(p.width, p.height), QSize(1280, 720));
        QCOMPARE(p.videoFrames, 180);
        QVERIFY2(qAbs(p.duration - 6.0) < 0.1, qPrintable(QString::number(p.duration)));
        QVERIFY(p.faststart);
    }

    void outputPicturesAreTheRightFrames()
    {
        Edit edit;
        QVERIFY(openMedia(m_landscape, &edit));
        edit.setTrim(30, 270);
        edit.addCut(Range{100, 160});
        const QString out = save(edit, QStringLiteral("frames.mp4"));
        QVERIFY(!out.isEmpty());
        // Just after the join the output must show source frame 160, not 100.
        const QImage expected = frameFromFile(m_landscape, 160 / 30.0, m_dir.path());
        const QImage wrong = frameFromFile(m_landscape, 100 / 30.0, m_dir.path());
        const QImage actual = frameFromFile(out, 70 / 30.0, m_dir.path());
        QVERIFY(difference(actual, expected) < difference(actual, wrong));
        QVERIFY(difference(actual, expected) < 6.0);
    }

    // "What he sees in the preview must match the saved file."
    void previewMatchesSavedFile()
    {
        Edit edit;
        QVERIFY(openMedia(m_landscape, &edit));
        edit.setCropShape(QStringLiteral("square"));
        edit.captionName = QStringLiteral("Bike Show 2026");
        edit.captionStyle = CaptionStyle::Bold;
        const QString out = save(edit, QStringLiteral("match.mp4"));
        QVERIFY(!out.isEmpty());
        const QImage preview = renderFrame(edit, 90);
        const QImage saved = frameFromFile(out, 3.0, m_dir.path());
        QCOMPARE(saved.size(), QSize(720, 720));
        const double diff = difference(preview, saved);
        QVERIFY2(diff < 6.0, qPrintable(QString::number(diff)));
        // And the caption really is in there: without it the difference is obvious.
        Edit plain = edit;
        plain.captionName.clear();
        QVERIFY(difference(renderFrame(plain, 90), saved) > diff + 1.0);
    }

    void portraitStaysPortrait()
    {
        Edit edit;
        QVERIFY(openMedia(m_portrait, &edit));
        edit.setTrim(30, 120);
        const QString out = save(edit, QStringLiteral("portrait.mp4"));
        QVERIFY(!out.isEmpty());
        const Probe p = probe(out);
        QCOMPARE(QSize(p.width, p.height), QSize(720, 1280));
        QCOMPARE(p.rotation, 0);
        QCOMPARE(p.videoCodec, QStringLiteral("h264"));
        QCOMPARE(p.videoFrames, 90);
        // Upright: matches what ffmpeg itself shows for the original, which obeys the tag.
        const QImage source = frameFromFile(m_portrait, 2.0, m_dir.path());
        const QImage saved = frameFromFile(out, 1.0, m_dir.path());
        QVERIFY2(difference(source, saved) < 6.0,
                 qPrintable(QString::number(difference(source, saved))));
    }

    void rotateAndFlip()
    {
        Edit edit;
        QVERIFY(openMedia(m_landscape, &edit));
        edit.setTrim(0, 30);
        edit.rotateClockwise();
        const QString rotated = save(edit, QStringLiteral("rotated.mp4"));
        QVERIFY(!rotated.isEmpty());
        Probe p = probe(rotated);
        QCOMPARE(QSize(p.width, p.height), QSize(720, 1280));
        const QImage source = frameFromFile(m_landscape, 0.5, m_dir.path());
        const QImage turned = source.transformed(QTransform().rotate(90));
        QVERIFY(difference(frameFromFile(rotated, 0.5, m_dir.path()), turned) < 6.0);

        Edit flipped;
        QVERIFY(openMedia(m_landscape, &flipped));
        flipped.setTrim(0, 30);
        flipped.toggleFlip();
        const QString mirrored = save(flipped, QStringLiteral("flipped.mp4"));
        QVERIFY(!mirrored.isEmpty());
        const QImage expected = source.flipped(Qt::Horizontal);
        QVERIFY(difference(frameFromFile(mirrored, 0.5, m_dir.path()), expected) < 6.0);
    }

    void cropTakesTheRightPart()
    {
        Edit edit;
        QVERIFY(openMedia(m_landscape, &edit));
        edit.setTrim(0, 30);
        edit.setCropShape(QStringLiteral("free"));
        edit.setCropRect(QRectF(0.5, 0.5, 0.5, 0.5));
        const QString out = save(edit, QStringLiteral("crop.mp4"));
        QVERIFY(!out.isEmpty());
        const QImage saved = frameFromFile(out, 0.5, m_dir.path());
        QCOMPARE(saved.size(), QSize(640, 360));
        const QImage source = frameFromFile(m_landscape, 0.5, m_dir.path());
        QVERIFY(difference(saved, source.copy(640, 360, 640, 360)) < 6.0);
    }

    void variableFrameRateKeepsItsLength()
    {
        Edit edit;
        QVERIFY(openMedia(m_vfr, &edit));
        const Probe source = probe(m_vfr);
        const QString out = save(edit, QStringLiteral("vfr.mp4"));
        QVERIFY(!out.isEmpty());
        const Probe p = probe(out);
        QVERIFY2(qAbs(p.duration - source.duration) < 0.3,
                 qPrintable(QStringLiteral("%1 vs %2").arg(p.duration).arg(source.duration)));
        QCOMPARE(p.audioCodec, QStringLiteral("aac"));
    }

    void hdrBecomesStandardVideo()
    {
        Edit edit;
        QVERIFY(openMedia(m_hdr, &edit));
        const QString out = save(edit, QStringLiteral("hdr-out.mp4"));
        QVERIFY(!out.isEmpty());
        const Probe p = probe(out);
        QCOMPARE(p.videoCodec, QStringLiteral("h264"));
        QCOMPARE(p.pixelFormat, QStringLiteral("yuv420p"));
        QVERIFY(p.colorTransfer != QLatin1String("arib-std-b67"));
    }

    void oldFormatWithoutAudioStillSaves()
    {
        Edit edit;
        QVERIFY(openMedia(m_old, &edit));
        const QString out = save(edit, QStringLiteral("old-out.mp4"));
        QVERIFY(!out.isEmpty());
        const Probe p = probe(out);
        QCOMPARE(p.videoCodec, QStringLiteral("h264"));
        QCOMPARE(p.width % 2, 0);
        QVERIFY(qAbs(double(p.width) / p.height - 16.0 / 9.0) < 0.02);
    }

    // Old cameras did not tag their colours. The saved video must still look like the original.
    void oldFormatKeepsItsColours()
    {
        Edit edit;
        QVERIFY(openMedia(m_old, &edit));
        const QString out = save(edit, QStringLiteral("old-colours.mp4"));
        QVERIFY(!out.isEmpty());
        const QSize size = edit.outputSize();
        const QImage original = frameFromFile(m_old, 2.0, m_dir.path())
                                    .scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        const QImage video = frameFromFile(out, 2.0, m_dir.path());
        const double videoDiff = difference(video, original);
        QVERIFY2(videoDiff < 6.0, qPrintable(QString::number(videoDiff)));
    }

    void unicodeAndPercentInPaths()
    {
        Edit edit;
        QVERIFY(openMedia(m_unicode, &edit));
        edit.setTrim(0, 30);
        const QString out = save(edit, QStringLiteral("שמור 50%.mp4"));
        QVERIFY(!out.isEmpty());
        QCOMPARE(probe(out).videoFrames, 30);
    }

    void savedPictureIsFullSize()
    {
        Edit edit;
        QVERIFY(openMedia(m_landscape, &edit));
        const QImage picture = renderFrame(edit, 150);
        QCOMPARE(picture.size(), QSize(1280, 720));
        QVERIFY(difference(picture, frameFromFile(m_landscape, 5.0, m_dir.path())) < 4.0);
    }

    void photoCropToSquare()
    {
        Edit edit;
        QVERIFY(openMedia(m_jpg, &edit));
        edit.setCropShape(QStringLiteral("square"));
        const QImage picture = renderFrame(edit, 0);
        QCOMPARE(picture.size(), QSize(1500, 1500));
        const QImage source(m_jpg);
        QVERIFY(difference(picture, source.copy(250, 0, 1500, 1500)) < 4.0);
    }

    void photoRotatedAndCaptioned()
    {
        Edit edit;
        QVERIFY(openMedia(m_png, &edit));
        edit.rotateClockwise();
        edit.captionName = QStringLiteral("Hello");
        QCOMPARE(renderFrame(edit, 0).size(), QSize(800, 1200));
    }

    // The picture itself must turn, not just the frame around it.
    void photoRotateAndFlipTurnThePicture()
    {
        Edit edit;
        QVERIFY(openMedia(m_png, &edit));
        edit.rotateClockwise();
        edit.toggleFlip();
        const QImage picture = renderFrame(edit, 0);
        const QImage expected
            = QImage(m_png).transformed(QTransform().rotate(90)).flipped(Qt::Horizontal);
        QCOMPARE(picture.size(), expected.size());
        const double diff = difference(picture, expected);
        QVERIFY2(diff < 4.0, qPrintable(QString::number(diff)));
    }

    // Names placed on a photo land where they were put, in their colours and fonts.
    void labelsAreDrawnWhereTheyWerePlaced()
    {
        Edit edit;
        QVERIFY(openMedia(m_jpg, &edit));
        TextLabel pink;
        pink.text = QStringLiteral("MMMM");
        pink.center = QPointF(0.25, 0.25);
        pink.size = 2;
        TextLabel blue = pink;
        blue.center = QPointF(0.75, 0.75);
        blue.color = QStringLiteral("#2F80FF");
        blue.font = QStringLiteral("rounded");
        edit.labels << pink << blue;
        const QImage picture = renderFrame(edit, 0);
        const QImage plain = QImage(m_jpg).convertToFormat(QImage::Format_RGB32);
        const auto colourNear = [&](const QPointF &center, const QColor &want) {
            const QPoint c(qRound(center.x() * picture.width()),
                           qRound(center.y() * picture.height()));
            int hits = 0;
            for (int y = c.y() - 60; y < c.y() + 60; ++y) {
                for (int x = c.x() - 200; x < c.x() + 200; ++x) {
                    const QColor got = picture.pixelColor(x, y);
                    if (qAbs(got.red() - want.red()) + qAbs(got.green() - want.green())
                            + qAbs(got.blue() - want.blue())
                        < 40)
                        ++hits;
                }
            }
            return hits;
        };
        QVERIFY(colourNear(pink.center, QColor(pink.color)) > 500);
        QVERIFY(colourNear(blue.center, QColor(blue.color)) > 500);
        // And nowhere else: the opposite corners are untouched.
        QVERIFY(colourNear(QPointF(0.75, 0.25), QColor(pink.color)) < 20);
        QVERIFY(difference(picture.copy(0,
                                        picture.height() / 2,
                                        picture.width() / 2,
                                        picture.height() / 2),
                           plain.copy(0, plain.height() / 2, plain.width() / 2, plain.height() / 2))
                < 1.0);
    }

    // Each photo fills its own cell, the border goes round the edge, and the picture is the size
    // asked for.
    void collageDrawsEachPhotoInItsCell()
    {
        const QColor colours[] = {Qt::red, Qt::green, Qt::blue, Qt::yellow};
        const QSize shapes[] = {{1200, 900}, {900, 1200}, {1000, 1000}, {1600, 900}};
        std::deque<Collage::Photo> photos;
        std::vector<Collage::Photo *> pointers;
        QHash<QString, QColor> colourOf;
        for (int i = 0; i < 4; ++i) {
            QImage image(shapes[i], QImage::Format_RGB32);
            image.fill(colours[i]);
            const QString file = path(QStringLiteral("solid%1.png").arg(i));
            QVERIFY(image.save(file));
            QSize size;
            const QString readable = uprightPhotoPath(file, &size);
            QCOMPARE(size, shapes[i]);
            photos.push_back(Collage::Photo{readable, size.width(), size.height()});
            pointers.push_back(&photos.back());
            colourOf.insert(readable, colours[i]);
        }
        auto page = Collage::makeGaplessPage(pointers, 16.0 / 9.0, 7);
        const Collage::Layout layout = Collage::layoutFromPage(*page, pointers);
        const QSize size(1350, 2400);
        CollageStyle style;
        style.border = 0.02;
        style.borderColor = Qt::white;
        const QImage collage = renderCollage(layout, {}, size, style, [&](int i, const QSize &cover) {
            return loadPhotoToCover(pointers[size_t(i)]->path, cover);
        });
        QCOMPARE(collage.size(), size);
        const QVector<QRect> rects = collageCellRects(layout, size);
        QCOMPARE(rects.size(), 4);
        for (int i = 0; i < 4; ++i)
            QCOMPARE(collage.pixelColor(rects[i].center()),
                     colourOf.value(pointers[size_t(layout[size_t(i)].photo)]->path));
        QCOMPARE(collage.pixelColor(2, 2), QColor(Qt::white));
        QCOMPARE(collage.pixelColor(size.width() - 3, size.height() - 3), QColor(Qt::white));
    }

    // Text and stickers land where they were put, turned, in front of the photos, and look the
    // same drawn small (the preview) and full size (the saved picture).
    void collageItemsAreDrawnInPlace()
    {
        QImage red(100, 100, QImage::Format_ARGB32_Premultiplied);
        red.fill(Qt::red);
        const StickerImage sticker = [&](const QString &id) {
            return id == QLatin1String("red") ? red : QImage();
        };
        CollageItem square;
        square.kind = CollageItem::Kind::Sticker;
        square.sticker = QStringLiteral("red");
        square.center = QPointF(0.25, 0.75);
        square.size = 0.2;
        CollageItem turned = square;
        turned.center = QPointF(0.75, 0.25);
        turned.rotation = 45;
        CollageItem words;
        words.text = QStringLiteral("מזל טוב");
        words.center = QPointF(0.5, 0.5);
        words.size = 0.1;
        const QVector<CollageItem> items{square, turned, words};

        const auto draw = [&](int side) {
            QImage page(side, side, QImage::Format_RGB32);
            page.fill(Qt::black);
            QPainter painter(&page);
            paintCollageItems(painter, items, page.size(), sticker);
            painter.end();
            return page;
        };
        const QImage big = draw(2400);
        // The square fills its spot and stops at its edge: 0.2 of 2400 is 480 px, 240 each way.
        QCOMPARE(big.pixelColor(600, 1800), QColor(Qt::red));
        QCOMPARE(big.pixelColor(600 + 260, 1800), QColor(Qt::black));
        // Turned 45°, its corners point up, down and sideways: straight above the middle is red
        // further out than a plain square reaches, and its old corner is empty.
        QCOMPARE(big.pixelColor(1800, 600 - 300), QColor(Qt::red));
        QCOMPARE(big.pixelColor(1800 + 230, 600 - 230), QColor(Qt::black));
        // The words are drawn in white at the middle.
        int white = 0;
        for (int y = 1100; y < 1300; y += 2) {
            for (int x = 900; x < 1500; x += 2) {
                if (qRed(big.pixel(x, y)) > 240 && qGreen(big.pixel(x, y)) > 240)
                    ++white;
            }
        }
        QVERIFY2(white > 200, qPrintable(QString::number(white)));
        // The preview draws the same picture, only smaller.
        const QImage small = draw(600);
        const QImage shrunk = big.scaled(600, 600, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        QVERIFY2(difference(small, shrunk) < 6.0,
                 qPrintable(QString::number(difference(small, shrunk))));
    }

    // Space between photos shows the background, and rounded corners show it at each corner.
    void collageSpacingAndCornersShowTheBackground()
    {
        std::deque<Collage::Photo> photos;
        std::vector<Collage::Photo *> pointers;
        for (int i = 0; i < 2; ++i) {
            QImage image(QSize(1000, 1000), QImage::Format_RGB32);
            image.fill(Qt::blue);
            const QString file = path(QStringLiteral("blue%1.png").arg(i));
            QVERIFY(image.save(file));
            photos.push_back(Collage::Photo{file, 1000, 1000});
            pointers.push_back(&photos.back());
        }
        auto page = Collage::makeGaplessPage(pointers, 1.0, 1);
        const Collage::Layout layout = Collage::layoutFromPage(*page, pointers);
        CollageStyle style;
        style.border = 0.05;
        style.radius = 0.06;
        style.borderColor = Qt::yellow;
        const QSize size(1000, 1000);
        const QImage collage = renderCollage(layout, {}, size, style, [&](int i, const QSize &cover) {
            return loadPhotoToCover(pointers[size_t(i)]->path, cover);
        });
        const QVector<QRect> rects = collageCellRects(layout, size);
        for (const QRect &rect : rects) {
            QCOMPARE(collage.pixelColor(rect.center()), QColor(Qt::blue));
            // Just inside the frame, at the corner, the rounding leaves background.
            QCOMPARE(collage.pixelColor(rect.left() + 52, rect.top() + 52), QColor(Qt::yellow));
        }
        QCOMPARE(collage.pixelColor(20, 500), QColor(Qt::yellow));
    }

    // Zoom and move show the chosen part of a photo, in the preview and the saved picture alike.
    void collagePhotoZoomShowsThePartChosen()
    {
        // Left half red, right half blue.
        QImage photo(QSize(1000, 1000), QImage::Format_RGB32);
        photo.fill(Qt::blue);
        QPainter painter(&photo);
        painter.fillRect(0, 0, 500, 1000, Qt::red);
        painter.end();
        const Collage::Layout layout{Collage::Slot{QRectF(0, 0, 1, 1), 0}};
        const auto draw = [&](const Collage::PhotoCrop &crop) {
            return renderCollage(layout,
                                 {crop},
                                 QSize(400, 400),
                                 CollageStyle(),
                                 [&](int, const QSize &) { return photo; });
        };
        // Zoom 1: the whole photo, so both colours.
        const QImage whole = draw(Collage::PhotoCrop());
        QCOMPARE(whole.pixelColor(50, 200), QColor(Qt::red));
        QCOMPARE(whole.pixelColor(350, 200), QColor(Qt::blue));
        // Zoom 2 on the left half: all red.
        const QImage left = draw(Collage::PhotoCrop{2.0, 0.25, 0.5});
        QCOMPARE(left.pixelColor(20, 200), QColor(Qt::red));
        QCOMPARE(left.pixelColor(380, 200), QColor(Qt::red));
        // Moved past the edge, it stops at the edge rather than showing beyond the photo.
        const QImage right = draw(Collage::PhotoCrop{2.0, 1.5, 0.5});
        QCOMPARE(right.pixelColor(20, 200), QColor(Qt::blue));
        QCOMPARE(right.pixelColor(380, 200), QColor(Qt::blue));
    }

    void captionPictureHasText()
    {
        Edit edit;
        QVERIFY(openMedia(m_landscape, &edit));
        edit.captionName = QStringLiteral("שלום Levi");
        const QImage image = renderCaption(edit, QSize(1280, 720));
        QCOMPARE(qAlpha(image.pixel(10, 10)), 0);
        int white = 0;
        for (int y = 500; y < 720; ++y) {
            for (int x = 0; x < 1280; ++x) {
                if (qRed(image.pixel(x, y)) > 240 && qAlpha(image.pixel(x, y)) > 240)
                    ++white;
            }
        }
        QVERIFY(white > 100);
    }

    // Long words placed at the edge used to grow off the picture. They are kept on it, starting
    // at the edge, as the preview shows them.
    void labelsAtTheEdgeStayOnThePicture()
    {
        Edit edit;
        QVERIFY(openMedia(m_landscape, &edit));
        TextLabel label;
        label.text = QStringLiteral("Rabbi Shlomo Chaim Kanarek");
        label.center = QPointF(0.0, 0.5);
        label.size = 2;
        edit.labels << label;
        const QSize frame(1280, 720);
        const QImage image = renderCaption(edit, frame);
        int left = frame.width();
        int right = -1;
        for (int y = 0; y < frame.height(); ++y) {
            for (int x = 0; x < frame.width(); ++x) {
                if (qAlpha(image.pixel(x, y)) > 128) {
                    left = qMin(left, x);
                    right = qMax(right, x);
                }
            }
        }
        // Starts at the left edge, and all of the words fit: they reach well past the middle.
        QVERIFY2(left < 12, qPrintable(QString::number(left)));
        QVERIFY2(right > frame.width() / 2, qPrintable(QString::number(right)));
    }

    void thumbnailsWork()
    {
        QVERIFY(!thumbnailFor(m_landscape).isEmpty());
        QVERIFY(!thumbnailFor(m_portrait).isEmpty());
        QVERIFY(!thumbnailFor(m_jpg).isEmpty());
    }
};

QTEST_MAIN(TestSimplePipeline)
#include "test_simple_pipeline.moc"
