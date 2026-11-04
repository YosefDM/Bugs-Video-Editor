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

    // On a small screen a tool's options need all the height: the note gives way, and the
    // scroll bar stays visible so it is clear there is more below.
    readonly property bool roomForNote: options.implicitHeight + note.implicitHeight + 28 <= height

    ScrollView {
        id: scroll

        anchors.fill: parent
        anchors.bottomMargin: root.roomForNote ? note.height + 28 : 0
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: options.implicitHeight > scroll.height ? ScrollBar.AlwaysOn
                                                                          : ScrollBar.AlwaysOff

        ColumnLayout {
            id: options

            width: scroll.availableWidth
            spacing: 16

            Item {
                Layout.preferredHeight: 4
            }

            // Nothing open yet
            Text {
                visible: !simple.hasFile && !simple.collageActive
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                text: "Pick a video or photo from the left to start."
                wrapMode: Text.WordWrap
                color: root.t.muted
                font.family: "Inter"
                font.pixelSize: 14
            }

            // Collage
            ColumnLayout {
                visible: simple.collageActive
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                spacing: 14

                Heading {
                    title: "Collage"
                    body: "Pick a shape and a border. Shuffle tries a new arrangement, and tapping two photos swaps them."
                }

                Text {
                    text: "Shape"
                    color: root.t.text2
                    font.family: "Inter"
                    font.pixelSize: 13
                }

                Grid {
                    id: collageShapes

                    Layout.fillWidth: true
                    columns: 2
                    spacing: 8

                    Repeater {
                        model: [
                            { id: "square", label: "Square", w: 22, h: 22 },
                            { id: "story", label: "Story", w: 14, h: 24 },
                            { id: "wide", label: "Wide", w: 32, h: 18 },
                            { id: "portrait", label: "4:5", w: 18, h: 22 }
                        ]

                        delegate: Rectangle {
                            required property var modelData
                            readonly property bool selected: simple.collageShape === modelData.id

                            width: (collageShapes.width - 8) / 2
                            height: 56
                            radius: 10
                            color: selected ? root.t.accentSoft : "transparent"
                            border.width: 1.5
                            border.color: selected ? root.t.accent : root.t.softBorder
                            Accessible.role: Accessible.Button
                            Accessible.name: modelData.label

                            Row {
                                anchors.centerIn: parent
                                spacing: 10

                                Rectangle {
                                    width: modelData.w
                                    height: modelData.h
                                    radius: 3
                                    color: "transparent"
                                    border.width: 2
                                    border.color: parent.parent.selected ? root.t.accent : root.t.muted
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                Text {
                                    text: modelData.label
                                    color: root.t.text
                                    font.family: "Inter"
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: simple.setCollageShape(modelData.id)
                            }
                        }
                    }
                }

                ActionButton {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    icon: "shuffle"
                    text: "Shuffle"
                    onClicked: simple.shuffleCollage()
                }

                Text {
                    text: "Border"
                    color: root.t.text2
                    font.family: "Inter"
                    font.pixelSize: 13
                }

                Row {
                    id: borderRow

                    Layout.fillWidth: true
                    spacing: 8

                    Repeater {
                        model: ["None", "Thin", "Thick"]

                        delegate: Rectangle {
                            required property string modelData
                            required property int index
                            readonly property bool selected: simple.collageBorder === index

                            width: (borderRow.width - 16) / 3
                            height: 40
                            radius: 10
                            color: selected ? root.t.accentSoft : "transparent"
                            border.width: 1.5
                            border.color: selected ? root.t.accent : root.t.softBorder

                            Text {
                                anchors.centerIn: parent
                                text: modelData
                                color: root.t.text
                                font.family: "Inter"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: simple.setCollageBorder(index)
                            }
                        }
                    }
                }

                Row {
                    visible: simple.collageBorder > 0
                    spacing: 10

                    Repeater {
                        model: ["#FFFFFF", "#111111", "#E5392B"]

                        delegate: Rectangle {
                            required property string modelData
                            readonly property bool selected: simple.collageBorderColor.toLowerCase() === modelData.toLowerCase()

                            width: 40
                            height: 40
                            radius: 20
                            color: "transparent"
                            border.width: selected ? 2.5 : 0
                            border.color: root.t.accent

                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: 5
                                radius: width / 2
                                color: parent.modelData
                                border.width: 1
                                border.color: root.t.softBorder
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: simple.setCollageBorderColor(modelData)
                            }
                        }
                    }
                }

                Text {
                    text: "Back to my photos"
                    color: root.t.muted
                    font.family: "Inter"
                    font.pixelSize: 12
                    font.underline: true

                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -8
                        cursorShape: Qt.PointingHandCursor
                        onClicked: simple.askToLeaveCollage()
                    }
                }
            }

            // Trim
            ColumnLayout {
                visible: simple.hasFile && !simple.collageActive && simple.tool === "trim"
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
                visible: simple.hasFile && !simple.collageActive && simple.tool === "cut"
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
                visible: simple.hasFile && !simple.collageActive && simple.tool === "crop"
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
                visible: simple.hasFile && !simple.collageActive && simple.tool === "caption"
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

            // Text
            ColumnLayout {
                id: textPanel

                readonly property int selected: simple.selectedLabel
                readonly property var label: selected >= 0 && selected < simple.labels.length
                                             ? simple.labels[selected] : null

                visible: simple.hasFile && !simple.collageActive && simple.tool === "text"
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                spacing: 14

                Heading {
                    title: "Text"
                    body: "Put words anywhere, like names next to people. Drag them into place on the picture."
                }

                ActionButton {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    icon: "plus"
                    text: "Add text"
                    primary: true
                    onClicked: {
                        simple.addLabel();
                        wordsInput.forceActiveFocus();
                        wordsInput.selectAll();
                    }
                }

                Text {
                    visible: simple.labels.length === 0
                    Layout.fillWidth: true
                    text: "Press Add text, then drag it where you want it."
                    wrapMode: Text.WordWrap
                    color: root.t.muted
                    font.family: "Inter"
                    font.pixelSize: 12
                }

                ColumnLayout {
                    visible: textPanel.label !== null
                    Layout.fillWidth: true
                    spacing: 14

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            text: "Words (Enter starts a new line)"
                            color: root.t.text2
                            font.family: "Inter"
                            font.pixelSize: 13
                        }

                        TextArea {
                            id: wordsInput

                            Layout.fillWidth: true
                            Layout.minimumHeight: 64
                            wrapMode: TextEdit.Wrap
                            color: root.t.text
                            selectByMouse: true
                            selectedTextColor: root.t.onAccent
                            selectionColor: root.t.accent
                            leftPadding: 12
                            rightPadding: 12
                            topPadding: 10
                            bottomPadding: 10
                            font.family: "Inter"
                            font.pixelSize: 15
                            background: Rectangle {
                                radius: 10
                                color: root.t.drop
                                border.width: wordsInput.activeFocus ? 1.5 : 1
                                border.color: wordsInput.activeFocus ? root.t.accent : root.t.softBorder
                            }
                            onTextChanged: {
                                if (activeFocus && textPanel.label)
                                    simple.setLabelText(textPanel.selected, text);
                            }
                            Binding on text {
                                value: textPanel.label ? textPanel.label.text : ""
                                when: !wordsInput.activeFocus
                            }
                        }
                    }

                    Text {
                        text: "Font"
                        color: root.t.text2
                        font.family: "Inter"
                        font.pixelSize: 13
                    }

                    Grid {
                        id: fontGrid

                        Layout.fillWidth: true
                        columns: 2
                        spacing: 8

                        Repeater {
                            model: simple.labelFonts

                            delegate: Rectangle {
                                required property var modelData
                                readonly property bool selected: textPanel.label && textPanel.label.font === modelData.id

                                width: (fontGrid.width - 8) / 2
                                height: 44
                                radius: 10
                                color: selected ? root.t.accentSoft : "transparent"
                                border.width: 1.5
                                border.color: selected ? root.t.accent : root.t.softBorder
                                Accessible.role: Accessible.Button
                                Accessible.name: modelData.name

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.name
                                    color: root.t.text
                                    font.family: modelData.family
                                    font.bold: true
                                    font.pixelSize: 17
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: simple.setLabelFont(textPanel.selected, modelData.id)
                                }
                            }
                        }
                    }

                    Text {
                        text: "Colour"
                        color: root.t.text2
                        font.family: "Inter"
                        font.pixelSize: 13
                    }

                    Grid {
                        id: colorGrid

                        Layout.fillWidth: true
                        columns: 5
                        spacing: 10

                        Repeater {
                            model: simple.labelColors

                            delegate: Rectangle {
                                required property string modelData
                                readonly property bool selected: textPanel.label && textPanel.label.color.toLowerCase() === modelData.toLowerCase()

                                width: (colorGrid.width - 40) / 5
                                height: width
                                radius: width / 2
                                color: "transparent"
                                border.width: selected ? 2.5 : 0
                                border.color: root.t.accent
                                Accessible.role: Accessible.Button
                                Accessible.name: "Colour " + modelData

                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: 5
                                    radius: width / 2
                                    color: parent.modelData
                                    border.width: 1
                                    border.color: root.t.softBorder
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: simple.setLabelColor(textPanel.selected, modelData)
                                }
                            }
                        }
                    }

                    Text {
                        text: "Size"
                        color: root.t.text2
                        font.family: "Inter"
                        font.pixelSize: 13
                    }

                    Row {
                        id: sizeRow

                        Layout.fillWidth: true
                        spacing: 8

                        Repeater {
                            model: ["Small", "Medium", "Large"]

                            delegate: Rectangle {
                                required property string modelData
                                required property int index
                                readonly property bool selected: textPanel.label && textPanel.label.size === index

                                width: (sizeRow.width - 16) / 3
                                height: 40
                                radius: 10
                                color: selected ? root.t.accentSoft : "transparent"
                                border.width: 1.5
                                border.color: selected ? root.t.accent : root.t.softBorder

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData
                                    color: root.t.text
                                    font.family: "Inter"
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: simple.setLabelSize(textPanel.selected, index)
                                }
                            }
                        }
                    }

                    Text {
                        text: "Remove this text"
                        color: root.t.danger
                        font.family: "Inter"
                        font.pixelSize: 12
                        font.underline: true

                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -8
                            cursorShape: Qt.PointingHandCursor
                            onClicked: simple.removeLabel(textPanel.selected)
                        }
                    }
                }
            }

            // Save picture
            ColumnLayout {
                visible: simple.hasFile && !simple.collageActive && simple.tool === "frame"
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

        visible: root.roomForNote
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
