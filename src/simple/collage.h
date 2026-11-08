/*
 * Collage layout, ported to C++ for BUGS! Video Editor from PhotoCollage
 * (https://github.com/adrienverge/PhotoCollage, photocollage/collage.py and the
 * page setup in photocollage/gtkgui.py, version of 2025-07-09).
 *
 * Copyright (C) 2014 Adrien Vergé
 * Copyright (C) 2026 BUGS! Video Editor contributors (C++ port)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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

#ifndef COLLAGE_H
#define COLLAGE_H

#include <QRectF>
#include <QString>

#include <functional>
#include <memory>
#include <vector>

// The layout works like PhotoCollage's (see collage.py for the full description):
//
// A. Photos are added one by one to the shortest column. When two neighbouring columns are about
//    the same height, a wide photo may span both: its Cell sits in the left column and an extent
//    reserves the space in the right one.
// B. Every column is then stretched or squeezed to the same height.
//
// The structure keeps PhotoCollage's object model, so its tests port over directly.
namespace Collage {

struct Photo
{
    QString path;
    int w{1};
    int h{1};
    // Which part of the photo stays in view when its cell crops it, from 0 to 1.
    double offsetW{0.5};
    double offsetH{0.5};

    double ratio() const { return double(h) / double(w); }
    void move(double x, double y);
};

class Column;
class Page;

class Cell
{
public:
    // A cell showing a photo, in one column or across two.
    Cell(std::vector<Column *> parents, Photo *photo);
    // An extent: reserves the space of `origin` in its second column.
    explicit Cell(Cell *origin);

    std::vector<Column *> parents() const;
    void setParents(std::vector<Column *> parents) { m_parents = std::move(parents); }
    Photo *photo() const;
    void setPhoto(Photo *photo) { m_photo = photo; }
    double x() const;
    double y() const;
    double w() const;
    double h() const;
    void setH(double h) { m_h = h; }
    double ratio() const { return h() / w(); }
    double wantedRatio() const { return photo()->ratio(); }
    void scale(double alpha);

    bool isExtended() const { return !isExtension() && m_extent; }
    bool isExtension() const { return m_origin != nullptr; }
    Cell *extent() const { return m_extent; }
    Cell *origin() const { return m_origin; }

    // Where the whole photo would sit if it were not cropped: never smaller than the cell.
    QRectF contentCoords() const;
    Cell *topNeighbor() const;
    Cell *bottomNeighbor() const;
    QString repr() const;

private:
    std::vector<Column *> m_parents;
    Photo *m_photo{nullptr};
    double m_h{0};
    Cell *m_extent{nullptr};
    Cell *m_origin{nullptr};
};

class Column
{
public:
    Column(Page *parent, double w);

    std::vector<Cell *> cells;
    double w;

    double h() const;
    double x() const;
    void scale(double alpha);
    Column *leftNeighbor() const;
    Column *rightNeighbor() const;
    void adjustHeight(double targetH);
    QString repr() const;

private:
    Page *m_parent;
};

class Page
{
public:
    Page(double w, double targetRatio, int noCols);

    std::vector<std::unique_ptr<Column>> cols;
    double targetRatio;

    // PhotoCollage uses Python's random module. Tests replace these to force choices.
    std::function<double()> random;             // 0 <= value < 1
    std::function<size_t(size_t count)> choose; // 0 <= index < count

    int noCols() const { return int(cols.size()); }
    double w() const;
    double h() const;
    double ratio() const { return h() / w(); }
    void scale(double alpha);
    void scaleToFit(double maxW, double maxH = -1);
    Column *nextFreeCol() const;
    void addCellSingleCol(Column *col, Photo *photo);
    void addCellMultiCol(Column *col1, Column *col2, Photo *photo);
    void addCell(Photo *photo);
    void removeEmptyCols();
    void removeBottomHoles();
    void adjustColsHeights();
    void adjust();
    Cell *cellAt(double x, double y) const;
    static void swapPhotos(Cell *cell1, Cell *cell2);
    // Every cell that shows a photo (not the extents), for drawing.
    std::vector<Cell *> photoCells() const;
    QString repr() const;

private:
    Cell *own(std::unique_ptr<Cell> cell);
    std::vector<std::unique_ptr<Cell>> m_cells;
};

// PhotoCollage's page setup (gtkgui.py, UserCollage.make_page): picks a good number of columns
// for the photos and the page shape, shuffles the photos and lays them out on a page 1 wide.
// `ratio` is the page's height / width. `seed` makes the same layout every time.
std::unique_ptr<Page> makePage(std::vector<Photo *> photos, double ratio, unsigned seed);

// BUGS! addition. About 1 layout in 100 with many photos leaves a gap, in PhotoCollage too. This
// tries `seed`, then seed + 1 and so on until a layout covers the whole page, so the same seed
// always gives the same gap-free layout.
std::unique_ptr<Page> makeGaplessPage(const std::vector<Photo *> &photos,
                                      double ratio,
                                      unsigned seed);
// How much of the page the photos cover, from 0 to 1.
double coverage(const Page &page);

// --- BUGS! additions: layouts other than PhotoCollage's columns ---

// One photo's frame on the page. `rect` is a fraction of the page (0 to 1 both ways); `photo` is
// an index into the collage's list of photos.
struct Slot
{
    QRectF rect;
    int photo{0};
    bool operator==(const Slot &o) const { return rect == o.rect && photo == o.photo; }
};
// A finished layout, whichever way it was made. Every layout covers the whole page, with no gaps
// and no overlaps, and shows every photo once.
using Layout = std::vector<Slot>;

// Which part of a photo shows in its frame. With zoom 1 the photo just covers the frame; higher
// zooms show less of it. (cx, cy) is the middle of what shows, as a fraction of the photo.
struct PhotoCrop
{
    double zoom{1.0};
    double cx{0.5};
    double cy{0.5};
    bool operator==(const PhotoCrop &o) const { return zoom == o.zoom && cx == o.cx && cy == o.cy; }
};

// The column layout as slots. `photos` is the list the page was made from, in collage order.
Layout layoutFromPage(const Page &page, const std::vector<Photo *> &photos);

// A mosaic: the page is cut in two again and again, like G'MIC's montage and the BRIC layouts in
// HP's papers, and each part gets room in proportion to the weights of the photos in it. Many
// arrangements are tried from `seed`, and the one that crops the photos least wins.
// `aspects` are each photo's height / width; `weights` how much room each gets (the big photo
// more); `ratio` the page's height / width.
Layout makeMosaicLayout(const std::vector<double> &aspects,
                        const std::vector<double> &weights,
                        double ratio,
                        unsigned seed);

// Even rows of frames, as square as the page allows. A short last row is stretched to the full
// width so there are no holes. Seed 1 keeps the photos in order; others shuffle them.
Layout makeGridLayout(int count, double ratio, unsigned seed);

// How much of each photo would be cut off to fit its frame, averaged over the page area: 0 is
// none. `ratio` is the page's height / width.
double cropLoss(const Layout &layout, const std::vector<double> &aspects, double ratio);

} // namespace Collage

#endif // COLLAGE_H
