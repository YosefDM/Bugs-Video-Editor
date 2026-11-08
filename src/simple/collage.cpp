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

#include "collage.h"

#include <QStringList>

#include <algorithm>
#include <cmath>
#include <random>

namespace Collage {

namespace {

double clampOffset(double offset)
{
    return std::clamp(offset, 0.0, 1.0);
}

// Python's "%d" % value for a float: truncates toward zero.
QString pyInt(double value)
{
    return QString::number(qint64(value));
}

template<typename T>
int indexOf(const std::vector<T *> &list, const T *item)
{
    for (size_t i = 0; i < list.size(); ++i) {
        if (list[i] == item)
            return int(i);
    }
    return -1;
}

template<typename T>
void removeItem(std::vector<T *> &list, const T *item)
{
    const int i = indexOf(list, item);
    if (i >= 0)
        list.erase(list.begin() + i);
}

} // namespace

void Photo::move(double x, double y)
{
    offsetW = clampOffset(offsetW + x);
    offsetH = clampOffset(offsetH + y);
}

// Cell

Cell::Cell(std::vector<Column *> parents, Photo *photo)
    : m_parents(std::move(parents))
    , m_photo(photo)
{
    m_h = w() * wantedRatio();
}

Cell::Cell(Cell *origin)
    : m_origin(origin)
{
    origin->m_extent = this;
}

std::vector<Column *> Cell::parents() const
{
    if (isExtension())
        return {m_origin->parents()[1]};
    return m_parents;
}

Photo *Cell::photo() const
{
    return isExtension() ? m_origin->photo() : m_photo;
}

double Cell::x() const
{
    return parents()[0]->x();
}

double Cell::y() const
{
    if (isExtension())
        return m_origin->y();
    // Assumes the cell is in a single column: the previous cell's y + h.
    const Cell *prev = nullptr;
    for (const Cell *c : parents()[0]->cells) {
        if (c == this)
            return prev ? prev->y() + prev->h() : 0.0;
        prev = c;
    }
    return 0.0;
}

double Cell::w() const
{
    double sum = 0;
    for (const Column *c : parents())
        sum += c->w;
    return sum;
}

double Cell::h() const
{
    return isExtension() ? m_origin->h() : m_h;
}

void Cell::scale(double alpha)
{
    if (!isExtension())
        m_h *= alpha;
}

QRectF Cell::contentCoords() const
{
    double x = this->x(), y = this->y(), w = this->w(), h = this->h();
    if (wantedRatio() < ratio()) {
        // The photo is too wide to fit
        const double contentW = h / wantedRatio();
        x -= (contentW - w) / 2.0;
        w = contentW;
    } else if (wantedRatio() > ratio()) {
        // The photo is too tall to fit
        const double contentH = w * wantedRatio();
        y -= (contentH - h) / 2.0;
        h = contentH;
    }
    return QRectF(x, y, w, h);
}

Cell *Cell::topNeighbor() const
{
    Cell *prev = nullptr;
    for (Cell *c : parents()[0]->cells) {
        if (c == this)
            return prev;
        prev = c;
    }
    return nullptr;
}

Cell *Cell::bottomNeighbor() const
{
    const auto &cells = parents()[0]->cells;
    Cell *prev = nullptr;
    for (auto it = cells.rbegin(); it != cells.rend(); ++it) {
        if (*it == this)
            return prev;
        prev = *it;
    }
    return nullptr;
}

QString Cell::repr() const
{
    if (isExtension())
        return QStringLiteral("------]");
    return QStringLiteral("[%1 %2%3")
        .arg(pyInt(w()), pyInt(h()), m_extent ? QStringLiteral("--") : QStringLiteral("]"));
}

// Column

Column::Column(Page *parent, double w)
    : w(w)
    , m_parent(parent)
{}

double Column::h() const
{
    // Not the sum of the cells' heights: there can be empty space between cells.
    if (cells.empty())
        return 0;
    return cells.back()->y() + cells.back()->h();
}

double Column::x() const
{
    double x = 0;
    for (const auto &c : m_parent->cols) {
        if (c.get() == this)
            break;
        x += c->w;
    }
    return x;
}

void Column::scale(double alpha)
{
    w *= alpha;
    for (Cell *c : cells)
        c->scale(alpha);
}

Column *Column::leftNeighbor() const
{
    Column *prev = nullptr;
    for (const auto &c : m_parent->cols) {
        if (c.get() == this)
            return prev;
        prev = c.get();
    }
    return nullptr;
}

Column *Column::rightNeighbor() const
{
    Column *prev = nullptr;
    for (auto it = m_parent->cols.rbegin(); it != m_parent->cols.rend(); ++it) {
        if (it->get() == this)
            return prev;
        prev = it->get();
    }
    return nullptr;
}

void Column::adjustHeight(double targetH)
{
    // Make groups of movable cells. Extents cannot move, so only the cells between them are
    // resized, each group on its own.
    struct Group
    {
        double y{0};
        double h{0};
        std::vector<Cell *> cells;
    };
    std::vector<Group> groups(1);
    for (Cell *c : cells) {
        if (!c->isExtension()) {
            groups.back().cells.push_back(c);
        } else {
            groups.back().h = c->y() - groups.back().y;
            Group next;
            next.y = c->y() + c->h();
            groups.push_back(next);
        }
    }
    groups.back().h = targetH - groups.back().y;

    for (const Group &group : groups) {
        if (group.cells.empty())
            continue;
        double sum = 0;
        for (const Cell *c : group.cells)
            sum += c->h();
        const double alpha = group.h / sum;
        for (Cell *c : group.cells)
            c->setH(c->h() * alpha);
    }
}

QString Column::repr() const
{
    QStringList lines;
    for (const Cell *c : cells)
        lines << c->repr();
    return lines.join(QLatin1Char('\n'));
}

// Page

Page::Page(double w, double targetRatio, int noCols)
    : targetRatio(targetRatio)
{
    const double colW = w / noCols;
    for (int i = 0; i < noCols; ++i)
        cols.push_back(std::make_unique<Column>(this, colW));
    auto engine = std::make_shared<std::mt19937>(std::random_device{}());
    random = [engine]() { return std::uniform_real_distribution<double>(0.0, 1.0)(*engine); };
    choose = [engine](size_t count) {
        return std::uniform_int_distribution<size_t>(0, count - 1)(*engine);
    };
}

Cell *Page::own(std::unique_ptr<Cell> cell)
{
    m_cells.push_back(std::move(cell));
    return m_cells.back().get();
}

double Page::w() const
{
    double sum = 0;
    for (const auto &c : cols)
        sum += c->w;
    return sum;
}

double Page::h() const
{
    double max = 0;
    for (const auto &c : cols)
        max = std::max(max, c->h());
    return max;
}

void Page::scale(double alpha)
{
    for (const auto &c : cols)
        c->scale(alpha);
}

void Page::scaleToFit(double maxW, double maxH)
{
    if (maxH < 0 || w() * maxH > h() * maxW)
        scale(maxW / w());
    else
        scale(maxH / h());
}

Column *Page::nextFreeCol() const
{
    double minimum = cols.front()->h();
    for (const auto &c : cols)
        minimum = std::min(minimum, c->h());
    std::vector<Column *> candidates;
    for (const auto &c : cols) {
        if (c->h() == minimum)
            candidates.push_back(c.get());
    }
    return candidates[choose(candidates.size())];
}

void Page::addCellSingleCol(Column *col, Photo *photo)
{
    col->cells.push_back(own(std::make_unique<Cell>(std::vector<Column *>{col}, photo)));
}

void Page::addCellMultiCol(Column *col1, Column *col2, Photo *photo)
{
    Cell *cell = own(std::make_unique<Cell>(std::vector<Column *>{col1, col2}, photo));
    Cell *extent = own(std::make_unique<Cell>(cell));
    col1->cells.push_back(cell);
    col2->cells.push_back(extent);
}

void Page::addCell(Photo *photo)
{
    // If possible, and if it is worth it, make a cell spanning two columns.
    Column *col = nextFreeCol();
    Column *left = col->leftNeighbor();
    Column *right = col->rightNeighbor();
    if (2 * random() > photo->ratio()) {
        if (left && std::abs(col->h() - left->h()) < 0.5 * col->w)
            return addCellMultiCol(left, col, photo);
        if (right && std::abs(col->h() - right->h()) < 0.5 * col->w)
            return addCellMultiCol(col, right, photo);
    }
    addCellSingleCol(col, photo);
}

void Page::removeEmptyCols()
{
    cols.erase(std::remove_if(cols.begin(),
                              cols.end(),
                              [](const std::unique_ptr<Column> &c) { return c->cells.empty(); }),
               cols.end());
}

void Page::removeBottomHoles()
{
    // Fill the holes extended cells leave at the bottom of the page. See collage.py for
    // pictures of each case.
    for (const auto &colPtr : cols) {
        Column *col = colPtr.get();
        Cell *cell = col->cells.back();
        if (cell == col->cells.front())
            continue;

        // Case A: a plain cell below an extended cell, with nothing under the latter, is extended.
        if (!cell->isExtended() && !cell->isExtension()) {
            Cell *top = cell->topNeighbor();
            if (top->isExtended() && top->extent()->bottomNeighbor() == nullptr) {
                // A1: extend the cell to the right
                Column *right = col->rightNeighbor();
                Cell *extent = own(std::make_unique<Cell>(cell));
                right->cells.push_back(extent);
                cell->setParents({col, right});
            } else if (top->isExtension() && top->origin()->bottomNeighbor() == nullptr) {
                // A2: extend the cell to the left
                Column *left = col->leftNeighbor();
                removeItem(col->cells, cell);
                left->cells.push_back(cell);
                Cell *extent = own(std::make_unique<Cell>(cell));
                col->cells.push_back(extent);
                cell->setParents({left, col});
            }
        }
        // Case B: an extended cell below another extended cell moves right under it.
        else if (cell->isExtended() && cell->extent()->bottomNeighbor() == nullptr) {
            Cell *extentTop = cell->extent()->topNeighbor();
            Cell *top = cell->topNeighbor();
            if (extentTop->isExtended() && extentTop->extent()->bottomNeighbor() == nullptr) {
                // B1: move the cell to the right
                Column *right = col->rightNeighbor();
                Column *rightRight = right->rightNeighbor();
                removeItem(col->cells, cell);
                removeItem(right->cells, cell->extent());
                right->cells.push_back(cell);
                rightRight->cells.push_back(cell->extent());
                cell->setParents({right, rightRight});
            } else if (top->isExtension() && top->origin()->bottomNeighbor() == nullptr) {
                // B2: move the cell to the left
                Column *right = col->rightNeighbor();
                Column *left = col->leftNeighbor();
                removeItem(col->cells, cell);
                removeItem(right->cells, cell->extent());
                left->cells.push_back(cell);
                col->cells.push_back(cell->extent());
                cell->setParents({left, col});
            }
        }
    }
}

void Page::adjustColsHeights()
{
    // Set all columns to the same height by stretching or squeezing their cells.
    const double targetH = w() * targetRatio;
    for (const auto &c : cols)
        c->adjustHeight(targetH);
}

void Page::adjust()
{
    removeEmptyCols();
    removeBottomHoles();
    adjustColsHeights();
}

Cell *Page::cellAt(double x, double y) const
{
    for (const auto &col : cols) {
        if (x >= col->x() && x < col->x() + col->w) {
            for (Cell *cell : col->cells) {
                if (y >= cell->y() && y < cell->y() + cell->h())
                    return cell->isExtension() ? cell->origin() : cell;
            }
        }
    }
    return nullptr;
}

void Page::swapPhotos(Cell *cell1, Cell *cell2)
{
    Photo *photo = cell1->photo();
    cell1->setPhoto(cell2->photo());
    cell2->setPhoto(photo);
}

std::vector<Cell *> Page::photoCells() const
{
    std::vector<Cell *> result;
    for (const auto &col : cols) {
        for (Cell *cell : col->cells) {
            if (!cell->isExtension())
                result.push_back(cell);
        }
    }
    return result;
}

QString Page::repr() const
{
    QStringList lines;
    int n = 0;
    bool end = false;
    while (!end) {
        lines << QString();
        end = true;
        for (const auto &col : cols) {
            const QStringList cells = col->repr().split(QLatin1Char('\n'));
            qsizetype width = 0;
            for (const auto &c : cells)
                width = std::max(width, c.size());
            if (col != cols.back())
                width += 1;
            QString cell(width, QLatin1Char(' '));
            if (n < cells.size()) {
                cell = cells[n] + QString(width - cells[n].size(), QLatin1Char(' '));
                if (n < cells.size() - 1)
                    end = false;
            }
            lines.back() += cell;
        }
        ++n;
    }
    return lines.join(QLatin1Char('\n'));
}

std::unique_ptr<Page> makePage(std::vector<Photo *> photos, double ratio, unsigned seed)
{
    // The number of columns is inversely proportional to the square root of the page ratio
    // and proportional to the square root of the photos' average ratio. About one photo in
    // three spans two columns, taking the space of two, so the "virtual" count is doubled.
    double sum = 0;
    for (const Photo *photo : photos)
        sum += photo->ratio();
    const double avgRatio = photos.empty() ? 1.0 : sum / photos.size();
    const double virtualCount = 2.0 * photos.size();
    const int noCols = std::max(1, int(std::nearbyint(std::sqrt(avgRatio / ratio * virtualCount))));

    auto page = std::make_unique<Page>(1.0, ratio, noCols);
    auto engine = std::make_shared<std::mt19937>(seed);
    page->random = [engine]() { return std::uniform_real_distribution<double>(0.0, 1.0)(*engine); };
    page->choose = [engine](size_t count) {
        return std::uniform_int_distribution<size_t>(0, count - 1)(*engine);
    };
    std::shuffle(photos.begin(), photos.end(), *engine);
    for (Photo *photo : photos)
        page->addCell(photo);
    page->adjust();
    return page;
}

double coverage(const Page &page)
{
    double area = 0;
    for (const Cell *cell : page.photoCells())
        area += cell->w() * cell->h();
    const double pageArea = page.w() * page.h();
    return pageArea > 0 ? area / pageArea : 0.0;
}

std::unique_ptr<Page> makeGaplessPage(const std::vector<Photo *> &photos,
                                      double ratio,
                                      unsigned seed)
{
    std::unique_ptr<Page> page;
    for (unsigned attempt = 0; attempt < 100; ++attempt) {
        page = makePage(photos, ratio, seed + attempt);
        if (coverage(*page) > 0.9999)
            break;
    }
    return page;
}

Layout layoutFromPage(const Page &page, const std::vector<Photo *> &photos)
{
    Layout layout;
    const double w = page.w();
    const double h = page.h();
    if (w <= 0 || h <= 0)
        return layout;
    for (const Cell *cell : page.photoCells()) {
        const auto found = std::find(photos.begin(), photos.end(), cell->photo());
        if (found == photos.end())
            continue;
        layout.push_back(Slot{QRectF(cell->x() / w, cell->y() / h, cell->w() / w, cell->h() / h),
                              int(found - photos.begin())});
    }
    return layout;
}

double cropLoss(const Layout &layout, const std::vector<double> &aspects, double ratio)
{
    double loss = 0;
    double area = 0;
    for (const Slot &slot : layout) {
        if (slot.photo < 0 || slot.photo >= int(aspects.size()) || slot.rect.width() <= 0)
            continue;
        // In page units, where the page is 1 wide and `ratio` tall.
        const double cellAspect = slot.rect.height() * ratio / slot.rect.width();
        const double photoAspect = aspects[size_t(slot.photo)];
        const double kept = std::min(cellAspect / photoAspect, photoAspect / cellAspect);
        const double a = slot.rect.width() * slot.rect.height();
        loss += (1.0 - kept) * a;
        area += a;
    }
    return area > 0 ? loss / area : 0.0;
}

namespace {

// Cuts `rect` (page units) between the photos in `ids`, recursively.
void slice(const QRectF &rect,
           const std::vector<int> &ids,
           const std::vector<double> &weights,
           std::mt19937 &rng,
           Layout &out)
{
    if (ids.size() == 1) {
        out.push_back(Slot{rect, ids.front()});
        return;
    }
    // Split the photos into two groups somewhere near the middle, by weight.
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    double total = 0;
    for (int id : ids)
        total += weights[size_t(id)];
    const double target = total * (0.35 + 0.3 * unit(rng));
    size_t cut = 1;
    double first = weights[size_t(ids[0])];
    while (cut < ids.size() - 1 && first + weights[size_t(ids[cut])] / 2 < target) {
        first += weights[size_t(ids[cut])];
        ++cut;
    }
    const std::vector<int> a(ids.begin(), ids.begin() + long(cut));
    const std::vector<int> b(ids.begin() + long(cut), ids.end());
    const double share = first / total;
    // Usually across the longer side, so frames do not get thin; sometimes the other way.
    const bool longSideIsWidth = rect.width() >= rect.height();
    const bool sideBySide = unit(rng) < 0.8 ? longSideIsWidth : !longSideIsWidth;
    if (sideBySide) {
        const double w = rect.width() * share;
        slice(QRectF(rect.x(), rect.y(), w, rect.height()), a, weights, rng, out);
        slice(QRectF(rect.x() + w, rect.y(), rect.width() - w, rect.height()), b, weights, rng, out);
    } else {
        const double h = rect.height() * share;
        slice(QRectF(rect.x(), rect.y(), rect.width(), h), a, weights, rng, out);
        slice(QRectF(rect.x(), rect.y() + h, rect.width(), rect.height() - h), b, weights, rng, out);
    }
}

// From page units (1 wide, `ratio` tall) to fractions of the page.
Layout toFractions(Layout layout, double ratio)
{
    for (Slot &slot : layout)
        slot.rect = QRectF(slot.rect.x(),
                           slot.rect.y() / ratio,
                           slot.rect.width(),
                           slot.rect.height() / ratio);
    return layout;
}

} // namespace

Layout makeMosaicLayout(const std::vector<double> &aspects,
                        const std::vector<double> &weights,
                        double ratio,
                        unsigned seed)
{
    const int n = int(aspects.size());
    if (n == 0 || ratio <= 0)
        return {};
    std::vector<double> w = weights;
    w.resize(size_t(n), 1.0);
    std::mt19937 rng(seed);
    Layout best;
    double bestScore = 1e9;
    for (int attempt = 0; attempt < 400; ++attempt) {
        std::vector<int> ids(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i)
            ids[size_t(i)] = i;
        std::shuffle(ids.begin(), ids.end(), rng);
        Layout layout;
        slice(QRectF(0, 0, 1, ratio), ids, w, rng, layout);
        // Crop loss, plus a little for very thin frames, which look odd even when they fit.
        double score = cropLoss(toFractions(layout, ratio), aspects, ratio);
        for (const Slot &slot : layout) {
            const double thin = std::max(slot.rect.width() / slot.rect.height(),
                                         slot.rect.height() / slot.rect.width());
            if (thin > 3.0)
                score += 0.05 * (thin - 3.0);
        }
        if (score < bestScore) {
            bestScore = score;
            best = layout;
        }
    }
    return toFractions(best, ratio);
}

Layout makeGridLayout(int count, double ratio, unsigned seed)
{
    if (count <= 0 || ratio <= 0)
        return {};
    // Frames about square: rows / columns close to the page's height / width.
    int cols = std::max(1, int(std::lround(std::sqrt(count / ratio))));
    cols = std::min(cols, count);
    const int rows = (count + cols - 1) / cols;
    std::vector<int> order(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i)
        order[size_t(i)] = i;
    if (seed > 1) {
        std::mt19937 rng(seed);
        std::shuffle(order.begin(), order.end(), rng);
    }
    Layout layout;
    int next = 0;
    for (int r = 0; r < rows; ++r) {
        const int inRow = std::min(cols, count - next);
        for (int c = 0; c < inRow; ++c) {
            layout.push_back(
                Slot{QRectF(double(c) / inRow, double(r) / rows, 1.0 / inRow, 1.0 / rows),
                     order[size_t(next++)]});
        }
    }
    return layout;
}

} // namespace Collage
