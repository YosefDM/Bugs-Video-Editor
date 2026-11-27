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

#ifndef SIMPLEMEDIA_H
#define SIMPLEMEDIA_H

#include "collage.h"
#include "simpleedit.h"

#include <QColor>

#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QSizeF>
#include <QString>
#include <QStringList>

#include <functional>

namespace Simple {

bool isPhotoPath(const QString &path);
bool isMediaPath(const QString &path);

// BUGS! keeps its own working files here. Nothing in this folder belongs to Levi.
QString cacheDir();
// The Finished folder, in the system's Videos folder so it is easy to find when sharing.
QString finishedDir();

// Reads the file's facts into a fresh Edit. Safe to call from a worker thread. For HEIC this
// also makes the working copy, so it can take a moment. Returns false if the file is unreadable.
bool openMedia(const QString &path, Edit *edit);

// A small picture for the library, cached on disk. Safe to call from a worker thread.
QString thumbnailFor(const QString &path);

// Evenly spaced pictures along a video for the filmstrip. Calls back after each one.
// Safe to call from a worker thread. progress returns false to stop early.
QStringList filmstripFor(const Edit &edit,
                         int count,
                         const std::function<bool(const QStringList &)> &progress);

// The transparent full-frame picture holding the caption. Used by both preview and save.
QImage renderCaption(const Edit &edit, const QSize &frame);
// Writes it into the cache and returns the path, or an empty string when there is no caption.
QString writeCaptionImage(const Edit &edit);

// For each hidden part that blurs, a picture the shape of the turned video, white except for a
// black box where the blur goes, as Shotcut's shape mask reads it. Black-out parts need none and
// get an empty string. Written into the cache; safe to call from a worker thread.
QStringList writeHideMasks(const Edit &edit);

// Clean up an old tape. Looks through the whole video for where the tape tore up (see
// tapeGlitches()) and saves what it found in the cache for next time. progress(fraction)
// returns false to stop, and then this returns false. Safe to call from a worker thread.
bool scanTape(const Edit &edit,
              QVector<Range> *glitches,
              const std::function<bool(double)> &progress = nullptr);
// What an earlier scan found for this file. False when it has not been scanned.
bool scannedTape(const Edit &edit, QVector<Range> *glitches);
// The last good frame before each torn stretch, turned and flipped like the edit, as pictures in
// the cache. With make false, only pictures already made; the others are empty.
QStringList writeHoldImages(const Edit &edit, const QVector<Range> &glitches, bool make = true);
// Puts what Clean up an old tape needs into `options`, from the cache. Returns false when the
// clean-up is on and the video has not been scanned yet, or, with make false, a picture is
// still to be made.
bool addTapeCleanUp(const Edit &edit, CompileOptions *options, bool make = true);

// A photo with Levi's rotation and flip already applied, made by Qt as a lossless copy in the cache,
// because the engine's photo reader cannot turn a picture. Returns the plain media path when the
// photo is not turned or flipped. Safe to call from a worker thread.
QString photoForEdit(const Edit &edit);

// Renders one frame of the edited result at full size. Safe to call from a worker thread.
QImage renderFrame(const Edit &edit, int editedFrame);

// The file a photo's pixels are read from (a working copy for HEIC), upright, and its upright
// size. Returns an empty path if it cannot be read. Safe to call from a worker thread.
QString uprightPhotoPath(const QString &path, QSize *size);
// Reads a photo upright, just large enough to cover `cover` without losing quality.
QImage loadPhotoToCover(const QString &readablePath, const QSize &cover);

struct CollageStyle
{
    double border{0}; // fraction of the collage's shorter side
    double radius{0}; // rounded photo corners, fraction of the collage's shorter side
    QColor borderColor{Qt::white};
    // A sticker drawn as big as fits behind the photos, showing between and around them.
    QImage backdrop;
    // The photos together cut into a shape: "heart", "circle", "star", or empty for none.
    QString outline;
};
// The outline shape's path on a page `size` big, kept `margin` pixels in from the edge.
QPainterPath collageOutline(const QString &outline, const QSize &size, double margin);
// Draws a collage at `size`: each photo covers its frame, cropped and zoomed by its PhotoCrop,
// with the spacing, rounded corners and background from `style`. `image(photo, needed)` returns a
// photo's picture; `needed` is the smallest size that keeps it sharp, and any size works.
// `crops` is by photo index, like Slot::photo; missing ones show the middle at zoom 1.
QImage renderCollage(const Collage::Layout &layout,
                     const QVector<Collage::PhotoCrop> &crops,
                     const QSize &size,
                     const CollageStyle &style,
                     const std::function<QImage(int photo, const QSize &needed)> &image);
// Frame rectangles in pixels for a layout drawn at `size`, before spacing, matching
// renderCollage(). Neighbouring frames share an edge.
QVector<QRect> collageCellRects(const Collage::Layout &layout, const QSize &size);
// The part of a picture `picture` shown in a frame `frame` big, for `crop`, in picture pixels.
QRectF collageSourceRect(const QSize &picture, const QSizeF &frame, const Collage::PhotoCrop &crop);

// Text and stickers on a collage. One drawing function serves the preview and the saved picture,
// so they always match. `sticker(id)` returns a sticker's picture.
using StickerImage = std::function<QImage(const QString &id)>;
// The item's box before rotation, in pixels, on a page of `page` pixels.
QSizeF collageItemSize(const CollageItem &item, const QSize &page, const StickerImage &sticker);
// Draws the item, unrotated, filling `box`.
void paintCollageItemInBox(QPainter &painter,
                           const CollageItem &item,
                           const QSize &page,
                           const QRectF &box,
                           const StickerImage &sticker);
// Draws every item in order, each at its place and rotation, on a page of `page` pixels.
void paintCollageItems(QPainter &painter,
                       const QVector<CollageItem> &items,
                       const QSize &page,
                       const StickerImage &sticker);

} // namespace Simple

#endif // SIMPLEMEDIA_H
