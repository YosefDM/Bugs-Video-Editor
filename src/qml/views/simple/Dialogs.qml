import QtQuick
import QtQuick.Layouts
import "Theme.js" as Theme

// Dialogs shown over Simple mode, in its own style rather than the system's.
Rectangle {
    id: root

    readonly property var t: Theme.colors(simple.dark)

    color: "#99000000"
    focus: true
    Keys.onEscapePressed: simple.dismissDialog()

    // Clicks outside the card do nothing, like any dialog.
    MouseArea {
        anchors.fill: parent
    }

    component Card: Rectangle {
        default property alias content: column.data
        property alias spacing: column.spacing

        anchors.centerIn: parent
        width: Math.min(root.width - 48, 440)
        height: column.implicitHeight + 48
        radius: 16
        color: root.t.panel
        border.width: 1
        border.color: root.t.border
        Accessible.role: Accessible.Dialog

        ColumnLayout {
            id: column

            anchors.fill: parent
            anchors.margins: 24
            spacing: 16
        }
    }

    component Body: Text {
        Layout.fillWidth: true
        textFormat: Text.StyledText
        wrapMode: Text.WordWrap
        lineHeight: 1.25
        color: root.t.text2
        linkColor: root.t.accent
        font.family: "Inter"
        font.pixelSize: 13
        onLinkActivated: (link) => Qt.openUrlExternally(link)

        HoverHandler {
            cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
        }
    }

    // Closing with changes that were never finished
    Card {
        visible: simple.dialog === "close"
        Accessible.name: closeTitle.text

        Rectangle {
            Layout.preferredWidth: 44
            Layout.preferredHeight: 44
            radius: 22
            color: root.t.accentSoft

            Icon {
                anchors.centerIn: parent
                size: 22
                name: "shield"
                color: root.t.accent
            }
        }

        Text {
            id: closeTitle

            Layout.fillWidth: true
            text: "Close without finishing?"
            wrapMode: Text.WordWrap
            color: root.t.text
            font.family: "Inter"
            font.pixelSize: 18
            font.weight: Font.DemiBold
        }

        Body {
            text: "Your changes haven't been saved. To keep them, go back and press <b>Finish</b>.<br><br>Your original stays untouched either way."
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            spacing: 8

            ActionButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                text: "Close anyway"
                onClicked: simple.closeAnyway()
            }

            ActionButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                text: "Keep editing"
                primary: true
                onClicked: simple.dismissDialog()
            }
        }
    }

    // About
    Card {
        visible: simple.dialog === "about"
        Accessible.name: "About BUGS! Video Editor"
        spacing: 14

        Row {
            spacing: 12

            Image {
                width: 48
                height: 48
                source: "assets/" + root.t.mark
                sourceSize: Qt.size(96, 96)
                fillMode: Image.PreserveAspectFit
                anchors.verticalCenter: parent.verticalCenter
            }

            Column {
                spacing: 4
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    textFormat: Text.RichText
                    text: 'BUGS<span style="color:#E5392B">!</span> Video Editor'
                    color: root.t.text
                    font.family: "Outfit"
                    font.weight: Font.ExtraBold
                    font.pixelSize: 22
                }

                Text {
                    text: "Version " + Qt.application.version + " · Created by Yosef"
                    color: root.t.muted
                    font.family: "Inter"
                    font.pixelSize: 12
                }
            }
        }

        Body {
            text: "A simple editor for your videos and photos. It works offline, needs no account, and never changes your originals."
        }

        // The open-source credit and license notice, kept small.
        Body {
            color: root.t.muted
            font.pixelSize: 11
            text: "Built with open-source software, including <a href=\"https://www.shotcut.org/\">Shotcut</a> by Meltytech, LLC, MLT, FFmpeg and Qt. Free software under the <a href=\"https://www.gnu.org/licenses/gpl-3.0.html\">GNU General Public License v3</a>."
        }

        ActionButton {
            Layout.alignment: Qt.AlignRight
            Layout.topMargin: 4
            Layout.preferredWidth: 120
            Layout.preferredHeight: 44
            text: "Close"
            primary: true
            onClicked: simple.dismissDialog()
        }
    }
}
