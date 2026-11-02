import QtQuick
import QtQuick.Layouts
import "Theme.js" as Theme

// First launch only. Once there is something in the library the app opens straight to it.
Rectangle {
    id: root

    readonly property var t: Theme.colors(simple.dark)

    color: t.bg

    DropArea {
        id: drop

        anchors.fill: parent
        onDropped: (event) => {
            simple.addUrls(event.urls);
            event.accept();
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 560)
        spacing: 24

        Image {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 112
            Layout.preferredHeight: 112
            source: "assets/" + root.t.mark
            sourceSize: Qt.size(224, 224)
            fillMode: Image.PreserveAspectFit
        }

        Column {
            Layout.alignment: Qt.AlignHCenter
            spacing: 6

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                textFormat: Text.RichText
                text: 'BUGS<span style="color:#E5392B">!</span>'
                color: root.t.text
                font.family: "Outfit"
                font.weight: Font.ExtraBold
                font.pixelSize: 44
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "VIDEO EDITOR"
                color: root.t.muted
                font.family: "Outfit"
                font.weight: Font.Bold
                font.pixelSize: 12
                font.letterSpacing: 2
            }
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "Add your videos and photos"
            color: root.t.text
            font.family: "Inter"
            font.pixelSize: 28
            font.weight: Font.Bold
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 160
            radius: 16
            color: addMouse.containsMouse || drop.containsDrag ? root.t.accentSoft : root.t.drop
            border.width: 1.5
            border.color: addMouse.containsMouse || drop.containsDrag ? root.t.accent : root.t.dashed
            Accessible.role: Accessible.Button
            Accessible.name: "Add videos or photos"

            Column {
                anchors.centerIn: parent
                spacing: 12

                Icon {
                    size: 36
                    name: "upload"
                    color: root.t.text2
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Add videos or photos"
                    color: root.t.text
                    font.family: "Inter"
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "or drop them here"
                    color: root.t.muted
                    font.family: "Inter"
                    font.pixelSize: 13
                }
            }

            MouseArea {
                id: addMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: simple.addFiles()
            }
        }

        Row {
            Layout.alignment: Qt.AlignHCenter
            spacing: 8

            Icon {
                size: 16
                name: "lock"
                color: root.t.muted
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                text: "Works offline · No account needed"
                color: root.t.muted
                font.family: "Inter"
                font.pixelSize: 13
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
}
