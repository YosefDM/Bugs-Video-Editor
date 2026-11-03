import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Theme.js" as Theme

Rectangle {
    id: root

    readonly property var t: Theme.colors(simple.dark)
    readonly property bool onOriginals: simple.tab === "originals"

    color: t.panel

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: root.t.border
    }

    DropArea {
        anchors.fill: parent
        onDropped: (drop) => {
            simple.addUrls(drop.urls);
            drop.accept();
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // Originals / Finished
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            radius: 12
            color: root.t.seg

            Row {
                anchors.fill: parent
                anchors.margins: 3
                spacing: 2

                Repeater {
                    model: [
                        { id: "originals", label: "Originals", count: simple.originalsCount },
                        { id: "finished", label: "Finished", count: simple.finishedCount }
                    ]

                    delegate: Rectangle {
                        required property var modelData
                        readonly property bool selected: simple.tab === modelData.id

                        width: (parent.width - 2) / 2
                        height: parent.height
                        radius: 9
                        color: selected ? root.t.segActive : "transparent"

                        Row {
                            anchors.centerIn: parent
                            spacing: 6

                            Text {
                                text: modelData.label
                                color: selected ? root.t.text : root.t.muted
                                font.family: "Inter"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Rectangle {
                                width: Math.max(20, countLabel.implicitWidth + 12)
                                height: 20
                                radius: 10
                                color: root.t.countBg
                                anchors.verticalCenter: parent.verticalCenter

                                Text {
                                    id: countLabel
                                    anchors.centerIn: parent
                                    text: modelData.count
                                    color: root.t.text2
                                    font.family: "Inter"
                                    font.pixelSize: 11
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: simple.tab = modelData.id
                        }
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: root.onOriginals ? "Your untouched files. Pick one to start editing."
                                   : "Everything you finished. Ready to share or send."
            wrapMode: Text.WordWrap
            lineHeight: 1.2
            color: root.t.muted
            font.family: "Inter"
            font.pixelSize: 12
        }

        // Add, or find the Finished folder to share from.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.onOriginals ? 64 : 44
            radius: 12
            color: addMouse.containsMouse ? root.t.chip : root.t.drop
            border.width: root.onOriginals ? 1.5 : 1
            border.color: root.onOriginals ? root.t.dashed : root.t.softBorder

            Row {
                anchors.centerIn: parent
                spacing: 10

                Icon {
                    name: root.onOriginals ? "upload" : "folder"
                    color: root.t.text2
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    text: root.onOriginals ? "Add videos or photos" : "Open Finished folder"
                    color: root.t.text2
                    font.family: "Inter"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            MouseArea {
                id: addMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.onOriginals ? simple.addFiles() : simple.showFinishedFolder()
            }
        }

        // Collage: start picking, then the picking bar
        ActionButton {
            visible: root.onOriginals && !simple.collagePicking && !simple.collageActive
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            icon: "collage"
            text: "Make a collage"
            onClicked: simple.startCollage()
        }

        Rectangle {
            visible: simple.collagePicking
            Layout.fillWidth: true
            Layout.preferredHeight: pickColumn.implicitHeight + 24
            radius: 12
            color: root.t.accentSoft
            border.width: 1
            border.color: root.t.accent

            ColumnLayout {
                id: pickColumn

                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                Text {
                    Layout.fillWidth: true
                    text: simple.collagePickCount < 2
                          ? "Tap the photos for your collage. Pick at least two."
                          : simple.collagePickCount + " photos picked. Tap more, or make it."
                    wrapMode: Text.WordWrap
                    color: root.t.text
                    font.family: "Inter"
                    font.pixelSize: 13
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    ActionButton {
                        Layout.fillWidth: true
                        text: "Cancel"
                        onClicked: simple.leaveCollage()
                    }

                    ActionButton {
                        Layout.fillWidth: true
                        text: "Make collage"
                        primary: true
                        enabled: simple.collagePickCount >= 2
                        onClicked: simple.makeCollage()
                    }
                }
            }
        }

        // All / Videos / Photos
        Row {
            spacing: 6

            Repeater {
                model: [
                    { id: "all", label: "All" },
                    { id: "videos", label: "Videos" },
                    { id: "photos", label: "Photos" }
                ]

                delegate: Rectangle {
                    required property var modelData
                    readonly property bool selected: simple.filter === modelData.id

                    width: filterLabel.implicitWidth + 28
                    height: 32
                    radius: 16
                    color: selected ? root.t.strong : "transparent"
                    border.width: 1
                    border.color: selected ? root.t.strong : root.t.softBorder

                    Text {
                        id: filterLabel
                        anchors.centerIn: parent
                        text: modelData.label
                        color: parent.selected ? root.t.onStrong : root.t.text2
                        font.family: "Inter"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: simple.filter = modelData.id
                    }
                }
            }
        }

        GridView {
            id: grid

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            cellWidth: width / 2
            cellHeight: (width / 2 - 12) * 10 / 16 + 34
            boundsBehavior: Flickable.StopAtBounds
            model: simple.items

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: Item {
                id: cell

                required property var modelData

                width: grid.cellWidth
                height: grid.cellHeight

                Column {
                    width: parent.width - 12
                    spacing: 6

                    Rectangle {
                        width: parent.width
                        height: width * 10 / 16
                        radius: 9
                        color: root.t.drop
                        border.width: cell.modelData.selected ? 3 : 0
                        border.color: root.t.accent
                        clip: true

                        Image {
                            anchors.fill: parent
                            anchors.margins: cell.modelData.selected ? 3 : 0
                            source: cell.modelData.thumb
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            opacity: cell.modelData.missing || (simple.collagePicking && !cell.modelData.pickable) ? 0.3 : 1
                        }

                        Rectangle {
                            visible: cell.modelData.badge.length > 0
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: 6
                            width: badge.implicitWidth + 12
                            height: 18
                            radius: 5
                            color: "#D90B0C0E"

                            Text {
                                id: badge
                                anchors.centerIn: parent
                                text: cell.modelData.badge
                                color: "#FFFFFF"
                                font.family: "Inter"
                                font.pixelSize: 11
                            }
                        }

                        // Collage pick number
                        Rectangle {
                            visible: cell.modelData.pick > 0
                            x: 6
                            y: 6
                            width: 26
                            height: 26
                            radius: 13
                            color: root.t.accent
                            z: 3

                            Text {
                                anchors.centerIn: parent
                                text: cell.modelData.pick
                                color: root.t.onAccent
                                font.family: "Inter"
                                font.pixelSize: 13
                                font.weight: Font.Bold
                            }
                        }

                        Rectangle {
                            visible: cell.modelData.done
                            x: 6
                            y: 6
                            width: 22
                            height: 22
                            radius: 11
                            color: root.t.done

                            Icon {
                                anchors.centerIn: parent
                                size: 13
                                name: "check"
                                color: "#FFFFFF"
                                strokeWidth: 3
                            }
                        }

                        // Takes it off this list only. The file itself stays where it is.
                        Rectangle {
                            visible: !cell.modelData.done && cellMouse.containsMouse && !simple.collagePicking
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 6
                            width: 22
                            height: 22
                            radius: 11
                            color: "#D90B0C0E"
                            z: 2

                            Icon {
                                anchors.centerIn: parent
                                size: 12
                                name: "close"
                                color: "#FFFFFF"
                                strokeWidth: 3
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: simple.removeFromLibrary(cell.modelData.path)
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        text: cell.modelData.name
                        color: root.t.text
                        font.family: "Inter"
                        font.pixelSize: 12
                        font.weight: Font.Medium
                        elide: Text.ElideMiddle
                    }
                }

                MouseArea {
                    id: cellMouse
                    anchors.fill: parent
                    z: -1
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: simple.collagePicking ? simple.toggleCollagePick(cell.modelData.path)
                                                     : simple.openFile(cell.modelData.path)
                }
            }
        }
    }
}
