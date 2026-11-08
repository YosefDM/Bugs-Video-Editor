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
#include <QFontMetricsF>
#include <QImageReader>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextOption>
#include <QTransform>
#include <QtMath>

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

namespace {
void drawCaption(QPainter &painter, const Edit &edit, const QSize &frame);
void drawLabels(QPainter &painter, const Edit &edit, const QSize &frame);
} // namespace

QImage renderCaption(const Edit &edit, const QSize &frame)
{
    QImage image(frame, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    if (edit.hasCaption())
        drawCaption(painter, edit, frame);
    drawLabels(painter, edit, frame);
    painter.end();
    return image;
}

namespace {

void drawCaption(QPainter &painter, const Edit &edit, const QSize &frame)
{
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
}

// Each label is centred on its point, in bold, with a soft shadow so it reads on any picture.
// The Text tool draws the same thing live in Vui.qml while labels are being placed.
void drawLabels(QPainter &painter, const Edit &edit, const QSize &frame)
{
    const int shorter = qMin(frame.width(), frame.height());
    for (const auto &label : edit.labels) {
        if (label.isEmpty())
            continue;
        QFont font(labelFontFamily(label.font));
        font.setWeight(QFont::Bold);
        font.setPixelSize(qMax(8, qRound(labelFontFraction(label.size) * shorter)));
        painter.setFont(font);
        const QString text = label.text.trimmed();
        const int flags = Qt::AlignHCenter | Qt::AlignVCenter;
        QRect box = painter.boundingRect(QRect(0, 0, frame.width() * 4, frame.height() * 4),
                                         flags,
                                         text);
        box.moveCenter(QPoint(qRound(label.center.x() * frame.width()),
                              qRound(label.center.y() * frame.height())));
        // Longer words grow both ways from the centre, so they could run off the side. Keep them on
        // the picture; Vui.qml does the same, so the preview matches.
        if (box.width() <= frame.width())
            box.moveLeft(qBound(0, box.left(), frame.width() - box.width()));
        if (box.height() <= frame.height())
            box.moveTop(qBound(0, box.top(), frame.height() - box.height()));
        const int shadow = qMax(1, font.pixelSize() / 16);
        painter.setPen(QColor(0, 0, 0, 150));
        painter.drawText(box.translated(shadow, shadow), flags, text);
        painter.setPen(QColor(label.color));
        painter.drawText(box, flags, text);
    }
}

} // namespace

QString writeCaptionImage(const Edit &edit)
{
    if (!edit.hasOverlay())
        return QString();
    const QSize frame = edit.outputSize();
    QString key = QStringLiteral("%1|%2|%3|%4x%5")
                      .arg(edit.hasCaption() ? edit.captionName : QString(),
                           edit.captionSecond,
                           QString::number(int(edit.captionStyle)))
                      .arg(frame.width())
                      .arg(frame.height());
    for (const auto &label : edit.labels) {
        key += QStringLiteral("|%1@%2,%3:%4%5")
                   .arg(label.text)
                   .arg(label.center.x(), 0, 'f', 5)
                   .arg(label.center.y(), 0, 'f', 5)
                   .arg(label.size)
                   .arg(label.color + label.font);
    }
    const QString name = QString::fromLatin1(
        QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex().left(24));
    const QString target
        = QDir(cacheDir()).filePath(QStringLiteral("caption-") + name + QStringLiteral(".png"));
    if (!QFileInfo::exists(target)) {
        // Light compression: this is a working file, redrawn each time the words change, and
        // full compression of a full-size picture is slow. Written under another
        // name first, because it may be made off the screen's thread while the preview asks
        // for it, and a half-written picture must never be read.
        QSaveFile file(target);
        if (file.open(QIODevice::WriteOnly) && renderCaption(edit, frame).save(&file, "PNG", 90))
            file.commit();
    }
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

QString uprightPhotoPath(const QString &path, QSize *size)
{
    QString readable = path;
    if (isHeic(path)) {
        readable = heicWorkingCopy(path);
        if (readable.isEmpty())
            return QString();
    }
    QImageReader reader(readable);
    reader.setAutoTransform(true);
    QSize raw = reader.size();
    if (!raw.isValid())
        return QString();
    // The reader reports the stored size; a quarter turn swaps it.
    if (reader.transformation() & QImageIOHandler::TransformationRotate90)
        raw.transpose();
    if (size)
        *size = raw;
    return readable;
}

QImage loadPhotoToCover(const QString &readablePath, const QSize &cover)
{
    QImageReader reader(readablePath);
    reader.setAutoTransform(true);
    QSize stored = reader.size();
    const bool turned = reader.transformation() & QImageIOHandler::TransformationRotate90;
    const QSize upright = turned ? stored.transposed() : stored;
    if (upright.isValid() && cover.isValid()) {
        const double scale = qMax(double(cover.width()) / upright.width(),
                                  double(cover.height()) / upright.height());
        // Only ever shrink while reading; enlarging happens when drawing.
        if (scale < 1.0)
            reader.setScaledSize(QSize(qMax(1, qCeil(stored.width() * scale)),
                                       qMax(1, qCeil(stored.height() * scale))));
    }
    return reader.read();
}

QVector<QRect> collageCellRects(const Collage::Layout &layout, const QSize &size)
{
    // Edges are rounded on their own, so neighbouring cells share an edge with no gap.
    QVector<QRect> rects;
    for (const Collage::Slot &slot : layout) {
        const int left = qRound(slot.rect.left() * size.width());
        const int top = qRound(slot.rect.top() * size.height());
        const int right = qRound(slot.rect.right() * size.width());
        const int bottom = qRound(slot.rect.bottom() * size.height());
        rects << QRect(QPoint(left, top), QPoint(right - 1, bottom - 1));
    }
    return rects;
}

QRectF collageSourceRect(const QSize &picture, const QSizeF &frame, const Collage::PhotoCrop &crop)
{
    if (picture.isEmpty() || frame.isEmpty())
        return QRectF(QPointF(0, 0), QSizeF(picture));
    // The largest part with the frame's shape, then smaller by the zoom.
    const double frameRatio = frame.height() / frame.width();
    double w = picture.width();
    double h = w * frameRatio;
    if (h > picture.height()) {
        h = picture.height();
        w = h / frameRatio;
    }
    const double zoom = qBound(1.0, crop.zoom, 8.0);
    w /= zoom;
    h /= zoom;
    // Centred where the crop says, kept inside the picture.
    const double x = qBound(0.0, crop.cx * picture.width() - w / 2, picture.width() - w);
    const double y = qBound(0.0, crop.cy * picture.height() - h / 2, picture.height() - h);
    return QRectF(x, y, w, h);
}

QImage renderCollage(const Collage::Layout &layout,
                     const QVector<Collage::PhotoCrop> &crops,
                     const QSize &size,
                     const CollageStyle &style,
                     const std::function<QImage(int photo, const QSize &needed)> &image)
{
    QImage canvas(size, QImage::Format_RGB32);
    canvas.fill(style.borderColor);
    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setRenderHint(QPainter::Antialiasing);

    // The spacing shows the background between photos and round the edge, the same width
    // everywhere, as PhotoCollage's draw_borders() did with lines.
    const double shorter = qMin(size.width(), size.height());
    const double border = style.border * shorter;
    const double half = border / 2.0;
    const double radius = style.radius * shorter;
    const QVector<QRect> rects = collageCellRects(layout, size);
    for (int i = 0; i < int(layout.size()); ++i) {
        const QRect cell = rects.at(i);
        QRectF target(cell.x(), cell.y(), cell.width(), cell.height());
        target.adjust(cell.left() <= 0 ? border : half,
                      cell.top() <= 0 ? border : half,
                      cell.right() >= size.width() - 1 ? -border : -half,
                      cell.bottom() >= size.height() - 1 ? -border : -half);
        if (target.width() < 1 || target.height() < 1)
            continue;
        const int photo = layout[size_t(i)].photo;
        const Collage::PhotoCrop crop = photo >= 0 && photo < crops.size() ? crops.at(photo)
                                                                           : Collage::PhotoCrop();
        const double zoom = qBound(1.0, crop.zoom, 8.0);
        const QImage picture
            = image(photo, QSize(qCeil(target.width() * zoom), qCeil(target.height() * zoom)));
        if (picture.isNull())
            continue;
        const QRectF source = collageSourceRect(picture.size(), target.size(), crop);
        if (radius > 0) {
            QPainterPath clip;
            const double r = qMin(radius, qMin(target.width(), target.height()) / 2);
            clip.addRoundedRect(target, r, r);
            painter.save();
            painter.setClipPath(clip);
            painter.drawImage(target, picture, source);
            painter.restore();
        } else {
            painter.drawImage(target, picture, source);
        }
    }
    painter.end();
    return canvas;
}

namespace {

QFont collageTextFont(const CollageItem &item, const QSize &page)
{
    QFont font(labelFontFamily(item.font));
    font.setWeight(QFont::Bold);
    font.setPixelSize(qMax(4, qRound(item.size * qMin(page.width(), page.height()))));
    return font;
}

const int kTextFlags = Qt::AlignHCenter | Qt::AlignVCenter;

} // namespace

QSizeF collageItemSize(const CollageItem &item, const QSize &page, const StickerImage &sticker)
{
    const double shorter = qMin(page.width(), page.height());
    if (item.kind == CollageItem::Kind::Sticker) {
        const QImage image = sticker ? sticker(item.sticker) : QImage();
        const double w = item.size * shorter;
        const double ratio = image.isNull() || image.width() == 0
                                 ? 1.0
                                 : double(image.height()) / image.width();
        return QSizeF(w, w * ratio);
    }
    const QFontMetricsF metrics(collageTextFont(item, page));
    const QString text = item.text.trimmed().isEmpty() ? QStringLiteral(" ") : item.text.trimmed();
    const QRectF box = metrics.boundingRect(QRectF(0, 0, page.width() * 8, page.height() * 8),
                                            kTextFlags,
                                            text);
    // A little room for the shadow.
    const double pad = metrics.height() * 0.12;
    return QSizeF(box.width() + 2 * pad, box.height() + 2 * pad);
}

void paintCollageItemInBox(QPainter &painter,
                           const CollageItem &item,
                           const QSize &page,
                           const QRectF &box,
                           const StickerImage &sticker)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    if (item.kind == CollageItem::Kind::Sticker) {
        const QImage image = sticker ? sticker(item.sticker) : QImage();
        if (!image.isNull())
            painter.drawImage(box, image);
    } else {
        // Same look as text on photos and videos (drawLabels): bold, with a soft shadow.
        const QSizeF natural = collageItemSize(item, page, sticker);
        painter.translate(box.center());
        // The box may be a few pixels off its natural size on screen; scale to fill it exactly.
        if (natural.width() > 0 && natural.height() > 0)
            painter.scale(box.width() / natural.width(), box.height() / natural.height());
        const QFont font = collageTextFont(item, page);
        painter.setFont(font);
        const QRectF rect(-natural.width() / 2,
                          -natural.height() / 2,
                          natural.width(),
                          natural.height());
        const QString text = item.text.trimmed();
        const double shadow = qMax(1.0, font.pixelSize() / 16.0);
        painter.setPen(QColor(0, 0, 0, 150));
        painter.drawText(rect.translated(shadow, shadow), kTextFlags, text);
        painter.setPen(QColor(item.color));
        painter.drawText(rect, kTextFlags, text);
    }
    painter.restore();
}

void paintCollageItems(QPainter &painter,
                       const QVector<CollageItem> &items,
                       const QSize &page,
                       const StickerImage &sticker)
{
    for (const CollageItem &item : items) {
        const QSizeF size = collageItemSize(item, page, sticker);
        painter.save();
        painter.translate(item.center.x() * page.width(), item.center.y() * page.height());
        painter.rotate(item.rotation);
        paintCollageItemInBox(painter,
                              item,
                              page,
                              QRectF(-size.width() / 2,
                                     -size.height() / 2,
                                     size.width(),
                                     size.height()),
                              sticker);
        painter.restore();
    }
}

} // namespace Simple
