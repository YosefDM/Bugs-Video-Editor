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

#include "simpleedit.h"

#include <QImage>
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

// A photo with Levi's rotation and flip already applied, made by Qt as a lossless copy in the cache,
// because the engine's photo reader cannot turn a picture. Returns the plain media path when the
// photo is not turned or flipped. Safe to call from a worker thread.
QString photoForEdit(const Edit &edit);

// Renders one frame of the edited result at full size. Safe to call from a worker thread.
QImage renderFrame(const Edit &edit, int editedFrame);

} // namespace Simple

#endif // SIMPLEMEDIA_H
