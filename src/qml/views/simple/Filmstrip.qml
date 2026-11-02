import QtQuick
import "Theme.js" as Theme

// The WhatsApp-style strip: always the whole source video, so a removed part is still visible
// where it used to be. Positions are source frames.
Item {
    id: strip

    readonly property var t: Theme.colors(simple.dark)
    readonly property int handleWidth: 14
    readonly property int frames: Math.max(1, simple.frames)
    readonly property bool cutTool: simple.tool === "cut"

    function xForFrame(frame) {
        return frame / frames * width;
    }
    function frameForX(x) {
        return Math.round(Math.max(0, Math.min(width, x)) / width * frames);
    }

    // Pictures
    Row {
        id: pictures
        anchors.fill: parent

        Repeater {
            model: simple.filmstrip

            delegate: Image {
                required property string modelData

                width: strip.width / Math.max(1, simple.filmstrip.length)
                height: pictures.height
                source: modelData
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                clip: true
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        z: -1
        radius: 10
        color: strip.t.chip
    }

    // Scrub by clicking or dragging anywhere on the strip.
    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onPressed: (mouse) => {
            simple.pausePreview();
            simple.seekSource(strip.frameForX(mouse.x));
        }
        onPositionChanged: (mouse) => {
            if (pressed)
                simple.seekSource(strip.frameForX(mouse.x));
        }
    }

    // Trimmed off
    Rectangle {
        x: 0
        width: strip.xForFrame(simple.trimStart)
        height: parent.height
        color: "#B3000000"
    }

    Rectangle {
        x: strip.xForFrame(simple.trimEnd)
        width: parent.width - x
        height: parent.height
        color: "#B3000000"
    }

    // Parts already removed
    Repeater {
        model: simple.cuts

        delegate: Rectangle {
            required property var modelData

            x: strip.xForFrame(modelData.start)
            width: strip.xForFrame(modelData.end) - x
            height: strip.height
            color: "#CC000000"
            border.width: 1
            border.color: "#E5484D"

            Icon {
                anchors.centerIn: parent
                visible: parent.width > 20
                size: 14
                name: "cut"
                color: "#FF8A8D"
                strokeWidth: 2.4
            }
        }
    }

    // The selection for Cut out
    Rectangle {
        id: selection

        visible: strip.cutTool && simple.hasSelection
        x: strip.xForFrame(simple.selectionStart)
        width: strip.xForFrame(simple.selectionEnd) - x
        height: parent.height
        color: "#59E5484D"
        border.width: 0

        Rectangle {
            width: parent.width
            height: 3
            color: "#E5484D"
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 3
            color: "#E5484D"
        }

        Rectangle {
            visible: parent.width > 76
            anchors.centerIn: parent
            width: cutRow.implicitWidth + 16
            height: 20
            radius: 6
            color: "#E5484D"

            Row {
                id: cutRow
                anchors.centerIn: parent
                spacing: 4

                Icon {
                    size: 12
                    name: "cut"
                    color: "#FFFFFF"
                    strokeWidth: 2.4
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    text: "Cut out"
                    color: "#FFFFFF"
                    font.family: "Inter"
                    font.pixelSize: 11
                    font.weight: Font.Bold
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        // Drag the middle to move it.
        MouseArea {
            property int grabFrame: 0
            property int grabStart: 0
            property int grabEnd: 0

            anchors.fill: parent
            cursorShape: Qt.SizeAllCursor
            onPressed: (mouse) => {
                grabFrame = strip.frameForX(mapToItem(strip, mouse.x, 0).x);
                grabStart = simple.selectionStart;
                grabEnd = simple.selectionEnd;
            }
            onPositionChanged: (mouse) => {
                if (!pressed)
                    return;
                var delta = strip.frameForX(mapToItem(strip, mouse.x, 0).x) - grabFrame;
                delta = Math.max(simple.trimStart - grabStart, Math.min(simple.trimEnd - grabEnd, delta));
                simple.setSelection(grabStart + delta, grabEnd + delta);
            }
            onReleased: simple.endSelectionDrag()
        }

        Rectangle {
            x: -5
            y: -3
            width: 10
            height: parent.height + 6
            radius: 4
            color: "#E5484D"

            MouseArea {
                anchors.fill: parent
                anchors.margins: -6
                cursorShape: Qt.SizeHorCursor
                onPositionChanged: (mouse) => {
                    if (pressed)
                        simple.setSelection(strip.frameForX(mapToItem(strip, mouse.x, 0).x), simple.selectionEnd);
                }
                onReleased: simple.endSelectionDrag()
            }
        }

        Rectangle {
            x: parent.width - 5
            y: -3
            width: 10
            height: parent.height + 6
            radius: 4
            color: "#E5484D"

            MouseArea {
                anchors.fill: parent
                anchors.margins: -6
                cursorShape: Qt.SizeHorCursor
                onPositionChanged: (mouse) => {
                    if (pressed)
                        simple.setSelection(simple.selectionStart, strip.frameForX(mapToItem(strip, mouse.x, 0).x));
                }
                onReleased: simple.endSelectionDrag()
            }
        }
    }

    // Yellow frame around what is kept
    Rectangle {
        x: strip.xForFrame(simple.trimStart)
        width: strip.xForFrame(simple.trimEnd) - x
        height: parent.height
        radius: 10
        color: "transparent"
        border.width: 3
        border.color: strip.t.trim
    }

    // Trim handles. They sit inside the kept area so they stay on the strip at the ends.
    Rectangle {
        id: startHandle

        x: strip.xForFrame(simple.trimStart)
        width: strip.handleWidth
        height: parent.height
        radius: 6
        color: strip.t.trim
        Accessible.role: Accessible.Slider
        Accessible.name: "Start of the video"

        Rectangle {
            anchors.centerIn: parent
            width: 2
            height: 18
            radius: 1
            color: strip.t.onTrim
        }

        MouseArea {
            anchors.fill: parent
            anchors.margins: -4
            cursorShape: Qt.SizeHorCursor
            onPressed: simple.beginTrimDrag()
            onPositionChanged: (mouse) => {
                if (pressed)
                    simple.dragTrim(strip.frameForX(mapToItem(strip, mouse.x, 0).x), simple.trimEnd);
            }
            onReleased: simple.endTrimDrag()
        }
    }

    Rectangle {
        id: endHandle

        x: strip.xForFrame(simple.trimEnd) - width
        width: strip.handleWidth
        height: parent.height
        radius: 6
        color: strip.t.trim
        Accessible.role: Accessible.Slider
        Accessible.name: "End of the video"

        Rectangle {
            anchors.centerIn: parent
            width: 2
            height: 18
            radius: 1
            color: strip.t.onTrim
        }

        MouseArea {
            anchors.fill: parent
            anchors.margins: -4
            cursorShape: Qt.SizeHorCursor
            onPressed: simple.beginTrimDrag()
            onPositionChanged: (mouse) => {
                if (pressed)
                    simple.dragTrim(simple.trimStart, strip.frameForX(mapToItem(strip, mouse.x, 0).x));
            }
            onReleased: simple.endTrimDrag()
        }
    }

    // Playhead
    Rectangle {
        x: strip.xForFrame(simple.playhead) - 1
        y: -4
        width: 3
        height: parent.height + 8
        radius: 2
        color: "#FFFFFF"
        border.width: 0.5
        border.color: "#66000000"
    }
}
