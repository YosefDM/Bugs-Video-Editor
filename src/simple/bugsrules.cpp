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

#include <QCryptographicHash>

namespace BugsRules {

bool isSederTime(const QDateTime &when)
{
    const int day = when.date().dayOfWeek(); // Monday 1 ... Sunday 7
    const bool sederDay = day == Qt::Sunday || day <= Qt::Thursday;
    if (!sederDay)
        return false;
    const QTime time = when.time();
    return (time >= QTime(10, 0) && time < QTime(13, 0))
           || (time >= QTime(16, 0) && time < QTime(18, 30));
}

QByteArray hashCode(const QString &code)
{
    return QCryptographicHash::hash(QByteArrayLiteral("bugs-mode:") + code.trimmed().toUtf8(),
                                    QCryptographicHash::Sha256)
        .toHex();
}

QByteArray hashMachineId(const QString &id)
{
    return QCryptographicHash::hash(QByteArrayLiteral("bugs-pc:") + id.trimmed().toUpper().toUtf8(),
                                    QCryptographicHash::Sha256)
        .toHex();
}

bool isAllowedComputer(const QStringList &ids, const QList<QByteArray> &allowed)
{
    for (const QString &id : ids) {
        if (!id.trimmed().isEmpty() && allowed.contains(hashMachineId(id)))
            return true;
    }
    return false;
}

QStringList smbiosSerials(const QByteArray &raw)
{
    // RawSMBIOSData: four version bytes, a 32-bit length, then the structures.
    QStringList serials;
    if (raw.size() < 8)
        return serials;
    const auto *bytes = reinterpret_cast<const uchar *>(raw.constData());
    const quint32 length = quint32(bytes[4]) | quint32(bytes[5]) << 8 | quint32(bytes[6]) << 16
                           | quint32(bytes[7]) << 24;
    const uchar *p = bytes + 8;
    const uchar *end = bytes + qMin<qint64>(raw.size(), 8 + qint64(length));
    while (p + 4 <= end) {
        const uchar type = p[0];
        const uchar formatted = p[1];
        if (formatted < 4 || p + formatted > end)
            break;
        // The structure's strings follow it, each ending in a zero, with one more zero after the
        // last (two zeros when there are none).
        QList<QByteArray> strings;
        const uchar *s = p + formatted;
        while (s < end && *s) {
            const uchar *e = s;
            while (e < end && *e)
                ++e;
            strings << QByteArray(reinterpret_cast<const char *>(s), int(e - s));
            s = e + 1;
        }
        const uchar *next = strings.isEmpty() ? s + 2 : s + 1;
        // Both keep the serial number's string number at offset 7.
        if ((type == 1 || type == 3) && formatted > 7) {
            const int index = p[7];
            if (index > 0 && index <= strings.size()) {
                const QString serial = QString::fromLatin1(strings.at(index - 1)).trimmed();
                if (!serial.isEmpty() && !serials.contains(serial))
                    serials << serial;
            }
        }
        if (type == 127) // end of table
            break;
        p = next;
    }
    return serials;
}

CodeResult checkCode(const QString &code,
                     const QDate &day,
                     const std::array<QByteArray, 7> &dayHashes,
                     const QByteArray &foreverHash)
{
    const QByteArray hash = hashCode(code);
    if (hash == foreverHash)
        return CodeResult::Forever;
    const int index = day.dayOfWeek() - 1;
    if (index >= 0 && index < int(dayHashes.size()) && hash == dayHashes[size_t(index)])
        return CodeResult::Today;
    return CodeResult::Wrong;
}

} // namespace BugsRules
