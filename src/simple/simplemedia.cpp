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

#include "simplemedia.h"

#include "Logger.h"

#include <Mlt.h>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QLinearGradient>
#include <QPainter>
#include <QProcess>
#include <QStandardPaths>
#include <QTextOption>
#include <QTransform>

namespace Simple {

namespace {

const QStringList kPhotoExtensions{QStringLiteral("jpg"),
                                   QStringLiteral("jpeg"),
                                   QStringLiteral("png"),
                                   QStringLiteral("heic"),
                                   QStringLiteral("heif"),
                                   QStringLiteral("webp"),
                                   QStringLiteral("bmp"),
                                   QStringLiteral("tif"),
                                   QStringLiteral("tiff")};
const QStringList kVideoExtensions{QStringLiteral("mp4"),
                                   QStringLiteral("mov"),
                                   QStringLiteral("m4v"),
                                   QStringLiteral("3gp"),
                                   QStringLiteral("3g2"),
                                   QStringLiteral("avi"),
                                   QStringLiteral("mkv"),
                                   QStringLiteral("webm"),
                                   QStringLiteral("wmv"),
                                   QStringLiteral("asf"),
                                   QStringLiteral("mpg"),
                                   QStringLiteral("mpeg"),
                                   QStringLiteral("vob"),
                                   QStringLiteral("mts"),
                                   QStringLiteral("m2ts"),
                                   QStringLiteral("ts"),
                                   QStringLiteral("flv")};

// Photo pixels are decoded once and shown as a still, so one frame of length is not enough for
// the player. This is ten seconds at 25 frames, which nobody ever sees: photos have no timeline.
const int kPhotoFrames = 250;
const int kStripHeight = 96;
const int kThumbWidth = 320;

QString hashFor(const QString &path)
{
    const QFileInfo info(path);
    const QByteArray key = path.toUtf8() + '|' + QByteArray::number(info.size()) + '|'
                           + QByteArray::number(info.lastModified().toMSecsSinceEpoch());
    return QString::fromLatin1(
        QCryptographicHash::hash(key, QCryptographicHash::Sha1).toHex().left(24));
}

QString findTool(const QString &name)
{
    const QFileInfo beside(QCoreApplication::applicationDirPath(), name);
    if (beside.exists())
        return beside.absoluteFilePath();
#ifdef Q_OS_WIN
    const QFileInfo besideExe(QCoreApplication::applicationDirPath(), name + ".exe");
    if (besideExe.exists())
        return besideExe.absoluteFilePath();
#endif
    return QStandardPaths::findExecutable(name);
}

bool isHeic(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == QLatin1String("heic") || suffix == QLatin1String("heif");
}

// HEIC needs a working copy because the engine reads photos through Qt, which does not include
// a decoder for it on Windows. The original is only read.
QString heicWorkingCopy(const QString &path)
{
    const QString target = QDir(cacheDir()).filePath(hashFor(path) + QStringLiteral(".png"));
    if (QFileInfo::exists(target))
        return target;
    QImageReader reader(path);
    reader.setAutoTransform(true);
    if (reader.canRead()) {
        const QImage image = reader.read();
        if (!image.isNull() && image.save(target))
            return target;
    }
    const QString ffmpeg = findTool(QStringLiteral("ffmpeg"));
    if (ffmpeg.isEmpty())
        return QString();
    const QString partial = target + QStringLiteral(".part.png");
    QProcess process;
    process.start(ffmpeg,
                  {QStringLiteral("-hide_banner"),
                   QStringLiteral("-loglevel"),
                   QStringLiteral("error"),
                   QStringLiteral("-y"),
                   QStringLiteral("-i"),
                   path,
                   QStringLiteral("-frames:v"),
                   QStringLiteral("1"),
                   QStringLiteral("-update"),
                   QStringLiteral("1"),
                   partial});
    process.waitForFinished(120000);
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        LOG_WARNING() << "HEIC conversion failed" << process.readAllStandardError();
        QFile::remove(partial);
        return QString();
    }
    return QFile::rename(partial, target) ? target : QString();
}

// Finds which stream index in the file is the video being used.
int videoStreamIndex(Mlt::Producer &producer)
{
    const int count = producer.get_int("meta.media.nb_streams");
    for (int i = 0; i < count; ++i) {
        const QByteArray key = QByteArrayLiteral("meta.media.") + QByteArray::number(i)
                               + QByteArrayLiteral(".stream.type");
        if (!qstrcmp(producer.get(key.constData()), "video"))
            return i;
    }
    return -1;
}

void setProfileFor(Mlt::Profile &profile, const Edit &edit, const QSize &frame)
{
    profile.set_width(frame.width());
    profile.set_height(frame.height());
    profile.set_progressive(1);
    profile.set_sample_aspect(1, 1);
    profile.set_display_aspect(frame.width(), frame.height());
    profile.set_frame_rate(edit.fpsNum, edit.fpsDen);
    profile.set_colorspace(709);
    profile.set_explicit(1);
}

// The same conversion Shotcut's Export Frame uses.
QImage frameImage(Mlt::Frame &frame, int width, int height, const char *rescale)
{
    frame.set("consumer.rescale", rescale);
    mlt_image_format format = mlt_image_rgba;
    const uchar *pixels = frame.get_image(format, width, height);
    return pixels ? QImage(pixels, width, height, QImage::Format_RGBA8888).copy() : QImage();
}

} // namespace

bool isPhotoPath(const QString &path)
{
    return kPhotoExtensions.contains(QFileInfo(path).suffix().toLower());
}

bool isMediaPath(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return kPhotoExtensions.contains(suffix) || kVideoExtensions.contains(suffix);
}

QString cacheDir()
{
    const QString dir = QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
                            .filePath(QStringLiteral("simple"));
    QDir().mkpath(dir);
    return dir;
}

QString finishedDir()
{
    const QString dir = QDir(QStandardPaths::writableLocation(QStandardPaths::MoviesLocation))
                            .filePath(QStringLiteral("BUGS! Finished"));
    QDir().mkpath(dir);
    return dir;
}

bool openMedia(const QString &path, Edit *edit)
{
    if (!QFileInfo::exists(path))
        return false;
    Edit result;
    result.sourcePath = path;
    result.mediaPath = path;
    result.isPhoto = isPhotoPath(path);

    if (result.isPhoto) {
        if (isHeic(path)) {
            result.mediaPath = heicWorkingCopy(path);
            if (result.mediaPath.isEmpty())
                return false;
        }
        QImageReader reader(result.mediaPath);
        reader.setAutoTransform(true);
        const QImage image = reader.read();
        if (image.isNull())
            return false;
        result.sourceSize = image.size();
        result.frames = kPhotoFrames;
        result.fpsNum = 25;
        result.fpsDen = 1;
        result.hasAudio = false;
    } else {
        Mlt::Profile probeProfile;
        Mlt::Producer probe(probeProfile, "avformat", path.toUtf8().constData());
        if (!probe.is_valid() || probe.get_int("error"))
            return false;
        const int stream = videoStreamIndex(probe);
        if (stream < 0)
            return false;
        const QByteArray prefix = QByteArrayLiteral("meta.media.") + QByteArray::number(stream)
                                  + QByteArrayLiteral(".codec.");
        result.baseRotation = (probe.get_int((prefix + "rotate").constData()) % 360 + 360) % 360;
        const int transfer = probe.get_int((prefix + "color_trc").constData());
        if (transfer == 16)
            result.colorTransfer = QStringLiteral("smpte2084");
        else if (transfer == 18)
            result.colorTransfer = QStringLiteral("arib-std-b67");

        // Per-stream facts are known on open; the plain meta.media ones only after decoding.
        int width = probe.get_int((prefix + "width").constData());
        int height = probe.get_int((prefix + "height").constData());
        const double sarNum = probe.get_double("meta.media.sample_aspect_num");
        const double sarDen = probe.get_double("meta.media.sample_aspect_den");
        // Old formats use non-square pixels. Output is always square so it plays right anywhere.
        if (sarNum > 0 && sarDen > 0)
            width = qRound(width * sarNum / sarDen);
        if (result.baseRotation == 90 || result.baseRotation == 270)
            std::swap(width, height);
        if (width < 2 || height < 2)
            return false;
        result.sourceSize = QSize(width & ~1, height & ~1);

        const QByteArray streamPrefix = QByteArrayLiteral("meta.media.")
                                        + QByteArray::number(stream)
                                        + QByteArrayLiteral(".stream.");
        normalizeFrameRate(probe.get_double((streamPrefix + "frame_rate").constData()),
                           &result.fpsNum,
                           &result.fpsDen);
        result.hasAudio = probe.get_int("audio_index") >= 0;

        // The length in frames depends on the frame rate, so read it at the rate we will use.
        Mlt::Profile profile;
        setProfileFor(profile, result, result.sourceSize);
        Mlt::Producer sized(profile, "avformat", path.toUtf8().constData());
        if (!sized.is_valid())
            return false;
        result.frames = sized.get_length();
        if (result.frames < 1)
            return false;
    }
    result.reset();
    *edit = result;
    return true;
}

QString thumbnailFor(const QString &path)
{
    const QString target = QDir(cacheDir()).filePath(hashFor(path) + QStringLiteral("-thumb.jpg"));
    if (QFileInfo::exists(target))
        return target;
    QImage image;
    if (isPhotoPath(path)) {
        Edit edit;
        QString source = path;
        if (isHeic(path)) {
            if (!openMedia(path, &edit))
                return QString();
            source = edit.mediaPath;
        }
        QImageReader reader(source);
        reader.setAutoTransform(true);
        const QSize original = reader.size();
        if (original.isValid() && original.width() > kThumbWidth * 2)
            reader.setScaledSize(
                original.scaled(kThumbWidth * 2, kThumbWidth * 2, Qt::KeepAspectRatio));
        image = reader.read();
    } else {
        Edit edit;
        if (!openMedia(path, &edit))
            return QString();
        const QSize size = edit.sourceSize.scaled(kThumbWidth, kThumbWidth, Qt::KeepAspectRatio);
        const QSize frame(qMax(2, size.width() & ~1), qMax(2, size.height() & ~1));
        Mlt::Profile profile;
        setProfileFor(profile, edit, frame);
        Mlt::Producer producer(profile, "avformat", path.toUtf8().constData());
        if (producer.is_valid()) {
            // Past the first moments, which are often black or a shaky start.
            producer.seek(qMin(edit.frames - 1, int(edit.fps())));
            Mlt::Frame *mltFrame = producer.get_frame();
            if (mltFrame && mltFrame->is_valid()) {
                image = frameImage(*mltFrame, frame.width(), frame.height(), "bilinear");
            }
            delete mltFrame;
        }
    }
    if (image.isNull())
        return QString();
    image = image.scaled(kThumbWidth, kThumbWidth, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return image.convertToFormat(QImage::Format_RGB32).save(target, "JPG", 85) ? target : QString();
}

QStringList filmstripFor(const Edit &edit,
                         int count,
                         const std::function<bool(const QStringList &)> &progress)
{
    QStringList result;
    if (edit.isPhoto || edit.frames < 1 || count < 1)
        return result;
    const int width = qMax(2,
                           int(kStripHeight * double(edit.sourceSize.width())
                               / qMax(1, edit.sourceSize.height()))
                               & ~1);
    Mlt::Profile profile;
    setProfileFor(profile, edit, QSize(width, kStripHeight));
    Mlt::Producer producer(profile, "avformat", edit.mediaPath.toUtf8().constData());
    if (!producer.is_valid())
        return result;
    const QString prefix
        = QDir(cacheDir()).filePath(hashFor(edit.sourcePath) + QStringLiteral("-strip-"));
    for (int i = 0; i < count; ++i) {
        const QString target = prefix + QString::number(i) + QStringLiteral(".jpg");
        if (!QFileInfo::exists(target)) {
            producer.seek(qMin(edit.frames - 1, int((i + 0.5) * edit.frames / count)));
            Mlt::Frame *frame = producer.get_frame();
            if (frame && frame->is_valid()) {
                frameImage(*frame, width, kStripHeight, "bilinear").save(target, "JPG", 80);
            }
            delete frame;
        }
        result << target;
        if (progress && !progress(result))
            break;
    }
    return result;
}

QImage renderCaption(const Edit &edit, const QSize &frame)
{
    QImage image(frame, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    if (!edit.hasCaption())
        return image;

    // Sizes come from the mockup, where the preview is 820 wide. Tall videos use their shorter
    // side so the caption does not swallow a narrow picture.
    const double scale = qMax(frame.width(), qMin(frame.height(), frame.width() * 9 / 16)) / 820.0;
    const int paddingX = qRound(28 * scale);
    const int paddingTop = qRound(22 * scale);
    const int paddingBottom = qRound(20 * scale);
    const int gap = qRound(2 * scale);
    const int textWidth = qMax(1, frame.width() - paddingX * 2);

    QFont nameFont(QStringLiteral("Inter"));
    nameFont.setWeight(QFont::Bold);
    nameFont.setPixelSize(qMax(10, qRound(30 * scale)));
    QFont secondFont(QStringLiteral("Inter"));
    secondFont.setPixelSize(qMax(8, qRound(15 * scale)));

    const bool centered = edit.captionStyle == CaptionStyle::Center;
    const int flags = Qt::TextWordWrap | (centered ? Qt::AlignHCenter : Qt::AlignLeft)
                      | Qt::AlignTop;
    const QString name = edit.captionName.trimmed();
    const QString second = edit.captionSecond.trimmed();

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setFont(nameFont);
    const QRect nameBox = painter.boundingRect(QRect(0, 0, textWidth, frame.height()), flags, name);
    painter.setFont(secondFont);
    const QRect secondBox = second.isEmpty()
                                ? QRect()
                                : painter.boundingRect(QRect(0, 0, textWidth, frame.height()),
                                                       flags,
                                                       second);
    const int boxHeight = qMin(frame.height(),
                               paddingTop + nameBox.height()
                                   + (second.isEmpty() ? 0 : gap + secondBox.height())
                                   + paddingBottom);
    const QRect box(0, frame.height() - boxHeight, frame.width(), boxHeight);

    if (edit.captionStyle == CaptionStyle::Bold) {
        // Fixed, unlike the mockup's theme accent: the saved video must not depend on the theme.
        painter.fillRect(box, QColor(0x2F, 0x62, 0xE0, 0xE6));
    } else {
        QLinearGradient gradient(box.topLeft(), box.bottomLeft());
        gradient.setColorAt(0, QColor(0, 0, 0, 0));
        gradient.setColorAt(1, QColor(0, 0, 0, centered ? 0xB3 : 0xCC));
        painter.fillRect(box, gradient);
    }

    int y = box.top() + paddingTop;
    painter.setPen(Qt::white);
    painter.setFont(nameFont);
    painter.drawText(QRect(paddingX, y, textWidth, nameBox.height()), flags, name);
    if (!second.isEmpty()) {
        y += nameBox.height() + gap;
        painter.setPen(QColor(0xE8, 0xEA, 0xEE));
        painter.setFont(secondFont);
        painter.drawText(QRect(paddingX, y, textWidth, secondBox.height()), flags, second);
    }
    painter.end();
    return image;
}

QString writeCaptionImage(const Edit &edit)
{
    if (!edit.hasCaption())
        return QString();
    const QSize frame = edit.outputSize();
    const QString key = QStringLiteral("%1|%2|%3|%4x%5")
                            .arg(edit.captionName,
                                 edit.captionSecond,
                                 QString::number(int(edit.captionStyle)))
                            .arg(frame.width())
                            .arg(frame.height());
    const QString name = QString::fromLatin1(
        QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex().left(24));
    const QString target
        = QDir(cacheDir()).filePath(QStringLiteral("caption-") + name + QStringLiteral(".png"));
    if (!QFileInfo::exists(target))
        renderCaption(edit, frame).save(target, "PNG");
    return target;
}

QString photoForEdit(const Edit &edit)
{
    if (!edit.isPhoto || (edit.rotation == 0 && !edit.flipped))
        return edit.mediaPath;
    const QString target = QDir(cacheDir())
                               .filePath(QStringLiteral("%1-r%2%3.png")
                                             .arg(hashFor(edit.mediaPath))
                                             .arg(edit.rotation)
                                             .arg(edit.flipped ? QStringLiteral("f") : QString()));
    if (QFileInfo::exists(target))
        return target;
    QImageReader reader(edit.mediaPath);
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (image.isNull())
        return edit.mediaPath;
    // Turned first, then mirrored, the same order as for videos.
    image = image.transformed(QTransform().rotate(edit.rotation));
    if (edit.flipped)
        image = image.flipped(Qt::Horizontal);
    return image.save(target, "PNG") ? target : edit.mediaPath;
}

QImage renderFrame(const Edit &edit, int editedFrame)
{
    CompileOptions options;
    options.captionImage = writeCaptionImage(edit);
    options.photoPath = photoForEdit(edit);
    const QString xml = compileXml(edit, options);
    const QSize size = edit.outputSize();
    Mlt::Profile profile;
    setProfileFor(profile, edit, size);
    Mlt::Producer producer(profile, "xml-string", xml.toUtf8().constData());
    if (!producer.is_valid())
        return QImage();
    producer.seek(editedFrame);
    QImage image;
    Mlt::Frame *frame = producer.get_frame();
    if (frame && frame->is_valid()) {
        image = frameImage(*frame, size.width(), size.height(), "bicubic");
    }
    delete frame;
    return image;
}

} // namespace Simple
