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
    readonly property bool hideTool: simple.tool === "hide"
    readonly property color hideColor: "#8A4FD8"
    // The yellow frame and handles only show while trimming. Elsewhere the trimmed-off ends just
    // stay dimmed.
    readonly property bool trimTool: simple.tool === "trim"

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

    // Hidden parts: blacked out or blurred while the sound plays on
    Repeater {
        model: simple.hides

        delegate: Rectangle {
            id: band

            required property var modelData
            required property int index
            readonly property bool selected: strip.hideTool && index === simple.selectedHide

            x: strip.xForFrame(modelData.start)
            width: Math.max(4, strip.xForFrame(modelData.end) - x)
            height: strip.height
            color: selected ? "#738A4FD8" : "#4D8A4FD8"
            border.width: selected ? 0 : 1
            border.color: strip.hideColor

            Rectangle {
                visible: band.selected
                width: parent.width
                height: 3
                color: strip.hideColor
            }

            Rectangle {
                visible: band.selected
                anchors.bottom: parent.bottom
                width: parent.width
                height: 3
                color: strip.hideColor
            }

            Icon {
                anchors.centerIn: parent
                visible: parent.width > 22
                size: 14
                name: "hide"
                color: "#FFFFFF"
                strokeWidth: 2.2
            }

            // In Hide, tap one to pick it, drag the middle to move it.
            MouseArea {
                property int grabFrame: 0
                property int grabStart: 0
                property int grabEnd: 0

                anchors.fill: parent
                enabled: strip.hideTool
                cursorShape: band.selected ? Qt.SizeAllCursor : Qt.PointingHandCursor
                onPressed: (mouse) => {
                    if (!band.selected)
                        simple.selectHide(band.index);
                    grabFrame = strip.frameForX(mapToItem(strip, mouse.x, 0).x);
                    grabStart = band.modelData.start;
                    grabEnd = band.modelData.end;
                }
                onPositionChanged: (mouse) => {
                    if (!pressed)
                        return;
                    var delta = strip.frameForX(mapToItem(strip, mouse.x, 0).x) - grabFrame;
                    delta = Math.max(simple.trimStart - grabStart, Math.min(simple.trimEnd - grabEnd, delta));
                    simple.setHideTime(grabStart + delta, grabEnd + delta);
                }
                onReleased: simple.endHideTimeDrag()
            }

            Rectangle {
                visible: band.selected
                x: -5
                y: -3
                width: 10
                height: parent.height + 6
                radius: 4
                color: strip.hideColor

                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -6
                    cursorShape: Qt.SizeHorCursor
                    onPositionChanged: (mouse) => {
                        if (pressed)
                            simple.setHideTime(strip.frameForX(mapToItem(strip, mouse.x, 0).x), band.modelData.end);
                    }
                    onReleased: simple.endHideTimeDrag()
                }
            }

            Rectangle {
                visible: band.selected
                x: parent.width - 5
                y: -3
                width: 10
                height: parent.height + 6
                radius: 4
                color: strip.hideColor

                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -6
                    cursorShape: Qt.SizeHorCursor
                    onPositionChanged: (mouse) => {
                        if (pressed)
                            simple.setHideTime(band.modelData.start, strip.frameForX(mapToItem(strip, mouse.x, 0).x));
                    }
                    onReleased: simple.endHideTimeDrag()
                }
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
        visible: strip.trimTool
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
        visible: strip.trimTool

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
            enabled: strip.trimTool
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
        visible: strip.trimTool

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
            enabled: strip.trimTool
            cursorShape: Qt.SizeHorCursor
            onPressed: simple.beginTrimDrag()
            onPositionChanged: (mouse) => {
                if (pressed)
                    simple.dragTrim(simple.trimStart, strip.frameForX(mapToItem(strip, mouse.x, 0).x));
            }
            onReleased: simple.endTrimDrag()
        }
    }

    // Playhead. Its handle stands above the strip, clear of the red box and the trim handles, so
    // it can always be dragged on its own.
    Item {
        id: playhead

        x: strip.xForFrame(simple.playhead) - width / 2
        y: -24
        width: 22
        height: strip.height + 28
        Accessible.role: Accessible.Slider
        Accessible.name: "Where the video is"

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 18
            width: 3
            height: parent.height - 18
            radius: 2
            color: "#FFFFFF"
            border.width: 0.5
            border.color: "#66000000"
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 18
            height: 18
            radius: 9
            color: "#FFFFFF"
            border.width: 1.5
            border.color: "#80000000"
        }

        MouseArea {
            // Only the handle above the strip, so the strip, the red box and the purple parts
            // below still take their own drags.
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 22
            anchors.leftMargin: -8
            anchors.rightMargin: -8
            anchors.topMargin: -6
            cursorShape: Qt.SizeHorCursor
            onPressed: (mouse) => {
                simple.pausePreview();
                simple.seekSource(strip.frameForX(mapToItem(strip, mouse.x, 0).x));
            }
            onPositionChanged: (mouse) => {
                if (pressed)
                    simple.seekSource(strip.frameForX(mapToItem(strip, mouse.x, 0).x));
            }
        }
    }
}
