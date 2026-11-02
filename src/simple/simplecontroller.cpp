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
#include "sharedframe.h"
#include "simplemedia.h"
#include "videowidget.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
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
    connect(&m_captionTimer, &QTimer::timeout, this, [this]() { rebuildPreview(m_playhead); });

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
        if (!isPhotoPath(path)) {
            Edit edit;
            if (openMedia(path, &edit))
                badge = badgeForMedia(edit);
        }
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self, path, thumb, badge]() {
                if (!self)
                    return;
                self->m_thumbsPending.removeAll(path);
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
        if (!m_originals.contains(path))
            m_originals.prepend(path);
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
            = QStringLiteral("%1 → %2").arg(formatTime(cut.start / m_edit.fps()),
                                            formatTime(cut.end / m_edit.fps()));
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
    return QStringLiteral("%1 → %2").arg(formatTime(m_selection.start / m_edit.fps()),
                                         formatTime(m_selection.end / m_edit.fps()));
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
        label.color = m_edit.labels.at(m_selectedLabel).color;
        label.size = m_edit.labels.at(m_selectedLabel).size;
        label.font = m_edit.labels.at(m_selectedLabel).font;
    }
    m_edit.labels << label;
    m_selectedLabel = n;
    pushUndo(before);
    afterEdit(false);
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
}

QString SimpleController::advancedResource()
{
    if (!hasFile())
        return QString();
    if (!m_edit.hasChanges() && !m_edit.isHdr())
        return m_edit.sourcePath;
    CompileOptions options;
    options.captionImage = writeCaptionImage(m_edit);
    const QString path = QDir(cacheDir())
                             .filePath(QFileInfo(m_edit.sourcePath).completeBaseName()
                                       + QStringLiteral(" - edit.mlt"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return m_edit.sourcePath;
    options.photoPath = photoForEdit(m_edit);
    QString xml = compileXml(m_edit, options);
    file.write(xml.toUtf8());
    file.close();
    return path;
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
        MAIN.player()->play();
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

    const QSize frame = options.applyCrop ? m_edit.outputSize() : m_edit.rotatedSize();
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

    options.photoPath = photoForEdit(m_edit);
    const QString xml = compileXml(m_edit, options);
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

    const int position = (m_edit.isPhoto || m_previewFullTime)
                             ? qMax(0, sourceFrame)
                             : m_edit.editedFromSource(sourceFrame);
    m_playhead = m_edit.isPhoto ? 0 : sourceFrame;
    MLT.pause();
    MAIN.player()->seek(m_edit.isPhoto ? 0 : position);
    m_playing = false;
    if (wasPlaying && !m_edit.isPhoto)
        MAIN.player()->play();
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
    if (!hasFile() || m_edit.isPhoto)
        return;
    pausePreview();
    const Edit edit = m_edit;
    const int position = m_edit.editedFromSource(m_playhead);
    const QString stamp
        = formatTime(m_playhead / m_edit.fps()).replace(QLatin1Char(':'), QLatin1Char('-'));
    const QString base = QStringLiteral("%1 - picture at %2")
                             .arg(QFileInfo(edit.sourcePath).completeBaseName(), stamp);
    QPointer<SimpleController> self(this);
    (void) QtConcurrent::run([self, edit, position, base]() {
        const QImage image = renderFrame(edit, position);
        QString path;
        bool ok = false;
        if (!image.isNull()) {
            path = uniquePath(finishedDir(), base, QStringLiteral("jpg"));
            ok = image.convertToFormat(QImage::Format_RGB32).save(path, "JPG", 95);
        }
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self, ok, path]() {
                if (!self)
                    return;
                if (ok)
                    self->afterSaved(path, QStringLiteral("Picture saved to Finished"));
                else
                    self->showToast(QStringLiteral(
                                        "Sorry, that picture didn't save. Please try again."),
                                    true);
            },
            Qt::QueuedConnection);
    });
}

void SimpleController::finish()
{
    if (!hasFile() || m_saving)
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
    const QString base = QFileInfo(edit.sourcePath).completeBaseName()
                         + QStringLiteral(" - edited");
    QPointer<SimpleController> self(this);
    (void) QtConcurrent::run([self, edit, keepPng, base]() {
        const QImage image = renderFrame(edit, 0);
        QString path;
        bool ok = false;
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
                    self->afterSaved(path,
                                     QStringLiteral(
                                         "Saved to Finished. Your original is untouched."));
                    self->setTab(QStringLiteral("finished"));
                } else {
                    self->showToast(
                        QStringLiteral(
                            "Sorry, that didn't save. Your original is safe. Please try again."),
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

    // Trust the file rather than the exit code: Finished must only ever hold good videos.
    Edit check;
    const bool valid = success && QFileInfo(m_exportPartial).size() > 0
                       && openMedia(m_exportPartial, &check) && check.frames > 0;
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
    const QString finalPath = uniquePath(target.absolutePath(),
                                         target.completeBaseName(),
                                         target.suffix());
    if (!QFile::rename(m_exportPartial, finalPath)) {
        QFile::remove(m_exportPartial);
        finishExportFailed();
        return;
    }
    m_saving = false;
    emit savingChanged();
    m_savedEdits.insert(m_exportEdit.sourcePath, m_exportEdit);
    afterSaved(finalPath, QStringLiteral("Saved to Finished. Your original is untouched."));
    setTab(QStringLiteral("finished"));
}

void SimpleController::finishExportFailed()
{
    m_saving = false;
    emit savingChanged();
    showToast(QStringLiteral("Sorry, that didn't save. Your original is safe. Please try again."),
              true);
}

void SimpleController::cancelFinish()
{
    if (!m_saving || !m_job)
        return;
    m_exportCancelled = true;
    m_job->stop();
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

void SimpleController::closeAnyway()
{
    dismissDialog();
    emit closeConfirmed();
}

void SimpleController::showToast(const QString &text, bool isError)
{
    m_toast = text;
    m_toastIsError = isError;
    m_toastTimer.start();
    emit toastChanged();
}
