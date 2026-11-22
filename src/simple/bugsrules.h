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

#ifndef BUGSRULES_H
#define BUGSRULES_H

#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

#include <array>

// When Bugs mode comes on, and the codes that turn it off. No windows here, so it can be tested.
namespace BugsRules {

enum class CodeResult { Wrong, Today, Forever };

// Sunday to Thursday, 10 am to 1 pm and 4 pm to 6:30 pm.
bool isSederTime(const QDateTime &when);
// The codes are kept only as these, so they cannot be read from the program.
QByteArray hashCode(const QString &code);
// A computer's own serial number, such as a Dell's service tag, kept the same way, so whose
// computer it is cannot be read from the program either. Capitals and spaces don't matter.
QByteArray hashMachineId(const QString &id);
// Bugs mode only ever runs on a computer with one of the `allowed` serial numbers.
bool isAllowedComputer(const QStringList &ids, const QList<QByteArray> &allowed);
// The serial numbers in the firmware's SMBIOS table, as Windows' GetSystemFirmwareTable('RSMB')
// returns it: the system's (type 1) and the case's (type 3). On a Dell both are the service tag.
QStringList smbiosSerials(const QByteArray &rawSmbios);
// `dayHashes` are by Qt's day of the week, Monday (1) first.
CodeResult checkCode(const QString &code,
                     const QDate &day,
                     const std::array<QByteArray, 7> &dayHashes,
                     const QByteArray &foreverHash);

} // namespace BugsRules

#endif // BUGSRULES_H
