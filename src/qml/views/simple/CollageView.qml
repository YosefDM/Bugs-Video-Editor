import QtQuick
import BugsSimple
import "Theme.js" as Theme

// The collage, shown where the video preview usually is. Tap a photo, then another, to swap them.
// Text and stickers sit on top: drag to move, the corner to resize, the knob above to turn.
Rectangle {
    id: root

    readonly property var t: Theme.colors(simple.dark)

    color: "#0B0C0E"

    Image {
        id: collage

        anchors.fill: parent
        anchors.margins: 24
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

        // Text and stickers, over the photos and the same size as the picture.
        Item {
            id: itemLayer

            x: collage.shownX
            y: collage.shownY
            width: collage.paintedWidth
            height: collage.paintedHeight

            // Counted rather than given the list, so items survive edits: rebuilding them would
            // end a drag after its first step.
            Repeater {
                model: simple.collageItems.length

                delegate: Item {
                    id: box

                    required property int index
                    readonly property var d: simple.collageItems[index] || null
                    readonly property real centerX: d ? d.x * itemLayer.width : 0
                    readonly property real centerY: d ? d.y * itemLayer.height : 0

                    width: d ? d.w * itemLayer.width : 0
                    height: d ? d.h * itemLayer.height : 0
                    x: centerX - width / 2
                    y: centerY - height / 2
                    rotation: d ? d.rotation : 0
                    z: index

                    CollageItemView {
                        anchors.fill: parent
                        controller: simple
                        index: box.index
                        look: box.d ? JSON.stringify(box.d) : ""
                        pageSize: Qt.size(itemLayer.width, itemLayer.height)
                    }

                    // Drag the item itself to move it.
                    MouseArea {
                        property point grab

                        anchors.fill: parent
                        anchors.margins: -6
                        cursorShape: Qt.SizeAllCursor
                        onPressed: (mouse) => {
                            simple.selectCollageItem(box.index);
                            const at = mapToItem(itemLayer, mouse.x, mouse.y);
                            grab = Qt.point(at.x - box.centerX, at.y - box.centerY);
                        }
                        onPositionChanged: (mouse) => {
                            if (!pressed)
                                return;
                            const at = mapToItem(itemLayer, mouse.x, mouse.y);
                            simple.moveCollageItem(box.index, (at.x - grab.x) / itemLayer.width,
                                                   (at.y - grab.y) / itemLayer.height);
                        }
                        onReleased: simple.endCollageItemGesture()
                    }
                }
            }

            // The selection frame and handles, above every item, so an item sent to the back can
            // still be turned and resized where something covers it.
            Item {
                id: sel

                readonly property int index: simple.collageSelectedItem
                readonly property var d: index >= 0 ? simple.collageItems[index] || null : null
                readonly property real centerX: d ? d.x * itemLayer.width : 0
                readonly property real centerY: d ? d.y * itemLayer.height : 0

                visible: d !== null
                z: 100000
                width: d ? d.w * itemLayer.width : 0
                height: d ? d.h * itemLayer.height : 0
                x: centerX - width / 2
                y: centerY - height / 2
                rotation: d ? d.rotation : 0

                // Shows which one the options on the right change.
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -4
                    color: "transparent"
                    border.width: 2
                    border.color: "#FFFFFF"
                    radius: 4

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -1
                        color: "transparent"
                        border.width: 1
                        border.color: "#80000000"
                        radius: 5
                    }
                }

                // Turn knob, on a short stem above the top edge.
                Rectangle {
                    x: sel.width / 2 - 1
                    y: -26
                    width: 2
                    height: 22
                    color: "#FFFFFF"
                }

                Rectangle {
                    x: sel.width / 2 - 11
                    y: -44
                    width: 22
                    height: 22
                    radius: 11
                    color: root.t.accent
                    border.width: 2
                    border.color: "#FFFFFF"
                    Accessible.role: Accessible.Button
                    Accessible.name: "Turn"

                    Icon {
                        anchors.centerIn: parent
                        size: 14
                        name: "rotate"
                        color: "#FFFFFF"
                        strokeWidth: 2.2
                    }

                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -8
                        cursorShape: Qt.PointingHandCursor
                        onPositionChanged: (mouse) => {
                            if (!pressed)
                                return;
                            const at = mapToItem(itemLayer, mouse.x, mouse.y);
                            const angle = Math.atan2(at.y - sel.centerY, at.x - sel.centerX) * 180 / Math.PI + 90;
                            simple.rotateCollageItem(sel.index, angle);
                        }
                        onReleased: simple.endCollageItemGesture()
                    }
                }

                // Size handle at the bottom-right corner: further from the middle is bigger.
                Rectangle {
                    x: sel.width - 9
                    y: sel.height - 9
                    width: 18
                    height: 18
                    radius: 9
                    color: "#FFFFFF"
                    border.width: 2
                    border.color: root.t.accent
                    Accessible.role: Accessible.Button
                    Accessible.name: "Resize"

                    MouseArea {
                        property real startSize
                        property real startDistance

                        anchors.fill: parent
                        anchors.margins: -8
                        cursorShape: Qt.SizeFDiagCursor
                        onPressed: (mouse) => {
                            const at = mapToItem(itemLayer, mouse.x, mouse.y);
                            startSize = sel.d.size;
                            startDistance = Math.max(4, Math.hypot(at.x - sel.centerX, at.y - sel.centerY));
                        }
                        onPositionChanged: (mouse) => {
                            if (!pressed)
                                return;
                            const at = mapToItem(itemLayer, mouse.x, mouse.y);
                            const distance = Math.hypot(at.x - sel.centerX, at.y - sel.centerY);
                            simple.resizeCollageItem(sel.index, startSize * distance / startDistance);
                        }
                        onReleased: simple.endCollageItemGesture()
                    }
                }
            }
        }
    }
}
