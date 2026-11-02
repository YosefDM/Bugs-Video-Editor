import QtQuick
import "Theme.js" as Theme

// Every plain button in Simple mode. Primary is the accent colour, danger is red.
Rectangle {
    id: root

    property string icon
    property string text
    property bool primary: false
    property bool danger: false
    property int fontSize: primary || danger ? 14 : 13
    readonly property var t: Theme.colors(simple.dark)

    signal clicked()

    implicitWidth: content.implicitWidth + (primary ? 44 : 28)
    implicitHeight: 40
    radius: 10
    color: danger ? t.danger : primary ? t.accent : mouse.containsMouse ? t.chip : "transparent"
    border.width: primary || danger ? 0 : 1
    border.color: t.softBorder
    opacity: enabled ? (mouse.containsMouse && (primary || danger) ? 0.9 : 1) : 0.4
    Accessible.role: Accessible.Button
    Accessible.name: text

    Row {
        id: content
        anchors.centerIn: parent
        spacing: 8

        Icon {
            visible: root.icon.length > 0
            size: 18
            name: root.icon
            strokeWidth: 2
            color: label.color
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            id: label
            text: root.text
            color: root.danger ? "#FFFFFF" : root.primary ? root.t.onAccent : root.t.text2
            font.family: "Inter"
            font.pixelSize: root.fontSize
            font.weight: root.primary || root.danger ? Font.DemiBold : Font.Medium
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.clicked()
    }
}
