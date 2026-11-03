/*
 * Tests for the collage layout. The first group is PhotoCollage's own test suite
 * (https://github.com/adrienverge/PhotoCollage, tests/test_collage.py), ported with the same
 * forced choices and expected layouts, so the C++ port is checked against the original.
 *
 * Copyright (C) 2014 Adrien Vergé
 * Copyright (C) 2026 BUGS! Video Editor contributors
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

#include <QSet>
#include <QtTest/QtTest>

#include <deque>

using namespace Collage;

class TestCollage : public QObject
{
    Q_OBJECT

    // Photos must outlive the page that points to them.
    std::deque<Photo> m_photos;

    Photo *photo(int w, int h)
    {
        m_photos.push_back(Photo{QStringLiteral("img"), w, h});
        return &m_photos.back();
    }

    // These stand in for PhotoCollage's patches of random.choice and random.random.
    static void forceCellPosition(Page &page, size_t pos)
    {
        page.choose = [pos](size_t) { return pos; };
    }
    static void preventCellExtension(Page &page)
    {
        page.random = []() { return 0.0; };
    }
    static void forceCellExtension(Page &page)
    {
        page.random = []() { return 1.0; };
    }

private slots:
    void init() { m_photos.clear(); }

    // From PhotoCollage

    void nextFreeCol()
    {
        {
            Page page(100, 0.6, 4);
            forceCellPosition(page, 0);
            preventCellExtension(page);
            page.addCell(photo(10, 10));
            page.addCell(photo(10, 10));
            QCOMPARE(page.repr(), QStringLiteral("[25 25] [25 25]  "));
        }
        {
            Page page(40, 0.6, 4);
            forceCellPosition(page, 0);
            preventCellExtension(page);
            page.addCell(photo(10, 20));
            page.addCell(photo(10, 15));
            page.addCell(photo(10, 10));
            page.addCell(photo(10, 10));
            page.addCell(photo(10, 10));
            page.addCell(photo(10, 10));
            page.addCell(photo(10, 22));
            QCOMPARE(page.repr(),
                     QStringLiteral("[10 20] [10 15] [10 10] [10 10]\n"
                                    "        [10 22] [10 10] [10 10]"));
        }
        {
            Page page(50, 0.6, 5);
            forceCellPosition(page, 0);
            forceCellExtension(page);
            page.addCell(photo(10, 15));
            page.addCell(photo(10, 10));
            preventCellExtension(page);
            page.addCell(photo(10, 10));
            page.addCell(photo(10, 50));
            QCOMPARE(page.repr(),
                     QStringLiteral("[20 30-- ------] [20 20-- ------] [10 10]\n"
                                    "                                  [10 50]"));
        }
    }

    void removeEmptyCols()
    {
        Page page(1, 0.6, 100);
        preventCellExtension(page);
        for (int i = 0; i < 5; ++i)
            page.addCell(photo(10, 10));
        page.removeEmptyCols();
        QCOMPARE(page.noCols(), 5);
    }

    void bottomHoleA1()
    {
        Page page(30, 0.6, 3);
        forceCellPosition(page, 0);
        preventCellExtension(page);
        page.addCell(photo(10, 15));
        page.addCell(photo(10, 10));
        page.addCell(photo(10, 10));
        forceCellExtension(page);
        page.addCell(photo(10, 5));
        preventCellExtension(page);
        page.addCell(photo(10, 15));
        page.addCell(photo(10, 10));
        QCOMPARE(page.repr(),
                 QStringLiteral("[10 15] [10 10]  [10 10]\n"
                                "[10 15] [20 10-- ------]\n"
                                "        [10 10]         "));
        page.removeBottomHoles();
        QCOMPARE(page.repr(),
                 QStringLiteral("[10 15] [10 10]  [10 10]\n"
                                "[10 15] [20 10-- ------]\n"
                                "        [20 10-- ------]"));
    }

    void bottomHoleA2()
    {
        Page page(30, 0.6, 3);
        forceCellPosition(page, 0);
        preventCellExtension(page);
        page.addCell(photo(10, 15));
        page.addCell(photo(10, 10));
        page.addCell(photo(10, 10));
        forceCellExtension(page);
        page.addCell(photo(10, 5));
        preventCellExtension(page);
        page.addCell(photo(10, 15));
        forceCellPosition(page, 1);
        page.addCell(photo(10, 10));
        QCOMPARE(page.repr(),
                 QStringLiteral("[10 15] [10 10]  [10 10]\n"
                                "[10 15] [20 10-- ------]\n"
                                "                 [10 10]"));
        page.removeBottomHoles();
        QCOMPARE(page.repr(),
                 QStringLiteral("[10 15] [10 10]  [10 10]\n"
                                "[10 15] [20 10-- ------]\n"
                                "        [20 10-- ------]"));
    }

    void bottomHoleB2()
    {
        Page page(30, 0.6, 3);
        forceCellPosition(page, 0);
        preventCellExtension(page);
        page.addCell(photo(10, 10));
        page.addCell(photo(10, 10));
        page.addCell(photo(10, 10));
        forceCellExtension(page);
        page.addCell(photo(10, 5));
        preventCellExtension(page);
        page.addCell(photo(10, 10));
        forceCellPosition(page, 2);
        forceCellExtension(page);
        page.addCell(photo(10, 5));
        QCOMPARE(page.repr(),
                 QStringLiteral("[10 10]  [10 10]  [10 10]\n"
                                "[20 10-- ------]  [10 10]\n"
                                "         [20 10-- ------]"));
        page.removeBottomHoles();
        QCOMPARE(page.repr(),
                 QStringLiteral("[10 10]  [10 10] [10 10]\n"
                                "[20 10-- ------] [10 10]\n"
                                "[20 10-- ------]        "));
    }

    void bottomHoleB1()
    {
        Page page(30, 0.6, 3);
        forceCellPosition(page, 0);
        preventCellExtension(page);
        page.addCell(photo(10, 10));
        page.addCell(photo(10, 10));
        page.addCell(photo(10, 10));
        page.addCell(photo(10, 10));
        forceCellExtension(page);
        page.addCell(photo(10, 5));
        page.addCell(photo(10, 5));
        QCOMPARE(page.repr(),
                 QStringLiteral("[10 10]  [10 10]  [10 10]\n"
                                "[10 10]  [20 10-- ------]\n"
                                "[20 10-- ------]         "));
        page.removeBottomHoles();
        QCOMPARE(page.repr(),
                 QStringLiteral("[10 10] [10 10]  [10 10]\n"
                                "[10 10] [20 10-- ------]\n"
                                "        [20 10-- ------]"));
    }

    // BUGS! checks of whole layouts

    void layoutsFillThePageExactly_data()
    {
        QTest::addColumn<int>("count");
        QTest::addColumn<double>("ratio");
        for (int count : {2, 3, 5, 8, 13, 24}) {
            for (double ratio : {1.0, 16.0 / 9.0, 9.0 / 16.0, 5.0 / 4.0}) {
                const QByteArray name = QByteArray::number(count) + " photos, ratio "
                                        + QByteArray::number(ratio, 'f', 2);
                QTest::newRow(name.constData()) << count << ratio;
            }
        }
    }

    void layoutsFillThePageExactly()
    {
        QFETCH(int, count);
        QFETCH(double, ratio);
        std::vector<Photo *> photos;
        // A mix of portrait, landscape and square photos, as from a phone.
        const QSize shapes[] = {{4032, 3024}, {3024, 4032}, {1080, 1920}, {2000, 2000}, {1600, 900}};
        for (int i = 0; i < count; ++i)
            photos.push_back(photo(shapes[i % 5].width(), shapes[i % 5].height()));

        for (unsigned seed = 1; seed <= 20; ++seed) {
            // The page may come out narrower than 1 when a column ends up empty, as in PhotoCollage;
            // its shape is what matters.
            auto page = makeGaplessPage(photos, ratio, seed);
            QVERIFY(qAbs(page->ratio() - ratio) < 1e-9);

            // Every photo appears exactly once.
            const auto cells = page->photoCells();
            QCOMPARE(int(cells.size()), count);
            QSet<const Photo *> seen;
            for (const Cell *cell : cells)
                seen.insert(cell->photo());
            QCOMPARE(seen.size(), count);

            // Cells cover the page with no gaps or overlaps: their areas add up to the page, and
            // no two of them overlap.
            double area = 0;
            for (size_t i = 0; i < cells.size(); ++i) {
                const QRectF a(cells[i]->x(), cells[i]->y(), cells[i]->w(), cells[i]->h());
                QVERIFY(a.width() > 0 && a.height() > 0);
                area += a.width() * a.height();
                for (size_t j = i + 1; j < cells.size(); ++j) {
                    const QRectF b(cells[j]->x(), cells[j]->y(), cells[j]->w(), cells[j]->h());
                    const QRectF overlap = a.intersected(b);
                    QVERIFY(overlap.width() * overlap.height() < 1e-9);
                }
            }
            QVERIFY2(qAbs(area - page->w() * page->h()) < 1e-6,
                     qPrintable(QStringLiteral("seed %1 area %2 page %3")
                                    .arg(seed)
                                    .arg(area)
                                    .arg(page->w() * page->h())));
        }
    }

    // PhotoCollage itself leaves a gap in about 1 layout in 100 with many photos; makeGaplessPage
    // moves past those.
    void gaplessSkipsLayoutsWithGaps()
    {
        std::vector<Photo *> photos;
        const QSize shapes[] = {{4032, 3024}, {3024, 4032}, {1080, 1920}, {2000, 2000}, {1600, 900}};
        for (int i = 0; i < 24; ++i)
            photos.push_back(photo(shapes[i % 5].width(), shapes[i % 5].height()));
        int gaps = 0;
        for (unsigned seed = 1; seed <= 300; ++seed) {
            if (coverage(*makePage(photos, 9.0 / 16.0, seed)) < 0.9999)
                ++gaps;
            QVERIFY(coverage(*makeGaplessPage(photos, 9.0 / 16.0, seed)) > 0.9999);
        }
        qInfo() << "plain layouts with a gap:" << gaps << "of 300";
    }

    void sameSeedSameLayout()
    {
        std::vector<Photo *> photos;
        for (int i = 0; i < 7; ++i)
            photos.push_back(photo(1000 + 300 * i, 1500 - 100 * i));
        const QString a = makePage(photos, 1.0, 42)->repr();
        QCOMPARE(makePage(photos, 1.0, 42)->repr(), a);
        bool differs = false;
        for (unsigned seed = 1; seed < 10 && !differs; ++seed)
            differs = makePage(photos, 1.0, seed)->repr() != a;
        QVERIFY(differs);
    }

    void findAndSwapCells()
    {
        Photo *a = photo(4000, 3000);
        Photo *b = photo(3000, 4000);
        auto page = makePage({a, b}, 1.0, 3);
        page->scaleToFit(1000, 1000);
        const auto cells = page->photoCells();
        QCOMPARE(int(cells.size()), 2);
        Cell *first = page->cellAt(cells[0]->x() + 1, cells[0]->y() + 1);
        Cell *second = page->cellAt(cells[1]->x() + 1, cells[1]->y() + 1);
        QCOMPARE(first, cells[0]);
        QCOMPARE(second, cells[1]);
        Photo *before = first->photo();
        Page::swapPhotos(first, second);
        QCOMPARE(second->photo(), before);
        QVERIFY(page->cellAt(-1, -1) == nullptr);
    }

    void photoOffsetsStayInRange()
    {
        Photo p{QStringLiteral("img"), 100, 50};
        p.move(0.8, -0.9);
        QCOMPARE(p.offsetW, 1.0);
        QCOMPARE(p.offsetH, 0.0);
    }
};

QTEST_GUILESS_MAIN(TestCollage)
#include "test_collage.moc"
