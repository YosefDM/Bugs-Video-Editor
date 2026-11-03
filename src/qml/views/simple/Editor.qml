import QtQuick
import QtQuick.Layouts
import "Theme.js" as Theme

// Play button, filmstrip and the row of tools, under the preview.
Rectangle {
    id: root

    readonly property var t: Theme.colors(simple.dark)
    readonly property bool showTimeline: simple.hasFile && !simple.isPhoto && !simple.collageActive

    color: t.bg

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
            spacing: 14
            visible: root.showTimeline

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
                            text: modelData.label
                            color: parent.parent.selected ? root.t.accent : root.t.text2
                            font.family: "Inter"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
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
