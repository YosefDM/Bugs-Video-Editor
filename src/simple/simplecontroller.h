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

#include "collage.h"
#include "simpleedit.h"
#include "simplemedia.h"

#include <QElapsedTimer>
#include <QFileSystemWatcher>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QPointer>
#include <QRectF>
#include <QSet>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

#include <deque>
#include <memory>

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
    // Collage: picking photos, then the collage screen
    Q_PROPERTY(bool collagePicking READ collagePicking NOTIFY changed)
    Q_PROPERTY(int collagePickCount READ collagePickCount NOTIFY changed)
    // True while the picked photos are being read for a new collage.
    Q_PROPERTY(bool collageMaking READ collageMaking NOTIFY changed)
    Q_PROPERTY(bool collageActive READ collageActive NOTIFY changed)
    Q_PROPERTY(QString collageShape READ collageShape NOTIFY changed)
    // columns, mosaic or grid, and each one's picture for the picker: [{id, title, cells}]
    Q_PROPERTY(QString collageLayout READ collageLayout NOTIFY changed)
    Q_PROPERTY(QVariantList collageLayouts READ collageLayouts NOTIFY changed)
    // The selected photo's zoom, and whether it is the big one
    Q_PROPERTY(double collagePhotoZoom READ collagePhotoZoom NOTIFY changed)
    Q_PROPERTY(bool collagePhotoIsBig READ collagePhotoIsBig NOTIFY changed)
    // Space between photos and round the edge, and photo corner rounding: fractions of the
    // shorter side. The background shows in the space.
    Q_PROPERTY(double collageSpacing READ collageSpacing NOTIFY changed)
    Q_PROPERTY(double collageCorners READ collageCorners NOTIFY changed)
    Q_PROPERTY(QString collageBackground READ collageBackground NOTIFY changed)
    Q_PROPERTY(QStringList collageBackgrounds READ collageBackgrounds CONSTANT)
    // Text and stickers on the collage, and the one whose options show
    Q_PROPERTY(QVariantList collageItems READ collageItems NOTIFY changed)
    Q_PROPERTY(int collageSelectedItem READ collageSelectedItem NOTIFY changed)
    // The approved stickers by category: [{id, title, stickers: [{id, name, url}]}]
    Q_PROPERTY(QVariantList stickerCategories READ stickerCategories CONSTANT)
    Q_PROPERTY(QStringList collagePhrases READ collagePhrases CONSTANT)
    Q_PROPERTY(QString collagePreview READ collagePreview NOTIFY changed)
    Q_PROPERTY(double collageAspect READ collageAspect NOTIFY changed)
    Q_PROPERTY(QVariantList collageCells READ collageCells NOTIFY changed)
    Q_PROPERTY(int collageSelected READ collageSelected NOTIFY changed)
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
    Q_PROPERTY(QString selectionStartText READ selectionStartText NOTIFY changed)
    Q_PROPERTY(QString selectionEndText READ selectionEndText NOTIFY changed)
    Q_PROPERTY(QString trimStartText READ trimStartText NOTIFY changed)
    Q_PROPERTY(QString trimEndText READ trimEndText NOTIFY changed)
    Q_PROPERTY(QString lengthText READ lengthText NOTIFY changed)
    Q_PROPERTY(QStringList filmstrip READ filmstrip NOTIFY filmstripChanged)
    Q_PROPERTY(int playhead READ playhead NOTIFY positionChanged)
    Q_PROPERTY(QString positionText READ positionText NOTIFY positionChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    // How fast the preview plays: 1, 1.5 or 2. The saved video is always normal speed.
    Q_PROPERTY(double speed READ speed NOTIFY playingChanged)
    // Crop, in fractions of the rotated picture
    Q_PROPERTY(QRectF cropRect READ cropRect NOTIFY changed)
    Q_PROPERTY(QString cropShape READ cropShape NOTIFY changed)
    Q_PROPERTY(double cropAspect READ cropAspect NOTIFY changed)
    Q_PROPERTY(double pictureAspect READ pictureAspect NOTIFY changed)
    // Caption
    Q_PROPERTY(QString captionName READ captionName WRITE setCaptionName NOTIFY changed)
    Q_PROPERTY(QString captionSecond READ captionSecond WRITE setCaptionSecond NOTIFY changed)
    Q_PROPERTY(QString captionStyle READ captionStyle WRITE setCaptionStyle NOTIFY changed)
    // Text labels placed anywhere on the picture
    Q_PROPERTY(QVariantList labels READ labels NOTIFY changed)
    Q_PROPERTY(int selectedLabel READ selectedLabel NOTIFY changed)
    Q_PROPERTY(QStringList labelColors READ labelColors CONSTANT)
    Q_PROPERTY(QVariantList labelFonts READ labelFonts CONSTANT)
    // Save picture
    Q_PROPERTY(QString frameThumb READ frameThumb NOTIFY frameThumbChanged)
    // Pictures saved from the open video, newest first, so one can be taken back.
    Q_PROPERTY(QVariantList savedPictures READ savedPictures NOTIFY changed)
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
    // On the collage screen, undo covers everything on it: layout, swaps, zoom, text, stickers
    // and look.
    bool canUndo() const { return m_collageActive ? !m_collageUndo.isEmpty() : !m_undo.isEmpty(); }
    bool canRedo() const { return m_collageActive ? !m_collageRedo.isEmpty() : !m_redo.isEmpty(); }

    bool collagePicking() const { return m_collagePicking; }
    int collagePickCount() const { return m_collagePicks.size(); }
    bool collageMaking() const { return m_collageMaking; }
    bool collageActive() const { return m_collageActive; }
    QString collageShape() const { return m_collageDesign.shape; }
    QString collageLayout() const { return m_collageDesign.layoutStyle; }
    QVariantList collageLayouts() const { return m_collageLayouts; }
    double collagePhotoZoom() const;
    bool collagePhotoIsBig() const;
    double collageSpacing() const { return m_collageDesign.spacing; }
    double collageCorners() const { return m_collageDesign.corners; }
    QString collageBackground() const { return m_collageDesign.background; }
    QStringList collageBackgrounds() const;
    QVariantList collageItems() const;
    int collageSelectedItem() const { return m_collageSelectedItem; }
    QVariantList stickerCategories() const;
    // Draws text or a sticker for the preview, with the same code that draws it in the saved
    // picture. `page` is the size the collage is shown at.
    void paintCollageItem(QPainter *painter, int index, const QRectF &box, const QSizeF &page) const;
    QStringList collagePhrases() const;
    QString collagePreview() const { return m_collagePreview; }
    double collageAspect() const;
    QVariantList collageCells() const;
    int collageSelected() const { return m_collageSelected; }

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
    QString selectionStartText() const;
    QString selectionEndText() const;
    QString trimStartText() const;
    QString trimEndText() const;
    QString lengthText() const;
    QStringList filmstrip() const { return m_filmstrip; }
    int playhead() const { return m_playhead; }
    QString positionText() const;
    bool playing() const { return m_playing; }
    double speed() const { return m_speed; }

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

    QVariantList labels() const;
    int selectedLabel() const { return m_selectedLabel; }
    QStringList labelColors() const { return Simple::labelColors(); }
    QVariantList labelFonts() const;
    Q_INVOKABLE double labelFontFraction(int size) const { return Simple::labelFontFraction(size); }

    QString frameThumb() const { return m_frameThumb; }
    QVariantList savedPictures() const;
    QString dialog() const { return m_dialog; }

    // Called by the Simple mode widget when the screen takes over or gives way.
    void activate();
    void deactivate();
    // The edits to carry into Advanced, as an MLT file Advanced puts on its timeline.
    QString advancedResource();
    // Where Advanced keeps the Combine clips project for the open file, so work kept there
    // reopens next time. Empty when no file is open.
    QString combineProject() const;
    bool hasUnfinishedEdits() const;
    // Names of the files with unfinished edits, for the close dialog.
    Q_INVOKABLE QStringList unfinishedFileNames() const;

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
    // Jumps back or forward, such as -10 or 30 seconds.
    void skipSeconds(double seconds);
    // 1x, then 1.5x, then 2x, then back to 1x.
    void cycleSpeed();

    void beginTrimDrag();
    void dragTrim(int start, int end);
    void endTrimDrag();
    bool setTrimStartText(const QString &text);
    bool setTrimEndText(const QString &text);
    void resetTrim();

    void setSelection(int start, int end);
    void endSelectionDrag();
    bool setSelectionStartText(const QString &text);
    bool setSelectionEndText(const QString &text);
    void selectAnotherPart();
    void removeSelection();
    void restoreCut(int index);

    void pickCropShape(const QString &shape);
    void setCropFromDrag(double x, double y, double width, double height);
    void endCropDrag();
    void rotate();
    void flip();

    void addLabel();
    void selectLabel(int index);
    void setLabelText(int index, const QString &text);
    void setLabelColor(int index, const QString &color);
    void setLabelSize(int index, int size);
    void setLabelFont(int index, const QString &font);
    void moveLabel(int index, double x, double y);
    void endLabelDrag();
    void removeLabel(int index);

    void undo();
    void redo();

    void savePicture();
    // Takes back a picture saved from this video. It goes to the Recycle Bin.
    void removeSavedPicture(const QString &path);
    void finish();

    void startCollage();
    void toggleCollagePick(const QString &path);
    void makeCollage();
    void leaveCollage();
    void setCollageShape(const QString &shape);
    void setCollageSpacing(double spacing);
    void setCollageCorners(double corners);
    void setCollageBackground(const QString &color);
    // Text and stickers. Gestures (move, resize, rotate) make one undo step, ended by
    // endCollageItemGesture().
    void addCollageText(const QString &text);
    void addSticker(const QString &id);
    void selectCollageItem(int index);
    void moveCollageItem(int index, double x, double y);
    void resizeCollageItem(int index, double size);
    void rotateCollageItem(int index, double degrees);
    void endCollageItemGesture();
    void setCollageItemText(int index, const QString &text);
    void setCollageItemColor(int index, const QString &color);
    void setCollageItemFont(int index, const QString &font);
    void removeCollageItem(int index);
    void bringCollageItemToFront(int index);
    void sendCollageItemToBack(int index);
    void shuffleCollage();
    void tapCollage(double x, double y);
    void setCollageLayout(const QString &style);
    // For the selected photo: make it the big one (or not), zoom it, and move it in its frame.
    // `dx`, `dy` are how far the pointer moved, as fractions of the frame.
    void setCollagePhotoBig(bool big);
    void setCollagePhotoZoom(double zoom);
    void zoomCollagePhoto(double factor);
    void panCollagePhoto(double dx, double dy);
    void endCollagePhotoGesture();
    void deselectCollagePhoto();
    void cancelFinish();
    void requestAdvanced();
    void showAbout();
    // Shows the "close without finishing?" question, then answers it.
    void askToClose();
    void closeAnyway();
    // Leaves a collage that was never saved, after Levi agreed, and opens the file he picked.
    void leaveCollageAnyway();
    // Leaves the collage, asking first when it was never saved.
    void askToLeaveCollage();
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
    // A file just added that can't be opened leaves the library again, with one message for all
    // of them.
    void rejectAddedFile(const QString &path);
    void showRejectedFiles();
    QSet<QString> m_justAdded;
    QStringList m_rejectedNames;
    QTimer m_rejectTimer;
    void showToast(const QString &text, bool isError = false);
    // False, with a message, when a file the save needs has been moved or deleted since it was opened.
    bool filesStillThere(const QStringList &paths);
    void pushUndo(const Simple::Edit &before, const QString &coalesceKey = QString());
    void afterEdit(bool rebuildPreview);
    void rebuildPreview(int sourceFrame);
    void updateFrameThumb();
    void onFrameDisplayed(const SharedFrame &frame);
    void loadFilmstrip();
    void storeSession();
    void startVideoExport(bool allowHardware);
    void onExportFinished(AbstractJob *job, bool success);
    void onExportChecked(bool valid);
    // Another program, such as OneDrive or a virus scanner, can hold the new file for a moment,
    // so the rename is tried again for a few seconds before giving up.
    void moveExportIntoPlace(const QString &finalPath, int attempt);
    void exportSaved(const QString &finalPath);
    // Whether the library should jump to Finished: only when Levi is still on the saved file.
    void showFinishedFor(const QString &sourcePath);
    void finishExportFailed();
    void afterSaved(const QString &path, const QString &message);
    void detectHardwareEncoder();

    QSettings m_settings;
    QString m_lastPicture; // file and frame, so a double-click saves one picture, not two
    QElapsedTimer m_lastPictureTime;
    bool m_dark{false};
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
    QString m_previewXml; // what the preview shows now, so an unchanged one is not reloaded
    int m_playhead{0};
    bool m_playing{false};
    double m_speed{1.0};
    QTimer m_captionTimer;
    int m_captionGeneration{0};
    int m_selectedLabel{-1};
    // Picks the colour that stands out for each label Levi has not coloured himself.
    void refreshLabelColors();
    void applyAutoLabelColors();
    QImage m_labelBackdrop;
    QString m_labelBackdropKey;
    int m_labelBackdropGeneration{0};
    bool m_labelDragging{false};
    QTimer m_frameThumbTimer;
    QString m_frameThumb;
    QHash<QString, QStringList> m_savedPictures; // by source video, newest first
    QStringList m_picturesBeingSaved;            // paths taken, so two saves never share one
    QString m_dialog;
    QString m_openAfterCollage;

    bool m_collagePicking{false};
    QStringList m_collagePicks;
    bool m_collageMaking{false};
    int m_collageMakeGeneration{0};
    bool m_collageActive{false};
    bool m_collageUnsaved{false};               // changed since it was last saved
    std::deque<Collage::Photo> m_collagePhotos; // .path is the readable file, upright
    // Everything on the collage that undo covers.
    struct CollageDesign
    {
        QVector<Simple::CollageItem> items;
        double spacing{0.012};
        double corners{0};
        QString background{QStringLiteral("#FFFFFF")};
        QString shape{QStringLiteral("square")};
        QString layoutStyle{QStringLiteral("columns")}; // columns, mosaic or grid
        unsigned seed{1};
        int hero{-1}; // the photo Mosaic makes biggest, or -1
        Collage::Layout layout;
        QVector<Collage::PhotoCrop> crops; // by photo index
        // Everything but the text and stickers, which QML draws over the picture.
        bool sameLook(const CollageDesign &o) const
        {
            return spacing == o.spacing && corners == o.corners && background == o.background
                   && shape == o.shape && layoutStyle == o.layoutStyle && seed == o.seed
                   && hero == o.hero && layout == o.layout && crops == o.crops;
        }
        bool operator==(const CollageDesign &o) const { return items == o.items && sameLook(o); }
        bool operator!=(const CollageDesign &o) const { return !(*this == o); }
    };
    CollageDesign m_collageDesign;
    CollageDesign m_collageGestureStart;
    bool m_collageGesture{false};
    QVector<CollageDesign> m_collageUndo;
    QVector<CollageDesign> m_collageRedo;
    QString m_collageCoalesceKey;
    int m_collageSelectedItem{-1};
    void pushCollageUndo(const CollageDesign &before, const QString &coalesceKey = QString());
    // After a change to the text or stickers, which QML draws over the preview picture.
    void collageItemsChanged();
    void applyCollageDesign(const CollageDesign &design);
    Simple::CollageStyle collageStyle() const;
    // Sticker pictures, read once from the bundled folder.
    QImage stickerImage(const QString &id) const;
    mutable QHash<QString, QImage> m_stickerImages;
    mutable QVariantList m_stickerCategories;
    int m_collageSelected{-1};
    QString m_collagePreview;
    int m_collageGeneration{0};
    QHash<QString, QImage> m_collageThumbs; // preview-size pictures by readable path
    // Lays the photos out again for the design's shape, style, seed and big photo.
    Collage::Layout buildCollageLayout(const CollageDesign &design) const;
    // Changes the design with one undo step and redraws.
    void changeCollageDesign(const CollageDesign &design, const QString &coalesceKey = QString());
    QVariantList m_collageLayouts; // the three layout pictures for the picker
    void updateCollageLayoutChoices();
    bool m_collagePhotoGesture{false};
    void collagePhotosRead(const std::vector<Collage::Photo> &photos,
                           const QHash<QString, QImage> &thumbs,
                           const QStringList &unreadable);
    void drawCollagePreview();
    QSize collageSize(int longSide) const;
    // Height / width of the chosen shape. Preview and save must use exactly the same value, or
    // they could get different layouts.
    double collageRatio() const;
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
