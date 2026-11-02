import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Theme.js" as Theme

// The right panel: only the options for the selected tool, in plain words.
Rectangle {
    id: root

    readonly property var t: Theme.colors(simple.dark)

    color: t.panel

    Rectangle {
        width: 1
        height: parent.height
        color: root.t.border
    }

    component Heading: ColumnLayout {
        property string title
        property string body

        Layout.fillWidth: true
        spacing: 4

        Text {
            text: parent.title
            color: root.t.text
            font.family: "Inter"
            font.pixelSize: 17
            font.weight: Font.DemiBold
        }

        Text {
            Layout.fillWidth: true
            text: parent.body
            wrapMode: Text.WordWrap
            lineHeight: 1.2
            color: root.t.muted
            font.family: "Inter"
            font.pixelSize: 13
        }
    }

    component Field: ColumnLayout {
        id: field

        property string title
        property alias text: input.text
        property alias input: input
        property bool strong: false

        signal edited()
        signal committed()

        Layout.fillWidth: true
        spacing: 6

        Text {
            text: field.title
            color: root.t.text2
            font.family: "Inter"
            font.pixelSize: 13
        }

        TextField {
            id: input

            Layout.fillWidth: true
            Layout.preferredHeight: field.strong ? 44 : 40
            color: root.t.text
            selectByMouse: true
            selectedTextColor: root.t.onAccent
            selectionColor: root.t.accent
            leftPadding: 12
            rightPadding: 12
            font.family: "Inter"
            font.pixelSize: field.strong ? 15 : 14
            background: Rectangle {
                radius: 10
                color: root.t.drop
                border.width: input.activeFocus ? 1.5 : 1
                border.color: input.activeFocus ? root.t.accent : root.t.softBorder
            }
            onTextEdited: field.edited()
            onEditingFinished: field.committed()
        }
    }

    ScrollView {
        id: scroll

        anchors.fill: parent
        anchors.bottomMargin: note.height + 28
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: scroll.availableWidth
            spacing: 16

            Item {
                Layout.preferredHeight: 4
            }

            // Nothing open yet
            Text {
                visible: !simple.hasFile
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                text: "Pick a video or photo from the left to start."
                wrapMode: Text.WordWrap
                color: root.t.muted
                font.family: "Inter"
                font.pixelSize: 14
            }

            // Trim
            ColumnLayout {
                visible: simple.hasFile && simple.tool === "trim"
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                spacing: 16

                Heading {
                    title: "Trim"
                    body: "Drag the yellow handles on the strip to cut off the start or end."
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Field {
                        id: startField
                        title: "Starts at"
                        onCommitted: {
                            simple.setTrimStartText(text);
                            text = Qt.binding(() => simple.trimStartText);
                        }
                        Binding on text {
                            value: simple.trimStartText
                            when: !startField.input.activeFocus
                        }
                    }

                    Field {
                        id: endField
                        title: "Ends at"
                        onCommitted: {
                            simple.setTrimEndText(text);
                            text = Qt.binding(() => simple.trimEndText);
                        }
                        Binding on text {
                            value: simple.trimEndText
                            when: !endField.input.activeFocus
                        }
                    }
                }

                Text {
                    text: "Reset trim"
                    color: root.t.muted
                    font.family: "Inter"
                    font.pixelSize: 12
                    font.underline: true

                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -8
                        cursorShape: Qt.PointingHandCursor
                        onClicked: simple.resetTrim()
                    }
                }
            }

            // Cut out
            ColumnLayout {
                visible: simple.hasFile && simple.tool === "cut"
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                spacing: 12

                Heading {
                    title: "Cut out a part"
                    body: "Drag the red box over the part you don't want. Everything else stays and joins up."
                }

                Rectangle {
                    visible: simple.hasSelection
                    Layout.fillWidth: true
                    Layout.preferredHeight: selectionColumn.implicitHeight + 28
                    radius: 12
                    color: root.t.drop
                    border.width: 1
                    border.color: root.t.softBorder

                    Column {
                        id: selectionColumn
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 8

                        Text {
                            text: "Selected part"
                            color: root.t.muted
                            font.family: "Inter"
                            font.pixelSize: 12
                        }

                        Text {
                            text: simple.selectionText
                            color: root.t.text
                            font.family: "Inter"
                            font.pixelSize: 20
                            font.weight: Font.DemiBold
                        }

                        Text {
                            width: parent.width
                            text: simple.selectionInfo
                            wrapMode: Text.WordWrap
                            color: root.t.muted
                            font.family: "Inter"
                            font.pixelSize: 12
                        }
                    }
                }

                ActionButton {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 46
                    icon: "cut"
                    text: "Remove this part"
                    danger: true
                    enabled: simple.canRemoveSelection
                    onClicked: simple.removeSelection()
                }

                ActionButton {
                    Layout.fillWidth: true
                    icon: "plus"
                    text: "Select another part"
                    onClicked: simple.selectAnotherPart()
                }

                ColumnLayout {
                    visible: simple.cuts.length > 0
                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        text: "Removed so far"
                        color: root.t.muted
                        font.family: "Inter"
                        font.pixelSize: 12
                    }

                    Repeater {
                        model: simple.cuts

                        delegate: RowLayout {
                            required property var modelData
                            required property int index

                            Layout.fillWidth: true

                            Text {
                                Layout.fillWidth: true
                                text: modelData.label
                                color: root.t.text2
                                font.family: "Inter"
                                font.pixelSize: 13
                            }

                            Text {
                                text: "Put back"
                                color: root.t.accent
                                font.family: "Inter"
                                font.pixelSize: 12
                                font.underline: true

                                MouseArea {
                                    anchors.fill: parent
                                    anchors.margins: -6
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: simple.restoreCut(index)
                                }
                            }
                        }
                    }
                }
            }

            // Crop
            ColumnLayout {
                visible: simple.hasFile && simple.tool === "crop"
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                spacing: 16

                Heading {
                    title: "Crop"
                    body: "Pick a shape, then drag the box on the picture."
                }

                Grid {
                    id: shapes

                    Layout.fillWidth: true
                    columns: 3
                    spacing: 8

                    Repeater {
                        model: [
                            { id: "original", label: "Original", w: 30, h: 17 },
                            { id: "free", label: "Free", w: 26, h: 20 },
                            { id: "square", label: "Square", w: 22, h: 22 },
                            { id: "9:16", label: "9:16", w: 14, h: 24 },
                            { id: "16:9", label: "16:9", w: 32, h: 18 },
                            { id: "4:5", label: "4:5", w: 18, h: 22 }
                        ]

                        delegate: Rectangle {
                            required property var modelData
                            readonly property bool selected: simple.cropShape === modelData.id

                            width: (shapes.width - 16) / 3
                            height: 64
                            radius: 10
                            color: selected ? root.t.accentSoft : "transparent"
                            border.width: 1.5
                            border.color: selected ? root.t.accent : root.t.softBorder
                            Accessible.role: Accessible.Button
                            Accessible.name: modelData.label

                            Column {
                                anchors.centerIn: parent
                                spacing: 6

                                Rectangle {
                                    width: modelData.w
                                    height: modelData.h
                                    radius: 3
                                    color: "transparent"
                                    border.width: 2
                                    border.color: parent.parent.selected ? root.t.accent : root.t.muted
                                    anchors.horizontalCenter: parent.horizontalCenter
                                }

                                Text {
                                    text: modelData.label
                                    color: root.t.text
                                    font.family: "Inter"
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                    anchors.horizontalCenter: parent.horizontalCenter
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: simple.pickCropShape(modelData.id)
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    ActionButton {
                        Layout.fillWidth: true
                        text: "Rotate"
                        onClicked: simple.rotate()
                    }

                    ActionButton {
                        Layout.fillWidth: true
                        text: "Flip"
                        onClicked: simple.flip()
                    }
                }
            }

            // Caption
            ColumnLayout {
                visible: simple.hasFile && simple.tool === "caption"
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                spacing: 16

                Heading {
                    title: "Caption"
                    body: simple.isPhoto ? "Put a name across the bottom of the photo."
                                         : "Put a name across the bottom of the video."
                }

                Field {
                    id: nameField
                    title: "Name"
                    strong: true
                    onEdited: simple.captionName = text
                    Binding on text {
                        value: simple.captionName
                        when: !nameField.input.activeFocus
                    }
                }

                Field {
                    id: secondField
                    title: "Second line (optional)"
                    onEdited: simple.captionSecond = text
                    Binding on text {
                        value: simple.captionSecond
                        when: !secondField.input.activeFocus
                    }
                }

                Text {
                    text: "Style"
                    color: root.t.text2
                    font.family: "Inter"
                    font.pixelSize: 13
                }

                Row {
                    id: styles

                    Layout.fillWidth: true
                    spacing: 8

                    Repeater {
                        model: [
                            { id: "bar", label: "Bar", tint: "#B3000000", center: false },
                            { id: "center", label: "Center", tint: "#99000000", center: true },
                            { id: "bold", label: "Bold", tint: "#2F62E0", center: false }
                        ]

                        delegate: Rectangle {
                            required property var modelData
                            readonly property bool selected: simple.captionStyle === modelData.id

                            width: (styles.width - 16) / 3
                            height: 70
                            radius: 10
                            color: "#3A3F4A"
                            border.width: 1.5
                            border.color: selected ? root.t.accent : "transparent"
                            clip: true
                            Accessible.role: Accessible.Button
                            Accessible.name: modelData.label

                            Rectangle {
                                anchors.bottom: parent.bottom
                                anchors.margins: 1.5
                                anchors.left: parent.left
                                anchors.right: parent.right
                                height: 28
                                color: modelData.tint

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    horizontalAlignment: modelData.center ? Text.AlignHCenter : Text.AlignLeft
                                    text: modelData.label
                                    color: "#FFFFFF"
                                    font.family: "Inter"
                                    font.pixelSize: 11
                                    font.weight: Font.Bold
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: simple.captionStyle = modelData.id
                            }
                        }
                    }
                }
            }

            // Save picture
            ColumnLayout {
                visible: simple.hasFile && simple.tool === "frame"
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                spacing: 12

                Heading {
                    title: "Save a picture"
                    body: "Pause on the moment you like, then save it as a photo. Same quality as the video."
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: width * 9 / 16
                    radius: 10
                    color: "#0B0C0E"
                    clip: true

                    Image {
                        anchors.fill: parent
                        source: simple.frameThumb
                        fillMode: Image.PreserveAspectFit
                        cache: false
                    }
                }

                Text {
                    text: "Frame at " + simple.positionText
                    color: root.t.muted
                    font.family: "Inter"
                    font.pixelSize: 12
                }

                ActionButton {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 46
                    icon: "frame"
                    text: "Save this picture"
                    primary: true
                    onClicked: simple.savePicture()
                }

                Text {
                    text: "It goes into Finished, ready to share."
                    color: root.t.muted
                    font.family: "Inter"
                    font.pixelSize: 12
                }
            }
        }
    }

    // Always the same promise, whichever tool is open.
    Rectangle {
        id: note

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 16
        height: noteRow.implicitHeight + 24
        radius: 12
        color: root.t.drop

        RowLayout {
            id: noteRow

            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            Icon {
                Layout.alignment: Qt.AlignTop
                size: 18
                name: "shield"
                color: root.t.text2
            }

            Text {
                Layout.fillWidth: true
                textFormat: Text.StyledText
                text: "Your original is never changed. <b>Finish</b> saves a new copy to the Finished tab."
                wrapMode: Text.WordWrap
                lineHeight: 1.2
                color: root.t.text2
                font.family: "Inter"
                font.pixelSize: 12
            }
        }
    }
}
