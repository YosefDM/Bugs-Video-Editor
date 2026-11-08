import QtQuick
import QtQuick.Layouts
import "Theme.js" as Theme

// Play button, filmstrip and the row of tools, under the preview.
Rectangle {
    id: root

    readonly property var t: Theme.colors(simple.dark)
    readonly property bool showTimeline: simple.hasFile && !simple.isPhoto && !simple.collageActive

    color: t.bg

    // Jumps back or forward by a number of seconds, shown inside a round arrow.
    component SkipButton: Rectangle {
        id: skip

        property int seconds

        Layout.preferredWidth: 40
        Layout.preferredHeight: 40
        radius: 20
        color: skipArea.containsMouse ? root.t.chip : "transparent"
        Accessible.role: Accessible.Button
        Accessible.name: seconds < 0 ? "Back " + -seconds + " seconds" : "Forward " + seconds + " seconds"

        Icon {
            anchors.centerIn: parent
            size: 32
            name: skip.seconds < 0 ? "back" : "forward"
            color: root.t.text2
            strokeWidth: 1.6
        }

        Text {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: 2
            text: Math.abs(skip.seconds)
            color: root.t.text
            font.family: "Inter"
            font.pixelSize: 10
            font.weight: Font.Bold
        }

        MouseArea {
            id: skipArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: simple.skipSeconds(skip.seconds)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        anchors.topMargin: 12
        anchors.bottomMargin: 14
        spacing: 12

        Item {
            Layout.fillHeight: true
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.maximumWidth: 820
            Layout.alignment: Qt.AlignHCenter
            spacing: 8
            visible: root.showTimeline

            SkipButton {
                seconds: -10
            }

            Rectangle {
                Layout.preferredWidth: 48
                Layout.preferredHeight: 48
                radius: 24
                color: root.t.strong
                Accessible.role: Accessible.Button
                Accessible.name: simple.playing ? "Pause" : "Play"

                Icon {
                    anchors.centerIn: parent
                    name: simple.playing ? "pause" : "play"
                    filled: true
                    color: root.t.onStrong
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: simple.togglePlay()
                }
            }

            SkipButton {
                seconds: 30
            }

            Item {
                Layout.preferredWidth: 4
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Filmstrip {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                }

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: "0:00"
                        color: root.t.muted
                        font.family: "Inter"
                        font.pixelSize: 12
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Text {
                        textFormat: Text.RichText
                        text: '<b style="color:' + root.t.text + '">' + simple.positionText + "</b>"
                              + (simple.playing ? " playing" : "")
                        color: root.t.muted
                        font.family: "Inter"
                        font.pixelSize: 12
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Text {
                        text: simple.lengthText + " long"
                        color: root.t.muted
                        font.family: "Inter"
                        font.pixelSize: 12
                    }
                }
            }

            // Watch faster to find a moment. The saved video stays normal speed.
            Rectangle {
                Layout.preferredWidth: 52
                Layout.preferredHeight: 32
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: 6
                radius: 16
                color: simple.speed > 1 ? root.t.accentSoft : root.t.chip
                border.width: 1.5
                border.color: simple.speed > 1 ? root.t.accent : root.t.softBorder
                Accessible.role: Accessible.Button
                Accessible.name: "Speed " + simple.speed + " times"

                Text {
                    anchors.centerIn: parent
                    text: simple.speed + "×"
                    color: simple.speed > 1 ? root.t.accent : root.t.text
                    font.family: "Inter"
                    font.pixelSize: 13
                    font.weight: Font.Bold
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: simple.cycleSpeed()
                }
            }
        }

        // Tools. A photo only offers the ones that make sense for it.
        Text {
            visible: simple.collageActive
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: simple.collageSelected >= 0 ? "Now tap the photo to swap it with."
                                              : "Tap a photo, then another, to swap them. Finish saves the collage."
            wrapMode: Text.WordWrap
            color: root.t.muted
            font.family: "Inter"
            font.pixelSize: 14
        }

        Row {
            id: toolRow

            visible: !simple.collageActive

            readonly property var tools: simple.isPhoto
                ? [{ id: "crop", label: "Crop", icon: "crop" },
                   { id: "caption", label: "Caption", icon: "caption" },
                   { id: "text", label: "Text", icon: "text" }]
                : [{ id: "trim", label: "Trim", icon: "trim" },
                   { id: "cut", label: "Cut out", icon: "cut" },
                   { id: "crop", label: "Crop", icon: "crop" },
                   { id: "caption", label: "Caption", icon: "caption" },
                   { id: "text", label: "Text", icon: "text" },
                   { id: "frame", label: "Save picture", icon: "frame" }]

            Layout.fillWidth: true
            Layout.maximumWidth: 820
            Layout.alignment: Qt.AlignHCenter
            spacing: 8

            Repeater {
                model: toolRow.tools

                delegate: Rectangle {
                    required property var modelData
                    readonly property bool selected: simple.tool === modelData.id

                    width: (toolRow.width - toolRow.spacing * (toolRow.tools.length - 1)) / toolRow.tools.length
                    height: 64
                    radius: 14
                    color: selected ? root.t.accentSoft : root.t.panel
                    border.width: 1.5
                    border.color: selected ? root.t.accent : root.t.border
                    opacity: simple.hasFile ? 1 : 0.5
                    Accessible.role: Accessible.Button
                    Accessible.name: modelData.label

                    Column {
                        anchors.centerIn: parent
                        spacing: 5

                        Icon {
                            size: 22
                            name: modelData.icon
                            color: label.color
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        Text {
                            id: label
                            // In a narrow window "Save picture" goes onto two lines, not over
                            // its neighbour.
                            width: Math.min(implicitWidth, parent.parent.width - 8)
                            text: modelData.label
                            color: parent.parent.selected ? root.t.accent : root.t.text2
                            font.family: "Inter"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            // A single long word, like "Caption", gets a little smaller instead.
                            fontSizeMode: Text.HorizontalFit
                            minimumPixelSize: 10
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: simple.hasFile ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: simple.tool = modelData.id
                    }
                }
            }
        }
    }
}
