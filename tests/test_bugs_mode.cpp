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

#include "bugsrules.h"

#include <QtTest/QtTest>

using namespace BugsRules;

// The real codes are only with Yosef, so these tests use their own.
class TestBugsMode : public QObject
{
    Q_OBJECT

    static QDateTime at(int year, int month, int day, int hour, int minute)
    {
        return QDateTime(QDate(year, month, day), QTime(hour, minute));
    }

private slots:
    void sederTimes()
    {
        // 2026-11-22 is a Sunday.
        QVERIFY(isSederTime(at(2026, 11, 22, 10, 0)));
        QVERIFY(isSederTime(at(2026, 11, 22, 12, 59)));
        QVERIFY(!isSederTime(at(2026, 11, 22, 13, 0)));
        QVERIFY(!isSederTime(at(2026, 11, 22, 9, 59)));
        QVERIFY(!isSederTime(at(2026, 11, 22, 14, 30)));
        QVERIFY(isSederTime(at(2026, 11, 22, 16, 0)));
        QVERIFY(isSederTime(at(2026, 11, 22, 18, 29)));
        QVERIFY(!isSederTime(at(2026, 11, 22, 18, 30)));
        QVERIFY(!isSederTime(at(2026, 11, 22, 21, 0)));
        // Monday to Thursday too.
        for (int day = 23; day <= 26; ++day)
            QVERIFY(isSederTime(at(2026, 11, day, 11, 0)));
        // Not Friday or Shabbos.
        QVERIFY(!isSederTime(at(2026, 11, 27, 11, 0)));
        QVERIFY(!isSederTime(at(2026, 11, 28, 11, 0)));
        QVERIFY(!isSederTime(at(2026, 11, 28, 17, 0)));
    }

    void eachDayHasItsOwnCode()
    {
        std::array<QByteArray, 7> days;
        for (int i = 0; i < 7; ++i)
            days[size_t(i)] = hashCode(QString::number(1000 + i));
        const QByteArray foreverHash = hashCode(QStringLiteral("4321"));
        // 2026-11-23 is a Monday, which is 1000 here; Sunday the 22nd is 1006.
        QCOMPARE(checkCode(QStringLiteral("1000"), QDate(2026, 11, 23), days, foreverHash),
                 CodeResult::Today);
        QCOMPARE(checkCode(QStringLiteral("1006"), QDate(2026, 11, 22), days, foreverHash),
                 CodeResult::Today);
        // Another day's code does not work today.
        QCOMPARE(checkCode(QStringLiteral("1000"), QDate(2026, 11, 22), days, foreverHash),
                 CodeResult::Wrong);
        QCOMPARE(checkCode(QStringLiteral("9999"), QDate(2026, 11, 22), days, foreverHash),
                 CodeResult::Wrong);
        QCOMPARE(checkCode(QString(), QDate(2026, 11, 22), days, foreverHash), CodeResult::Wrong);
        // The one for good works on any day, and spaces round a code don't matter.
        for (int day = 22; day <= 28; ++day)
            QCOMPARE(checkCode(QStringLiteral(" 4321 "), QDate(2026, 11, day), days, foreverHash),
                     CodeResult::Forever);
    }

    void onlyLevisComputer()
    {
        const QList<QByteArray> allowed = {hashMachineId(QStringLiteral("ABC1234"))};
        QVERIFY(isAllowedComputer({QStringLiteral("ABC1234")}, allowed));
        // Small letters and spaces don't matter.
        QVERIFY(isAllowedComputer({QStringLiteral(" abc1234 ")}, allowed));
        QVERIFY(isAllowedComputer({QString(), QStringLiteral("Abc1234")}, allowed));
        QVERIFY(!isAllowedComputer({QStringLiteral("XYZ9876")}, allowed));
        QVERIFY(!isAllowedComputer({}, allowed));
        QVERIFY(!isAllowedComputer({QStringLiteral("ABC1234")}, {}));
        QVERIFY(!hashMachineId(QStringLiteral("ABC1234")).contains("ABC"));
    }

    // A firmware table laid out as a Dell's is: BIOS, system and case, then the end marker.
    void readsTheServiceTagFromTheFirmware()
    {
        QByteArray table;
        const auto structure =
            [&](uchar type, QByteArray formatted, const QList<QByteArray> &strings) {
                formatted[0] = char(type);
                formatted[1] = char(formatted.size());
                table += formatted;
                for (const QByteArray &string : strings)
                    table += string + '\0';
                table += strings.isEmpty() ? QByteArray(2, '\0') : QByteArray(1, '\0');
            };
        QByteArray bios(0x18, '\0');
        bios[4] = 1; // vendor is string 1
        structure(0, bios, {"Dell Inc.", "1.22.0"});
        QByteArray system(0x1b, '\0');
        system[4] = 1;
        system[5] = 2;
        system[7] = 3; // serial number is string 3
        structure(1, system, {"Dell Inc.", "Latitude 7300", "ABC1234"});
        QByteArray chassis(0x15, '\0');
        chassis[7] = 2;
        structure(3, chassis, {"Dell Inc.", "ABC1234"});
        structure(127, QByteArray(4, '\0'), {});
        QByteArray raw(8, '\0');
        raw[1] = 3; // SMBIOS 3.x
        const quint32 length = quint32(table.size());
        raw[4] = char(length & 0xff);
        raw[5] = char((length >> 8) & 0xff);
        raw += table;
        QCOMPARE(smbiosSerials(raw), QStringList{QStringLiteral("ABC1234")});
        QVERIFY(isAllowedComputer(smbiosSerials(raw), {hashMachineId(QStringLiteral("ABC1234"))}));
        // Cut short or empty, it finds nothing and does not read past the end.
        QVERIFY(smbiosSerials(raw.left(20)).isEmpty());
        QVERIFY(smbiosSerials(QByteArray()).isEmpty());
    }

    void codesAreNotKeptAsTheyAre()
    {
        const QByteArray hash = hashCode(QStringLiteral("1234"));
        QCOMPARE(hash.size(), 64);
        QVERIFY(!hash.contains("1234"));
        QCOMPARE(hashCode(QStringLiteral("1234")), hash);
        QVERIFY(hashCode(QStringLiteral("1235")) != hash);
    }
};

QTEST_MAIN(TestBugsMode)
#include "test_bugs_mode.moc"
