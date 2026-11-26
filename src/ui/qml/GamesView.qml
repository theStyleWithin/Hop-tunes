// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// The game shelf (collection → games): Steam games and ROMs found in the folders the person added, shown like a gamer's bookshelf:
// covers standing on wooden planks, one chip per system to narrow the shelf (PC, Switch, Wii U, PS2 ...). Browse only: nothing here
// launches a game. A game's page keeps the person's own title, system, notes and cover picture, saved in this profile on this computer.
Item {
    id: root

    signal addFolderRequested()

    readonly property var status: hop.gamesStatus
    readonly property var folders: hop.gamesFolders
    property string openPath: ""                         // the game whose page is open ("" = the shelf)
    readonly property bool detailOpen: openPath !== ""
    property int detailRev: 0                            // bumped when the open game's saved details change
    readonly property var detail: { hop.games.count; detailRev; return openPath !== "" ? hop.gameDetail(openPath) : ({}) }
    function closeDetail() { openPath = "" }
    Connections {   // the open game is gone (its drive was unplugged, or the folder removed): back to the shelf
        target: hop.games
        function onCountChanged() { if (root.openPath !== "" && !hop.gameDetail(root.openPath).path) root.openPath = "" }
    }

    // a drop-down in the app's own colours (the stock one is a bright box in the dark theme)
    component ThemedCombo: ComboBox {
        id: cb
        font { family: Theme.font; pixelSize: Theme.fs(15) }
        implicitHeight: 40
        contentItem: Label {
            leftPadding: 12; rightPadding: 30
            text: cb.displayText
            color: Theme.text; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
            font: cb.font
        }
        indicator: Label { x: cb.width - width - 12; y: (cb.height - height) / 2; text: "\u25BE"; color: Theme.muted; font { pixelSize: Theme.fs(16) } }
        background: Rectangle { color: Theme.panel; border { width: cb.activeFocus || cb.popup.visible ? 2 : 0; color: Theme.accent } }
        delegate: Component {
            ItemDelegate {
                id: dlg
                required property int index
                required property var modelData
                width: cb.width
                highlighted: cb.highlightedIndex === index
                contentItem: Label { text: dlg.modelData; color: dlg.highlighted ? Theme.ink : Theme.text; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; font: cb.font }
                background: Rectangle { color: dlg.highlighted ? Theme.accent : "transparent" }
            }
        }
        popup: Popup {
            y: cb.height
            width: cb.width
            implicitHeight: Math.min(contentItem.implicitHeight + 2, 320)
            padding: 1
            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: cb.popup.visible ? cb.delegateModel : null
                currentIndex: cb.highlightedIndex
                ScrollBar.vertical: ScrollBar {}
            }
            background: Rectangle { color: Theme.panel; border { width: 1; color: Theme.border } }
        }
    }

    // wood for the planks: the same in the light and the dark theme (a shelf does not change colour with the room)
    readonly property color woodTop: "#8a6240"
    readonly property color woodMid: "#6b4a33"
    readonly property color woodLow: "#4a3322"

    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        visible: !root.detailOpen

        // ---- header: scanning note, count, search, add folder ----
        Flow {
            Layout.fillWidth: true
            spacing: 14
            Label {
                visible: root.status.scanning
                text: root.status.total > 0 ? "scanning " + root.status.done + " / " + root.status.total : "scanning…"
                color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) }
            }
            Label {
                visible: !root.status.scanning && hop.games.total > 0
                objectName: "gamesCount"
                text: hop.games.count === hop.games.total
                      ? hop.games.total + (hop.games.total === 1 ? " game" : " games")
                      : hop.games.count + " of " + hop.games.total + " games"
                color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) }
            }
            TextField {
                id: gamesSearch
                objectName: "gamesSearch"
                visible: hop.games.total > 0
                implicitWidth: Theme.compact ? 150 : 240
                placeholderText: "search games"
                placeholderTextColor: Theme.dim
                color: Theme.text
                selectByMouse: true
                font { family: Theme.font; pixelSize: Theme.fs(15) }
                onTextEdited: hop.games.textFilter = text
                Keys.onEscapePressed: { text = ""; hop.games.textFilter = "" }
                Accessible.name: "search your games"
                background: Rectangle { color: Theme.panel; border { width: gamesSearch.activeFocus ? 2 : 0; color: Theme.accent } }
            }
            AbstractButton {
                id: addByHand
                objectName: "gamesAddByHand"
                visible: !hop.guest && !handForm.visible
                hoverEnabled: true
                onClicked: handForm.open()
                contentItem: Label {
                    text: "+ add a game by hand"
                    color: addByHand.hovered ? Theme.accent : Theme.muted
                    font { family: Theme.font; pixelSize: Theme.fs(14); underline: addByHand.hovered }
                }
                background: Item {}
            }
            AbstractButton {
                id: addMore
                objectName: "gamesAddFolder"
                visible: hop.games.total > 0 && !hop.guest
                hoverEnabled: true
                onClicked: root.addFolderRequested()
                contentItem: Label {
                    text: "+ add folder"
                    color: addMore.hovered ? Theme.accent : Theme.muted
                    font { family: Theme.font; pixelSize: Theme.fs(14); underline: addMore.hovered }
                }
                background: Item {}
            }
        }

        // ---- add a game by hand: for what a folder cannot show (a game from another launcher, say) ----
        Flow {
            id: handForm
            objectName: "gamesHandForm"
            visible: false
            Layout.fillWidth: true
            spacing: 10
            function open() { visible = true; handName.text = ""; handNote.text = ""; handName.forceActiveFocus() }
            function submit() {
                const err = hop.addManualGame(handName.text, handSystem.choices[handSystem.currentIndex].key)
                handNote.text = err
                if (err === "") visible = false
            }
            TextField {
                id: handName
                objectName: "gamesHandName"
                width: Math.min(320, root.width)
                placeholderText: "game name"; placeholderTextColor: Theme.dim
                maximumLength: 200
                selectByMouse: true
                color: Theme.text
                font { family: Theme.font; pixelSize: Theme.fs(15) }
                onAccepted: handForm.submit()
                Keys.onEscapePressed: handForm.visible = false
                background: Rectangle { color: Theme.panel; border { width: handName.activeFocus ? 2 : 0; color: Theme.accent } }
            }
            ThemedCombo {
                id: handSystem
                objectName: "gamesHandSystem"
                readonly property var choices: hop.gameSystemChoices()
                model: choices.map(function (c) { return c.name })
                width: Math.min(190, root.width)
                font { family: Theme.font; pixelSize: Theme.fs(14) }
                Component.onCompleted: { for (let i = 0; i < choices.length; ++i) if (choices[i].key === "pcother") currentIndex = i }
            }
            Button {
                objectName: "gamesHandAdd"
                text: "add"
                onClicked: handForm.submit()
                contentItem: Label { text: parent.text; color: Theme.ink; font { family: Theme.font; pixelSize: Theme.fs(15); weight: Font.Medium } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { implicitWidth: 80; implicitHeight: 40; color: parent.hovered ? Qt.lighter(Theme.accent, 1.08) : Theme.accent }
            }
            Label { id: handNote; objectName: "gamesHandNote"; color: Theme.muted; visible: text !== ""; font { family: Theme.font; pixelSize: Theme.fs(13) } }
        }

        // ---- the systems: one chip each, the shelf narrows to it ----
        Flickable {
            id: chipFlick
            objectName: "gamesSystems"
            visible: hop.games.total > 0
            Layout.fillWidth: true
            implicitHeight: 40
            contentWidth: chipRow.implicitWidth; contentHeight: height
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            Row {
                id: chipRow
                spacing: 8
                height: chipFlick.height
                Repeater {
                    model: [{ key: "", name: "all", color: "", count: hop.games.total }].concat(hop.games.systems)
                    delegate: AbstractButton {
                        id: chip
                        required property var modelData
                        readonly property bool on: hop.games.systemFilter === modelData.key
                        objectName: "gamesChip_" + (modelData.key === "" ? "all" : modelData.key)
                        anchors.verticalCenter: parent.verticalCenter
                        implicitHeight: 32
                        leftPadding: 13; rightPadding: 13
                        implicitWidth: chipRow2.implicitWidth + 26
                        hoverEnabled: true
                        onClicked: hop.games.systemFilter = modelData.key
                        Accessible.name: modelData.name + ", " + modelData.count
                        Accessible.role: Accessible.RadioButton
                        background: Rectangle {
                            radius: height / 2
                            color: chip.on ? Theme.accent : (chip.hovered ? Theme.hover : "transparent")
                            border { width: chip.on ? 0 : 1; color: Theme.borderSoft }
                        }
                        contentItem: Row {
                            id: chipRow2
                            spacing: 7
                            Rectangle {
                                visible: chip.modelData.color !== ""
                                width: 10; height: 10; radius: 5
                                anchors.verticalCenter: parent.verticalCenter
                                color: chip.modelData.color
                                border { width: 1; color: Qt.rgba(1, 1, 1, 0.35) }
                            }
                            Label {
                                text: chip.modelData.name + "  " + chip.modelData.count
                                color: chip.on ? Theme.ink : Theme.text2
                                font { family: Theme.font; pixelSize: Theme.fs(14); weight: chip.on ? Font.Medium : Font.Normal }
                            }
                        }
                    }
                }
            }
        }

        // ---- nothing yet ----
        ColumnLayout {
            visible: hop.games.total === 0 && !root.status.scanning
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.topMargin: 30
            spacing: 18
            Label {
                objectName: "gamesEmptyTitle"
                text: root.folders.length === 0 ? "build your game shelf" : "no games found yet"
                color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(40); weight: Font.Light }
                wrapMode: Text.WordWrap; Layout.fillWidth: true
            }
            Label {
                text: root.folders.length === 0
                      ? "add the folders your games live in: a Steam library (the folder with steamapps in it), a GOG games folder, or folders of roms and disc images from your emulators. hop-tunes finds them and puts them on the shelf by system: PC, Switch, Wii U, Wii, GameCube, N64, Game Boy, PSP, PS2, PS3 and more. games from other launchers (Epic Games, for one, is not scanned) can be added by hand under third-party PC. it only reads file names: it never opens or runs a game, and nothing is uploaded."
                      : "hop-tunes looked in " + root.folders.join(", ") + " and found no games. roms are found by file type (.sfc, .z64, .gba, .nsp ...), and disc images (.iso, .cue, .chd, .zip) when they sit in a folder named after the system, like “PS2” or “Wii U”."
                color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(16) }
                wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.maximumWidth: 700
            }
            Button {
                objectName: "gamesChooseFolder"
                visible: !hop.guest
                text: "choose a games folder…"
                onClicked: root.addFolderRequested()
                contentItem: Label { text: parent.text; color: Theme.ink; font { family: Theme.font; pixelSize: Theme.fs(18); weight: Font.Medium } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { implicitHeight: 52; implicitWidth: 300; color: parent.hovered ? Qt.lighter(Theme.accent, 1.08) : Theme.accent }
            }
            Button {
                objectName: "gamesAddByHandEmpty"
                visible: !hop.guest && !handForm.visible
                text: "or add a game by hand"
                onClicked: handForm.open()
                contentItem: Label { text: parent.text; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(16) } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { implicitHeight: 44; implicitWidth: 230; color: "transparent"; border { width: 2; color: Theme.border } }
            }
            Item { Layout.fillHeight: true }
        }

        // ---- the shelf ----
        GridView {
            id: shelf
            objectName: "gamesShelf"
            visible: hop.games.total > 0
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true
            model: hop.games
            readonly property int columns: Theme.compact ? 3 : Math.max(3, Math.floor(width / 176))
            cellWidth: width / columns
            readonly property real coverW: cellWidth - 22
            cellHeight: coverW * 1.5 + 12 + 46
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            keyNavigationEnabled: true
            Repeater {     // one plank per row of the shelf, behind the covers
                parent: shelf.contentItem
                model: Math.ceil(shelf.count / shelf.columns)
                delegate: Rectangle {
                    required property int index
                    z: -1
                    x: 0; width: shelf.width
                    y: index * shelf.cellHeight + shelf.coverW * 1.5 + 4
                    height: 12
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: root.woodTop }
                        GradientStop { position: 0.35; color: root.woodMid }
                        GradientStop { position: 1.0; color: root.woodLow }
                    }
                }
            }
            delegate: Item {
                id: cell
                required property int index
                required property string path
                required property string title
                required property string systemName
                required property string systemColor
                required property string cover
                required property bool customCover
                width: shelf.cellWidth; height: shelf.cellHeight
                objectName: "gameCard_" + index

                // (the planks themselves are drawn by the shelf, one per row, so a half-empty last row still stands on a whole plank)
                Item {
                    id: plank
                    anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: shelf.coverW * 1.5 + 4 }
                    height: 12
                }
                Rectangle {   // the shadow the plank throws on the wall below it
                    anchors { left: parent.left; right: parent.right; top: plank.bottom }
                    height: 5
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.28) }
                        GradientStop { position: 1.0; color: "transparent" }
                    }
                }

                AbstractButton {
                    id: card
                    x: 11; y: 4
                    width: shelf.coverW; height: shelf.coverW * 1.5
                    hoverEnabled: true
                    onClicked: { shelf.currentIndex = cell.index; root.openPath = cell.path }
                    Accessible.name: cell.title + ", " + cell.systemName
                    Accessible.role: Accessible.Button
                    scale: card.hovered ? 1.03 : 1.0
                    transformOrigin: Item.Bottom
                    Behavior on scale { NumberAnimation { duration: 110 } }
                    background: Rectangle {
                        color: cell.systemColor
                        // the drawn cover, for a game without a picture
                        Column {
                            visible: cell.cover === ""
                            anchors { fill: parent; margins: 8 }
                            spacing: 6
                            Item { width: 1; height: parent.height * 0.12 }
                            Label {
                                width: parent.width
                                text: cell.title
                                color: "white"; wrapMode: Text.WordWrap; maximumLineCount: 5; elide: Text.ElideRight
                                horizontalAlignment: Text.AlignHCenter
                                font { family: Theme.font; pixelSize: Math.max(11, Theme.fs(15)); weight: Font.DemiBold }
                            }
                            Label {
                                width: parent.width
                                text: cell.systemName
                                color: Qt.rgba(1, 1, 1, 0.7); elide: Text.ElideRight
                                horizontalAlignment: Text.AlignHCenter
                                font { family: Theme.font; pixelSize: Math.max(10, Theme.fs(12)) }
                            }
                        }
                    }
                    contentItem: Image {
                        visible: cell.cover !== ""
                        source: cell.cover
                        asynchronous: true
                        cache: false
                        fillMode: Image.PreserveAspectCrop
                        sourceSize.width: 360
                    }
                    Rectangle {   // a thin edge, so the cover reads as a box on the shelf
                        anchors.fill: parent; color: "transparent"
                        border { width: card.activeFocus ? 2 : 1; color: card.activeFocus ? Theme.accent : Qt.rgba(0, 0, 0, 0.45) }
                    }
                }
                Label {
                    anchors { left: parent.left; right: parent.right; top: plank.bottom; topMargin: 8; leftMargin: 11; rightMargin: 11 }
                    text: cell.title
                    color: card.hovered ? Theme.accent : Theme.text
                    elide: Text.ElideRight; maximumLineCount: 2; wrapMode: Text.WordWrap
                    font { family: Theme.font; pixelSize: Theme.fs(13) }
                }
            }
        }
    }

    // ---- one game's page ----
    Flickable {
        id: page
        objectName: "gameDetail"
        visible: root.detailOpen
        anchors.fill: parent
        contentWidth: width; contentHeight: pageCol.implicitHeight + 24
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        readonly property bool editable: root.detail.editable === true
        property string coverNote: ""

        ColumnLayout {
            id: pageCol
            width: Math.min(page.width, 760)
            spacing: 12

            AbstractButton {
                id: backBtn
                objectName: "gameBack"
                hoverEnabled: true
                onClicked: root.closeDetail()
                Accessible.name: "back to the shelf"
                contentItem: Label { text: "‹ games"; color: backBtn.hovered ? Theme.accent : Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(16) } }
                background: Item {}
            }

            Flow {
                Layout.fillWidth: true
                spacing: 24
                Rectangle {
                    id: bigCover
                    width: Theme.compact ? Math.min(page.width - 2 * Theme.pad, 220) : 240
                    height: width * 1.5
                    color: root.detail.systemColor || Theme.panel
                    Label {
                        visible: !bigImg.visible
                        anchors { fill: parent; margins: 14 }
                        text: root.detail.title || ""
                        color: "white"; wrapMode: Text.WordWrap; elide: Text.ElideRight
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        font { family: Theme.font; pixelSize: Theme.fs(22); weight: Font.DemiBold }
                    }
                    Image {
                        id: bigImg
                        anchors.fill: parent
                        visible: !!root.detail.cover
                        source: root.detail.cover || ""
                        asynchronous: true; cache: false
                        fillMode: Image.PreserveAspectCrop
                        sourceSize.width: 600
                    }
                    Rectangle { anchors.fill: parent; color: "transparent"; border { width: 1; color: Qt.rgba(0, 0, 0, 0.45) } }
                }

                ColumnLayout {
                    width: Math.max(220, Math.min(page.width, 760) - (Theme.compact ? 0 : 240 + 24))
                    spacing: 10
                    Label {
                        objectName: "gameTitle"
                        Layout.fillWidth: true
                        text: root.detail.title || ""
                        color: Theme.strong; wrapMode: Text.WordWrap
                        font { family: Theme.font; pixelSize: Theme.fs(30); weight: Font.Light }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: (root.detail.systemName || "") + (root.detail.source === "steam" ? " · Steam app " + root.detail.appId
                                                : root.detail.source === "gog" ? " · GOG game " + root.detail.appId
                                                : root.detail.manual === true ? " · added by hand" : " · rom or disc image")
                        color: Theme.muted; wrapMode: Text.WordWrap
                        font { family: Theme.font; pixelSize: Theme.fs(15) }
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: root.detail.manual !== true
                        text: root.detail.path || ""
                        color: Theme.dim; elide: Text.ElideMiddle
                        font { family: Theme.font; pixelSize: Theme.fs(12) }
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: 10
                        Button {
                            objectName: "gameChangeCover"
                            visible: page.editable
                            text: "change cover…"
                            onClicked: coverDialog.open()
                            contentItem: Label { text: parent.text; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(15) } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { implicitWidth: 150; implicitHeight: 40; color: "transparent"; border { width: 2; color: Theme.border } }
                        }
                        Button {
                            objectName: "gameRemoveManual"
                            visible: page.editable && root.detail.manual === true
                            text: "remove from the shelf"
                            onClicked: { const p = root.openPath; root.closeDetail(); hop.removeManualGame(p) }
                            contentItem: Label { text: parent.text; color: Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(15) } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { implicitWidth: 180; implicitHeight: 40; color: parent.hovered ? Theme.hover : "transparent" }
                        }
                        Button {
                            objectName: "gameResetCover"
                            visible: page.editable && root.detail.customCover === true
                            text: "use the original"
                            onClicked: hop.clearGameCover(root.openPath)
                            contentItem: Label { text: parent.text; color: Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(15) } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { implicitWidth: 150; implicitHeight: 40; color: parent.hovered ? Theme.hover : "transparent" }
                        }
                    }
                    Label {
                        objectName: "gameCoverNote"
                        Layout.fillWidth: true
                        text: page.coverNote
                        visible: text !== ""
                        color: Theme.muted; wrapMode: Text.WordWrap
                        font { family: Theme.font; pixelSize: Theme.fs(13) }
                    }
                }
            }

            Label { text: "title"; color: Theme.muted; visible: page.editable; font { family: Theme.font; pixelSize: Theme.fs(13) } }
            TextField {
                id: titleEdit
                objectName: "gameTitleEdit"
                visible: page.editable
                Layout.fillWidth: true
                maximumLength: 200
                selectByMouse: true
                color: Theme.text
                font { family: Theme.font; pixelSize: Theme.fs(17) }
                placeholderText: root.detail.scannedTitle || "title"; placeholderTextColor: Theme.dim
                text: root.detail.renamed ? root.detail.title : ""
                onEditingFinished: hop.setGameField(root.openPath, "title", text)
                background: Rectangle { color: Theme.panel; border { width: titleEdit.activeFocus ? 2 : 0; color: Theme.accent } }
            }
            Label { text: "system (for a file hop-tunes could not place, or to move a game)"; color: Theme.muted; visible: page.editable; wrapMode: Text.WordWrap; Layout.fillWidth: true; font { family: Theme.font; pixelSize: Theme.fs(13) } }
            ThemedCombo {
                id: systemPick
                objectName: "gameSystemPick"
                visible: page.editable
                Layout.fillWidth: true; Layout.maximumWidth: 360
                readonly property var choices: [{ key: "", name: "as hop-tunes found it" }].concat(hop.gameSystemChoices())
                model: choices.map(function (c) { return c.name })
                font { family: Theme.font; pixelSize: Theme.fs(15) }
                currentIndex: {
                    if (root.detail.systemChosen !== true) return 0
                    for (let i = 1; i < choices.length; ++i) if (choices[i].key === root.detail.system) return i
                    return 0
                }
                onActivated: function (i) { hop.setGameField(root.openPath, "system", choices[i].key) }
            }
            Label { text: "notes"; color: Theme.muted; visible: page.editable; font { family: Theme.font; pixelSize: Theme.fs(13) } }
            TextArea {
                id: notesEdit
                objectName: "gameNotes"
                visible: page.editable
                Layout.fillWidth: true; Layout.preferredHeight: 130
                wrapMode: TextArea.Wrap
                selectByMouse: true
                color: Theme.text
                font { family: Theme.font; pixelSize: Theme.fs(15) }
                placeholderText: "your own notes: how far you got, which version, a cheat …"; placeholderTextColor: Theme.dim
                text: root.detail.notes || ""
                onActiveFocusChanged: if (!activeFocus) hop.setGameField(root.openPath, "notes", text)
                background: Rectangle { color: Theme.panel; border { width: notesEdit.activeFocus ? 2 : 0; color: Theme.accent } }
            }
            Label {
                Layout.fillWidth: true
                text: "your title, system, notes and cover are saved in this profile, on this computer only. hop-tunes never opens or runs a game."
                color: Theme.muted; wrapMode: Text.WordWrap
                font { family: Theme.font; pixelSize: Theme.fs(13) }
            }
        }
    }

    FileDialog {
        id: coverDialog
        title: "choose a cover picture"
        nameFilters: ["pictures (*.png *.jpg *.jpeg *.webp *.bmp)"]
        onAccepted: {
            const err = hop.setGameCover(root.openPath, selectedFile)
            page.coverNote = err
        }
    }
    Connections {
        target: hop
        function onGameDetailChanged() { root.detailRev++; page.coverNote = "" }
    }
}
