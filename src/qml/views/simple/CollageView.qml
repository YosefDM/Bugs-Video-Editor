import QtQuick
import "Theme.js" as Theme

// The collage, shown where the video preview usually is. Tap a photo, then another, to swap them.
Rectangle {
    id: root

    readonly property var t: Theme.colors(simple.dark)

    color: "#0B0C0E"

    Image {
        id: collage

        anchors.fill: parent
        anchors.margins: 12
        source: simple.collagePreview
        fillMode: Image.PreserveAspectFit
        cache: false
        asynchronous: false

        // The area the picture actually covers inside the Image item.
        readonly property real shownX: (width - paintedWidth) / 2
        readonly property real shownY: (height - paintedHeight) / 2

        Rectangle {
            readonly property var cell: simple.collageSelected >= 0 ? simple.collageCells[simple.collageSelected] : null

            visible: cell !== null
            x: cell ? collage.shownX + cell.x * collage.paintedWidth : 0
            y: cell ? collage.shownY + cell.y * collage.paintedHeight : 0
            width: cell ? cell.w * collage.paintedWidth : 0
            height: cell ? cell.h * collage.paintedHeight : 0
            color: "#33FFFFFF"
            border.width: 4
            border.color: root.t.accent
        }

        MouseArea {
            x: collage.shownX
            y: collage.shownY
            width: collage.paintedWidth
            height: collage.paintedHeight
            cursorShape: Qt.PointingHandCursor
            onClicked: (mouse) => simple.tapCollage(mouse.x / width, mouse.y / height)
        }
    }
}
