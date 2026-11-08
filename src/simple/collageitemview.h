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

#ifndef COLLAGEITEMVIEW_H
#define COLLAGEITEMVIEW_H

#include <QPointer>
#include <QQuickPaintedItem>
#include <QVariant>

class SimpleController;

// One text box or sticker on the collage preview. It paints with the same C++ code that draws the
// saved picture, so the two match; QML only places and turns it.
class CollageItemView : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QObject *controller READ controller WRITE setController NOTIFY changed)
    Q_PROPERTY(int index READ index WRITE setIndex NOTIFY changed)
    // Anything that changes the item's look; a new value repaints it.
    Q_PROPERTY(QVariant look READ look WRITE setLook NOTIFY changed)
    // The size the whole collage is shown at, which sets the letter size.
    Q_PROPERTY(QSizeF pageSize READ pageSize WRITE setPageSize NOTIFY changed)

public:
    explicit CollageItemView(QQuickItem *parent = nullptr);

    QObject *controller() const;
    void setController(QObject *controller);
    int index() const { return m_index; }
    void setIndex(int index);
    QVariant look() const { return m_look; }
    void setLook(const QVariant &look);
    QSizeF pageSize() const { return m_pageSize; }
    void setPageSize(const QSizeF &size);

    void paint(QPainter *painter) override;

signals:
    void changed();

private:
    QPointer<SimpleController> m_controller;
    int m_index{-1};
    QVariant m_look;
    QSizeF m_pageSize;
};

#endif // COLLAGEITEMVIEW_H
