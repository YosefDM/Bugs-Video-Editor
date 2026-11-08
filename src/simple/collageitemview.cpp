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

#include "collageitemview.h"

#include "simplecontroller.h"

#include <QPainter>

CollageItemView::CollageItemView(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
}

QObject *CollageItemView::controller() const
{
    return m_controller.data();
}

void CollageItemView::setController(QObject *controller)
{
    m_controller = qobject_cast<SimpleController *>(controller);
    emit changed();
    update();
}

void CollageItemView::setIndex(int index)
{
    if (index == m_index)
        return;
    m_index = index;
    emit changed();
    update();
}

void CollageItemView::setLook(const QVariant &look)
{
    if (look == m_look)
        return;
    m_look = look;
    emit changed();
    update();
}

void CollageItemView::setPageSize(const QSizeF &size)
{
    if (size == m_pageSize)
        return;
    m_pageSize = size;
    emit changed();
    update();
}

void CollageItemView::paint(QPainter *painter)
{
    if (m_controller && !m_pageSize.isEmpty())
        m_controller->paintCollageItem(painter, m_index, boundingRect(), m_pageSize);
}
