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

#include "simplemodewidget.h"

#include "Logger.h"
#include "qmltypes/qmlutilities.h"
#include "simplecontroller.h"
#include "videowidget.h"

#include <QApplication>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMimeData>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWidget>
#include <QResizeEvent>
#include <QVBoxLayout>

namespace {

const int kHeaderHeight = 64;
const int kLibraryWidth = 300;
const int kOptionsWidth = 300;
// Below this window width, such as a window snapped to half the screen, the side panels narrow
// so the video and tools keep enough room.
const int kNarrowWindow = 1100;
const int kNarrowSideWidth = 240;
const int kControlsHeight = 178;

QDir simpleQmlDir()
{
    QDir dir = QmlUtilities::qmlDir();
    dir.cd(QStringLiteral("views"));
    dir.cd(QStringLiteral("simple"));
    return dir;
}

void loadFonts()
{
    // Bundled so the app looks right offline and on any Windows install.
    QDir dir = simpleQmlDir();
    dir.cd(QStringLiteral("fonts"));
    const auto files = dir.entryList({QStringLiteral("*.ttf")}, QDir::Files);
    for (const auto &file : files) {
        if (QFontDatabase::addApplicationFont(dir.filePath(file)) < 0)
            LOG_WARNING() << "failed to load font" << file;
    }
}

} // namespace

SimpleModeWidget::SimpleModeWidget(QWidget *parent)
    : QWidget(parent)
    , m_controller(new SimpleController(this))
{
    loadFonts();
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(true);
    // Simple mode is in English. A right-to-left language chosen in Advanced would otherwise
    // swap the library and tool panels while the words stay left to right.
    setLayoutDirection(Qt::LeftToRight);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *header = addView(QStringLiteral("Header.qml"));
    header->setFixedHeight(kHeaderHeight);
    root->addWidget(header);

    auto *body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    root->addLayout(body, 1);

    m_library = addView(QStringLiteral("Library.qml"));
    m_library->setFixedWidth(kLibraryWidth);
    body->addWidget(m_library);

    m_centerLayout = new QVBoxLayout;
    m_centerLayout->setContentsMargins(0, 0, 0, 0);
    m_centerLayout->setSpacing(0);
    body->addLayout(m_centerLayout, 1);

    // Holds the borrowed video widget with the mockup's breathing room around it.
    auto *preview = new QWidget(this);
    m_previewLayout = new QVBoxLayout(preview);
    m_previewLayout->setContentsMargins(24, 20, 24, 0);
    // Takes the video's place while a collage is open.
    m_collageView = addView(QStringLiteral("CollageView.qml"));
    m_collageView->hide();
    m_previewLayout->addWidget(m_collageView);
    m_centerLayout->addWidget(preview, 1);

    auto *controls = addView(QStringLiteral("Editor.qml"));
    controls->setFixedHeight(kControlsHeight);
    m_centerLayout->addWidget(controls);

    m_options = addView(QStringLiteral("Options.qml"));
    m_options->setFixedWidth(kOptionsWidth);
    body->addWidget(m_options);

    m_welcome = addView(QStringLiteral("Welcome.qml"));
    m_welcome->setParent(this);
    m_welcome->hide();

    // Drawn over everything, with see-through dimming around the card.
    m_dialogs = addView(QStringLiteral("Dialogs.qml"));
    m_dialogs->setParent(this);
    m_dialogs->setAttribute(Qt::WA_AlwaysStackOnTop);
    m_dialogs->setAttribute(Qt::WA_TranslucentBackground);
    m_dialogs->setClearColor(Qt::transparent);
    m_dialogs->hide();
    connect(m_controller, &SimpleController::dialogChanged, this, [this]() {
        if (!m_controller->dialog().isEmpty()) {
            m_dialogs->setGeometry(rect());
            m_dialogs->show();
            m_dialogs->raise();
            m_dialogs->setFocus();
        } else {
            m_dialogs->hide();
            setFocus();
        }
    });

    connect(m_controller, &SimpleController::changed, this, &SimpleModeWidget::applyTheme);
    connect(m_controller, &SimpleController::changed, this, &SimpleModeWidget::updateWelcome);
    connect(m_controller, &SimpleController::changed, this, &SimpleModeWidget::updateCollageView);
    applyTheme();
    hide();
}

QQuickWidget *SimpleModeWidget::addView(const QString &fileName)
{
    auto *view = new QQuickWidget(QmlUtilities::sharedEngine(), this);
    view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    QmlUtilities::setCommonProperties(view->rootContext());
    view->rootContext()->setContextProperty(QStringLiteral("simple"), m_controller);
    view->setSource(QUrl::fromLocalFile(simpleQmlDir().absoluteFilePath(fileName)));
    if (view->status() != QQuickWidget::Ready) {
        for (const auto &error : view->errors())
            LOG_ERROR() << error.toString();
    }
    // Keeps keyboard focus on the page unless a text box wants it.
    view->setFocusPolicy(Qt::ClickFocus);
    m_views << view;
    return view;
}

void SimpleModeWidget::applyTheme()
{
    const QColor background = m_controller->dark() ? QColor(0x11, 0x12, 0x14)
                                                   : QColor(0xF6, 0xF7, 0xF9);
    QPalette p = palette();
    p.setColor(QPalette::Window, background);
    setPalette(p);
    for (auto *view : std::as_const(m_views)) {
        if (view != m_dialogs)
            view->setClearColor(background);
    }
}

void SimpleModeWidget::updateCollageView()
{
    const bool collage = m_active && m_controller->collageActive();
    m_collageView->setVisible(collage);
    if (m_videoWidget)
        m_videoWidget->setVisible(!collage);
}

void SimpleModeWidget::updateWelcome()
{
    if (!m_welcome)
        return;
    const bool show = m_active && m_controller->welcome();
    if (show != m_welcome->isVisible()) {
        m_welcome->setVisible(show);
        if (show) {
            m_welcome->setGeometry(rect());
            m_welcome->raise();
        }
    }
}

void SimpleModeWidget::enter(QWidget *videoWidget)
{
    if (m_active)
        return;
    m_active = true;
    m_videoWidget = videoWidget;
    m_previewLayout->addWidget(m_videoWidget);
    m_videoWidget->show();

    auto *video = qobject_cast<Mlt::VideoWidget *>(videoWidget);
    video->rootContext()->setContextProperty(QStringLiteral("simple"), m_controller);
    video->setPersistentVui(
        QUrl::fromLocalFile(simpleQmlDir().absoluteFilePath(QStringLiteral("Vui.qml"))));
    // The preview stays dark in both light and dark mode.
    video->setClearColor(QColor(0x0B, 0x0C, 0x0E));
    video->quickWindow()->setColor(QColor(0x0B, 0x0C, 0x0E));
    m_videoWidget->installEventFilter(this);

    qApp->installEventFilter(this);
    show();
    raise();
    m_controller->activate();
    updateWelcome();
    setFocus();
}

void SimpleModeWidget::leave()
{
    if (!m_active)
        return;
    m_active = false;
    qApp->removeEventFilter(this);
    m_controller->deactivate();
    m_videoWidget->removeEventFilter(this);
    m_previewLayout->removeWidget(m_videoWidget);

    auto *video = qobject_cast<Mlt::VideoWidget *>(m_videoWidget);
    video->setPersistentVui(QUrl());
    video->setClearColor(palette().window().color());
    m_videoWidget = nullptr;
    m_welcome->hide();
    hide();
}

bool SimpleModeWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (m_active) {
        switch (event->type()) {
        case QEvent::ShortcutOverride:
            // Advanced's hundreds of keyboard shortcuts must not act on its hidden timeline.
            event->accept();
            return false;
        case QEvent::KeyPress:
            // The video widget hands keys it does not use to Advanced, which swallows Space.
            if (watched == m_videoWidget) {
                keyPressEvent(static_cast<QKeyEvent *>(event));
                return event->isAccepted();
            }
            break;
        case QEvent::Wheel:
            // Advanced uses the wheel to zoom the video.
            if (watched == m_videoWidget)
                return true;
            break;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void SimpleModeWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    const bool narrow = width() < kNarrowWindow;
    m_library->setFixedWidth(narrow ? kNarrowSideWidth : kLibraryWidth);
    m_options->setFixedWidth(narrow ? kNarrowSideWidth : kOptionsWidth);
    if (m_welcome)
        m_welcome->setGeometry(rect());
    if (m_dialogs)
        m_dialogs->setGeometry(rect());
}

void SimpleModeWidget::keyPressEvent(QKeyEvent *event)
{
    const bool control = event->modifiers() & Qt::ControlModifier;
    const bool shift = event->modifiers() & Qt::ShiftModifier;
    switch (event->key()) {
    case Qt::Key_Space:
        m_controller->togglePlay();
        return;
    case Qt::Key_Left:
        m_controller->stepFrames(shift ? -30 : -1);
        return;
    case Qt::Key_Right:
        m_controller->stepFrames(shift ? 30 : 1);
        return;
    case Qt::Key_Z:
        if (control) {
            if (shift)
                m_controller->redo();
            else
                m_controller->undo();
            return;
        }
        break;
    case Qt::Key_Y:
        if (control) {
            m_controller->redo();
            return;
        }
        break;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

void SimpleModeWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void SimpleModeWidget::dropEvent(QDropEvent *event)
{
    m_controller->addUrls(event->mimeData()->urls());
    event->acceptProposedAction();
}
