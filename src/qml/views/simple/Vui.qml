import QtQuick
import Shotcut.Controls as Shotcut

// Drawn on top of the video itself: the crop box, confirmation messages and the empty state.
Shotcut.VuiBase {
    id: root

    readonly property real frameX: video.rect.x
    readonly property real frameY: video.rect.y
    readonly property real frameW: video.rect.width
    readonly property real frameH: video.rect.height
    readonly property real cropX: simple.cropRect.x * frameW
    readonly property real cropY: simple.cropRect.y * frameH
    readonly property real cropW: simple.cropRect.width * frameW
    readonly property real cropH: simple.cropRect.height * frameH
    readonly property real minSize: 0.05

    onDropped: (drop) => {
        simple.addUrls(drop.urls);
        drop.accept();
    }

    Text {
        visible: !simple.hasFile
        anchors.centerIn: parent
        text: simple.loading ? "Opening…" : "Your video or photo will show here"
        color: "#9A9EA8"
        font.family: "Inter"
        font.pixelSize: 15
    }

    // Everything outside the crop is dimmed.
    Item {
        id: cropLayer

        visible: simple.hasFile && simple.tool === "crop"
        x: root.frameX
        y: root.frameY
        width: root.frameW
        height: root.frameH

        Rectangle {
            width: parent.width
            height: root.cropY
            color: "#80000000"
        }

        Rectangle {
            y: root.cropY + root.cropH
            width: parent.width
            height: parent.height - y
            color: "#80000000"
        }

        Rectangle {
            y: root.cropY
            width: root.cropX
            height: root.cropH
            color: "#80000000"
        }

        Rectangle {
            x: root.cropX + root.cropW
            y: root.cropY
            width: parent.width - x
            height: root.cropH
            color: "#80000000"
        }

        Rectangle {
            id: box

            x: root.cropX
            y: root.cropY
            width: root.cropW
            height: root.cropH
            color: "transparent"
            border.width: 2
            border.color: "#FFFFFF"

            Repeater {
                model: [1 / 3, 2 / 3]

                delegate: Rectangle {
                    required property real modelData

                    x: box.width * modelData
                    width: 1
                    height: box.height
                    color: "#59FFFFFF"
                }
            }

            Repeater {
                model: [1 / 3, 2 / 3]

                delegate: Rectangle {
                    required property real modelData

                    y: box.height * modelData
                    width: box.width
                    height: 1
                    color: "#59FFFFFF"
                }
            }

            // Drag the middle to move the box.
            MouseArea {
                property point grab
                property rect start

                anchors.fill: parent
                cursorShape: Qt.SizeAllCursor
                onPressed: (mouse) => {
                    grab = mapToItem(cropLayer, mouse.x, mouse.y);
                    start = simple.cropRect;
                }
                onPositionChanged: (mouse) => {
                    if (!pressed)
                        return;
                    var at = mapToItem(cropLayer, mouse.x, mouse.y);
                    var x = start.x + (at.x - grab.x) / root.frameW;
                    var y = start.y + (at.y - grab.y) / root.frameH;
                    x = Math.max(0, Math.min(1 - start.width, x));
                    y = Math.max(0, Math.min(1 - start.height, y));
                    simple.setCropFromDrag(x, y, start.width, start.height);
                }
                onReleased: simple.endCropDrag()
            }
        }

        // Corners. xSide and ySide say which way each one pulls.
        Repeater {
            model: [
                { xSide: 0, ySide: 0 },
                { xSide: 1, ySide: 0 },
                { xSide: 0, ySide: 1 },
                { xSide: 1, ySide: 1 }
            ]

            delegate: Item {
                id: corner

                required property var modelData

                x: root.cropX + (modelData.xSide ? root.cropW : 0) - 22
                y: root.cropY + (modelData.ySide ? root.cropH : 0) - 22
                width: 44
                height: 44

                Rectangle {
                    x: corner.modelData.xSide ? 22 - 18 + 4 - 4 : 18
                    y: corner.modelData.ySide ? 22 + 4 - 4 : 18
                    width: 22
                    height: 4
                    color: "#FFFFFF"
                    transform: Translate { x: corner.modelData.xSide ? -4 : 0 }
                }

                Rectangle {
                    x: corner.modelData.xSide ? 22 : 18
                    y: corner.modelData.ySide ? 22 - 18 + 4 - 4 : 18
                    width: 4
                    height: 22
                    color: "#FFFFFF"
                    transform: Translate { y: corner.modelData.ySide ? -4 : 0 }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: corner.modelData.xSide === corner.modelData.ySide ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor
                    onPositionChanged: (mouse) => {
                        if (!pressed)
                            return;
                        var at = mapToItem(cropLayer, mouse.x, mouse.y);
                        var r = simple.cropRect;
                        // The opposite corner stays where it is.
                        var anchorX = corner.modelData.xSide ? r.x : r.x + r.width;
                        var anchorY = corner.modelData.ySide ? r.y : r.y + r.height;
                        var px = Math.max(0, Math.min(1, at.x / root.frameW));
                        var py = Math.max(0, Math.min(1, at.y / root.frameH));
                        var w = Math.max(root.minSize, corner.modelData.xSide ? px - anchorX : anchorX - px);
                        var h = Math.max(root.minSize, corner.modelData.ySide ? py - anchorY : anchorY - py);
                        var maxW = corner.modelData.xSide ? 1 - anchorX : anchorX;
                        var maxH = corner.modelData.ySide ? 1 - anchorY : anchorY;
                        w = Math.min(w, maxW);
                        h = Math.min(h, maxH);
                        if (simple.cropAspect > 0) {
                            // Fractions of a picture that is pictureAspect wide per one tall.
                            var ratio = simple.cropAspect / simple.pictureAspect;
                            if (w / h > ratio)
                                w = h * ratio;
                            else
                                h = w / ratio;
                        }
                        simple.setCropFromDrag(corner.modelData.xSide ? anchorX : anchorX - w,
                                               corner.modelData.ySide ? anchorY : anchorY - h,
                                               w, h);
                    }
                    onReleased: simple.endCropDrag()
                }
            }
        }
    }

    // Confirmation message
    Rectangle {
        visible: simple.toast.length > 0
        anchors.top: parent.top
        anchors.topMargin: 14
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width - 24, toastRow.implicitWidth + 28)
        height: 38
        radius: 10
        color: "#F016181D"
        Accessible.role: Accessible.AlertMessage
        Accessible.name: simple.toast

        Row {
            id: toastRow

            anchors.centerIn: parent
            spacing: 8

            Icon {
                size: 16
                name: simple.toastIsError ? "info" : "check"
                strokeWidth: 2.4
                color: simple.toastIsError ? "#FF8A8D" : "#5FD3A8"
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                text: simple.toast
                color: "#FFFFFF"
                font.family: "Inter"
                font.pixelSize: 13
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
}
