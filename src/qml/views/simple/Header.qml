import QtQuick
import QtQuick.Layouts
import "Theme.js" as Theme

Rectangle {
    id: root

    readonly property var t: Theme.colors(simple.dark)

    color: t.bar

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: root.t.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 12

        Image {
            Layout.preferredWidth: 38
            Layout.preferredHeight: 38
            source: "assets/" + root.t.mark
            sourceSize: Qt.size(76, 76)
            fillMode: Image.PreserveAspectFit
        }

        Text {
            textFormat: Text.RichText
            text: 'BUGS<span style="color:#E5392B">!</span>'
            color: root.t.text
            font.family: "Outfit"
            font.weight: Font.ExtraBold
            font.pixelSize: 22
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.preferredHeight: 24
            color: root.t.border
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 60
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: simple.collageActive ? "Collage of " + simple.collageCells.length + " photos"
                      : simple.collagePicking ? "Pick photos for your collage"
                      : simple.hasFile ? "Editing: " + simple.fileName : "Pick a video or photo to start"
                color: root.t.text
                font.family: "Inter"
                font.pixelSize: 14
                font.weight: Font.DemiBold
                elide: Text.ElideMiddle
            }

            Row {
                spacing: 5

                Icon {
                    size: 13
                    name: "shield"
                    color: root.t.muted
                    strokeWidth: 2
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    text: "Original stays untouched"
                    color: root.t.muted
                    font.family: "Inter"
                    font.pixelSize: 12
                }
            }
        }

        IconButton {
            icon: "undo"
            label: "Undo"
            enabled: simple.canUndo
            onClicked: simple.undo()
        }

        IconButton {
            icon: "redo"
            label: "Redo"
            enabled: simple.canRedo
            onClicked: simple.redo()
        }

        IconButton {
            icon: simple.dark ? "sun" : "moon"
            label: simple.dark ? "Switch to light mode" : "Switch to dark mode"
            filled: true
            onClicked: simple.dark = !simple.dark
        }

        IconButton {
            icon: "info"
            label: "About BUGS!"
            onClicked: simple.showAbout()
        }

        ActionButton {
            icon: "combine"
            text: "Combine clips"
            enabled: !simple.saving
            onClicked: simple.requestAdvanced()
        }

        // Finish, which becomes progress and a way to stop while saving.
        ActionButton {
            visible: !simple.saving
            icon: "check"
            text: "Finish"
            primary: true
            enabled: (simple.hasFile || simple.collageActive) && !simple.loading
            onClicked: simple.finish()
        }

        Row {
            id: savingRow
            visible: simple.saving
            spacing: 8

            // Stop sits where Finish was, so the second click of a double-click must not stop.
            property double shownAt: 0
            onVisibleChanged: if (visible) shownAt = Date.now()

            Rectangle {
                width: 150
                height: 40
                radius: 10
                color: root.t.chip
                clip: true

                Rectangle {
                    width: parent.width * simple.saveProgress / 100
                    height: parent.height
                    color: root.t.accent
                    opacity: 0.35
                }

                Text {
                    anchors.centerIn: parent
                    text: simple.saveProgress > 0 ? "Saving… " + simple.saveProgress + "%" : "Saving…"
                    color: root.t.text
                    font.family: "Inter"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }
            }

            ActionButton {
                text: "Stop"
                onClicked: {
                    if (Date.now() - savingRow.shownAt > 1000)
                        simple.cancelFinish()
                }
            }
        }
    }
}
