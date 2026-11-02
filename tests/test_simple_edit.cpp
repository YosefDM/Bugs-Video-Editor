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

#include "simpleedit.h"

#include <QFile>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace Simple;

class TestSimpleEdit : public QObject
{
    Q_OBJECT

    Edit video(int frames = 300) const
    {
        Edit edit;
        edit.sourcePath = QStringLiteral("/in/clip.mov");
        edit.mediaPath = edit.sourcePath;
        edit.frames = frames;
        edit.fpsNum = 30;
        edit.fpsDen = 1;
        edit.sourceSize = QSize(1920, 1080);
        edit.reset();
        return edit;
    }

private slots:
    void keptIsWholeVideoByDefault()
    {
        const Edit edit = video();
        QCOMPARE(edit.kept().size(), 1);
        QCOMPARE(edit.keptFrames(), 300);
        QVERIFY(!edit.hasChanges());
    }

    void trimAndCutsCombine()
    {
        Edit edit = video();
        QVERIFY(edit.setTrim(30, 270));
        QVERIFY(edit.addCut(Range{100, 130}));
        QVERIFY(edit.addCut(Range{200, 210}));
        const auto kept = edit.kept();
        QCOMPARE(kept.size(), 3);
        QCOMPARE(kept[0], (Range{30, 100}));
        QCOMPARE(kept[1], (Range{130, 200}));
        QCOMPARE(kept[2], (Range{210, 270}));
        QCOMPARE(edit.keptFrames(), 200);
    }

    void cutsMergeWhenTheyOverlap()
    {
        Edit edit = video();
        QVERIFY(edit.addCut(Range{100, 150}));
        QVERIFY(edit.addCut(Range{140, 200}));
        QCOMPARE(edit.cuts.size(), 1);
        QCOMPARE(edit.cuts[0], (Range{100, 200}));
    }

    void cannotRemoveEverything()
    {
        Edit edit = video();
        QVERIFY(!edit.addCut(Range{0, 300}));
        QVERIFY(edit.cuts.isEmpty());
        QVERIFY(edit.addCut(Range{0, 150}));
        QVERIFY(!edit.addCut(Range{150, 300}));
        QVERIFY(!edit.setTrim(50, 50));
    }

    void trimCannotExcludeAllThatIsLeft()
    {
        Edit edit = video();
        QVERIFY(edit.addCut(Range{0, 150}));
        QVERIFY(!edit.setTrim(0, 150));
        QCOMPARE(edit.trim, (Range{0, 300}));
    }

    void positionsMapBothWays()
    {
        Edit edit = video();
        edit.setTrim(30, 270);
        edit.addCut(Range{100, 130});
        QCOMPARE(edit.editedFromSource(30), 0);
        QCOMPARE(edit.editedFromSource(99), 69);
        QCOMPARE(edit.editedFromSource(130), 70);
        QCOMPARE(edit.sourceFromEdited(0), 30);
        QCOMPARE(edit.sourceFromEdited(69), 99);
        QCOMPARE(edit.sourceFromEdited(70), 130);
        for (int i = 0; i < edit.keptFrames(); ++i)
            QCOMPARE(edit.editedFromSource(edit.sourceFromEdited(i)), i);
    }

    void positionInsideRemovedPartLandsAfterIt()
    {
        Edit edit = video();
        edit.addCut(Range{100, 130});
        QCOMPARE(edit.editedFromSource(115), 100);
        QCOMPARE(edit.sourceFromEdited(edit.editedFromSource(115)), 130);
        QCOMPARE(edit.editedFromSource(9999), 269);
        QCOMPARE(edit.sourceFromEdited(9999), 299);
    }

    void cropShapesHaveTheRightAspect_data()
    {
        QTest::addColumn<QString>("shape");
        QTest::addColumn<double>("aspect");
        QTest::newRow("square") << "square" << 1.0;
        QTest::newRow("9:16") << "9:16" << 9.0 / 16.0;
        QTest::newRow("16:9") << "16:9" << 16.0 / 9.0;
        QTest::newRow("4:5") << "4:5" << 4.0 / 5.0;
    }

    void cropShapesHaveTheRightAspect()
    {
        QFETCH(QString, shape);
        QFETCH(double, aspect);
        for (const QSize &size : {QSize(1920, 1080), QSize(1080, 1920), QSize(4032, 3024)}) {
            Edit edit = video();
            edit.sourceSize = size;
            edit.setCropShape(shape);
            const QSize out = edit.outputSize();
            QVERIFY(qAbs(double(out.width()) / out.height() - aspect) < 0.01);
            QCOMPARE(out.width() % 2, 0);
            QCOMPARE(out.height() % 2, 0);
            QVERIFY(out.width() <= size.width());
            QVERIFY(out.height() <= size.height());
        }
    }

    void cropIsAlwaysEvenAndInside()
    {
        Edit edit = video();
        edit.sourceSize = QSize(1081, 1921);
        edit.setCropRect(QRectF(0.123, 0.456, 0.777, 0.5));
        const QRect pixels = edit.cropPixels();
        QCOMPARE(pixels.width() % 2, 0);
        QCOMPARE(pixels.height() % 2, 0);
        QVERIFY(pixels.right() < edit.rotatedSize().width());
        QVERIFY(pixels.bottom() < edit.rotatedSize().height());
    }

    void rotateSwapsSizeAndKeepsShape()
    {
        Edit edit = video();
        edit.setCropShape(QStringLiteral("square"));
        edit.rotateClockwise();
        QCOMPARE(edit.rotation, 90);
        QCOMPARE(edit.rotatedSize(), QSize(1080, 1920));
        QCOMPARE(edit.outputSize(), QSize(1080, 1080));
        for (int i = 0; i < 3; ++i)
            edit.rotateClockwise();
        QCOMPARE(edit.rotation, 0);
        QCOMPARE(edit.rotatedSize(), QSize(1920, 1080));
    }

    void freeCropTurnsWithThePicture()
    {
        Edit edit = video();
        edit.setCropShape(QStringLiteral("free"));
        edit.setCropRect(QRectF(0.1, 0.2, 0.3, 0.4));
        const QRectF start = edit.crop;
        edit.rotateClockwise();
        QVERIFY(qFuzzyCompare(edit.crop.x(), 1.0 - 0.2 - 0.4));
        QVERIFY(qFuzzyCompare(edit.crop.y(), 0.1));
        for (int i = 0; i < 3; ++i)
            edit.rotateClockwise();
        QVERIFY(qAbs(edit.crop.x() - start.x()) < 1e-9);
        QVERIFY(qAbs(edit.crop.y() - start.y()) < 1e-9);
    }

    void flipMirrorsTheCrop()
    {
        Edit edit = video();
        edit.setCropShape(QStringLiteral("free"));
        edit.setCropRect(QRectF(0.1, 0.2, 0.3, 0.4));
        edit.toggleFlip();
        QVERIFY(qAbs(edit.crop.x() - 0.6) < 1e-9);
        edit.toggleFlip();
        QVERIFY(qAbs(edit.crop.x() - 0.1) < 1e-9);
    }

    void formatsTimes()
    {
        QCOMPARE(formatTime(0), QStringLiteral("0:00"));
        QCOMPARE(formatTime(7.2), QStringLiteral("0:07"));
        QCOMPARE(formatTime(59.96), QStringLiteral("1:00"));
        QCOMPARE(formatTime(3723), QStringLiteral("1:02:03"));
        QCOMPARE(formatTime(62.5, true), QStringLiteral("1:02.5"));
        QCOMPARE(formatTime(62, true), QStringLiteral("1:02"));
        QCOMPARE(formatDuration(6), QStringLiteral("6 seconds"));
        QCOMPARE(formatDuration(1), QStringLiteral("1 second"));
        QCOMPARE(formatDuration(65), QStringLiteral("1 minute 5 seconds"));
    }

    void parsesWhatPeopleType_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<bool>("ok");
        QTest::addColumn<double>("seconds");
        QTest::newRow("seconds") << "5" << true << 5.0;
        QTest::newRow("minutes") << "1:30" << true << 90.0;
        QTest::newRow("hours") << "1:02:03" << true << 3723.0;
        QTest::newRow("decimal") << "1:02.5" << true << 62.5;
        QTest::newRow("comma") << "0:02,5" << true << 2.5;
        QTest::newRow("spaces") << " 0:05 " << true << 5.0;
        QTest::newRow("words") << "abc" << false << 0.0;
        QTest::newRow("empty") << "" << false << 0.0;
        QTest::newRow("negative") << "-5" << false << 0.0;
    }

    void parsesWhatPeopleType()
    {
        QFETCH(QString, text);
        QFETCH(bool, ok);
        QFETCH(double, seconds);
        double result = 0;
        QCOMPARE(parseTime(text, &result), ok);
        if (ok)
            QCOMPARE(result, seconds);
    }

    void neverPicksAnExistingFile()
    {
        QTemporaryDir dir;
        const QString first = uniquePath(dir.path(),
                                         QStringLiteral("clip - edited"),
                                         QStringLiteral("mp4"));
        QVERIFY(first.endsWith(QStringLiteral("clip - edited.mp4")));
        QFile file(first);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        const QString second = uniquePath(dir.path(),
                                          QStringLiteral("clip - edited"),
                                          QStringLiteral("mp4"));
        QVERIFY(second.endsWith(QStringLiteral("clip - edited (2).mp4")));
    }

    void cleansFileNames()
    {
        QCOMPARE(safeFileName(QStringLiteral("a:b/c?.")), QStringLiteral("a-b-c-"));
        QCOMPARE(safeFileName(QString()), QStringLiteral("Untitled"));
        QCOMPARE(safeFileName(QStringLiteral("סרט חדש")), QStringLiteral("סרט חדש"));
    }

    void snapsFrameRates()
    {
        int num = 0, den = 0;
        normalizeFrameRate(29.97, &num, &den);
        QCOMPARE(num, 30000);
        QCOMPARE(den, 1001);
        normalizeFrameRate(29.98, &num, &den);
        QCOMPARE(num, 30000);
        normalizeFrameRate(30.0, &num, &den);
        QCOMPARE(num, 30);
        normalizeFrameRate(59.94, &num, &den);
        QCOMPARE(num, 60000);
        normalizeFrameRate(15.0, &num, &den);
        QCOMPARE(double(num) / den, 15.0);
        normalizeFrameRate(-1, &num, &den);
        QCOMPARE(num, 30);
    }

    void xmlDescribesTheEdit()
    {
        Edit edit = video();
        edit.mediaPath = QStringLiteral("/in/a & b <c>.mov");
        edit.baseRotation = 90;
        edit.rotation = 90;
        edit.flipped = true;
        edit.setTrim(30, 270);
        edit.addCut(Range{100, 130});
        CompileOptions options;
        options.target = QStringLiteral("/out/x.mp4");
        const QString xml = compileXml(edit, options);
        QVERIFY(xml.contains(QStringLiteral("a &amp; b &lt;c&gt;.mov")));
        QVERIFY(xml.contains(QStringLiteral("<property name=\"rotate\">180</property>")));
        QVERIFY(xml.contains(QStringLiteral("hflip")));
        QVERIFY(xml.contains(QStringLiteral("in=\"30\" out=\"99\"")));
        QVERIFY(xml.contains(QStringLiteral("in=\"130\" out=\"269\"")));
        QVERIFY(xml.contains(QStringLiteral("target=\"/out/x.mp4\"")));
        QVERIFY(xml.contains(QStringLiteral("vcodec=\"libx264\"")));
    }

    void wholeSourceModeIgnoresTrimAndCuts()
    {
        Edit edit = video();
        edit.setTrim(30, 270);
        edit.addCut(Range{100, 130});
        CompileOptions options;
        options.applyTime = false;
        QVERIFY(compileXml(edit, options).contains(QStringLiteral("in=\"0\" out=\"299\"")));
    }

    void previewAndSaveShareOnePicturePipeline()
    {
        Edit edit = video();
        edit.setCropShape(QStringLiteral("square"));
        CompileOptions preview;
        preview.captionImage = QStringLiteral("/cache/cap.png");
        CompileOptions save = preview;
        save.target = QStringLiteral("/out/x.mp4");
        const auto body = [](const QString &xml) {
            return xml.mid(xml.indexOf(QStringLiteral("<producer")));
        };
        QCOMPARE(body(compileXml(edit, preview)), body(compileXml(edit, save)));
    }

    void labelsCountAsChanges()
    {
        Edit edit = video();
        QVERIFY(!edit.hasOverlay());
        TextLabel empty;
        empty.text = QStringLiteral("   ");
        edit.labels << empty;
        QVERIFY(!edit.hasLabels());
        QVERIFY(!edit.hasChanges());
        TextLabel name;
        name.text = QStringLiteral("R Henach\nShachar");
        edit.labels << name;
        QVERIFY(edit.hasLabels());
        QVERIFY(edit.hasOverlay());
        QVERIFY(edit.hasChanges());
        Edit moved = edit;
        moved.labels[1].center = QPointF(0.2, 0.8);
        QVERIFY(moved != edit);
        Edit refont = edit;
        refont.labels[1].font = QStringLiteral("handwritten");
        QVERIFY(refont != edit);
        edit.reset();
        QVERIFY(edit.labels.isEmpty());
    }

    void labelFontsAndColours()
    {
        QCOMPARE(labelFonts().size(), 6);
        QSet<QString> ids;
        for (const auto &font : labelFonts()) {
            QVERIFY(!font.family.isEmpty());
            ids.insert(font.id);
        }
        QCOMPARE(ids.size(), 6);
        QCOMPARE(labelFontFamily(QStringLiteral("rounded")), QStringLiteral("Rubik"));
        QCOMPARE(labelFontFamily(QStringLiteral("no such font")), QStringLiteral("Inter"));
        QCOMPARE(labelColors().size(), 10);
        QVERIFY(labelFontFraction(0) < labelFontFraction(1));
        QVERIFY(labelFontFraction(1) < labelFontFraction(2));
    }

    void hdrIsConvertedInBothPreviewAndSave()
    {
        Edit edit = video();
        edit.colorTransfer = QStringLiteral("arib-std-b67");
        QVERIFY(edit.isHdr());
        QVERIFY(compileXml(edit, CompileOptions()).contains(QStringLiteral("tonemap")));
    }
};

QTEST_MAIN(TestSimpleEdit)
#include "test_simple_edit.moc"
