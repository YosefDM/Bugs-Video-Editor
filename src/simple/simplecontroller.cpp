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

#include "simplecontroller.h"

#include "Logger.h"
#include "jobqueue.h"
#include "jobs/meltjob.h"
#include "mainwindow.h"
#include "mltcontroller.h"
#include "player.h"
#include "qmltypes/qmlutilities.h"
#include "sharedframe.h"
#include "simplemedia.h"
#include "videowidget.h"

#include <QCryptographicHash>
#include <QDate>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>

using namespace Simple;

namespace {

const char kLibraryKey[] = "simple/originals";
const char kDarkKey[] = "simple/dark";
const char kWelcomedKey[] = "simple/welcomed";
const int kFilmstripCells = 16;
const int kToastMs = 4000;

Mlt::VideoWidget *previewWidget()
{
    return qobject_cast<Mlt::VideoWidget *>(MLT.videoWidget());
}

QString badgeForMedia(const Edit &edit)
{
    return edit.isPhoto ? QStringLiteral("Photo") : formatTime(edit.frames / edit.fps());
}

} // namespace

SimpleController::SimpleController(QObject *parent)
    : QObject(parent)
{
    loadSettings();

    m_toastTimer.setSingleShot(true);
    m_toastTimer.setInterval(kToastMs);
    connect(&m_toastTimer, &QTimer::timeout, this, [this]() {
        m_toast.clear();
        emit toastChanged();
    });

    // Typing a caption redraws the picture only after a short pause.
    m_captionTimer.setSingleShot(true);
    m_captionTimer.setInterval(300);
    connect(&m_captionTimer, &QTimer::timeout, this, [this]() {
        // Saving the full-size caption picture takes about 70 ms for 1080p on a fast PC, and
        // longer on a slow laptop, so it is made off the screen's thread. The preview then finds
        // it ready.
        const Edit edit = m_edit;
        const int generation = ++m_captionGeneration;
        QPointer<SimpleController> self(this);
        (void) QtConcurrent::run([self, edit, generation]() {
            writeCaptionImage(edit);
            if (!self)
                return;
            QMetaObject::invokeMethod(
                self.data(),
                [self, generation]() {
                    if (self && generation == self->m_captionGeneration)
                        self->rebuildPreview(self->m_playhead);
                },
                Qt::QueuedConnection);
        });
    });

    m_rejectTimer.setSingleShot(true);
    m_rejectTimer.setInterval(500);
    connect(&m_rejectTimer, &QTimer::timeout, this, &SimpleController::showRejectedFiles);

    m_frameThumbTimer.setSingleShot(true);
    m_frameThumbTimer.setInterval(150);
    connect(&m_frameThumbTimer, &QTimer::timeout, this, &SimpleController::updateFrameThumb);

    m_finishedWatcher.addPath(finishedDir());
    connect(&m_finishedWatcher, &QFileSystemWatcher::directoryChanged, this, [this]() {
        refreshFinished();
    });
    refreshFinished();
    for (const auto &path : std::as_const(m_originals))
        ensureThumbnail(path);

#ifdef Q_OS_WIN
    detectHardwareEncoder();
#else
    m_hardwareEncoder = 0;
#endif
}

void SimpleController::loadSettings()
{
    m_dark = m_settings.value(kDarkKey, true).toBool();
    m_originals = m_settings.value(kLibraryKey).toStringList();
}

void SimpleController::saveLibrary()
{
    m_settings.setValue(kLibraryKey, m_originals);
    m_settings.sync();
}

void SimpleController::setDark(bool dark)
{
    if (dark == m_dark)
        return;
    m_dark = dark;
    m_settings.setValue(kDarkKey, dark);
    m_settings.sync();
    emit changed();
}

bool SimpleController::welcome() const
{
    return m_originals.isEmpty() && !m_settings.value(kWelcomedKey, false).toBool() && !hasFile();
}

void SimpleController::setTab(const QString &tab)
{
    if (tab == m_tab)
        return;
    // Collage photos are picked from Originals, so going to Finished stops picking.
    if (m_collagePicking && tab != QLatin1String("originals"))
        leaveCollage();
    m_tab = tab;
    emit changed();
    emit libraryChanged();
}

void SimpleController::setFilter(const QString &filter)
{
    if (filter == m_filter)
        return;
    m_filter = filter;
    emit changed();
    emit libraryChanged();
}

QVariantList SimpleController::items() const
{
    QVariantList result;
    const bool finished = m_tab == QLatin1String("finished");
    const auto &paths = finished ? m_finished : m_originals;
    for (const auto &path : paths) {
        const bool photo = isPhotoPath(path);
        if ((m_filter == QLatin1String("videos") && photo)
            || (m_filter == QLatin1String("photos") && !photo))
            continue;
        const bool missing = !QFileInfo::exists(path);
        QVariantMap item;
        item[QStringLiteral("path")] = path;
        item[QStringLiteral("name")] = QFileInfo(path).fileName();
        item[QStringLiteral("thumb")] = m_thumbs.contains(path)
                                            ? QUrl::fromLocalFile(m_thumbs.value(path)).toString()
                                            : QString();
        item[QStringLiteral("badge")] = missing ? QStringLiteral("Missing")
                                                : m_badges.value(path,
                                                                 photo ? QStringLiteral("Photo")
                                                                       : QString());
        item[QStringLiteral("done")] = finished;
        item[QStringLiteral("missing")] = missing;
        item[QStringLiteral("selected")] = path == m_edit.sourcePath;
        item[QStringLiteral("pickable")] = photo && !missing;
        item[QStringLiteral("pick")] = m_collagePicks.indexOf(path) + 1; // 0 when not picked
        result << item;
    }
    return result;
}

void SimpleController::refreshFinished()
{
    QDir dir(finishedDir());
    QStringList paths;
    const auto entries = dir.entryInfoList(QDir::Files, QDir::Time);
    for (const auto &entry : entries) {
        const QString path = entry.absoluteFilePath();
        if (isMediaPath(path) && !entry.fileName().contains(QLatin1String(".part.")))
            paths << path;
    }
    m_finished = paths;
    for (const auto &path : std::as_const(m_finished))
        ensureThumbnail(path);
    emit libraryChanged();
}

void SimpleController::ensureThumbnail(const QString &path)
{
    if (m_thumbs.contains(path) || m_thumbsPending.contains(path) || !QFileInfo::exists(path))
        return;
    m_thumbsPending << path;
    QPointer<SimpleController> self(this);
    (void) QtConcurrent::run([self, path]() {
        const QString thumb = thumbnailFor(path);
        QString badge;
        Edit edit;
        const bool readable = openMedia(path, &edit);
        if (readable && !isPhotoPath(path))
            badge = badgeForMedia(edit);
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self, path, thumb, badge, readable]() {
                if (!self)
                    return;
                self->m_thumbsPending.removeAll(path);
                // A file just added that BUGS! can't open, such as an empty or damaged one, is
                // taken out again straight away rather than sitting there blank.
                if (self->m_justAdded.remove(path) && !readable) {
                    self->rejectAddedFile(path);
                    return;
                }
                if (!thumb.isEmpty())
                    self->m_thumbs.insert(path, thumb);
                if (!badge.isEmpty())
                    self->m_badges.insert(path, badge);
                emit self->libraryChanged();
            },
            Qt::QueuedConnection);
    });
}

void SimpleController::addFiles()
{
    const QString filter = QStringLiteral(
        "Videos and photos (*.mp4 *.mov *.m4v *.3gp *.3g2 *.avi *.mkv *.webm *.wmv *.asf *.mpg "
        "*.mpeg *.vob *.mts *.m2ts *.ts *.flv *.jpg *.jpeg *.png *.heic *.heif *.webp *.bmp *.tif "
        "*.tiff);;All files (*)");
    const QStringList paths = QFileDialog::getOpenFileNames(&MAIN,
                                                            QStringLiteral("Add videos or photos"),
                                                            QStandardPaths::writableLocation(
                                                                QStandardPaths::PicturesLocation),
                                                            filter);
    if (!paths.isEmpty())
        addPaths(paths);
}

void SimpleController::addPaths(const QStringList &paths, bool openFirst)
{
    QString first;
    int skipped = 0;
    for (const auto &raw : paths) {
        const QString path = QFileInfo(raw).absoluteFilePath();
        if (QFileInfo(path).isDir() || !isMediaPath(path)) {
            ++skipped;
            continue;
        }
        if (first.isEmpty())
            first = path;
        if (!m_originals.contains(path)) {
            m_originals.prepend(path);
            m_justAdded.insert(path);
        }
        ensureThumbnail(path);
    }
    if (!first.isEmpty()) {
        m_settings.setValue(kWelcomedKey, true);
        saveLibrary();
        setTab(QStringLiteral("originals"));
        emit changed();
        emit libraryChanged();
        if (openFirst)
            openFile(first);
    }
    if (skipped)
        showToast(skipped == 1 ? QStringLiteral("That file isn't a video or photo BUGS! can open.")
                               : QStringLiteral("%1 files weren't videos or photos BUGS! can open.")
                                     .arg(skipped),
                  true);
}

void SimpleController::rejectAddedFile(const QString &path)
{
    m_originals.removeAll(path);
    saveLibrary();
    emit libraryChanged();
    // Files added together are reported together.
    m_rejectedNames << QFileInfo(path).fileName();
    m_rejectTimer.start();
}

void SimpleController::showRejectedFiles()
{
    const QStringList names = m_rejectedNames;
    m_rejectedNames.clear();
    if (names.isEmpty())
        return;
    const QString list = names.size() == 1
                             ? names.first()
                             : names.mid(0, names.size() - 1).join(QStringLiteral(", "))
                                   + QStringLiteral(" and ") + names.last();
    showToast(QStringLiteral("BUGS! couldn't open %1, so %2 added.")
                  .arg(list,
                       names.size() == 1 ? QStringLiteral("it wasn't")
                                         : QStringLiteral("they weren't")),
              true);
}

void SimpleController::addUrls(const QList<QUrl> &urls)
{
    QStringList paths;
    for (const auto &url : urls) {
        if (url.isLocalFile())
            paths << url.toLocalFile();
    }
    if (!paths.isEmpty())
        addPaths(paths);
}

void SimpleController::removeFromLibrary(const QString &path)
{
    if (!m_originals.removeAll(path))
        return;
    saveLibrary();
    if (m_edit.sourcePath == path) {
        pausePreview();
        m_sessions.remove(path);
        m_edit = Edit();
        m_undo.clear();
        m_redo.clear();
        m_selection = Range();
        m_selectedLabel = -1;
        m_filmstrip.clear();
        m_previewXml.clear();
        MLT.close();
        emit filmstripChanged();
        emit changed();
    }
    emit libraryChanged();
}

void SimpleController::showFinishedFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(finishedDir()));
}

void SimpleController::storeSession()
{
    if (!hasFile())
        return;
    Session session;
    session.edit = m_edit;
    session.undo = m_undo;
    session.redo = m_redo;
    m_sessions.insert(m_edit.sourcePath, session);
}

void SimpleController::openFile(const QString &path)
{
    // Opening a file from the library while a collage is open leaves the collage. A collage that
    // was made but not saved cannot be undone, so that asks first.
    if (m_collageActive && m_collageUnsaved && m_dialog.isEmpty()) {
        m_openAfterCollage = path;
        m_dialog = QStringLiteral("leaveCollage");
        emit dialogChanged();
        return;
    }
    if (m_collageActive || m_collagePicking)
        leaveCollage();
    if (path == m_edit.sourcePath && hasFile())
        return;
    if (!QFileInfo::exists(path)) {
        showToast(QStringLiteral("BUGS! can't find that file. It may have been moved or deleted."),
                  true);
        return;
    }
    pausePreview();
    storeSession();
    const int generation = ++m_openGeneration;
    m_loading = true;
    emit changed();

    QPointer<SimpleController> self(this);
    (void) QtConcurrent::run([self, path, generation]() {
        Edit edit;
        const bool ok = openMedia(path, &edit);
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self, path, generation, edit, ok]() {
                if (!self || generation != self->m_openGeneration)
                    return;
                self->m_loading = false;
                if (!ok) {
                    self->showToast(QStringLiteral("Sorry, BUGS! couldn't open that file."), true);
                    emit self->changed();
                    return;
                }
                const auto session = self->m_sessions.find(path);
                if (session != self->m_sessions.end()) {
                    self->m_edit = session->edit;
                    self->m_undo = session->undo;
                    self->m_redo = session->redo;
                } else {
                    self->m_edit = edit;
                    self->m_undo.clear();
                    self->m_redo.clear();
                }
                self->m_lastCoalesceKey.clear();
                self->m_selection = Range();
                self->m_selectedLabel = -1;
                self->m_tool = self->m_edit.isPhoto ? QStringLiteral("crop")
                                                    : QStringLiteral("trim");
                self->m_filmstrip.clear();
                emit self->filmstripChanged();
                self->m_playhead = self->m_edit.isPhoto ? 0 : self->m_edit.trim.start;
                self->rebuildPreview(self->m_playhead);
                self->loadFilmstrip();
                emit self->changed();
                emit self->libraryChanged();
            },
            Qt::QueuedConnection);
    });
}

void SimpleController::loadFilmstrip()
{
    if (m_edit.isPhoto)
        return;
    const int generation = ++m_filmstripGeneration;
    const Edit edit = m_edit;
    QPointer<SimpleController> self(this);
    (void) QtConcurrent::run([self, edit, generation]() {
        filmstripFor(edit, kFilmstripCells, [self, generation](const QStringList &paths) {
            if (!self || generation != self->m_filmstripGeneration)
                return false;
            QMetaObject::invokeMethod(
                self.data(),
                [self, generation, paths]() {
                    if (!self || generation != self->m_filmstripGeneration)
                        return;
                    self->m_filmstrip.clear();
                    for (const auto &path : paths)
                        self->m_filmstrip << QUrl::fromLocalFile(path).toString();
                    emit self->filmstripChanged();
                },
                Qt::QueuedConnection);
            return true;
        });
    });
}

QString SimpleController::fileName() const
{
    return QFileInfo(m_edit.sourcePath).fileName();
}

void SimpleController::setTool(const QString &tool)
{
    if (tool == m_tool || !hasFile())
        return;
    const bool wasCrop = m_tool == QLatin1String("crop");
    const bool wasText = m_tool == QLatin1String("text");
    m_tool = tool;
    const bool isCrop = tool == QLatin1String("crop");
    const bool isText = tool == QLatin1String("text");
    if (tool == QLatin1String("cut")) {
        if (m_selection.isEmpty())
            selectAnotherPart();
    } else {
        m_selection = Range();
    }
    if (tool == QLatin1String("frame"))
        updateFrameThumb();
    if (isText && m_selectedLabel < 0 && !m_edit.labels.isEmpty())
        m_selectedLabel = 0;
    // Crop shows the whole picture with the box on top; other tools show the cropped result.
    // Text draws the labels live so they can be dragged, so they leave the baked overlay.
    if (wasCrop != isCrop || wasText != isText)
        rebuildPreview(m_playhead);
    emit changed();
}

QVariantList SimpleController::cuts() const
{
    QVariantList result;
    for (const auto &cut : m_edit.cuts) {
        QVariantMap item;
        item[QStringLiteral("start")] = cut.start;
        item[QStringLiteral("end")] = cut.end;
        item[QStringLiteral("label")]
            = QStringLiteral("%1 → %2").arg(formatTime(cut.start / m_edit.fps(), true),
                                            formatTime(cut.end / m_edit.fps(), true));
        result << item;
    }
    return result;
}

bool SimpleController::canRemoveSelection() const
{
    if (m_selection.isEmpty())
        return false;
    Edit candidate = m_edit;
    return candidate.addCut(m_selection);
}

QString SimpleController::selectionText() const
{
    if (m_selection.isEmpty())
        return QString();
    // Tenths, so a part under a second does not read as "0:00 → 0:01".
    return QStringLiteral("%1 → %2").arg(formatTime(m_selection.start / m_edit.fps(), true),
                                         formatTime(m_selection.end / m_edit.fps(), true));
}

QString SimpleController::selectionInfo() const
{
    if (m_selection.isEmpty())
        return QString();
    if (!canRemoveSelection())
        return QStringLiteral("There has to be something left, so this can't all go.");
    Edit candidate = m_edit;
    candidate.addCut(m_selection);
    const double seconds = (m_edit.keptFrames() - candidate.keptFrames()) / m_edit.fps();
    if (seconds < 0.95)
        return QStringLiteral("Less than a second will be removed");
    return QStringLiteral("%1 will be removed").arg(formatDuration(seconds));
}

QString SimpleController::selectionStartText() const
{
    return m_selection.isEmpty() ? QString() : formatTime(m_selection.start / m_edit.fps(), true);
}

QString SimpleController::selectionEndText() const
{
    return m_selection.isEmpty() ? QString() : formatTime(m_selection.end / m_edit.fps(), true);
}

QString SimpleController::trimStartText() const
{
    return formatTime(m_edit.trim.start / m_edit.fps(), true);
}

QString SimpleController::trimEndText() const
{
    return formatTime(m_edit.trim.end / m_edit.fps(), true);
}

QString SimpleController::lengthText() const
{
    return formatTime(m_edit.keptFrames() / m_edit.fps());
}

QString SimpleController::positionText() const
{
    return formatTime(m_playhead / m_edit.fps());
}

double SimpleController::cropAspect() const
{
    return aspectForShape(m_edit.cropShape);
}

double SimpleController::pictureAspect() const
{
    const QSize size = m_edit.rotatedSize();
    return size.height() > 0 ? double(size.width()) / size.height() : 1.0;
}

QString SimpleController::captionStyle() const
{
    switch (m_edit.captionStyle) {
    case CaptionStyle::Center:
        return QStringLiteral("center");
    case CaptionStyle::Bold:
        return QStringLiteral("bold");
    default:
        return QStringLiteral("bar");
    }
}

void SimpleController::setCaptionName(const QString &text)
{
    if (text == m_edit.captionName)
        return;
    const Edit before = m_edit;
    m_edit.captionName = text;
    pushUndo(before, QStringLiteral("name"));
    emit changed();
    m_captionTimer.start();
}

void SimpleController::setCaptionSecond(const QString &text)
{
    if (text == m_edit.captionSecond)
        return;
    const Edit before = m_edit;
    m_edit.captionSecond = text;
    pushUndo(before, QStringLiteral("second"));
    emit changed();
    m_captionTimer.start();
}

void SimpleController::setCaptionStyle(const QString &style)
{
    const CaptionStyle value = style == QLatin1String("center") ? CaptionStyle::Center
                               : style == QLatin1String("bold") ? CaptionStyle::Bold
                                                                : CaptionStyle::Bar;
    if (value == m_edit.captionStyle)
        return;
    const Edit before = m_edit;
    m_edit.captionStyle = value;
    pushUndo(before);
    afterEdit(true);
}

QVariantList SimpleController::labels() const
{
    QVariantList result;
    for (int i = 0; i < m_edit.labels.size(); ++i) {
        const auto &label = m_edit.labels.at(i);
        QVariantMap item;
        item[QStringLiteral("index")] = i;
        item[QStringLiteral("text")] = label.text;
        item[QStringLiteral("x")] = label.center.x();
        item[QStringLiteral("y")] = label.center.y();
        item[QStringLiteral("size")] = label.size;
        item[QStringLiteral("color")] = label.color;
        item[QStringLiteral("font")] = label.font;
        item[QStringLiteral("family")] = labelFontFamily(label.font);
        result << item;
    }
    return result;
}

QVariantList SimpleController::labelFonts() const
{
    QVariantList result;
    for (const auto &font : Simple::labelFonts()) {
        QVariantMap item;
        item[QStringLiteral("id")] = font.id;
        item[QStringLiteral("name")] = font.name;
        item[QStringLiteral("family")] = font.family;
        result << item;
    }
    return result;
}

void SimpleController::refreshLabelColors()
{
    if (!hasFile())
        return;
    // The picture behind the labels: the edited frame at the playhead, without the labels.
    // Decoding it from the middle of a long video takes a while, so it is done off the screen's
    // thread, and labels Levi has not coloured himself change colour when it is ready.
    const int frame = m_edit.isPhoto ? 0 : m_edit.editedFromSource(m_playhead);
    const QString key = QStringLiteral("%1|%2|%3,%4,%5,%6|%7%8|%9")
                            .arg(m_edit.sourcePath)
                            .arg(frame)
                            .arg(m_edit.crop.x())
                            .arg(m_edit.crop.y())
                            .arg(m_edit.crop.width())
                            .arg(m_edit.crop.height())
                            .arg(m_edit.rotation)
                            .arg(m_edit.flipped)
                            .arg(m_edit.captionName);
    if (key == m_labelBackdropKey) {
        applyAutoLabelColors();
        return;
    }
    Edit plain = m_edit;
    plain.labels.clear();
    const int generation = ++m_labelBackdropGeneration;
    QPointer<SimpleController> self(this);
    (void) QtConcurrent::run([self, plain, frame, key, generation]() {
        const QImage image = renderFrame(plain, frame);
        const QImage small = image.isNull() ? QImage()
                                            : image.scaled(480, 480, Qt::KeepAspectRatio);
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self, small, key, generation, source = plain.sourcePath]() {
                if (!self || generation != self->m_labelBackdropGeneration
                    || source != self->m_edit.sourcePath)
                    return;
                self->m_labelBackdrop = small;
                self->m_labelBackdropKey = key;
                self->applyAutoLabelColors();
            },
            Qt::QueuedConnection);
    });
}

void SimpleController::applyAutoLabelColors()
{
    if (m_labelBackdrop.isNull())
        return;
    bool changedAny = false;
    for (auto &label : m_edit.labels) {
        if (!label.autoColor)
            continue;
        const QString color = contrastingLabelColor(m_labelBackdrop, label.center);
        if (color != label.color) {
            label.color = color;
            changedAny = true;
        }
    }
    if (!changedAny)
        return;
    // The Text tool draws labels live. Other tools show them baked into the picture, which must
    // be redrawn so the preview still matches the saved file.
    if (m_tool != QLatin1String("text"))
        rebuildPreview(m_playhead);
    emit changed();
}

void SimpleController::addLabel()
{
    if (!hasFile())
        return;
    const Edit before = m_edit;
    TextLabel label;
    label.text = QStringLiteral("Name");
    // Each new one a little lower, so they do not land on top of each other.
    const int n = m_edit.labels.size();
    label.center = QPointF(0.5, 0.3 + 0.1 * (n % 5));
    if (m_selectedLabel >= 0 && m_selectedLabel < n) {
        label.size = m_edit.labels.at(m_selectedLabel).size;
        label.font = m_edit.labels.at(m_selectedLabel).font;
    }
    m_edit.labels << label;
    m_selectedLabel = n;
    pushUndo(before);
    afterEdit(false);
    refreshLabelColors();
}

void SimpleController::selectLabel(int index)
{
    if (index == m_selectedLabel || index < -1 || index >= m_edit.labels.size())
        return;
    m_selectedLabel = index;
    m_lastCoalesceKey.clear();
    emit changed();
}

void SimpleController::setLabelText(int index, const QString &text)
{
    if (index < 0 || index >= m_edit.labels.size() || m_edit.labels.at(index).text == text)
        return;
    const Edit before = m_edit;
    m_edit.labels[index].text = text;
    pushUndo(before, QStringLiteral("label%1").arg(index));
    emit changed();
}

void SimpleController::setLabelColor(int index, const QString &color)
{
    if (index < 0 || index >= m_edit.labels.size() || m_edit.labels.at(index).color == color)
        return;
    const Edit before = m_edit;
    m_edit.labels[index].color = color;
    m_edit.labels[index].autoColor = false;
    pushUndo(before);
    afterEdit(false);
}

void SimpleController::setLabelSize(int index, int size)
{
    if (index < 0 || index >= m_edit.labels.size() || m_edit.labels.at(index).size == size)
        return;
    const Edit before = m_edit;
    m_edit.labels[index].size = qBound(0, size, 2);
    pushUndo(before);
    afterEdit(false);
}

void SimpleController::setLabelFont(int index, const QString &font)
{
    if (index < 0 || index >= m_edit.labels.size() || m_edit.labels.at(index).font == font)
        return;
    const Edit before = m_edit;
    m_edit.labels[index].font = font;
    pushUndo(before);
    afterEdit(false);
}

void SimpleController::moveLabel(int index, double x, double y)
{
    if (index < 0 || index >= m_edit.labels.size())
        return;
    if (!m_labelDragging) {
        m_dragStart = m_edit;
        m_labelDragging = true;
    }
    m_edit.labels[index].center = QPointF(qBound(0.0, x, 1.0), qBound(0.0, y, 1.0));
    emit changed();
}

void SimpleController::endLabelDrag()
{
    if (!m_labelDragging)
        return;
    m_labelDragging = false;
    if (m_edit != m_dragStart)
        pushUndo(m_dragStart);
    m_lastCoalesceKey.clear();
    emit changed();
    // A label Levi has not coloured himself takes the colour that suits its new spot.
    refreshLabelColors();
}

void SimpleController::removeLabel(int index)
{
    if (index < 0 || index >= m_edit.labels.size())
        return;
    const Edit before = m_edit;
    m_edit.labels.remove(index);
    m_selectedLabel = m_edit.labels.isEmpty() ? -1 : qMin(index, m_edit.labels.size() - 1);
    pushUndo(before);
    afterEdit(false);
}

void SimpleController::activate()
{
    if (m_active)
        return;
    m_active = true;
    connect(previewWidget(),
            &Mlt::VideoWidget::frameDisplayed,
            this,
            &SimpleController::onFrameDisplayed);
    connect(previewWidget(), &Mlt::VideoWidget::playing, this, [this]() {
        m_playing = true;
        emit playingChanged();
    });
    connect(previewWidget(), &Mlt::VideoWidget::paused, this, [this]() {
        m_playing = false;
        emit playingChanged();
    });
    if (hasFile())
        rebuildPreview(m_playhead);
}

void SimpleController::deactivate()
{
    if (!m_active)
        return;
    m_active = false;
    pausePreview();
    disconnect(previewWidget(), nullptr, this, nullptr);
    // Advanced puts its own video in the player.
    m_previewXml.clear();
}

QString SimpleController::combineProject() const
{
    if (!hasFile())
        return QString();
    const QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    dir.mkpath(QStringLiteral("combine"));
    // The path is part of the name, so two files called IMG_0001.MOV do not share a project.
    const QString key = QString::fromLatin1(
        QCryptographicHash::hash(m_edit.sourcePath.toUtf8(), QCryptographicHash::Sha1)
            .toHex()
            .left(10));
    return dir.filePath(QStringLiteral("combine/%1 %2.mlt")
                            .arg(QFileInfo(m_edit.sourcePath).completeBaseName(), key));
}

QString SimpleController::advancedResource()
{
    if (!hasFile())
        return QString();
    CompileOptions options;
    options.captionImage = writeCaptionImage(m_edit);
    // Next to the project, which refers to it, and so out of the cache that Windows may clear.
    QString path = combineProject();
    path.replace(path.length() - 4, 4, QStringLiteral(" clip.mlt"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return m_edit.sourcePath;
    options.photoPath = photoForEdit(m_edit);
    QString xml = compileXml(m_edit, options);
    file.write(xml.toUtf8());
    file.close();
    return path;
}

bool SimpleController::filesStillThere(const QStringList &paths)
{
    for (const QString &path : paths) {
        if (!path.isEmpty() && !QFileInfo::exists(path)) {
            showToast(QStringLiteral("BUGS! can't find %1 any more. It may have been moved or "
                                     "deleted, so it can't be saved.")
                          .arg(QFileInfo(path).fileName()),
                      true);
            emit libraryChanged();
            return false;
        }
    }
    return true;
}

QStringList SimpleController::unfinishedFileNames() const
{
    QStringList names;
    const auto add = [&](const Edit &edit) {
        if (edit.hasChanges()
            && (!m_savedEdits.contains(edit.sourcePath)
                || m_savedEdits.value(edit.sourcePath) != edit))
            names << QFileInfo(edit.sourcePath).fileName();
    };
    if (hasFile())
        add(m_edit);
    for (auto it = m_sessions.cbegin(); it != m_sessions.cend(); ++it) {
        if (it.key() != m_edit.sourcePath)
            add(it->edit);
    }
    return names;
}

bool SimpleController::hasUnfinishedEdits() const
{
    const auto isUnfinished = [this](const Edit &edit) {
        return edit.hasChanges()
               && (!m_savedEdits.contains(edit.sourcePath)
                   || m_savedEdits.value(edit.sourcePath) != edit);
    };
    if (hasFile() && isUnfinished(m_edit))
        return true;
    for (auto it = m_sessions.cbegin(); it != m_sessions.cend(); ++it) {
        if (it.key() != m_edit.sourcePath && isUnfinished(it->edit))
            return true;
    }
    return false;
}

void SimpleController::togglePlay()
{
    if (!hasFile() || m_edit.isPhoto)
        return;
    if (m_playing)
        MAIN.player()->pause();
    else
        MAIN.player()->play(m_speed);
}

void SimpleController::cycleSpeed()
{
    m_speed = m_speed < 1.25 ? 1.5 : m_speed < 1.75 ? 2.0 : 1.0;
    if (m_playing && hasFile() && !m_edit.isPhoto)
        MAIN.player()->play(m_speed);
    emit playingChanged();
}

void SimpleController::skipSeconds(double seconds)
{
    if (!hasFile() || m_edit.isPhoto || !MLT.producer())
        return;
    const int target = m_playhead + qRound(seconds * m_edit.fps());
    seekSource(qBound(m_edit.trim.start, target, m_edit.trim.end - 1));
}

void SimpleController::pausePreview()
{
    if (MLT.producer() && m_playing)
        MAIN.player()->pause();
}

void SimpleController::seekSource(int sourceFrame)
{
    if (!hasFile() || m_edit.isPhoto || !MLT.producer())
        return;
    sourceFrame = qBound(0, sourceFrame, m_edit.frames - 1);
    MAIN.player()->seek(m_previewFullTime ? sourceFrame : m_edit.editedFromSource(sourceFrame));
}

void SimpleController::stepFrames(int count)
{
    if (!hasFile() || m_edit.isPhoto || !MLT.producer())
        return;
    const int position = m_previewFullTime ? m_playhead : m_edit.editedFromSource(m_playhead);
    MAIN.player()->seek(qMax(0, position + count));
}

void SimpleController::beginTrimDrag()
{
    m_dragStart = m_edit;
    m_timeDragging = true;
    pausePreview();
    // Show the whole video so the handle can reveal parts that are currently trimmed off.
    rebuildPreview(m_playhead);
}

void SimpleController::dragTrim(int start, int end)
{
    if (!m_timeDragging)
        return;
    const int previousStart = m_edit.trim.start;
    if (m_edit.setTrim(start, end)) {
        seekSource(m_edit.trim.start != previousStart ? m_edit.trim.start : m_edit.trim.end - 1);
        emit changed();
    }
}

void SimpleController::endTrimDrag()
{
    if (!m_timeDragging)
        return;
    m_timeDragging = false;
    if (m_edit != m_dragStart) {
        // Parts wholly outside the new trim are gone from the picture, so drop them from the list.
        QVector<Range> remaining;
        for (const auto &cut : std::as_const(m_edit.cuts)) {
            if (cut.end > m_edit.trim.start && cut.start < m_edit.trim.end)
                remaining << cut;
        }
        m_edit.cuts = remaining;
        pushUndo(m_dragStart);
    }
    int frame = m_playhead;
    if (frame < m_edit.trim.start || frame >= m_edit.trim.end)
        frame = m_edit.trim.start;
    if (m_selection.isEmpty() == false) {
        m_selection.start = qBound(m_edit.trim.start, m_selection.start, m_edit.trim.end);
        m_selection.end = qBound(m_edit.trim.start, m_selection.end, m_edit.trim.end);
    }
    rebuildPreview(frame);
    emit changed();
}

bool SimpleController::setTrimStartText(const QString &text)
{
    double seconds = 0;
    const Edit before = m_edit;
    if (!parseTime(text, &seconds)
        || !m_edit.setTrim(qRound(seconds * m_edit.fps()), m_edit.trim.end)) {
        showToast(QStringLiteral("That start time doesn't fit this video."), true);
        emit changed();
        return false;
    }
    if (m_edit != before) {
        pushUndo(before);
        afterEdit(true);
    }
    return true;
}

bool SimpleController::setTrimEndText(const QString &text)
{
    double seconds = 0;
    const Edit before = m_edit;
    if (!parseTime(text, &seconds)
        || !m_edit.setTrim(m_edit.trim.start, qRound(seconds * m_edit.fps()))) {
        showToast(QStringLiteral("That end time doesn't fit this video."), true);
        emit changed();
        return false;
    }
    if (m_edit != before) {
        pushUndo(before);
        afterEdit(true);
    }
    return true;
}

void SimpleController::resetTrim()
{
    const Edit before = m_edit;
    m_edit.trim = Range{0, m_edit.frames};
    if (m_edit != before) {
        pushUndo(before);
        afterEdit(true);
    }
}

void SimpleController::setSelection(int start, int end)
{
    start = qBound(m_edit.trim.start, start, m_edit.trim.end);
    end = qBound(m_edit.trim.start, end, m_edit.trim.end);
    if (end <= start)
        return;
    const int previousStart = m_selection.start;
    m_selection = Range{start, end};
    pausePreview();
    seekSource(start != previousStart ? start : end - 1);
    emit changed();
}

void SimpleController::endSelectionDrag()
{
    emit changed();
}

bool SimpleController::setSelectionStartText(const QString &text)
{
    double seconds = 0;
    const int frame = parseTime(text, &seconds) ? qRound(seconds * m_edit.fps()) : -1;
    if (frame < m_edit.trim.start || frame >= m_edit.trim.end) {
        showToast(QStringLiteral("That time isn't in this video."), true);
        emit changed();
        return false;
    }
    if (m_selection.isEmpty())
        selectAnotherPart();
    int end = m_selection.end;
    // Starting after the old end keeps the part the same length, so the box stays visible.
    if (end <= frame)
        end = qMin(m_edit.trim.end, frame + qMax(1, m_selection.length()));
    setSelection(frame, end);
    return true;
}

bool SimpleController::setSelectionEndText(const QString &text)
{
    double seconds = 0;
    const int frame = parseTime(text, &seconds) ? qRound(seconds * m_edit.fps()) : -1;
    if (frame <= m_edit.trim.start || frame > m_edit.trim.end) {
        showToast(QStringLiteral("That time isn't in this video."), true);
        emit changed();
        return false;
    }
    if (m_selection.isEmpty())
        selectAnotherPart();
    int start = m_selection.start;
    if (start >= frame)
        start = qMax(m_edit.trim.start, frame - qMax(1, m_selection.length()));
    setSelection(start, frame);
    return true;
}

void SimpleController::selectAnotherPart()
{
    if (!hasFile() || m_edit.isPhoto)
        return;
    const int length = m_edit.trim.length();
    const int size = qMax(1, length / 7);
    int start = qBound(m_edit.trim.start,
                       m_playhead,
                       qMax(m_edit.trim.start, m_edit.trim.end - size));
    m_selection = Range{start, qMin(start + size, m_edit.trim.end)};
    emit changed();
}

void SimpleController::removeSelection()
{
    if (!canRemoveSelection())
        return;
    const Edit before = m_edit;
    const int resume = m_selection.end;
    m_edit.addCut(m_selection);
    m_selection = Range();
    pushUndo(before);
    rebuildPreview(resume);
    showToast(QStringLiteral("Part removed: video is now %1").arg(lengthText()));
    emit changed();
}

void SimpleController::restoreCut(int index)
{
    const Edit before = m_edit;
    m_edit.removeCutAt(index);
    if (m_edit != before) {
        pushUndo(before);
        afterEdit(true);
    }
}

void SimpleController::pickCropShape(const QString &shape)
{
    const Edit before = m_edit;
    m_edit.setCropShape(shape);
    if (m_edit != before) {
        pushUndo(before);
        afterEdit(false);
    }
}

void SimpleController::setCropFromDrag(double x, double y, double width, double height)
{
    if (m_dragStart.sourcePath != m_edit.sourcePath || !m_timeDragging) {
        m_dragStart = m_edit;
        m_timeDragging = true;
    }
    m_edit.setCropRect(QRectF(x, y, width, height));
    if (m_edit.cropShape == QLatin1String("original"))
        m_edit.cropShape = QStringLiteral("free");
    emit changed();
}

void SimpleController::endCropDrag()
{
    if (!m_timeDragging)
        return;
    m_timeDragging = false;
    if (m_edit != m_dragStart)
        pushUndo(m_dragStart);
    emit changed();
}

void SimpleController::rotate()
{
    const Edit before = m_edit;
    m_edit.rotateClockwise();
    pushUndo(before);
    afterEdit(true);
}

void SimpleController::flip()
{
    const Edit before = m_edit;
    m_edit.toggleFlip();
    pushUndo(before);
    afterEdit(true);
}

void SimpleController::pushUndo(const Edit &before, const QString &coalesceKey)
{
    // Typing a word is one undo, not one per letter.
    if (coalesceKey.isEmpty() || coalesceKey != m_lastCoalesceKey)
        m_undo.append(before);
    m_lastCoalesceKey = coalesceKey;
    m_redo.clear();
}

void SimpleController::afterEdit(bool rebuild)
{
    m_lastCoalesceKey.clear();
    if (rebuild)
        rebuildPreview(m_playhead);
    emit changed();
}

void SimpleController::undo()
{
    if (m_collageActive) {
        if (m_collageUndo.isEmpty())
            return;
        m_collageRedo.append(m_collageDesign);
        m_collageCoalesceKey.clear();
        applyCollageDesign(m_collageUndo.takeLast());
        return;
    }
    if (m_undo.isEmpty())
        return;
    m_redo.append(m_edit);
    m_edit = m_undo.takeLast();
    m_lastCoalesceKey.clear();
    m_selection = Range();
    m_selectedLabel = qMin(m_selectedLabel, m_edit.labels.size() - 1);
    rebuildPreview(m_playhead);
    emit changed();
}

void SimpleController::redo()
{
    if (m_collageActive) {
        if (m_collageRedo.isEmpty())
            return;
        m_collageUndo.append(m_collageDesign);
        m_collageCoalesceKey.clear();
        applyCollageDesign(m_collageRedo.takeLast());
        return;
    }
    if (m_redo.isEmpty())
        return;
    m_undo.append(m_edit);
    m_edit = m_redo.takeLast();
    m_lastCoalesceKey.clear();
    m_selection = Range();
    m_selectedLabel = qMin(m_selectedLabel, m_edit.labels.size() - 1);
    rebuildPreview(m_playhead);
    emit changed();
}

void SimpleController::rebuildPreview(int sourceFrame)
{
    if (!m_active || !hasFile())
        return;
    const bool wasPlaying = m_playing;
    const bool cropTool = m_tool == QLatin1String("crop");
    m_previewFullTime = m_timeDragging;

    CompileOptions options;
    options.applyTime = !m_previewFullTime;
    options.applyCrop = !cropTool;

    Edit shown = m_edit;
    if (cropTool) {
        shown.crop = QRectF(0, 0, 1, 1);
        shown.cropShape = QStringLiteral("original");
    }
    if (m_tool == QLatin1String("text"))
        shown.labels.clear();
    if (!cropTool)
        options.captionImage = writeCaptionImage(shown);
    options.photoPath = photoForEdit(m_edit);
    const QString xml = compileXml(m_edit, options);

    const int position = (m_edit.isPhoto || m_previewFullTime)
                             ? qMax(0, sourceFrame)
                             : m_edit.editedFromSource(sourceFrame);
    // Nothing visible changed, such as switching to Text with no text yet, or undoing a change
    // to words the Text tool draws itself. Reloading would only cost time, more so from a USB
    // stick, so the preview just moves to the frame.
    const QSize frame = options.applyCrop ? m_edit.outputSize() : m_edit.rotatedSize();
    const QString previewKey = QStringLiteral("%1x%2|").arg(frame.width()).arg(frame.height())
                               + xml;
    if (previewKey == m_previewXml && MLT.producer() && MLT.producer()->is_valid()) {
        if (!m_edit.isPhoto && sourceFrame != m_playhead)
            MAIN.player()->seek(position);
        m_playhead = m_edit.isPhoto ? 0 : sourceFrame;
        emit positionChanged();
        emit changed();
        return;
    }
    m_previewXml.clear();

    auto &profile = MLT.profile();
    profile.set_width(frame.width());
    profile.set_height(frame.height());
    profile.set_progressive(1);
    profile.set_sample_aspect(1, 1);
    profile.set_display_aspect(frame.width(), frame.height());
    profile.set_frame_rate(m_edit.fpsNum, m_edit.fpsDen);
    profile.set_colorspace(709);
    profile.set_explicit(1);
    MLT.updatePreviewProfile();
    // Very large videos preview at a smaller size so an Intel laptop can keep up. The saved file
    // is always full size.
    MLT.setPreviewScale(qMax(frame.width(), frame.height()) > 2000 ? 720 : 0);

    auto *producer = new Mlt::Producer(profile, "xml-string", xml.toUtf8().constData());
    if (!producer->is_valid()) {
        delete producer;
        showToast(QStringLiteral("Sorry, BUGS! couldn't show that file."), true);
        return;
    }
    if (MLT.setProducer(producer)) {
        showToast(QStringLiteral("Sorry, BUGS! couldn't show that file."), true);
        return;
    }
    MAIN.player()->onProducerOpened(false);
    m_previewXml = previewKey;

    m_playhead = m_edit.isPhoto ? 0 : sourceFrame;
    MLT.pause();
    MAIN.player()->seek(m_edit.isPhoto ? 0 : position);
    m_playing = false;
    if (wasPlaying && !m_edit.isPhoto)
        MAIN.player()->play(m_speed);
    emit positionChanged();
    emit playingChanged();
    emit changed();
}

void SimpleController::onFrameDisplayed(const SharedFrame &frame)
{
    if (!hasFile() || m_edit.isPhoto || !MLT.producer())
        return;
    const int position = frame.get_position();
    const int source = m_previewFullTime ? position : m_edit.sourceFromEdited(position);
    if (source != m_playhead) {
        m_playhead = source;
        emit positionChanged();
        if (m_tool == QLatin1String("frame") && !m_playing)
            m_frameThumbTimer.start();
    }
    if (m_playing && position >= MLT.producer()->get_length() - 1)
        MAIN.player()->pause();
}

void SimpleController::updateFrameThumb()
{
    if (!hasFile() || m_edit.isPhoto)
        return;
    const int generation = ++m_frameThumbGeneration;
    const Edit edit = m_edit;
    const int position = m_edit.editedFromSource(m_playhead);
    QPointer<SimpleController> self(this);
    (void) QtConcurrent::run([self, edit, position, generation]() {
        const QImage image = renderFrame(edit, position);
        if (image.isNull() || !self)
            return;
        const QString path
            = QDir(cacheDir()).filePath(QStringLiteral("frame-%1.jpg").arg(generation % 8));
        image.scaledToWidth(640, Qt::SmoothTransformation)
            .convertToFormat(QImage::Format_RGB32)
            .save(path, "JPG", 85);
        QMetaObject::invokeMethod(
            self.data(),
            [self, path, generation]() {
                if (!self || generation != self->m_frameThumbGeneration)
                    return;
                // The extra part of the URL stops the picture being reused from Qt's cache.
                self->m_frameThumb = QUrl::fromLocalFile(path).toString()
                                     + QStringLiteral("?%1").arg(generation);
                emit self->frameThumbChanged();
            },
            Qt::QueuedConnection);
    });
}

void SimpleController::savePicture()
{
    if (!hasFile() || m_edit.isPhoto || !filesStillThere({m_edit.sourcePath, m_edit.mediaPath}))
        return;
    const QString picture = m_edit.sourcePath + QLatin1Char('|') + QString::number(m_playhead);
    if (picture == m_lastPicture && m_lastPictureTime.isValid()
        && m_lastPictureTime.elapsed() < 1500)
        return;
    m_lastPicture = picture;
    m_lastPictureTime.start();
    pausePreview();
    const Edit edit = m_edit;
    const int position = m_edit.editedFromSource(m_playhead);
    // Tenths of a second, so two pictures from the same second get their own names.
    const QString stamp
        = formatTime(m_playhead / m_edit.fps(), true).replace(QLatin1Char(':'), QLatin1Char('-'));
    const QString base = QStringLiteral("%1 - picture at %2")
                             .arg(QFileInfo(edit.sourcePath).completeBaseName(), stamp);
    // The name is picked here, not on the worker: two pictures saving at once used to both pick
    // the same free name, and the second replaced the first.
    QString path = uniquePath(finishedDir(), base, QStringLiteral("jpg"));
    for (int i = 2; m_picturesBeingSaved.contains(path); ++i)
        path = uniquePath(finishedDir(),
                          QStringLiteral("%1 (%2)").arg(base).arg(i),
                          QStringLiteral("jpg"));
    m_picturesBeingSaved << path;
    const QString source = edit.sourcePath;
    QPointer<SimpleController> self(this);
    (void) QtConcurrent::run([self, edit, position, path, source]() {
        const QImage image = renderFrame(edit, position);
        const bool ok = !image.isNull()
                        && image.convertToFormat(QImage::Format_RGB32).save(path, "JPG", 95);
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self, ok, path, source]() {
                if (!self)
                    return;
                self->m_picturesBeingSaved.removeAll(path);
                if (ok) {
                    self->m_savedPictures[source].prepend(path);
                    emit self->changed();
                    self->afterSaved(path, QStringLiteral("Picture saved to Finished"));
                } else {
                    self->showToast(QStringLiteral(
                                        "Sorry, that picture didn't save. Please try again."),
                                    true);
                }
            },
            Qt::QueuedConnection);
    });
}

QVariantList SimpleController::savedPictures() const
{
    QVariantList result;
    for (const QString &path : m_savedPictures.value(m_edit.sourcePath)) {
        if (!QFileInfo::exists(path))
            continue;
        QVariantMap item;
        item[QStringLiteral("path")] = path;
        item[QStringLiteral("url")] = QUrl::fromLocalFile(path).toString();
        // "IMG_0057 - picture at 1-02.5" shows as "1:02.5".
        QString time = QFileInfo(path).completeBaseName().section(QStringLiteral(" - picture at "),
                                                                  1);
        item[QStringLiteral("time")] = time.replace(QLatin1Char('-'), QLatin1Char(':'));
        result << item;
    }
    return result;
}

void SimpleController::removeSavedPicture(const QString &path)
{
    // Only pictures BUGS! saved in this session, never anything else in Finished.
    QStringList &pictures = m_savedPictures[m_edit.sourcePath];
    if (!pictures.contains(path))
        return;
    if (QFileInfo::exists(path) && !QFile::moveToTrash(path) && !QFile::remove(path)) {
        showToast(QStringLiteral("Sorry, BUGS! couldn't remove that picture."), true);
        return;
    }
    pictures.removeAll(path);
    emit changed();
    refreshFinished();
    showToast(QStringLiteral("Picture removed"));
}

void SimpleController::finish()
{
    if (m_collageActive && !m_saving && !m_collageDesign.layout.empty()) {
        // Each photo is read at the size its frame needs, so many large photos fit in memory.
        m_saving = true;
        m_saveProgress = 0;
        emit savingChanged();
        const QSize size = collageSize(2400);
        QStringList files;
        for (const Collage::Photo &photo : m_collagePhotos)
            files << photo.path;
        if (!filesStillThere(files)) {
            m_saving = false;
            emit savingChanged();
            return;
        }
        // Copies, so nothing the worker reads can change underneath it.
        const Collage::Layout layout = m_collageDesign.layout;
        const QVector<Collage::PhotoCrop> crops = m_collageDesign.crops;
        const CollageStyle style = collageStyle();
        // Text and stickers, with the sticker pictures they use, drawn by the same code as the
        // preview's.
        const QVector<CollageItem> items = m_collageDesign.items;
        QHash<QString, QImage> stickers;
        for (const CollageItem &item : items) {
            if (item.kind == CollageItem::Kind::Sticker)
                stickers.insert(item.sticker, stickerImage(item.sticker));
        }
        QPointer<SimpleController> self(this);
        (void) QtConcurrent::run([self, files, layout, crops, size, style, items, stickers]() {
            QImage image
                = renderCollage(layout, crops, size, style, [&](int photo, const QSize &needed) {
                      return photo >= 0 && photo < files.size()
                                 ? loadPhotoToCover(files.at(photo), needed)
                                 : QImage();
                  });
            if (!image.isNull() && !items.isEmpty()) {
                QPainter painter(&image);
                paintCollageItems(painter, items, size, [&](const QString &id) {
                    return stickers.value(id);
                });
            }
            const QString path = uniquePath(finishedDir(),
                                            QStringLiteral("Collage %1")
                                                .arg(QDate::currentDate().toString(Qt::ISODate)),
                                            QStringLiteral("jpg"));
            const bool ok = !image.isNull() && image.save(path, "JPG", 92);
            if (!self)
                return;
            QMetaObject::invokeMethod(
                self.data(),
                [self, ok, path]() {
                    if (!self)
                        return;
                    self->m_saving = false;
                    emit self->savingChanged();
                    if (ok) {
                        self->m_collageUnsaved = false;
                        self->afterSaved(path, QStringLiteral("Collage saved to Finished"));
                    } else {
                        self->showToast(QStringLiteral(
                                            "Sorry, the collage didn't save. Please try again."),
                                        true);
                    }
                },
                Qt::QueuedConnection);
        });
        return;
    }
    // While photos are being picked for a collage, there is nothing to save yet.
    if (!hasFile() || m_saving || m_collagePicking)
        return;
    // MLT would save its "INVALID" placeholder in place of a file that is gone.
    if (!filesStillThere({m_edit.sourcePath, m_edit.mediaPath}))
        return;
    pausePreview();
    if (!m_edit.isPhoto) {
        m_exportEdit = m_edit;
        startVideoExport(m_hardwareEncoder == 1);
        return;
    }

    m_saving = true;
    m_saveProgress = 0;
    emit savingChanged();
    const Edit edit = m_edit;
    const bool keepPng
        = QFileInfo(edit.sourcePath).suffix().compare(QLatin1String("png"), Qt::CaseInsensitive)
          == 0;
    // A photo with no changes is copied as it is, keeping its quality and name. An iPhone photo
    // is still turned into a JPG, which every phone and computer can open.
    const bool unchanged = !edit.hasChanges();
    const bool copyAsIs = unchanged && edit.mediaPath == edit.sourcePath;
    const QString base = QFileInfo(edit.sourcePath).completeBaseName()
                         + (unchanged ? QString() : QStringLiteral(" - edited"));
    QPointer<SimpleController> self(this);
    (void) QtConcurrent::run([self, edit, keepPng, base, copyAsIs]() {
        QString path;
        bool ok = false;
        if (copyAsIs) {
            path = uniquePath(finishedDir(), base, QFileInfo(edit.sourcePath).suffix().toLower());
            ok = QFile::copy(edit.sourcePath, path);
        }
        const QImage image = copyAsIs ? QImage() : renderFrame(edit, 0);
        if (!image.isNull()) {
            path = uniquePath(finishedDir(),
                              base,
                              keepPng ? QStringLiteral("png") : QStringLiteral("jpg"));
            ok = keepPng ? image.save(path, "PNG")
                         : image.convertToFormat(QImage::Format_RGB32).save(path, "JPG", 95);
        }
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self, ok, path, edit]() {
                if (!self)
                    return;
                self->m_saving = false;
                emit self->savingChanged();
                if (ok) {
                    self->m_savedEdits.insert(edit.sourcePath, edit);
                    self->afterSaved(path, QStringLiteral("Saved to Finished."));
                    self->showFinishedFor(edit.sourcePath);
                } else {
                    self->showToast(QStringLiteral("Sorry, that didn't save. Please try again."),
                                    true);
                }
            },
            Qt::QueuedConnection);
    });
}

void SimpleController::startVideoExport(bool allowHardware)
{
    const Edit &edit = m_exportEdit;
    const QString base = QFileInfo(edit.sourcePath).completeBaseName()
                         + QStringLiteral(" - edited");
    m_exportTarget = uniquePath(finishedDir(), base, QStringLiteral("mp4"));
    // Written under a temporary name so a half-finished file never shows up in Finished.
    m_exportPartial = m_exportTarget + QStringLiteral(".part.mp4");
    QFile::remove(m_exportPartial);
    m_exportUsedHardware = allowHardware;
    m_exportCancelled = false;

    CompileOptions options;
    options.captionImage = writeCaptionImage(edit);
    options.target = QDir::toNativeSeparators(m_exportPartial);
    if (allowHardware)
        options.videoCodec = QStringLiteral("h264_qsv");
    // Like Shotcut's "Parallel processing": a few threads prepare frames at once, which is most
    // of the time a save takes. One core is left for the rest of the laptop.
    const int cores = QThread::idealThreadCount();
    options.realTime = cores > 2 ? -qMin(cores - 1, 4) : -1;

    auto *job = new MeltJob(QFileInfo(m_exportTarget).fileName(),
                            compileXml(edit, options),
                            edit.fpsNum,
                            edit.fpsDen);
    job->setTarget(m_exportTarget);
    m_job = job;
    m_saving = true;
    m_saveProgress = 0;
    emit savingChanged();
    connect(job, &AbstractJob::progressUpdated, this, [this](QStandardItem *, int percent) {
        m_saveProgress = percent;
        emit savingChanged();
    });
    connect(job,
            &AbstractJob::finished,
            this,
            [this](AbstractJob *finishedJob, bool success, QString) {
                onExportFinished(finishedJob, success);
            });
    JOBS.add(job);
}

void SimpleController::onExportFinished(AbstractJob *job, bool success)
{
    if (job != m_job)
        return;
    m_job.clear();

    if (m_exportCancelled) {
        QFile::remove(m_exportPartial);
        m_saving = false;
        emit savingChanged();
        showToast(QStringLiteral("Stopped. Nothing was saved."));
        return;
    }

    // Trust the file rather than the exit code: Finished must only ever hold good videos. Opening
    // a long video to check it takes a moment, so that happens off the screen's thread.
    const QString partial = m_exportPartial;
    QPointer<SimpleController> self(this);
    (void) QtConcurrent::run([self, partial, success]() {
        Edit check;
        const bool valid = success && QFileInfo(partial).size() > 0 && openMedia(partial, &check)
                           && check.frames > 0;
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self, valid]() {
                if (self)
                    self->onExportChecked(valid);
            },
            Qt::QueuedConnection);
    });
}

void SimpleController::onExportChecked(bool valid)
{
    if (!valid) {
        QFile::remove(m_exportPartial);
        if (m_exportUsedHardware) {
            // Graphics drivers vary. Fall back to the slower encoder that always works.
            LOG_WARNING() << "hardware encode failed; retrying in software";
            m_hardwareEncoder = 0;
            startVideoExport(false);
            return;
        }
        finishExportFailed();
        return;
    }
    // Picks a fresh name in case one appeared while this was saving. Never overwrites.
    const QFileInfo target(m_exportTarget);
    moveExportIntoPlace(uniquePath(target.absolutePath(),
                                   target.completeBaseName(),
                                   target.suffix()),
                        0);
}

void SimpleController::moveExportIntoPlace(const QString &finalPath, int attempt)
{
    QFile partial(m_exportPartial);
    if (partial.rename(finalPath)) {
        exportSaved(finalPath);
        return;
    }
    LOG_WARNING() << "could not rename" << m_exportPartial << "to" << finalPath << "attempt"
                  << attempt << partial.errorString();
    if (attempt < 10 && !QFileInfo::exists(finalPath)) {
        QTimer::singleShot(500, this, [this, finalPath, attempt]() {
            moveExportIntoPlace(finalPath, attempt + 1);
        });
        return;
    }
    // Copying works even when something keeps the file open for reading.
    const QFileInfo target(finalPath);
    const QString copyPath = uniquePath(target.absolutePath(),
                                        target.completeBaseName(),
                                        target.suffix());
    if (QFile::copy(m_exportPartial, copyPath)) {
        QFile::remove(m_exportPartial);
        exportSaved(copyPath);
        return;
    }
    QFile::remove(copyPath);
    QFile::remove(m_exportPartial);
    finishExportFailed();
}

void SimpleController::exportSaved(const QString &finalPath)
{
    m_saving = false;
    emit savingChanged();
    m_savedEdits.insert(m_exportEdit.sourcePath, m_exportEdit);
    if (m_exportEdit.sourcePath == m_edit.sourcePath)
        afterSaved(finalPath, QStringLiteral("Saved to Finished."));
    else
        afterSaved(finalPath,
                   QStringLiteral("%1 is saved to Finished.")
                       .arg(QFileInfo(m_exportEdit.sourcePath).fileName()));
    showFinishedFor(m_exportEdit.sourcePath);
}

void SimpleController::showFinishedFor(const QString &sourcePath)
{
    // Levi may have opened another file while this one saved. Switching the list then would
    // hide the file he is working on.
    if (sourcePath == m_edit.sourcePath)
        setTab(QStringLiteral("finished"));
}

void SimpleController::finishExportFailed()
{
    m_saving = false;
    emit savingChanged();
    showToast(QStringLiteral("Sorry, that didn't save. Please try again."), true);
}

void SimpleController::cancelFinish()
{
    if (!m_saving || !m_job)
        return;
    m_exportCancelled = true;
    m_job->stop();
}

void SimpleController::startCollage()
{
    pausePreview();
    m_collagePicking = true;
    m_collagePicks.clear();
    setTab(QStringLiteral("originals"));
    emit changed();
    emit libraryChanged();
}

void SimpleController::toggleCollagePick(const QString &path)
{
    if (!m_collagePicking || !isPhotoPath(path))
        return;
    if (!m_collagePicks.removeOne(path))
        m_collagePicks << path;
    emit changed();
    emit libraryChanged();
}

void SimpleController::makeCollage()
{
    if (m_collagePicks.size() < 2 || m_collageMaking)
        return;
    // Reading big phone photos, and turning iPhone photos into ones BUGS! can read, takes a
    // while, so it happens off the screen's thread.
    m_collageMaking = true;
    emit changed();
    const QStringList picks = m_collagePicks;
    const int generation = ++m_collageMakeGeneration;
    QPointer<SimpleController> self(this);
    (void) QtConcurrent::run([self, picks, generation]() {
        std::vector<Collage::Photo> photos;
        QHash<QString, QImage> thumbs;
        QStringList unreadable;
        for (const auto &path : picks) {
            QSize size;
            const QString readable = uprightPhotoPath(path, &size);
            if (readable.isEmpty() || size.isEmpty()) {
                unreadable << QFileInfo(path).fileName();
                continue;
            }
            photos.push_back(Collage::Photo{readable, size.width(), size.height()});
            // The preview reads them at this size; see drawCollagePreview().
            thumbs.insert(readable, loadPhotoToCover(readable, QSize(1200, 1200)));
        }
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self, photos, thumbs, unreadable, generation]() {
                if (!self || generation != self->m_collageMakeGeneration)
                    return;
                self->collagePhotosRead(photos, thumbs, unreadable);
            },
            Qt::QueuedConnection);
    });
}

void SimpleController::collagePhotosRead(const std::vector<Collage::Photo> &photos,
                                         const QHash<QString, QImage> &thumbs,
                                         const QStringList &unreadable)
{
    m_collageMaking = false;
    // Levi cancelled, or went to another file, while they were read.
    if (!m_collagePicking) {
        emit changed();
        return;
    }
    if (photos.size() < 2) {
        showToast(QStringLiteral("BUGS! couldn't open enough of those photos."), true);
        emit changed();
        return;
    }
    if (!unreadable.isEmpty())
        showToast(QStringLiteral("BUGS! couldn't open %1, so the collage is without %2.")
                      .arg(unreadable.join(QStringLiteral(", ")),
                           unreadable.size() == 1 ? QStringLiteral("it") : QStringLiteral("them")),
                  true);
    m_collagePhotos.assign(photos.begin(), photos.end());
    m_collageThumbs = thumbs;
    m_collagePicking = false;
    m_collageActive = true;
    // A new collage starts without text, stickers, zoom or a big photo, keeping the last shape,
    // layout style, spacing and colours.
    m_collageDesign.seed = 1;
    m_collageDesign.hero = -1;
    m_collageDesign.items.clear();
    m_collageDesign.crops = QVector<Collage::PhotoCrop>(int(m_collagePhotos.size()));
    m_collageDesign.layout = buildCollageLayout(m_collageDesign);
    m_collageSelected = -1;
    m_collageSelectedItem = -1;
    m_collageUndo.clear();
    m_collageRedo.clear();
    updateCollageLayoutChoices();
    drawCollagePreview();
    emit changed();
    emit libraryChanged();
}

void SimpleController::leaveCollage()
{
    m_collagePicking = false;
    // Photos still being read for a collage are no longer wanted.
    m_collageMaking = false;
    ++m_collageMakeGeneration;
    m_collageDesign.items.clear();
    m_collageSelectedItem = -1;
    m_collageGesture = false;
    m_collageUndo.clear();
    m_collageRedo.clear();
    m_collagePicks.clear();
    m_collageActive = false;
    m_collageDesign.layout.clear();
    m_collageDesign.crops.clear();
    m_collageDesign.hero = -1;
    m_collageLayouts.clear();
    m_collagePhotoGesture = false;
    m_collagePhotos.clear();
    m_collageThumbs.clear();
    m_collageSelected = -1;
    emit changed();
    emit libraryChanged();
}

QSize SimpleController::collageSize(int longSide) const
{
    if (m_collageDesign.shape == QLatin1String("story"))
        return QSize(longSide * 9 / 16, longSide);
    if (m_collageDesign.shape == QLatin1String("wide"))
        return QSize(longSide, longSide * 9 / 16);
    if (m_collageDesign.shape == QLatin1String("portrait"))
        return QSize(longSide * 4 / 5, longSide);
    return QSize(longSide, longSide);
}

double SimpleController::collageRatio() const
{
    if (m_collageDesign.shape == QLatin1String("story"))
        return 16.0 / 9.0;
    if (m_collageDesign.shape == QLatin1String("wide"))
        return 9.0 / 16.0;
    if (m_collageDesign.shape == QLatin1String("portrait"))
        return 5.0 / 4.0;
    return 1.0;
}

double SimpleController::collageAspect() const
{
    return 1.0 / collageRatio();
}

Collage::Layout SimpleController::buildCollageLayout(const CollageDesign &design) const
{
    const int n = int(m_collagePhotos.size());
    if (n == 0)
        return {};
    double ratio = 1.0;
    if (design.shape == QLatin1String("story"))
        ratio = 16.0 / 9.0;
    else if (design.shape == QLatin1String("wide"))
        ratio = 9.0 / 16.0;
    else if (design.shape == QLatin1String("portrait"))
        ratio = 5.0 / 4.0;
    if (design.layoutStyle == QLatin1String("grid"))
        return Collage::makeGridLayout(n, ratio, design.seed);
    if (design.layoutStyle == QLatin1String("mosaic")) {
        std::vector<double> aspects;
        std::vector<double> weights;
        for (int i = 0; i < n; ++i) {
            aspects.push_back(m_collagePhotos[size_t(i)].ratio());
            weights.push_back(i == design.hero ? 3.0 : 1.0);
        }
        return Collage::makeMosaicLayout(aspects, weights, ratio, design.seed);
    }
    // PhotoCollage's columns. Its photos are copied so laying out never touches the real ones.
    std::vector<Collage::Photo> copies(m_collagePhotos.begin(), m_collagePhotos.end());
    std::vector<Collage::Photo *> pointers;
    for (auto &photo : copies)
        pointers.push_back(&photo);
    const auto page = Collage::makeGaplessPage(pointers, ratio, design.seed);
    return Collage::layoutFromPage(*page, pointers);
}

void SimpleController::updateCollageLayoutChoices()
{
    // Small pictures of each style for the picker, with this collage's photos and big photo.
    static const struct
    {
        const char *id;
        const char *title;
    } styles[] = {{"columns", "Columns"}, {"mosaic", "Mosaic"}, {"grid", "Grid"}};
    m_collageLayouts.clear();
    for (const auto &style : styles) {
        CollageDesign design = m_collageDesign;
        design.layoutStyle = QString::fromLatin1(style.id);
        const Collage::Layout layout = design.layoutStyle == m_collageDesign.layoutStyle
                                           ? m_collageDesign.layout
                                           : buildCollageLayout(design);
        QVariantList cells;
        for (const Collage::Slot &slot : layout) {
            QVariantMap cell;
            cell[QStringLiteral("x")] = slot.rect.x();
            cell[QStringLiteral("y")] = slot.rect.y();
            cell[QStringLiteral("w")] = slot.rect.width();
            cell[QStringLiteral("h")] = slot.rect.height();
            cell[QStringLiteral("big")] = design.layoutStyle == QLatin1String("mosaic")
                                          && slot.photo == design.hero;
            cells << cell;
        }
        QVariantMap entry;
        entry[QStringLiteral("id")] = design.layoutStyle;
        entry[QStringLiteral("title")] = QString::fromLatin1(style.title);
        entry[QStringLiteral("cells")] = cells;
        m_collageLayouts << entry;
    }
}

void SimpleController::changeCollageDesign(const CollageDesign &design, const QString &coalesceKey)
{
    if (!m_collageActive || design == m_collageDesign)
        return;
    pushCollageUndo(m_collageDesign, coalesceKey);
    applyCollageDesign(design);
}

void SimpleController::drawCollagePreview()
{
    if (m_collageDesign.layout.empty())
        return;
    m_collageUnsaved = true;
    const QSize size = collageSize(1200);
    const CollageStyle style = collageStyle();
    const QImage image = renderCollage(
        m_collageDesign.layout, m_collageDesign.crops, size, style, [&](int photo, const QSize &) {
            if (photo < 0 || photo >= int(m_collagePhotos.size()))
                return QImage();
            const QString path = m_collagePhotos[size_t(photo)].path;
            auto it = m_collageThumbs.find(path);
            if (it == m_collageThumbs.end())
                it = m_collageThumbs.insert(path, loadPhotoToCover(path, QSize(1600, 1600)));
            return it.value();
        });
    const int generation = ++m_collageGeneration;
    const QString file
        = QDir(cacheDir()).filePath(QStringLiteral("collage-%1.jpg").arg(generation % 4));
    image.save(file, "JPG", 90);
    // The changing number stops the picture being reused from Qt's cache.
    m_collagePreview = QUrl::fromLocalFile(file).toString() + QStringLiteral("?%1").arg(generation);
    emit changed();
}

QVariantList SimpleController::collageCells() const
{
    QVariantList result;
    for (const Collage::Slot &slot : m_collageDesign.layout) {
        QVariantMap item;
        item[QStringLiteral("x")] = slot.rect.x();
        item[QStringLiteral("y")] = slot.rect.y();
        item[QStringLiteral("w")] = slot.rect.width();
        item[QStringLiteral("h")] = slot.rect.height();
        result << item;
    }
    return result;
}

void SimpleController::setCollageShape(const QString &shape)
{
    if (shape == m_collageDesign.shape || !m_collageActive)
        return;
    CollageDesign design = m_collageDesign;
    design.shape = shape;
    design.layout = buildCollageLayout(design);
    m_collageSelected = -1;
    changeCollageDesign(design);
}

void SimpleController::setCollageLayout(const QString &style)
{
    if (style == m_collageDesign.layoutStyle || !m_collageActive)
        return;
    CollageDesign design = m_collageDesign;
    design.layoutStyle = style;
    // Only Mosaic has a big photo.
    if (style != QLatin1String("mosaic"))
        design.hero = -1;
    design.layout = buildCollageLayout(design);
    m_collageSelected = -1;
    changeCollageDesign(design);
}

double SimpleController::collagePhotoZoom() const
{
    if (m_collageSelected < 0 || m_collageSelected >= int(m_collageDesign.layout.size()))
        return 1.0;
    const int photo = m_collageDesign.layout[size_t(m_collageSelected)].photo;
    return photo >= 0 && photo < m_collageDesign.crops.size() ? m_collageDesign.crops.at(photo).zoom
                                                              : 1.0;
}

bool SimpleController::collagePhotoIsBig() const
{
    return m_collageSelected >= 0 && m_collageSelected < int(m_collageDesign.layout.size())
           && m_collageDesign.layoutStyle == QLatin1String("mosaic")
           && m_collageDesign.layout[size_t(m_collageSelected)].photo == m_collageDesign.hero;
}

void SimpleController::setCollagePhotoBig(bool big)
{
    if (m_collageSelected < 0 || m_collageSelected >= int(m_collageDesign.layout.size()))
        return;
    const int photo = m_collageDesign.layout[size_t(m_collageSelected)].photo;
    CollageDesign design = m_collageDesign;
    // The big photo needs Mosaic, which can give one photo more room.
    design.layoutStyle = QStringLiteral("mosaic");
    design.hero = big ? photo : -1;
    design.layout = buildCollageLayout(design);
    changeCollageDesign(design);
    // Keep the same photo selected in its new frame.
    for (int i = 0; i < int(m_collageDesign.layout.size()); ++i) {
        if (m_collageDesign.layout[size_t(i)].photo == photo)
            m_collageSelected = i;
    }
    emit changed();
}

void SimpleController::setCollagePhotoZoom(double zoom)
{
    if (m_collageSelected < 0 || m_collageSelected >= int(m_collageDesign.layout.size()))
        return;
    const int photo = m_collageDesign.layout[size_t(m_collageSelected)].photo;
    if (photo < 0 || photo >= m_collageDesign.crops.size())
        return;
    CollageDesign design = m_collageDesign;
    design.crops[photo].zoom = qBound(1.0, zoom, 4.0);
    changeCollageDesign(design, QStringLiteral("zoom%1").arg(photo));
}

void SimpleController::zoomCollagePhoto(double factor)
{
    setCollagePhotoZoom(collagePhotoZoom() * factor);
}

void SimpleController::panCollagePhoto(double dx, double dy)
{
    if (m_collageSelected < 0 || m_collageSelected >= int(m_collageDesign.layout.size()))
        return;
    const Collage::Slot slot = m_collageDesign.layout[size_t(m_collageSelected)];
    const int photo = slot.photo;
    if (photo < 0 || photo >= m_collageDesign.crops.size() || photo >= int(m_collagePhotos.size()))
        return;
    if (!m_collagePhotoGesture) {
        m_collageGestureStart = m_collageDesign;
        m_collagePhotoGesture = true;
    }
    // How much of the photo the frame shows, as fractions of it, so the photo follows the
    // pointer exactly.
    const Collage::Photo &p = m_collagePhotos[size_t(photo)];
    const QSize page = collageSize(1200);
    const QRectF shown = collageSourceRect(QSize(p.w, p.h),
                                           QSizeF(slot.rect.width() * page.width(),
                                                  slot.rect.height() * page.height()),
                                           m_collageDesign.crops.at(photo));
    Collage::PhotoCrop &crop = m_collageDesign.crops[photo];
    const double sw = shown.width() / p.w;
    const double sh = shown.height() / p.h;
    // Dragging right shows more of the left of the photo.
    crop.cx = qBound(sw / 2, crop.cx - dx * sw, 1.0 - sw / 2);
    crop.cy = qBound(sh / 2, crop.cy - dy * sh, 1.0 - sh / 2);
    drawCollagePreview();
}

void SimpleController::endCollagePhotoGesture()
{
    if (!m_collagePhotoGesture)
        return;
    m_collagePhotoGesture = false;
    if (m_collageDesign != m_collageGestureStart)
        pushCollageUndo(m_collageGestureStart);
    m_collageCoalesceKey.clear();
    emit changed();
}

void SimpleController::deselectCollagePhoto()
{
    if (m_collageSelected < 0)
        return;
    m_collageSelected = -1;
    emit changed();
}

CollageStyle SimpleController::collageStyle() const
{
    CollageStyle style;
    style.border = m_collageDesign.spacing;
    style.radius = m_collageDesign.corners;
    style.borderColor = QColor(m_collageDesign.background);
    return style;
}

QStringList SimpleController::collageBackgrounds() const
{
    return {QStringLiteral("#FFFFFF"),
            QStringLiteral("#F6EEDF"),
            QStringLiteral("#FBE3EA"),
            QStringLiteral("#DDEEFB"),
            QStringLiteral("#E2F4E6"),
            QStringLiteral("#FFF3C4"),
            QStringLiteral("#E5392B"),
            QStringLiteral("#1F3A68"),
            QStringLiteral("#5B3A8C"),
            QStringLiteral("#111111")};
}

QStringList SimpleController::collagePhrases() const
{
    return {QStringLiteral("Mazel Tov!"),
            QStringLiteral("מזל טוב"),
            QStringLiteral("Good Shabbos"),
            QStringLiteral("שבת שלום"),
            QStringLiteral("Happy Birthday!"),
            QStringLiteral("Chag Sameach"),
            QStringLiteral("חג שמח"),
            QStringLiteral("Welcome Home")};
}

void SimpleController::applyCollageDesign(const CollageDesign &design)
{
    const bool lookChanged = !design.sameLook(m_collageDesign);
    const bool layoutChanged = design.layout != m_collageDesign.layout
                               || design.hero != m_collageDesign.hero;
    m_collageDesign = design;
    m_collageSelectedItem = qMin(m_collageSelectedItem, int(design.items.size()) - 1);
    // The frames moved, so the selected one would now mark another photo.
    if (layoutChanged || m_collageSelected >= int(design.layout.size()))
        m_collageSelected = -1;
    m_collageUnsaved = true;
    if (layoutChanged)
        updateCollageLayoutChoices();
    if (lookChanged)
        drawCollagePreview();
    emit changed();
}

void SimpleController::pushCollageUndo(const CollageDesign &before, const QString &coalesceKey)
{
    // Typing a word is one undo, not one per letter.
    if (coalesceKey.isEmpty() || coalesceKey != m_collageCoalesceKey)
        m_collageUndo.append(before);
    m_collageCoalesceKey = coalesceKey;
    m_collageRedo.clear();
}

void SimpleController::collageItemsChanged()
{
    m_collageUnsaved = true;
    emit changed();
}

void SimpleController::setCollageSpacing(double spacing)
{
    spacing = qBound(0.0, spacing, 0.06);
    if (!m_collageActive || qFuzzyCompare(spacing + 1, m_collageDesign.spacing + 1))
        return;
    CollageDesign design = m_collageDesign;
    design.spacing = spacing;
    pushCollageUndo(m_collageDesign, QStringLiteral("spacing"));
    applyCollageDesign(design);
}

void SimpleController::setCollageCorners(double corners)
{
    corners = qBound(0.0, corners, 0.08);
    if (!m_collageActive || qFuzzyCompare(corners + 1, m_collageDesign.corners + 1))
        return;
    CollageDesign design = m_collageDesign;
    design.corners = corners;
    pushCollageUndo(m_collageDesign, QStringLiteral("corners"));
    applyCollageDesign(design);
}

void SimpleController::setCollageBackground(const QString &color)
{
    if (!m_collageActive || color == m_collageDesign.background)
        return;
    CollageDesign design = m_collageDesign;
    design.background = color;
    pushCollageUndo(m_collageDesign);
    applyCollageDesign(design);
}

QImage SimpleController::stickerImage(const QString &id) const
{
    auto it = m_stickerImages.find(id);
    if (it == m_stickerImages.end()) {
        QDir dir = QmlUtilities::qmlDir();
        dir.cd(QStringLiteral("views/simple/stickers"));
        QImage image(dir.filePath(id + QStringLiteral(".png")));
        if (!image.isNull())
            image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        it = m_stickerImages.insert(id, image);
    }
    return it.value();
}

QVariantList SimpleController::stickerCategories() const
{
    // Read once: the list is long, and QML asks for it often.
    if (!m_stickerCategories.isEmpty())
        return m_stickerCategories;
    // Only the stickers on the approved list, never whatever else is in the folder.
    QDir dir = QmlUtilities::qmlDir();
    dir.cd(QStringLiteral("views/simple/stickers"));
    QFile file(dir.filePath(QStringLiteral("stickers.json")));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    QVariantList result;
    for (const QJsonValue &categoryValue : root.value(QStringLiteral("categories")).toArray()) {
        const QJsonObject category = categoryValue.toObject();
        const QString id = category.value(QStringLiteral("id")).toString();
        QVariantList stickers;
        for (const QJsonValue &stickerValue : root.value(QStringLiteral("stickers")).toArray()) {
            const QJsonObject sticker = stickerValue.toObject();
            if (sticker.value(QStringLiteral("category")).toString() != id)
                continue;
            const QString stickerId = sticker.value(QStringLiteral("id")).toString();
            QVariantMap item;
            item[QStringLiteral("id")] = stickerId;
            item[QStringLiteral("name")] = sticker.value(QStringLiteral("name")).toString();
            item[QStringLiteral("url")]
                = QUrl::fromLocalFile(dir.filePath(stickerId + QStringLiteral(".png"))).toString();
            stickers << item;
        }
        QVariantMap entry;
        entry[QStringLiteral("id")] = id;
        entry[QStringLiteral("title")] = category.value(QStringLiteral("title")).toString();
        entry[QStringLiteral("stickers")] = stickers;
        result << entry;
    }
    m_stickerCategories = result;
    return result;
}

void SimpleController::paintCollageItem(QPainter *painter,
                                        int index,
                                        const QRectF &box,
                                        const QSizeF &page) const
{
    if (!painter || index < 0 || index >= m_collageDesign.items.size())
        return;
    paintCollageItemInBox(*painter,
                          m_collageDesign.items.at(index),
                          page.toSize(),
                          box,
                          [this](const QString &id) { return stickerImage(id); });
}

QVariantList SimpleController::collageItems() const
{
    QVariantList result;
    // Sizes are worked out on a page the size of the preview picture, as fractions of the page,
    // so QML can lay the items out at whatever size it shows the collage.
    const QSize page = collageSize(1200);
    const auto sticker = [this](const QString &id) { return stickerImage(id); };
    for (int i = 0; i < m_collageDesign.items.size(); ++i) {
        const CollageItem &item = m_collageDesign.items.at(i);
        const QSizeF size = collageItemSize(item, page, sticker);
        QVariantMap map;
        map[QStringLiteral("index")] = i;
        map[QStringLiteral("kind")] = item.kind == CollageItem::Kind::Text
                                          ? QStringLiteral("text")
                                          : QStringLiteral("sticker");
        map[QStringLiteral("text")] = item.text;
        map[QStringLiteral("font")] = item.font;
        map[QStringLiteral("family")] = labelFontFamily(item.font);
        map[QStringLiteral("color")] = item.color;
        map[QStringLiteral("sticker")] = item.sticker;
        map[QStringLiteral("x")] = item.center.x();
        map[QStringLiteral("y")] = item.center.y();
        map[QStringLiteral("size")] = item.size;
        map[QStringLiteral("rotation")] = item.rotation;
        map[QStringLiteral("w")] = size.width() / page.width();
        map[QStringLiteral("h")] = size.height() / page.height();
        result << map;
    }
    return result;
}

void SimpleController::addCollageText(const QString &text)
{
    if (!m_collageActive)
        return;
    const CollageDesign before = m_collageDesign;
    CollageItem item;
    item.kind = CollageItem::Kind::Text;
    item.text = text.isEmpty() ? QStringLiteral("Mazel Tov!") : text;
    item.size = 0.08;
    // Each new one a little lower, so they do not land on top of each other.
    const int n = m_collageDesign.items.size();
    item.center = QPointF(0.5, 0.2 + 0.12 * (n % 6));
    // Keep the look of the selected text, so a second line matches the first.
    if (m_collageSelectedItem >= 0 && m_collageSelectedItem < n) {
        const CollageItem &selected = m_collageDesign.items.at(m_collageSelectedItem);
        if (selected.kind == CollageItem::Kind::Text) {
            item.font = selected.font;
            item.color = selected.color;
        }
    }
    m_collageDesign.items << item;
    m_collageSelectedItem = n;
    pushCollageUndo(before);
    collageItemsChanged();
}

void SimpleController::addSticker(const QString &id)
{
    if (!m_collageActive || stickerImage(id).isNull())
        return;
    const CollageDesign before = m_collageDesign;
    CollageItem item;
    item.kind = CollageItem::Kind::Sticker;
    item.sticker = id;
    item.size = 0.22;
    const int n = m_collageDesign.items.size();
    // Spread new stickers round the page instead of piling them in the middle.
    static const QPointF spots[] = {{0.5, 0.5},
                                    {0.25, 0.25},
                                    {0.75, 0.25},
                                    {0.25, 0.75},
                                    {0.75, 0.75},
                                    {0.5, 0.25},
                                    {0.5, 0.75}};
    item.center = spots[n % 7];
    m_collageDesign.items << item;
    m_collageSelectedItem = n;
    pushCollageUndo(before);
    collageItemsChanged();
}

void SimpleController::selectCollageItem(int index)
{
    if (index < -1 || index >= m_collageDesign.items.size() || index == m_collageSelectedItem)
        return;
    m_collageSelectedItem = index;
    m_collageCoalesceKey.clear();
    emit changed();
}

void SimpleController::moveCollageItem(int index, double x, double y)
{
    if (index < 0 || index >= m_collageDesign.items.size())
        return;
    if (!m_collageGesture) {
        m_collageGestureStart = m_collageDesign;
        m_collageGesture = true;
    }
    // The middle of an item stays on the page, so it can always be grabbed again.
    m_collageDesign.items[index].center = QPointF(qBound(0.0, x, 1.0), qBound(0.0, y, 1.0));
    collageItemsChanged();
}

void SimpleController::resizeCollageItem(int index, double size)
{
    if (index < 0 || index >= m_collageDesign.items.size())
        return;
    if (!m_collageGesture) {
        m_collageGestureStart = m_collageDesign;
        m_collageGesture = true;
    }
    m_collageDesign.items[index].size = qBound(0.02, size, 1.5);
    collageItemsChanged();
}

void SimpleController::rotateCollageItem(int index, double degrees)
{
    if (index < 0 || index >= m_collageDesign.items.size())
        return;
    if (!m_collageGesture) {
        m_collageGestureStart = m_collageDesign;
        m_collageGesture = true;
    }
    // Snaps to straight when close, so text is easy to put back level.
    double angle = std::fmod(degrees, 360.0);
    if (angle < 0)
        angle += 360.0;
    for (double straight : {0.0, 90.0, 180.0, 270.0, 360.0}) {
        if (qAbs(angle - straight) < 4.0)
            angle = std::fmod(straight, 360.0);
    }
    m_collageDesign.items[index].rotation = angle;
    collageItemsChanged();
}

void SimpleController::endCollageItemGesture()
{
    if (!m_collageGesture)
        return;
    m_collageGesture = false;
    if (m_collageDesign != m_collageGestureStart)
        pushCollageUndo(m_collageGestureStart);
    m_collageCoalesceKey.clear();
    emit changed();
}

void SimpleController::setCollageItemText(int index, const QString &text)
{
    if (index < 0 || index >= m_collageDesign.items.size()
        || m_collageDesign.items.at(index).text == text)
        return;
    const CollageDesign before = m_collageDesign;
    m_collageDesign.items[index].text = text;
    pushCollageUndo(before, QStringLiteral("text%1").arg(index));
    collageItemsChanged();
}

void SimpleController::setCollageItemColor(int index, const QString &color)
{
    if (index < 0 || index >= m_collageDesign.items.size()
        || m_collageDesign.items.at(index).color == color)
        return;
    const CollageDesign before = m_collageDesign;
    m_collageDesign.items[index].color = color;
    pushCollageUndo(before);
    collageItemsChanged();
}

void SimpleController::setCollageItemFont(int index, const QString &font)
{
    if (index < 0 || index >= m_collageDesign.items.size()
        || m_collageDesign.items.at(index).font == font)
        return;
    const CollageDesign before = m_collageDesign;
    m_collageDesign.items[index].font = font;
    pushCollageUndo(before);
    collageItemsChanged();
}

void SimpleController::removeCollageItem(int index)
{
    if (index < 0 || index >= m_collageDesign.items.size())
        return;
    const CollageDesign before = m_collageDesign;
    m_collageDesign.items.remove(index);
    m_collageSelectedItem = -1;
    pushCollageUndo(before);
    collageItemsChanged();
}

void SimpleController::bringCollageItemToFront(int index)
{
    const int last = m_collageDesign.items.size() - 1;
    if (index < 0 || index >= last)
        return;
    const CollageDesign before = m_collageDesign;
    m_collageDesign.items.move(index, last);
    m_collageSelectedItem = last;
    pushCollageUndo(before);
    collageItemsChanged();
}

void SimpleController::sendCollageItemToBack(int index)
{
    if (index <= 0 || index >= m_collageDesign.items.size())
        return;
    const CollageDesign before = m_collageDesign;
    m_collageDesign.items.move(index, 0);
    m_collageSelectedItem = 0;
    pushCollageUndo(before);
    collageItemsChanged();
}

void SimpleController::shuffleCollage()
{
    if (!m_collageActive)
        return;
    CollageDesign design = m_collageDesign;
    ++design.seed;
    design.layout = buildCollageLayout(design);
    m_collageSelected = -1;
    changeCollageDesign(design);
}

void SimpleController::tapCollage(double x, double y)
{
    if (m_collageDesign.layout.empty())
        return;
    // With text or a sticker selected, the first tap on a photo just lets go of it.
    if (m_collageSelectedItem >= 0) {
        m_collageSelectedItem = -1;
        emit changed();
        return;
    }
    // Tap one photo to select it (its zoom and size show on the right), then another to swap
    // them. Tapping the same one again lets go of it.
    int index = -1;
    for (int i = 0; i < int(m_collageDesign.layout.size()); ++i) {
        if (m_collageDesign.layout[size_t(i)].rect.contains(QPointF(x, y)))
            index = i;
    }
    if (index < 0)
        return;
    if (m_collageSelected < 0) {
        m_collageSelected = index;
        emit changed();
        return;
    }
    if (m_collageSelected != index) {
        CollageDesign design = m_collageDesign;
        std::swap(design.layout[size_t(m_collageSelected)].photo,
                  design.layout[size_t(index)].photo);
        m_collageSelected = -1;
        changeCollageDesign(design);
        return;
    }
    m_collageSelected = -1;
    emit changed();
}

void SimpleController::afterSaved(const QString &path, const QString &message)
{
    refreshFinished();
    ensureThumbnail(path);
    showToast(message);
}

void SimpleController::detectHardwareEncoder()
{
    QPointer<SimpleController> self(this);
    (void) QtConcurrent::run([self]() {
        // A very short test encode. If the laptop's graphics can't do it, Finish uses x264.
        const QFileInfo ffmpeg(QCoreApplication::applicationDirPath(), QStringLiteral("ffmpeg"));
        QProcess process;
        process.start(ffmpeg.absoluteFilePath(),
                      {QStringLiteral("-hide_banner"),
                       QStringLiteral("-f"),
                       QStringLiteral("lavfi"),
                       QStringLiteral("-i"),
                       QStringLiteral("color=c=black:s=640x360:r=30:d=0.2"),
                       QStringLiteral("-c:v"),
                       QStringLiteral("h264_qsv"),
                       QStringLiteral("-f"),
                       QStringLiteral("null"),
                       QStringLiteral("-")});
        const bool ok = process.waitForFinished(15000)
                        && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self, ok]() {
                if (self)
                    self->m_hardwareEncoder = ok ? 1 : 0;
            },
            Qt::QueuedConnection);
    });
}

void SimpleController::requestAdvanced()
{
    pausePreview();
    emit advancedRequested();
}

void SimpleController::showAbout()
{
    m_dialog = QStringLiteral("about");
    emit dialogChanged();
}

void SimpleController::askToClose()
{
    pausePreview();
    m_dialog = QStringLiteral("close");
    emit dialogChanged();
}

void SimpleController::dismissDialog()
{
    m_dialog.clear();
    emit dialogChanged();
}

void SimpleController::askToLeaveCollage()
{
    if (m_collageActive && m_collageUnsaved) {
        m_openAfterCollage.clear();
        m_dialog = QStringLiteral("leaveCollage");
        emit dialogChanged();
        return;
    }
    leaveCollage();
}

void SimpleController::leaveCollageAnyway()
{
    dismissDialog();
    leaveCollage();
    const QString path = m_openAfterCollage;
    m_openAfterCollage.clear();
    if (!path.isEmpty())
        openFile(path);
}

void SimpleController::closeAnyway()
{
    dismissDialog();
    emit closeConfirmed();
}

void SimpleController::showToast(const QString &text, bool isError)
{
    m_toast = text;
    m_toastIsError = isError;
    // Problems take longer to read, and matter more, than "Saved".
    m_toastTimer.start(isError ? 2 * kToastMs : kToastMs);
    emit toastChanged();
}
