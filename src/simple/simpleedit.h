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

#ifndef SIMPLEEDIT_H
#define SIMPLEEDIT_H

#include <QRect>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QVector>

namespace Simple {

// A half-open span of source frames: [start, end).
struct Range
{
    int start{0};
    int end{0};
    int length() const { return end - start; }
    bool isEmpty() const { return end <= start; }
    bool contains(int frame) const { return frame >= start && frame < end; }
    bool operator==(const Range &o) const { return start == o.start && end == o.end; }
};

enum class CaptionStyle { Bar, Center, Bold };

// Everything Levi has done to one file. A plain value, so undo is a stack of copies and
// nothing here can reach the original file.
struct Edit
{
    // Source facts, set once when the file is opened.
    QString sourcePath; // the original; only ever read
    QString mediaPath;  // what the engine reads; differs from sourcePath for HEIC
    bool isPhoto{false};
    int frames{0};
    int fpsNum{30};
    int fpsDen{1};
    QSize sourceSize;      // upright, square-pixel size, after any rotation tag in the file
    int baseRotation{0};   // rotation tag in the file, in degrees clockwise
    QString colorTransfer; // for example arib-std-b67 when the phone filmed in HDR
    bool hasAudio{true};

    // What Levi changed.
    Range trim;
    QVector<Range> cuts; // sorted, never overlapping
    int rotation{0};     // 0, 90, 180 or 270, clockwise, added to baseRotation
    bool flipped{false};
    QRectF crop{0, 0, 1, 1}; // fraction of the rotated picture
    QString cropShape{QStringLiteral("original")};
    QString captionName;
    QString captionSecond;
    CaptionStyle captionStyle{CaptionStyle::Bar};

    bool operator==(const Edit &o) const;
    bool operator!=(const Edit &o) const { return !(*this == o); }

    // Resets every edit but keeps the source facts.
    void reset();
    bool hasChanges() const;
    bool hasTimeChanges() const;
    bool hasCaption() const { return !captionName.trimmed().isEmpty(); }
    bool isHdr() const;

    // Timeline
    double fps() const { return fpsDen > 0 ? double(fpsNum) / fpsDen : 30.0; }
    QVector<Range> kept() const;
    int keptFrames() const;
    int editedFromSource(int sourceFrame) const; // nearest kept frame when it was removed
    int sourceFromEdited(int editedFrame) const;
    bool setTrim(int start, int end);
    // Returns false when removing the range would leave nothing, or nothing would change.
    bool addCut(const Range &range);
    void removeCutAt(int index);

    // Picture
    QSize rotatedSize() const;
    QRect cropPixels() const;
    QSize outputSize() const;
    void setCropShape(const QString &shape);
    void setCropRect(const QRectF &rect);
    void rotateClockwise();
    void toggleFlip();
};

// Largest centred rectangle of this shape inside a picture, as fractions of the picture.
QRectF cropForShape(const QString &shape, const QSize &picture);
// 0 when the shape has no fixed aspect ratio.
double aspectForShape(const QString &shape);

// "0:07", "1:02:03"; adds tenths only with withTenths and when they are not zero.
QString formatTime(double seconds, bool withTenths = false);
// Accepts "5", "0:05", "1:02.5" and "1:02:03". Commas also work as the decimal point.
bool parseTime(const QString &text, double *seconds);
// "6 seconds", "1 second", "1 minute 5 seconds"
QString formatDuration(double seconds);

// A path in dir that does not exist yet, so an existing file can never be overwritten.
QString uniquePath(const QString &dir, const QString &baseName, const QString &extension);
// Removes characters Windows does not allow in a file name.
QString safeFileName(const QString &name);

// Common rates are snapped so that phone footage reporting 29.97-ish stays on a standard rate.
void normalizeFrameRate(double fps, int *num, int *den);

struct CompileOptions
{
    bool applyTime{true}; // false shows the whole source, used while dragging a handle
    bool applyCrop{true};
    QString captionImage; // full-frame transparent picture; empty for none
    QString photoPath;    // a photo already turned and flipped, from photoForEdit(); empty for none
    QString target;       // when set, adds the encoder settings that save to this file
    QString videoCodec{QStringLiteral("libx264")};
    int threads{0};
};

// The one description of the edited result, in MLT XML. Preview and save both load this,
// which is what makes the saved file match what Levi saw.
QString compileXml(const Edit &edit, const CompileOptions &options);

} // namespace Simple

#endif // SIMPLEEDIT_H
