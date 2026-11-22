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

#ifndef BUGSMODE_H
#define BUGSMODE_H

#include <QAbstractNativeEventFilter>
#include <QDate>
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QQuickWidget>
#include <QSettings>
#include <QTimer>

// Bugs mode: during seder, or once BUGS! has been in use for three hours in a day, real bugs crawl
// over the window and now and then a spider jumps out. Turning them off takes a code from Yosef:
// each weekday has its own, which turns them off for the rest of that day, and one turns them off
// for good. It is in the window's title-bar menu (right-click the title bar, or Alt+Space).
class BugsMode : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    // Shorter waits between spiders, for trying it out (BUGS_SHOW_BUGS=fast)
    Q_PROPERTY(bool quickSpiders READ quickSpiders CONSTANT)

public:
    static const int kUsageLimitSeconds = 3 * 60 * 60;

    explicit BugsMode(QWidget *window);
    ~BugsMode() override;

    bool active() const { return m_active; }
    bool quickSpiders() const { return m_quick; }

    // Puts "Turn off the bugs…" in the window's title-bar menu. Called when the window shows, as
    // Windows can give it a new handle.
    void attachMenu();
    // Asks for a code and acts on it.
    void askForCode();

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

signals:
    void activeChanged();

private:
    void tick();
    void update();

    QPointer<QWidget> m_window;
    QSettings m_settings;
    QTimer m_timer;
    QElapsedTimer m_sinceTick;
    QDate m_usageDay;
    qint64 m_usageMs{0};
    bool m_active{false};
    bool m_allowed{false}; // Levi's laptop
    bool m_forced{false};
    bool m_quick{false};
    bool m_asking{false};
};

// The bugs themselves: a see-through layer over the whole window that clicks pass straight
// through, shown only while bugs mode is on.
class BugsOverlay : public QQuickWidget
{
    Q_OBJECT

public:
    BugsOverlay(BugsMode *mode, QWidget *window);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void follow();

    BugsMode *m_mode;
    QTimer m_raiseTimer;
};

#endif // BUGSMODE_H
