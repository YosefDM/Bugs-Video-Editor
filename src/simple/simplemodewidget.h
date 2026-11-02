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

#ifndef SIMPLEMODEWIDGET_H
#define SIMPLEMODEWIDGET_H

#include <QWidget>

class QQuickWidget;
class QVBoxLayout;
class SimpleController;

// The Simple mode screen. It sits over the whole main window and borrows the one video widget
// from the player while it is showing, so Advanced keeps working untouched behind it.
class SimpleModeWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SimpleModeWidget(QWidget *parent = nullptr);

    SimpleController *controller() const { return m_controller; }
    // Takes over the screen and the video widget, then gives them back.
    void enter(QWidget *videoWidget);
    void leave();
    bool isActive() const { return m_active; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    QQuickWidget *addView(const QString &fileName);
    void applyTheme();
    void updateWelcome();

    SimpleController *m_controller;
    QVBoxLayout *m_centerLayout;
    QVBoxLayout *m_previewLayout;
    QWidget *m_videoWidget{nullptr};
    QList<QQuickWidget *> m_views;
    QQuickWidget *m_welcome{nullptr};
    QQuickWidget *m_dialogs{nullptr};
    bool m_active{false};
};

#endif // SIMPLEMODEWIDGET_H
