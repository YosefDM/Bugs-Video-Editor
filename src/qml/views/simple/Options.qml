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
            // Enter lets go of the box, so Space plays again and the box follows the strip.
            onAccepted: root.forceActiveFocus()
        }
    }

    // A slider with its name above, for the collage's look.
    component LookSlider: ColumnLayout {
        id: lookSlider

        property string title
        property real from: 0
        property real to: 1
        property real value: 0
        signal moved(real value)

        Layout.fillWidth: true
        spacing: 2

        Text {
            text: lookSlider.title
            color: root.t.text2
            font.family: "Inter"
            font.pixelSize: 13
        }

        Slider {
            id: slider

            Layout.fillWidth: true
            from: lookSlider.from
            to: lookSlider.to
            value: lookSlider.value
            // Applies on release: each change redraws the whole collage.
            live: false
            onValueChanged: {
                if (value !== lookSlider.value)
                    lookSlider.moved(value);
            }
        }
    }

    // On a small screen a tool's options need all the height: the note gives way, and the
    // scroll bar stays visible so it is clear there is more below.
    readonly property bool roomForNote: options.implicitHeight + note.implicitHeight + 28 <= height

    ScrollView {
        id: scroll

        anchors.fill: parent
        anchors.bottomMargin: root.roomForNote ? note.height + 28 : 0
        // A long list, such as saved pictures, scrolls under the note instead of over it.
        clip: true
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
                id: collagePanel

                // Grid cells are sized from the panel, not from their own grid, whose width comes
                // from its cells: that loop never settled and froze the app.
                readonly property int selected: simple.collageSelectedItem
                readonly property var item: selected >= 0 && selected < simple.collageItems.length
                                            ? simple.collageItems[selected] : null
                property int stickerCategory: 0

                // The words box may still have the keyboard from the text selected before, so it
                // is filled in by hand when the selection changes.
                onSelectedChanged: collageWords.text = item && item.kind === "text" ? item.text : ""

                visible: simple.collageActive
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                spacing: 14

                Heading {
                    title: "Collage"
                    body: "Add words and stickers, then drag them into place. Tap a photo to zoom or move it, or tap two photos to swap them."
                }

                // The photo tapped on the collage, at the top so it shows without scrolling.
                Rectangle {
                    visible: simple.collageSelected >= 0
                    Layout.fillWidth: true
                    Layout.preferredHeight: photoColumn.implicitHeight + 24
                    radius: 12
                    color: root.t.drop
                    border.width: 1
                    border.color: root.t.accent

                    ColumnLayout {
                        id: photoColumn

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 12
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true

                            Text {
                                Layout.fillWidth: true
                                text: "Selected photo"
                                color: root.t.text2
                                font.family: "Inter"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }

                            Text {
                                text: "Done"
                                color: root.t.accent
                                font.family: "Inter"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold

                                MouseArea {
                                    anchors.fill: parent
                                    anchors.margins: -8
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: simple.deselectCollagePhoto()
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "Drag it to move it in its frame. Tap another photo to swap them."
                            wrapMode: Text.WordWrap
                            color: root.t.muted
                            font.family: "Inter"
                            font.pixelSize: 12
                        }

                        LookSlider {
                            title: "Zoom"
                            from: 1
                            to: 3
                            value: simple.collagePhotoZoom
                            onMoved: (value) => simple.setCollagePhotoZoom(value)
                        }

                        ActionButton {
                            Layout.fillWidth: true
                            icon: simple.collagePhotoIsBig ? "check" : "plus"
                            text: simple.collagePhotoIsBig ? "This is the big photo" : "Make it the big photo"
                            primary: !simple.collagePhotoIsBig
                            onClicked: simple.setCollagePhotoBig(!simple.collagePhotoIsBig)
                        }
                    }
                }

                // Text and stickers
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    ActionButton {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44
                        icon: "text"
                        text: "Add text"
                        primary: true
                        onClicked: {
                            simple.addCollageText("");
                            collageWords.text = collagePanel.item ? collagePanel.item.text : "";
                            collageWords.forceActiveFocus();
                            collageWords.selectAll();
                        }
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 6

                    Repeater {
                        model: simple.collagePhrases

                        delegate: Rectangle {
                            required property string modelData

                            width: phrase.implicitWidth + 20
                            height: 30
                            radius: 15
                            color: phraseArea.containsMouse ? root.t.accentSoft : root.t.chip
                            border.width: 1
                            border.color: root.t.softBorder
                            Accessible.role: Accessible.Button
                            Accessible.name: "Add " + modelData

                            Text {
                                id: phrase
                                anchors.centerIn: parent
                                text: modelData
                                color: root.t.text
                                font.family: "Inter"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                            }

                            MouseArea {
                                id: phraseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: simple.addCollageText(modelData)
                            }
                        }
                    }
                }

                // Stickers, only the approved ones
                Text {
                    text: "Stickers"
                    color: root.t.text2
                    font.family: "Inter"
                    font.pixelSize: 13
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 6

                    Repeater {
                        model: simple.stickerCategories

                        delegate: Rectangle {
                            required property var modelData
                            required property int index
                            readonly property bool selected: collagePanel.stickerCategory === index

                            width: categoryName.implicitWidth + 20
                            height: 28
                            radius: 14
                            color: selected ? root.t.strong : root.t.chip
                            Accessible.role: Accessible.Button
                            Accessible.name: modelData.title

                            Text {
                                id: categoryName
                                anchors.centerIn: parent
                                text: modelData.title
                                color: parent.selected ? root.t.onStrong : root.t.text
                                font.family: "Inter"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: collagePanel.stickerCategory = index
                            }
                        }
                    }
                }

                // A window onto the group, about four rows tall, that scrolls on its own: with
                // hundreds of stickers in a group, the options below would otherwise be a long
                // way down. Only the rows on screen are made.
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: stickerGrid.cellHeight * 4 + 12
                    radius: 10
                    color: root.t.drop
                    border.width: 1
                    border.color: root.t.softBorder
                    clip: true

                    GridView {
                        id: stickerGrid

                        anchors.fill: parent
                        anchors.margins: 6
                        anchors.rightMargin: 14
                        cellWidth: Math.floor(width / 5)
                        cellHeight: cellWidth
                        boundsBehavior: Flickable.StopAtBounds
                        model: simple.stickerCategories.length > 0
                               ? simple.stickerCategories[collagePanel.stickerCategory].stickers : []

                        ScrollBar.vertical: ScrollBar {
                            parent: stickerGrid.parent
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.right: parent.right
                            anchors.margins: 2
                            policy: ScrollBar.AlwaysOn
                        }

                        delegate: Rectangle {
                            required property var modelData

                            width: stickerGrid.cellWidth
                            height: stickerGrid.cellHeight
                            radius: 10
                            color: stickerArea.containsMouse ? root.t.accentSoft : "transparent"
                            Accessible.role: Accessible.Button
                            Accessible.name: "Add sticker: " + modelData.name

                            Image {
                                anchors.fill: parent
                                anchors.margins: 5
                                source: modelData.url
                                sourceSize.width: 96
                                sourceSize.height: 96
                                fillMode: Image.PreserveAspectFit
                                asynchronous: true
                            }

                            MouseArea {
                                id: stickerArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: simple.addSticker(modelData.id)
                            }
                        }
                    }
                }

                // The selected text or sticker, under the stickers so picking a sticker
                // never moves the grid under the pointer.
                Rectangle {
                    visible: collagePanel.item !== null
                    Layout.fillWidth: true
                    Layout.preferredHeight: selectedColumn.implicitHeight + 24
                    radius: 12
                    color: root.t.drop
                    border.width: 1
                    border.color: root.t.accent

                    ColumnLayout {
                        id: selectedColumn

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 12
                        spacing: 10

                        Text {
                            text: collagePanel.item && collagePanel.item.kind === "text" ? "Selected text" : "Selected sticker"
                            color: root.t.text2
                            font.family: "Inter"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }

                        TextArea {
                            id: collageWords

                            visible: collagePanel.item !== null && collagePanel.item.kind === "text"
                            Layout.fillWidth: true
                            Layout.minimumHeight: 56
                            wrapMode: TextEdit.Wrap
                            color: root.t.text
                            selectByMouse: true
                            selectedTextColor: root.t.onAccent
                            selectionColor: root.t.accent
                            leftPadding: 10
                            rightPadding: 10
                            topPadding: 8
                            bottomPadding: 8
                            font.family: "Inter"
                            font.pixelSize: 15
                            placeholderText: "Type here. Enter starts a new line."
                            background: Rectangle {
                                radius: 8
                                color: root.t.panel
                                border.width: collageWords.activeFocus ? 1.5 : 1
                                border.color: collageWords.activeFocus ? root.t.accent : root.t.softBorder
                            }
                            onTextChanged: {
                                if (activeFocus && collagePanel.item)
                                    simple.setCollageItemText(collagePanel.selected, text);
                            }
                            Binding on text {
                                value: collagePanel.item ? collagePanel.item.text : ""
                                when: !collageWords.activeFocus
                                // Keep the words when the box takes the keyboard, instead of
                                // going back to an older value and saving that.
                                restoreMode: Binding.RestoreNone
                            }
                        }

                        Grid {
                            id: collageFonts

                            visible: collagePanel.item !== null && collagePanel.item.kind === "text"
                            Layout.fillWidth: true
                            columns: 3
                            spacing: 6

                            Repeater {
                                model: simple.labelFonts

                                delegate: Rectangle {
                                    required property var modelData
                                    readonly property bool selected: collagePanel.item && collagePanel.item.font === modelData.id

                                    width: (collagePanel.width - 24 - 12) / 3
                                    height: 36
                                    radius: 8
                                    color: selected ? root.t.accentSoft : "transparent"
                                    border.width: 1.5
                                    border.color: selected ? root.t.accent : root.t.softBorder
                                    Accessible.role: Accessible.Button
                                    Accessible.name: modelData.name

                                    Text {
                                        anchors.centerIn: parent
                                        width: parent.width - 6
                                        horizontalAlignment: Text.AlignHCenter
                                        elide: Text.ElideRight
                                        text: modelData.name
                                        color: root.t.text
                                        font.family: modelData.family
                                        font.bold: true
                                        font.pixelSize: 14
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: simple.setCollageItemFont(collagePanel.selected, modelData.id)
                                    }
                                }
                            }
                        }

                        Grid {
                            id: collageColors

                            visible: collagePanel.item !== null && collagePanel.item.kind === "text"
                            Layout.fillWidth: true
                            columns: 5
                            spacing: 6

                            Repeater {
                                model: simple.labelColors

                                delegate: Rectangle {
                                    required property string modelData
                                    readonly property bool selected: collagePanel.item && collagePanel.item.color.toLowerCase() === modelData.toLowerCase()

                                    width: (collagePanel.width - 24 - 24) / 5
                                    height: 30
                                    radius: 15
                                    color: "transparent"
                                    border.width: selected ? 2.5 : 0
                                    border.color: root.t.accent
                                    Accessible.role: Accessible.Button
                                    Accessible.name: "Colour " + modelData

                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 22
                                        height: 22
                                        radius: 11
                                        color: parent.modelData
                                        border.width: 1
                                        border.color: root.t.softBorder
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: simple.setCollageItemColor(collagePanel.selected, modelData)
                                    }
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "Drag the corner dot to resize, and the blue knob to turn."
                            wrapMode: Text.WordWrap
                            color: root.t.muted
                            font.family: "Inter"
                            font.pixelSize: 12
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            ActionButton {
                                Layout.fillWidth: true
                                icon: "up"
                                text: "Front"
                                fontSize: 12
                                onClicked: simple.bringCollageItemToFront(collagePanel.selected)
                            }

                            ActionButton {
                                Layout.fillWidth: true
                                icon: "down"
                                text: "Back"
                                fontSize: 12
                                onClicked: simple.sendCollageItemToBack(collagePanel.selected)
                            }

                            ActionButton {
                                Layout.fillWidth: true
                                icon: "trash"
                                text: "Remove"
                                fontSize: 12
                                onClicked: simple.removeCollageItem(collagePanel.selected)
                            }
                        }
                    }
                }

                // The photos
                Text {
                    text: "Layout"
                    color: root.t.text2
                    font.family: "Inter"
                    font.pixelSize: 13
                }

                Row {
                    id: layoutRow

                    Layout.fillWidth: true
                    spacing: 8

                    Repeater {
                        model: simple.collageLayouts

                        delegate: Rectangle {
                            required property var modelData
                            readonly property bool selected: simple.collageLayout === modelData.id

                            width: (collagePanel.width - 16) / 3
                            height: width + 22
                            radius: 10
                            color: selected ? root.t.accentSoft : "transparent"
                            border.width: 1.5
                            border.color: selected ? root.t.accent : root.t.softBorder
                            Accessible.role: Accessible.Button
                            Accessible.name: "Layout " + modelData.title

                            // A small picture of the layout, the collage's own shape.
                            Item {
                                id: thumb

                                readonly property real box: parent.width - 20
                                readonly property real aspect: simple.collageAspect

                                x: (parent.width - width) / 2
                                y: 8 + (box - height) / 2
                                width: aspect >= 1 ? box : box * aspect
                                height: aspect >= 1 ? box / aspect : box

                                Repeater {
                                    model: modelData.cells

                                    delegate: Rectangle {
                                        required property var modelData

                                        x: modelData.x * thumb.width + 1
                                        y: modelData.y * thumb.height + 1
                                        width: Math.max(1, modelData.w * thumb.width - 2)
                                        height: Math.max(1, modelData.h * thumb.height - 2)
                                        radius: 2
                                        color: modelData.big ? root.t.accent : root.t.muted
                                        opacity: modelData.big ? 0.9 : 0.55
                                    }
                                }
                            }

                            Text {
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 5
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.title
                                color: root.t.text
                                font.family: "Inter"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: simple.setCollageLayout(modelData.id)
                            }
                        }
                    }
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
                            height: 48
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
                    text: "Shuffle the photos"
                    onClicked: simple.shuffleCollage()
                }

                // Look
                LookSlider {
                    title: "Space between photos"
                    to: 0.06
                    value: simple.collageSpacing
                    onMoved: (value) => simple.setCollageSpacing(value)
                }

                LookSlider {
                    title: "Rounded corners"
                    to: 0.08
                    value: simple.collageCorners
                    onMoved: (value) => simple.setCollageCorners(value)
                }

                Text {
                    text: "Background"
                    color: root.t.text2
                    font.family: "Inter"
                    font.pixelSize: 13
                }

                Grid {
                    id: backgrounds

                    Layout.fillWidth: true
                    columns: 5
                    spacing: 6

                    Repeater {
                        model: simple.collageBackgrounds

                        delegate: Rectangle {
                            required property string modelData
                            readonly property bool selected: simple.collageBackground.toLowerCase() === modelData.toLowerCase()

                            width: (collagePanel.width - 24) / 5
                            height: 34
                            radius: 17
                            color: "transparent"
                            border.width: selected ? 2.5 : 0
                            border.color: root.t.accent
                            Accessible.role: Accessible.Button
                            Accessible.name: "Background " + modelData

                            Rectangle {
                                anchors.centerIn: parent
                                width: 26
                                height: 26
                                radius: 13
                                color: parent.modelData
                                border.width: 1
                                border.color: root.t.softBorder
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: simple.setCollageBackground(modelData)
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
                            if (text !== simple.trimStartText)
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
                            if (text !== simple.trimEndText)
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
                    body: "Drag the red box over the part you don't want, or type the times. Everything else stays and joins up."
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Field {
                        id: cutStartField
                        title: "From"
                        onCommitted: {
                            if (text !== simple.selectionStartText)
                                simple.setSelectionStartText(text);
                            text = Qt.binding(() => simple.selectionStartText);
                        }
                        Binding on text {
                            value: simple.selectionStartText
                            when: !cutStartField.input.activeFocus
                        }
                    }

                    Field {
                        id: cutEndField
                        title: "To"
                        onCommitted: {
                            if (text !== simple.selectionEndText)
                                simple.setSelectionEndText(text);
                            text = Qt.binding(() => simple.selectionEndText);
                        }
                        Binding on text {
                            value: simple.selectionEndText
                            when: !cutEndField.input.activeFocus
                        }
                    }
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

                // The words box may still have the keyboard from the text selected before, so it
                // is filled in by hand when the selection changes.
                onSelectedChanged: wordsInput.text = label ? label.text : ""

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
                        wordsInput.text = textPanel.label ? textPanel.label.text : "";
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
                                // Keep the words when the box takes the keyboard, instead
                                // of going back to an older value and saving that.
                                restoreMode: Binding.RestoreNone
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
                    Layout.fillWidth: true
                    text: "It goes into Finished straight away. Save as many as you like."
                    wrapMode: Text.WordWrap
                    color: root.t.muted
                    font.family: "Inter"
                    font.pixelSize: 12
                }

                // Pictures saved from this video, so one he doesn't like can be taken back.
                ColumnLayout {
                    visible: simple.savedPictures.length > 0
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    spacing: 8

                    Text {
                        text: simple.savedPictures.length === 1 ? "1 picture saved from this video"
                                                                : simple.savedPictures.length + " pictures saved from this video"
                        color: root.t.text2
                        font.family: "Inter"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }

                    Repeater {
                        model: simple.savedPictures

                        delegate: Rectangle {
                            required property var modelData

                            Layout.fillWidth: true
                            Layout.preferredHeight: 56
                            radius: 10
                            color: root.t.drop
                            border.width: 1
                            border.color: root.t.softBorder

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 6
                                anchors.rightMargin: 12
                                spacing: 10

                                Image {
                                    Layout.preferredWidth: 76
                                    Layout.preferredHeight: 44
                                    source: modelData.url
                                    sourceSize.width: 152
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    clip: true
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: "At " + modelData.time
                                    elide: Text.ElideRight
                                    color: root.t.text
                                    font.family: "Inter"
                                    font.pixelSize: 13
                                }

                                Text {
                                    text: "Don't keep"
                                    color: root.t.danger
                                    font.family: "Inter"
                                    font.pixelSize: 12
                                    font.underline: true
                                    Accessible.role: Accessible.Button
                                    Accessible.name: "Don't keep the picture at " + modelData.time

                                    MouseArea {
                                        anchors.fill: parent
                                        anchors.margins: -8
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: simple.removeSavedPicture(modelData.path)
                                    }
                                }
                            }
                        }
                    }
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
