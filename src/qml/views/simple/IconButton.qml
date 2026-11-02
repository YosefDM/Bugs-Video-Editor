import QtQuick
import "Theme.js" as Theme

Rectangle {
    id: root

    property string icon
    property string label
    property bool filled: false
    readonly property var t: Theme.colors(simple.dark)

    signal clicked()

    width: 40
    height: 40
    implicitWidth: 40
    implicitHeight: 40
    radius: 10
    color: mouse.containsMouse && enabled ? t.chip : "transparent"
    opacity: enabled ? 1 : 0.35
    Accessible.role: Accessible.Button
    Accessible.name: label

    Icon {
        anchors.centerIn: parent
        name: root.icon
        color: root.t.text2
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.clicked()
    }
}
