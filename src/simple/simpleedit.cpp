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

#include "simpleedit.h"

#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QRegularExpression>
#include <QXmlStreamWriter>

#include <algorithm>
#include <cmath>

namespace Simple {

bool Edit::operator==(const Edit &o) const
{
    return sourcePath == o.sourcePath && trim == o.trim && cuts == o.cuts && rotation == o.rotation
           && flipped == o.flipped && crop == o.crop && cropShape == o.cropShape
           && captionName == o.captionName && captionSecond == o.captionSecond
           && captionStyle == o.captionStyle && labels == o.labels;
}

void Edit::reset()
{
    trim = Range{0, frames};
    cuts.clear();
    rotation = 0;
    flipped = false;
    crop = QRectF(0, 0, 1, 1);
    cropShape = QStringLiteral("original");
    captionName.clear();
    captionSecond.clear();
    captionStyle = CaptionStyle::Bar;
    labels.clear();
}

bool Edit::hasTimeChanges() const
{
    return !isPhoto && (trim.start != 0 || trim.end != frames || !cuts.isEmpty());
}

bool Edit::hasChanges() const
{
    return hasTimeChanges() || rotation != 0 || flipped || crop != QRectF(0, 0, 1, 1)
           || hasOverlay();
}

bool Edit::hasLabels() const
{
    for (const auto &label : labels) {
        if (!label.isEmpty())
            return true;
    }
    return false;
}

double labelFontFraction(int size)
{
    switch (size) {
    case 0:
        return 0.035;
    case 2:
        return 0.075;
    default:
        return 0.05;
    }
}

const QVector<LabelFont> &labelFonts()
{
    static const QVector<LabelFont> fonts{
        {QStringLiteral("clean"), QStringLiteral("Clean"), QStringLiteral("Inter")},
        {QStringLiteral("rounded"), QStringLiteral("Rounded"), QStringLiteral("Rubik")},
        {QStringLiteral("classic"), QStringLiteral("Classic"), QStringLiteral("Frank Ruhl Libre")},
        {QStringLiteral("headline"), QStringLiteral("Headline"), QStringLiteral("Bebas Neue")},
        {QStringLiteral("handwritten"), QStringLiteral("Handwritten"), QStringLiteral("Caveat")},
        {QStringLiteral("fancy"), QStringLiteral("Fancy"), QStringLiteral("Pacifico")},
    };
    return fonts;
}

QString labelFontFamily(const QString &id)
{
    for (const auto &font : labelFonts()) {
        if (font.id == id)
            return font.family;
    }
    return labelFonts().first().family;
}

const QStringList &labelColors()
{
    static const QStringList colors{QStringLiteral("#F04AD9"),
                                    QStringLiteral("#FFFFFF"),
                                    QStringLiteral("#FFD43B"),
                                    QStringLiteral("#E5392B"),
                                    QStringLiteral("#111111"),
                                    QStringLiteral("#2F80FF"),
                                    QStringLiteral("#2BD16B"),
                                    QStringLiteral("#FF8A1F"),
                                    QStringLiteral("#9B5CFF"),
                                    QStringLiteral("#21D4FD")};
    return colors;
}

namespace {

// WCAG relative luminance of an sRGB colour.
double luminance(QRgb rgb)
{
    const auto channel = [](int value) {
        const double c = value / 255.0;
        return c <= 0.03928 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(qRed(rgb)) + 0.7152 * channel(qGreen(rgb))
           + 0.0722 * channel(qBlue(rgb));
}

} // namespace

QString contrastingLabelColor(const QImage &picture, const QPointF &center)
{
    const QStringList &colors = labelColors();
    if (picture.isNull())
        return colors.first();
    // Roughly where a medium label's words sit: a band around its centre.
    const QRect area = QRect(int((center.x() - 0.15) * picture.width()),
                             int((center.y() - 0.04) * picture.height()),
                             qMax(1, int(0.3 * picture.width())),
                             qMax(1, int(0.08 * picture.height())))
                           .intersected(picture.rect());
    if (area.isEmpty())
        return colors.first();
    QVector<double> backdrop;
    const int step = qMax(1, qMax(area.width(), area.height()) / 40);
    for (int y = area.top(); y <= area.bottom(); y += step) {
        for (int x = area.left(); x <= area.right(); x += step)
            backdrop << luminance(picture.pixel(x, y));
    }
    QString best = colors.first();
    double bestScore = -1;
    for (const auto &color : colors) {
        const double l = luminance(QColor(color).rgb());
        double sum = 0;
        for (const double b : std::as_const(backdrop))
            sum += (qMax(l, b) + 0.05) / (qMin(l, b) + 0.05);
        const double score = sum / backdrop.size();
        if (score > bestScore) {
            bestScore = score;
            best = color;
        }
    }
    return best;
}

bool Edit::isHdr() const
{
    return colorTransfer == QLatin1String("arib-std-b67")
           || colorTransfer == QLatin1String("smpte2084");
}

QVector<Range> Edit::kept() const
{
    QVector<Range> result;
    int position = trim.start;
    for (const auto &cut : cuts) {
        if (cut.end <= position)
            continue;
        if (cut.start >= trim.end)
            break;
        if (cut.start > position)
            result << Range{position, cut.start};
        position = cut.end;
    }
    if (position < trim.end)
        result << Range{position, trim.end};
    return result;
}

int Edit::keptFrames() const
{
    int total = 0;
    for (const auto &range : kept())
        total += range.length();
    return total;
}

int Edit::editedFromSource(int sourceFrame) const
{
    int offset = 0;
    const auto ranges = kept();
    for (const auto &range : ranges) {
        if (sourceFrame < range.start)
            return offset; // inside a removed part: the first frame after it
        if (range.contains(sourceFrame))
            return offset + sourceFrame - range.start;
        offset += range.length();
    }
    return qMax(0, offset - 1);
}

int Edit::sourceFromEdited(int editedFrame) const
{
    int remaining = qMax(0, editedFrame);
    const auto ranges = kept();
    for (const auto &range : ranges) {
        if (remaining < range.length())
            return range.start + remaining;
        remaining -= range.length();
    }
    return ranges.isEmpty() ? trim.start : ranges.last().end - 1;
}

bool Edit::setTrim(int start, int end)
{
    start = qBound(0, start, frames);
    end = qBound(0, end, frames);
    if (end <= start)
        return false;
    Edit candidate = *this;
    candidate.trim = Range{start, end};
    if (candidate.keptFrames() < 1)
        return false;
    trim = candidate.trim;
    return true;
}

bool Edit::addCut(const Range &range)
{
    Range cut{qBound(trim.start, range.start, trim.end), qBound(trim.start, range.end, trim.end)};
    if (cut.isEmpty())
        return false;
    QVector<Range> merged;
    for (const auto &existing : cuts) {
        if (existing.end < cut.start || existing.start > cut.end) {
            merged << existing;
        } else {
            cut.start = qMin(cut.start, existing.start);
            cut.end = qMax(cut.end, existing.end);
        }
    }
    merged << cut;
    std::sort(merged.begin(), merged.end(), [](const Range &a, const Range &b) {
        return a.start < b.start;
    });
    Edit candidate = *this;
    candidate.cuts = merged;
    const int before = keptFrames();
    const int after = candidate.keptFrames();
    if (after < 1 || after == before)
        return false;
    cuts = merged;
    return true;
}

void Edit::removeCutAt(int index)
{
    if (index >= 0 && index < cuts.size())
        cuts.remove(index);
}

QSize Edit::rotatedSize() const
{
    return (rotation == 90 || rotation == 270) ? sourceSize.transposed() : sourceSize;
}

QRect Edit::cropPixels() const
{
    const QSize size = rotatedSize();
    // H.264 needs even sizes, so round down rather than stretch the picture.
    int x = qRound(crop.x() * size.width());
    int y = qRound(crop.y() * size.height());
    int w = qRound(crop.width() * size.width());
    int h = qRound(crop.height() * size.height());
    x = qBound(0, x, size.width() - 2);
    y = qBound(0, y, size.height() - 2);
    w = qBound(2, w, size.width() - x) & ~1;
    h = qBound(2, h, size.height() - y) & ~1;
    return QRect(x, y, w, h);
}

QSize Edit::outputSize() const
{
    return cropPixels().size();
}

void Edit::setCropShape(const QString &shape)
{
    cropShape = shape;
    if (shape != QLatin1String("free"))
        crop = cropForShape(shape, rotatedSize());
}

void Edit::setCropRect(const QRectF &rect)
{
    crop = rect.intersected(QRectF(0, 0, 1, 1));
}

void Edit::rotateClockwise()
{
    rotation = (rotation + 90) % 360;
    // A fixed shape stays that shape; a free box turns with the picture.
    if (cropShape == QLatin1String("free"))
        crop = QRectF(1.0 - crop.y() - crop.height(), crop.x(), crop.height(), crop.width());
    else
        crop = cropForShape(cropShape, rotatedSize());
}

void Edit::toggleFlip()
{
    flipped = !flipped;
    crop = QRectF(1.0 - crop.x() - crop.width(), crop.y(), crop.width(), crop.height());
}

double aspectForShape(const QString &shape)
{
    if (shape == QLatin1String("square"))
        return 1.0;
    if (shape == QLatin1String("9:16"))
        return 9.0 / 16.0;
    if (shape == QLatin1String("16:9"))
        return 16.0 / 9.0;
    if (shape == QLatin1String("4:5"))
        return 4.0 / 5.0;
    return 0.0;
}

QRectF cropForShape(const QString &shape, const QSize &picture)
{
    const double aspect = aspectForShape(shape);
    if (aspect <= 0.0 || picture.isEmpty())
        return QRectF(0, 0, 1, 1);
    const double pictureAspect = double(picture.width()) / picture.height();
    if (pictureAspect > aspect) {
        const double w = aspect / pictureAspect;
        return QRectF((1.0 - w) / 2.0, 0, w, 1);
    }
    const double h = pictureAspect / aspect;
    return QRectF(0, (1.0 - h) / 2.0, 1, h);
}

QString formatTime(double seconds, bool withTenths)
{
    seconds = qMax(0.0, seconds);
    // Work in tenths so 59.96 shows as 1:00 and not 0:60.
    qint64 tenths = qint64(std::llround(seconds * 10.0));
    if (!withTenths)
        tenths = (tenths + 5) / 10 * 10;
    const qint64 whole = tenths / 10;
    const int fraction = int(tenths % 10);
    const int h = int(whole / 3600);
    const int m = int((whole % 3600) / 60);
    const int s = int(whole % 60);
    QString text = h > 0 ? QStringLiteral("%1:%2:%3")
                               .arg(h)
                               .arg(m, 2, 10, QLatin1Char('0'))
                               .arg(s, 2, 10, QLatin1Char('0'))
                         : QStringLiteral("%1:%2").arg(m).arg(s, 2, 10, QLatin1Char('0'));
    if (withTenths && fraction)
        text += QStringLiteral(".%1").arg(fraction);
    return text;
}

bool parseTime(const QString &text, double *seconds)
{
    QString value = text.trimmed();
    value.replace(QLatin1Char(','), QLatin1Char('.'));
    static const QRegularExpression pattern(
        QStringLiteral("^(?:(?:(\\d+):)?(\\d+):)?(\\d+(?:\\.\\d*)?)$"));
    const auto match = pattern.match(value);
    if (!match.hasMatch())
        return false;
    const double result = match.captured(1).toDouble() * 3600.0
                          + match.captured(2).toDouble() * 60.0 + match.captured(3).toDouble();
    if (seconds)
        *seconds = result;
    return true;
}

QString formatDuration(double seconds)
{
    const int total = qMax(0, int(std::lround(seconds)));
    const int m = total / 60;
    const int s = total % 60;
    const QString secondsText = s == 1 ? QStringLiteral("1 second")
                                       : QStringLiteral("%1 seconds").arg(s);
    if (m == 0)
        return secondsText;
    const QString minutesText = m == 1 ? QStringLiteral("1 minute")
                                       : QStringLiteral("%1 minutes").arg(m);
    return s == 0 ? minutesText : minutesText + QLatin1Char(' ') + secondsText;
}

QString safeFileName(const QString &name)
{
    QString result = name;
    static const QRegularExpression notAllowed(QStringLiteral("[<>:\"/\\\\|?*\\x00-\\x1f]"));
    result.replace(notAllowed, QStringLiteral("-"));
    while (result.endsWith(QLatin1Char('.')) || result.endsWith(QLatin1Char(' ')))
        result.chop(1);
    return result.isEmpty() ? QStringLiteral("Untitled") : result;
}

QString uniquePath(const QString &dir, const QString &baseName, const QString &extension)
{
    const QDir folder(dir);
    const QString base = safeFileName(baseName);
    QString path = folder.filePath(base + QLatin1Char('.') + extension);
    for (int i = 2; QFileInfo::exists(path); ++i)
        path = folder.filePath(QStringLiteral("%1 (%2).%3").arg(base).arg(i).arg(extension));
    return path;
}

void normalizeFrameRate(double fps, int *num, int *den)
{
    static const struct
    {
        int num;
        int den;
    } standard[] = {{24000, 1001},
                    {24, 1},
                    {25, 1},
                    {30000, 1001},
                    {30, 1},
                    {50, 1},
                    {60000, 1001},
                    {60, 1},
                    {120, 1},
                    {240, 1}};
    if (fps <= 0.0 || fps > 1000.0 || std::isnan(fps))
        fps = 30.0;
    for (const auto &rate : standard) {
        const double candidate = double(rate.num) / rate.den;
        // 29.97 and 30 are 0.1% apart, so an exact-looking rate is kept first.
        if (std::abs(candidate - fps) / candidate < 0.0005) {
            *num = rate.num;
            *den = rate.den;
            return;
        }
    }
    for (const auto &rate : standard) {
        const double candidate = double(rate.num) / rate.den;
        if (std::abs(candidate - fps) / candidate < 0.02) {
            *num = rate.num;
            *den = rate.den;
            return;
        }
    }
    *num = int(std::lround(fps * 1000.0));
    *den = 1000;
}

namespace {

void writeProperty(QXmlStreamWriter &xml, const QString &name, const QString &value)
{
    xml.writeStartElement(QStringLiteral("property"));
    xml.writeAttribute(QStringLiteral("name"), name);
    xml.writeCharacters(value);
    xml.writeEndElement();
}

QString filterGraph(const Edit &edit)
{
    QStringList parts;
    if (edit.isHdr()) {
        // Phones film HDR by default. Without this the result looks washed out on most screens
        // and in WhatsApp. These are the settings Shotcut's own convert dialog uses.
        parts << QStringLiteral("zscale=t=linear:npl=100") << QStringLiteral("format=gbrpf32le")
              << QStringLiteral("zscale=p=bt709") << QStringLiteral("tonemap=tonemap=hable:desat=0")
              << QStringLiteral("zscale=t=bt709:m=bt709:r=tv") << QStringLiteral("format=yuv420p");
    }
    if (edit.flipped)
        parts << QStringLiteral("hflip");
    return parts.join(QLatin1Char(','));
}

} // namespace

QString compileXml(const Edit &edit, const CompileOptions &options)
{
    QString text;
    QXmlStreamWriter xml(&text);
    xml.setAutoFormatting(true);
    xml.writeStartElement(QStringLiteral("mlt"));
    xml.writeAttribute(QStringLiteral("LC_NUMERIC"), QStringLiteral("C"));
    xml.writeAttribute(QStringLiteral("version"), QStringLiteral("7.0.0"));
    xml.writeAttribute(QStringLiteral("title"), QStringLiteral("BUGS! edit"));
    xml.writeAttribute(QStringLiteral("producer"), QStringLiteral("main"));

    const QRect cropRect = options.applyCrop ? edit.cropPixels()
                                             : QRect(QPoint(0, 0), edit.rotatedSize());
    const QSize rotated = edit.rotatedSize();
    const QSize frame = cropRect.size();
    const bool isCropped = frame != rotated;

    if (!options.target.isEmpty()) {
        xml.writeEmptyElement(QStringLiteral("profile"));
        xml.writeAttribute(QStringLiteral("description"), QStringLiteral("BUGS!"));
        xml.writeAttribute(QStringLiteral("width"), QString::number(frame.width()));
        xml.writeAttribute(QStringLiteral("height"), QString::number(frame.height()));
        xml.writeAttribute(QStringLiteral("progressive"), QStringLiteral("1"));
        xml.writeAttribute(QStringLiteral("sample_aspect_num"), QStringLiteral("1"));
        xml.writeAttribute(QStringLiteral("sample_aspect_den"), QStringLiteral("1"));
        xml.writeAttribute(QStringLiteral("display_aspect_num"), QString::number(frame.width()));
        xml.writeAttribute(QStringLiteral("display_aspect_den"), QString::number(frame.height()));
        xml.writeAttribute(QStringLiteral("frame_rate_num"), QString::number(edit.fpsNum));
        xml.writeAttribute(QStringLiteral("frame_rate_den"), QString::number(edit.fpsDen));
        xml.writeAttribute(QStringLiteral("colorspace"), QStringLiteral("709"));

        xml.writeEmptyElement(QStringLiteral("consumer"));
        xml.writeAttribute(QStringLiteral("mlt_service"), QStringLiteral("avformat"));
        xml.writeAttribute(QStringLiteral("target"), options.target);
        xml.writeAttribute(QStringLiteral("f"), QStringLiteral("mp4"));
        xml.writeAttribute(QStringLiteral("movflags"), QStringLiteral("+faststart"));
        xml.writeAttribute(QStringLiteral("vcodec"), options.videoCodec);
        xml.writeAttribute(QStringLiteral("pix_fmt"), QStringLiteral("yuv420p"));
        if (options.videoCodec == QLatin1String("h264_qsv")) {
            xml.writeAttribute(QStringLiteral("qscale"), QStringLiteral("20"));
            xml.writeAttribute(QStringLiteral("preset"), QStringLiteral("faster"));
            xml.writeAttribute(QStringLiteral("look_ahead"), QStringLiteral("0"));
        } else {
            xml.writeAttribute(QStringLiteral("crf"), QStringLiteral("19"));
            xml.writeAttribute(QStringLiteral("preset"), QStringLiteral("veryfast"));
        }
        xml.writeAttribute(QStringLiteral("acodec"), QStringLiteral("aac"));
        xml.writeAttribute(QStringLiteral("ab"), QStringLiteral("192k"));
        xml.writeAttribute(QStringLiteral("ar"), QStringLiteral("48000"));
        xml.writeAttribute(QStringLiteral("channels"), QStringLiteral("2"));
        xml.writeAttribute(QStringLiteral("progressive"), QStringLiteral("1"));
        xml.writeAttribute(QStringLiteral("deinterlacer"), QStringLiteral("yadif"));
        xml.writeAttribute(QStringLiteral("rescale"), QStringLiteral("bicubic"));
        xml.writeAttribute(QStringLiteral("threads"), QString::number(options.threads));
        xml.writeAttribute(QStringLiteral("real_time"), QStringLiteral("-1"));
    }

    const int lastFrame = qMax(0, edit.frames - 1);
    xml.writeStartElement(QStringLiteral("producer"));
    xml.writeAttribute(QStringLiteral("id"), QStringLiteral("source"));
    xml.writeAttribute(QStringLiteral("in"), QStringLiteral("0"));
    xml.writeAttribute(QStringLiteral("out"), QString::number(lastFrame));
    writeProperty(xml, QStringLiteral("length"), QString::number(edit.frames));
    const bool turnedPhoto = edit.isPhoto && !options.photoPath.isEmpty();
    writeProperty(xml, QStringLiteral("resource"), turnedPhoto ? options.photoPath : edit.mediaPath);
    if (edit.isPhoto) {
        writeProperty(xml, QStringLiteral("mlt_service"), QStringLiteral("qimage"));
        writeProperty(xml, QStringLiteral("ttl"), QStringLiteral("1"));
    } else {
        writeProperty(xml, QStringLiteral("mlt_service"), QStringLiteral("avformat"));
        writeProperty(xml,
                      QStringLiteral("rotate"),
                      QString::number((edit.baseRotation + edit.rotation) % 360));
        const QString graph = filterGraph(edit);
        if (!graph.isEmpty())
            writeProperty(xml, QStringLiteral("filtergraph"), graph);
    }
    xml.writeEndElement();

    xml.writeStartElement(QStringLiteral("playlist"));
    xml.writeAttribute(QStringLiteral("id"), QStringLiteral("main"));
    QVector<Range> ranges;
    if (options.applyTime && !edit.isPhoto)
        ranges = edit.kept();
    else
        ranges << Range{0, edit.frames};
    for (const auto &range : ranges) {
        xml.writeEmptyElement(QStringLiteral("entry"));
        xml.writeAttribute(QStringLiteral("producer"), QStringLiteral("source"));
        xml.writeAttribute(QStringLiteral("in"), QString::number(range.start));
        xml.writeAttribute(QStringLiteral("out"), QString::number(range.end - 1));
    }
    if (isCropped) {
        xml.writeStartElement(QStringLiteral("filter"));
        writeProperty(xml, QStringLiteral("mlt_service"), QStringLiteral("crop"));
        writeProperty(xml, QStringLiteral("left"), QString::number(cropRect.left()));
        writeProperty(xml, QStringLiteral("top"), QString::number(cropRect.top()));
        writeProperty(xml,
                      QStringLiteral("right"),
                      QString::number(rotated.width() - cropRect.left() - cropRect.width()));
        writeProperty(xml,
                      QStringLiteral("bottom"),
                      QString::number(rotated.height() - cropRect.top() - cropRect.height()));
        xml.writeEndElement();
    }
    if (!options.captionImage.isEmpty()) {
        xml.writeStartElement(QStringLiteral("filter"));
        writeProperty(xml, QStringLiteral("mlt_service"), QStringLiteral("watermark"));
        writeProperty(xml, QStringLiteral("resource"), options.captionImage);
        writeProperty(xml,
                      QStringLiteral("composite.geometry"),
                      QStringLiteral("0%/0%:100%x100%:100"));
        writeProperty(xml, QStringLiteral("composite.progressive"), QStringLiteral("1"));
        writeProperty(xml, QStringLiteral("composite.distort"), QStringLiteral("1"));
        writeProperty(xml, QStringLiteral("producer.force_reload"), QStringLiteral("0"));
        xml.writeEndElement();
    }
    xml.writeEndElement(); // playlist
    xml.writeEndElement(); // mlt
    return text;
}

} // namespace Simple
