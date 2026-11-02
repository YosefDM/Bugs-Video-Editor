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

#ifndef SIMPLECONTROLLER_H
#define SIMPLECONTROLLER_H

#include "simpleedit.h"

#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QRectF>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

class AbstractJob;
class SharedFrame;

// Everything the Simple mode screens show and do. The QML is only a view of this object.
class SimpleController : public QObject
{
    Q_OBJECT
    // Library and app
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY changed)
    Q_PROPERTY(bool welcome READ welcome NOTIFY changed)
    Q_PROPERTY(QString tab READ tab WRITE setTab NOTIFY changed)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY changed)
    Q_PROPERTY(QVariantList items READ items NOTIFY libraryChanged)
    Q_PROPERTY(int originalsCount READ originalsCount NOTIFY libraryChanged)
    Q_PROPERTY(int finishedCount READ finishedCount NOTIFY libraryChanged)
    Q_PROPERTY(QString toast READ toast NOTIFY toastChanged)
    Q_PROPERTY(bool toastIsError READ toastIsError NOTIFY toastChanged)
    Q_PROPERTY(bool saving READ saving NOTIFY savingChanged)
    Q_PROPERTY(int saveProgress READ saveProgress NOTIFY savingChanged)
    // The open file
    Q_PROPERTY(bool hasFile READ hasFile NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(bool isPhoto READ isPhoto NOTIFY changed)
    Q_PROPERTY(QString fileName READ fileName NOTIFY changed)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY changed)
    Q_PROPERTY(QString tool READ tool WRITE setTool NOTIFY changed)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY changed)
    // Timeline, in source frames
    Q_PROPERTY(int frames READ frames NOTIFY changed)
    Q_PROPERTY(int trimStart READ trimStart NOTIFY changed)
    Q_PROPERTY(int trimEnd READ trimEnd NOTIFY changed)
    Q_PROPERTY(QVariantList cuts READ cuts NOTIFY changed)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY changed)
    Q_PROPERTY(int selectionStart READ selectionStart NOTIFY changed)
    Q_PROPERTY(int selectionEnd READ selectionEnd NOTIFY changed)
    Q_PROPERTY(bool canRemoveSelection READ canRemoveSelection NOTIFY changed)
    Q_PROPERTY(QString selectionText READ selectionText NOTIFY changed)
    Q_PROPERTY(QString selectionInfo READ selectionInfo NOTIFY changed)
    Q_PROPERTY(QString trimStartText READ trimStartText NOTIFY changed)
    Q_PROPERTY(QString trimEndText READ trimEndText NOTIFY changed)
    Q_PROPERTY(QString lengthText READ lengthText NOTIFY changed)
    Q_PROPERTY(QStringList filmstrip READ filmstrip NOTIFY filmstripChanged)
    Q_PROPERTY(int playhead READ playhead NOTIFY positionChanged)
    Q_PROPERTY(QString positionText READ positionText NOTIFY positionChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    // Crop, in fractions of the rotated picture
    Q_PROPERTY(QRectF cropRect READ cropRect NOTIFY changed)
    Q_PROPERTY(QString cropShape READ cropShape NOTIFY changed)
    Q_PROPERTY(double cropAspect READ cropAspect NOTIFY changed)
    Q_PROPERTY(double pictureAspect READ pictureAspect NOTIFY changed)
    // Caption
    Q_PROPERTY(QString captionName READ captionName WRITE setCaptionName NOTIFY changed)
    Q_PROPERTY(QString captionSecond READ captionSecond WRITE setCaptionSecond NOTIFY changed)
    Q_PROPERTY(QString captionStyle READ captionStyle WRITE setCaptionStyle NOTIFY changed)
    // Save picture
    Q_PROPERTY(QString frameThumb READ frameThumb NOTIFY frameThumbChanged)
    // The dialog showing over Simple mode: "close", "about" or empty for none
    Q_PROPERTY(QString dialog READ dialog NOTIFY dialogChanged)

public:
    explicit SimpleController(QObject *parent = nullptr);

    bool dark() const { return m_dark; }
    void setDark(bool dark);
    bool welcome() const;
    QString tab() const { return m_tab; }
    void setTab(const QString &tab);
    QString filter() const { return m_filter; }
    void setFilter(const QString &filter);
    QVariantList items() const;
    int originalsCount() const { return m_originals.size(); }
    int finishedCount() const { return m_finished.size(); }
    QString toast() const { return m_toast; }
    bool toastIsError() const { return m_toastIsError; }
    bool saving() const { return m_saving; }
    int saveProgress() const { return m_saveProgress; }

    bool hasFile() const { return !m_edit.sourcePath.isEmpty(); }
    bool loading() const { return m_loading; }
    bool isPhoto() const { return m_edit.isPhoto; }
    QString fileName() const;
    QString currentPath() const { return m_edit.sourcePath; }
    QString tool() const { return m_tool; }
    void setTool(const QString &tool);
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }

    int frames() const { return m_edit.frames; }
    int trimStart() const { return m_edit.trim.start; }
    int trimEnd() const { return m_edit.trim.end; }
    QVariantList cuts() const;
    bool hasSelection() const { return !m_selection.isEmpty(); }
    int selectionStart() const { return m_selection.start; }
    int selectionEnd() const { return m_selection.end; }
    bool canRemoveSelection() const;
    QString selectionText() const;
    QString selectionInfo() const;
    QString trimStartText() const;
    QString trimEndText() const;
    QString lengthText() const;
    QStringList filmstrip() const { return m_filmstrip; }
    int playhead() const { return m_playhead; }
    QString positionText() const;
    bool playing() const { return m_playing; }

    QRectF cropRect() const { return m_edit.crop; }
    QString cropShape() const { return m_edit.cropShape; }
    double cropAspect() const;
    double pictureAspect() const;

    QString captionName() const { return m_edit.captionName; }
    void setCaptionName(const QString &text);
    QString captionSecond() const { return m_edit.captionSecond; }
    void setCaptionSecond(const QString &text);
    QString captionStyle() const;
    void setCaptionStyle(const QString &style);

    QString frameThumb() const { return m_frameThumb; }
    QString dialog() const { return m_dialog; }

    // Called by the Simple mode widget when the screen takes over or gives way.
    void activate();
    void deactivate();
    // The file and edits to carry into Advanced: an MLT file when there are edits.
    QString advancedResource();
    bool hasUnfinishedEdits() const;

public slots:
    void addFiles();
    void addPaths(const QStringList &paths, bool openFirst = true);
    void addUrls(const QList<QUrl> &urls);
    void openFile(const QString &path);
    void removeFromLibrary(const QString &path);
    void showFinishedFolder();

    void togglePlay();
    void pausePreview();
    void seekSource(int sourceFrame);
    void stepFrames(int count);

    void beginTrimDrag();
    void dragTrim(int start, int end);
    void endTrimDrag();
    bool setTrimStartText(const QString &text);
    bool setTrimEndText(const QString &text);
    void resetTrim();

    void setSelection(int start, int end);
    void endSelectionDrag();
    void selectAnotherPart();
    void removeSelection();
    void restoreCut(int index);

    void pickCropShape(const QString &shape);
    void setCropFromDrag(double x, double y, double width, double height);
    void endCropDrag();
    void rotate();
    void flip();

    void undo();
    void redo();

    void savePicture();
    void finish();
    void cancelFinish();
    void requestAdvanced();
    void showAbout();
    // Shows the "close without finishing?" question, then answers it.
    void askToClose();
    void closeAnyway();
    void dismissDialog();

signals:
    void changed();
    void libraryChanged();
    void filmstripChanged();
    void positionChanged();
    void playingChanged();
    void toastChanged();
    void savingChanged();
    void frameThumbChanged();
    void advancedRequested();
    void dialogChanged();
    void closeConfirmed();

private:
    struct Session
    {
        Simple::Edit edit;
        QVector<Simple::Edit> undo;
        QVector<Simple::Edit> redo;
    };

    void loadSettings();
    void saveLibrary();
    void refreshFinished();
    void ensureThumbnail(const QString &path);
    void showToast(const QString &text, bool isError = false);
    void pushUndo(const Simple::Edit &before, const QString &coalesceKey = QString());
    void afterEdit(bool rebuildPreview);
    void rebuildPreview(int sourceFrame);
    void updateFrameThumb();
    void onFrameDisplayed(const SharedFrame &frame);
    void loadFilmstrip();
    void storeSession();
    void startVideoExport(bool allowHardware);
    void onExportFinished(AbstractJob *job, bool success);
    void finishExportFailed();
    void afterSaved(const QString &path, const QString &message);
    void detectHardwareEncoder();

    QSettings m_settings;
    bool m_dark{true};
    bool m_active{false};
    QString m_tab{QStringLiteral("originals")};
    QString m_filter{QStringLiteral("all")};
    QStringList m_originals;
    QStringList m_finished;
    QHash<QString, QString> m_thumbs;
    QHash<QString, QString> m_badges;
    QStringList m_thumbsPending;
    QFileSystemWatcher m_finishedWatcher;

    Simple::Edit m_edit;
    Simple::Edit m_dragStart;
    QVector<Simple::Edit> m_undo;
    QVector<Simple::Edit> m_redo;
    QString m_lastCoalesceKey;
    QHash<QString, Session> m_sessions;
    QHash<QString, Simple::Edit> m_savedEdits; // what each file looked like when last finished
    QString m_tool{QStringLiteral("trim")};
    Simple::Range m_selection;
    QStringList m_filmstrip;
    int m_filmstripGeneration{0};
    int m_openGeneration{0};
    bool m_loading{false};
    bool m_timeDragging{false};
    bool m_previewFullTime{false};
    int m_playhead{0};
    bool m_playing{false};
    QTimer m_captionTimer;
    QTimer m_frameThumbTimer;
    QString m_frameThumb;
    QString m_dialog;
    int m_frameThumbGeneration{0};

    QString m_toast;
    bool m_toastIsError{false};
    QTimer m_toastTimer;

    bool m_saving{false};
    int m_saveProgress{0};
    QPointer<AbstractJob> m_job;
    Simple::Edit m_exportEdit;
    QString m_exportPartial;
    QString m_exportTarget;
    bool m_exportUsedHardware{false};
    bool m_exportCancelled{false};
    int m_hardwareEncoder{-1}; // -1 not checked yet, 0 no, 1 yes
};

#endif // SIMPLECONTROLLER_H
