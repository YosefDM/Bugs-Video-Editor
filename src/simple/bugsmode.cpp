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

#include "bugsmode.h"
#include "bugsrules.h"

#include "Logger.h"
#include "qmltypes/qmlutilities.h"

#include <QDir>
#include <QEvent>
#include <QGuiApplication>
#include <QInputDialog>
#include <QMessageBox>
#include <QQmlContext>
#include <QQuickItem>
#include <QShortcut>
#include <QWidget>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

const char kUsageDayKey[] = "bugs/usageDay";
const char kUsageSecondsKey[] = "bugs/usageSeconds";
const char kOffDayKey[] = "bugs/offDay";
const char kOffForeverKey[] = "bugs/offForever";
const int kTickMs = 15000;
// The title-bar menu's own items use the high numbers and the lowest four bits.
const unsigned kMenuId = 0x0B10;

// Monday first, as Qt counts the days. Made by BugsRules::hashCode(); the codes themselves are
// only with Yosef.
const std::array<QByteArray, 7> kDayHashes = {
    QByteArrayLiteral("ff845f762fc7d5baa93baaef7ced07176bcb3ad336b7080d86aa0104172f83c1"), // Monday
    QByteArrayLiteral("2a736de227ec4e4bcc3884b0e59a616542a0c7d7abea98e93cb8c6b48bf4ca50"), // Tuesday
    QByteArrayLiteral(
        "177f69bb69477362bb6964feb5398c777b498828ff4683a38f1250d734cd103e"), // Wednesday
    QByteArrayLiteral(
        "f9150b630bd91c65ee3eadb64297916c4b3e88003aec81ea67f3b3702e59e51b"), // Thursday
    QByteArrayLiteral("b2deb133cd62cfc3bdcfea6c437f09426acd80700b777120f679df2222bd251d"), // Friday
    QByteArrayLiteral("5c447fdfce0b6a56f7f7de8837f9f737a31574a5f485f6ebfeaf2bc538c29bcb"), // Shabbos
    QByteArrayLiteral("02880a310fd22e414b94a542d84a62f8e8f827ffd373614a1425543287b41bc7"), // Sunday
};
// Levi's laptop, by its Dell service tag, made by BugsRules::hashMachineId(). Bugs mode never
// runs on any other computer.
const QList<QByteArray> kLeviComputers = {
    QByteArrayLiteral("8e161a2127b447750bc9c530a9bedaa0a6bd8bc0d89b6e9f80afe1f2225e6ced"),
};

// This computer's serial numbers from its firmware. Only Windows can say.
QStringList machineIds()
{
    QStringList ids;
#ifdef Q_OS_WIN
    const DWORD rsmb = 'RSMB';
    const UINT size = GetSystemFirmwareTable(rsmb, 0, nullptr, 0);
    if (size > 0) {
        QByteArray raw(int(size), '\0');
        if (GetSystemFirmwareTable(rsmb, 0, raw.data(), size) == size)
            ids = BugsRules::smbiosSerials(raw);
    }
#endif
    return ids;
}

const QByteArray kForeverHash = QByteArrayLiteral(
    "a7a216bb173fbc00050a2804cfd7e997cf5a6efcd6376da573d6d44e8a25274e");

} // namespace

BugsMode::BugsMode(QWidget *window)
    : QObject(window)
    , m_window(window)
{
    // For trying it out: BUGS_SHOW_BUGS=1 shows them now; =fast also sends spiders often.
    const QByteArray show = qgetenv("BUGS_SHOW_BUGS");
    m_forced = !show.isEmpty() && show != "0";
    m_quick = show == "fast";
    // Only Levi's laptop: anyone else who installs BUGS! never sees bugs or the menu item.
    m_allowed = m_forced || BugsRules::isAllowedComputer(machineIds(), kLeviComputers);
    if (!m_allowed)
        return;

    m_usageDay = m_settings.value(kUsageDayKey).toDate();
    m_usageMs = m_settings.value(kUsageSecondsKey, 0).toLongLong() * 1000;
    m_sinceTick.start();
    m_timer.setInterval(kTickMs);
    connect(&m_timer, &QTimer::timeout, this, &BugsMode::tick);
    m_timer.start();
    // Time counts while BUGS! is the window he is using.
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this]() { tick(); });
    qApp->installNativeEventFilter(this);

#ifndef Q_OS_WIN
    // There is no title-bar menu to add to outside Windows, so a key opens the question instead.
    auto *shortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+Shift+B")), window);
    shortcut->setContext(Qt::ApplicationShortcut);
    connect(shortcut, &QShortcut::activated, this, &BugsMode::askForCode);
#endif
    update();
}

BugsMode::~BugsMode()
{
    qApp->removeNativeEventFilter(this);
}

void BugsMode::tick()
{
    const qint64 elapsed = m_sinceTick.restart();
    const QDate today = QDate::currentDate();
    if (m_usageDay != today) {
        m_usageDay = today;
        m_usageMs = 0;
    }
    // Only time with BUGS! in front counts, and never more than one tick's worth at once, so a
    // laptop waking from sleep does not count the night.
    if (qGuiApp->applicationState() == Qt::ApplicationActive)
        m_usageMs += qMin<qint64>(elapsed, kTickMs + 5000);
    m_settings.setValue(kUsageDayKey, m_usageDay);
    m_settings.setValue(kUsageSecondsKey, m_usageMs / 1000);
    update();
}

void BugsMode::update()
{
    const QDate today = QDate::currentDate();
    const bool off = m_settings.value(kOffForeverKey, false).toBool()
                     || m_settings.value(kOffDayKey).toDate() == today;
    const bool due = BugsRules::isSederTime(QDateTime::currentDateTime())
                     || (m_usageDay == today && m_usageMs >= qint64(kUsageLimitSeconds) * 1000);
    const bool active = m_forced || (m_allowed && !off && due);
    if (active != m_active) {
        m_active = active;
        LOG_INFO() << "bugs mode" << (active ? "on" : "off");
        emit activeChanged();
    }
}

void BugsMode::askForCode()
{
    if (m_asking || !m_window || !m_allowed)
        return;
    if (m_settings.value(kOffForeverKey, false).toBool()) {
        QMessageBox::information(m_window,
                                 QStringLiteral("Bugs"),
                                 QStringLiteral("The bugs are already turned off for good."));
        return;
    }
    m_asking = true;
    bool ok = false;
    // Levi is not told that each day has its own code.
    const QString code = QInputDialog::getText(m_window,
                                               QStringLiteral("Turn off the bugs"),
                                               QStringLiteral(
                                                   "Call Yosef for the code, then type it here."),
                                               QLineEdit::Normal,
                                               QString(),
                                               &ok,
                                               Qt::WindowFlags(),
                                               Qt::ImhDigitsOnly);
    m_asking = false;
    if (!ok || code.trimmed().isEmpty())
        return;
    switch (BugsRules::checkCode(code, QDate::currentDate(), kDayHashes, kForeverHash)) {
    case BugsRules::CodeResult::Forever:
        m_settings.setValue(kOffForeverKey, true);
        m_settings.sync();
        update();
        attachMenu(); // takes the item away
        QMessageBox::information(m_window,
                                 QStringLiteral("Bugs"),
                                 QStringLiteral("The bugs are gone for good."));
        break;
    case BugsRules::CodeResult::Today:
        m_settings.setValue(kOffDayKey, QDate::currentDate());
        m_settings.sync();
        update();
        QMessageBox::information(m_window,
                                 QStringLiteral("Bugs"),
                                 QStringLiteral("The bugs are gone for the rest of today."));
        break;
    case BugsRules::CodeResult::Wrong:
        QMessageBox::warning(m_window,
                             QStringLiteral("Bugs"),
                             QStringLiteral("That isn't the code."));
        break;
    }
}

void BugsMode::attachMenu()
{
#ifdef Q_OS_WIN
    if (!m_window || !m_allowed)
        return;
    HWND hwnd = reinterpret_cast<HWND>(m_window->window()->winId());
    HMENU menu = GetSystemMenu(hwnd, FALSE);
    if (!menu)
        return;
    const bool present = GetMenuState(menu, kMenuId, MF_BYCOMMAND) != UINT(-1);
    const bool wanted = !m_settings.value(kOffForeverKey, false).toBool();
    if (wanted && !present) {
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kMenuId, L"Turn off the bugs…");
    } else if (!wanted && present) {
        DeleteMenu(menu, kMenuId, MF_BYCOMMAND);
    }
#endif
}

bool BugsMode::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        const MSG *msg = static_cast<const MSG *>(message);
        if (msg->message == WM_SYSCOMMAND && (msg->wParam & 0xFFF0) == kMenuId) {
            // After the menu has closed.
            QTimer::singleShot(0, this, &BugsMode::askForCode);
            if (result)
                *result = 0;
            return true;
        }
    }
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
    Q_UNUSED(result)
#endif
    return false;
}

BugsOverlay::BugsOverlay(BugsMode *mode, QWidget *window)
    : QQuickWidget(QmlUtilities::sharedEngine(), window)
    , m_mode(mode)
{
    // Drawn over everything else in the window, see-through, and never in the way of a click.
    setAttribute(Qt::WA_AlwaysStackOnTop);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_AcceptTouchEvents, false);
    setFocusPolicy(Qt::NoFocus);
    setClearColor(Qt::transparent);
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    QmlUtilities::setCommonProperties(rootContext());
    rootContext()->setContextProperty(QStringLiteral("bugs"), mode);
    QDir dir = QmlUtilities::qmlDir();
    dir.cd(QStringLiteral("views/simple"));
    setSource(QUrl::fromLocalFile(dir.absoluteFilePath(QStringLiteral("Bugs.qml"))));
    if (status() != QQuickWidget::Ready) {
        for (const auto &error : errors())
            LOG_ERROR() << error.toString();
    }
    window->installEventFilter(this);
    connect(mode, &BugsMode::activeChanged, this, &BugsOverlay::follow);
    // Other views raise themselves, such as Simple mode when it takes over; the bugs stay on top.
    m_raiseTimer.setInterval(1000);
    connect(&m_raiseTimer, &QTimer::timeout, this, [this]() { raise(); });
    follow();
}

bool BugsOverlay::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parent() && (event->type() == QEvent::Resize || event->type() == QEvent::Show))
        setGeometry(parentWidget()->rect());
    return QQuickWidget::eventFilter(watched, event);
}

void BugsOverlay::follow()
{
    if (m_mode->active()) {
        setGeometry(parentWidget()->rect());
        show();
        raise();
        m_raiseTimer.start();
    } else {
        m_raiseTimer.stop();
        hide();
    }
}
