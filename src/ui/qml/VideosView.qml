// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// collection › videos (spec §5b): a grid of what VideoScanner has found under the
// profile's folders — the same folders music is scanned from.
Item {
    id: root

    signal addFolderRequested(string category)

    readonly property var labels: ({ movies: "movies", shows: "shows", cartoons: "cartoons", anime: "anime", other: "other videos" })
    readonly property var hints: ({
        movies: "feature films.",
        shows: "series and episodes.",
        cartoons: "western animation.",
        anime: "anime series and films.",
        other: "everything else, like videos you saved from YouTube to keep."
    })
    readonly property string cat: hop.videoCategory
    readonly property bool slotOpen: hop.videoSlot !== ""
    readonly property var openSlotInfo: {
        const list = hop.videoSlots
        for (let i = 0; i < list.length; ++i) if (list[i].dir === hop.videoSlot) return list[i]
        return ({ name: "", count: 0 })
    }

    // Per-profile thumbnail override, like the album "image" button in collection › music.
    // The row is set right before open().
    FileDialog {
        id: thumbDialog
        property int row: -1
        title: "choose video thumbnail"
        nameFilters: ["pictures (*.png *.jpg *.jpeg)"]
        onAccepted: hop.setVideoThumb(row, selectedFile)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        visible: hop.videoDetailRow < 0   // the grid; a movie's page replaces it while open

        // movies · shows · cartoons · anime · other videos
        PillRow {
            visible: Theme.compact
            Layout.fillWidth: true
            namePrefix: "pillCat_"
            items: hop.videoCategories.map(function (c) { return [c === "other" ? "other" : root.labels[c], c] })
            current: root.cat
            onPicked: function (v) { hop.videoCategory = v }
        }
        RowLayout {
            visible: !Theme.compact
            Layout.fillWidth: true
            spacing: 22
            Repeater {
                model: hop.videoCategories
                delegate: AbstractButton {
                    id: catTab
                    required property string modelData
                    objectName: "videoCat_" + modelData
                    hoverEnabled: true
                    onClicked: hop.videoCategory = modelData
                    Accessible.name: root.labels[modelData]
                    contentItem: Label { text: root.labels[catTab.modelData]
                                         color: root.cat === catTab.modelData ? Theme.accent : (catTab.hovered ? Theme.text2 : Theme.dim)
                                         font { family: Theme.font; pixelSize: Theme.fs(17) } }
                    background: Item {}
                }
            }
            Item { Layout.fillWidth: true }
            AbstractButton {
                id: addCatFolder
                objectName: "videoCatAddFolder"
                hoverEnabled: true
                visible: !hop.guest && (hop.videoCategoryFolders[root.cat] || []).length > 0
                onClicked: root.addFolderRequested(root.cat)
                contentItem: Label { text: "+ add folder"; color: addCatFolder.hovered ? Theme.accent : Theme.muted
                                     font { family: Theme.font; pixelSize: Theme.fs(14); underline: addCatFolder.hovered } }
                background: Item {}
            }
        }

        RowLayout {
            Layout.fillWidth: true
            AbstractButton {
                id: slotBack
                objectName: "videoSlotBack"
                visible: root.slotOpen
                hoverEnabled: true
                onClicked: hop.closeVideoSlot()
                Accessible.name: "back to folders"
                contentItem: Label { text: "\u2039 folders"; color: slotBack.hovered ? Theme.accent : Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(16) } }
                background: Item {}
            }
            Label {
                visible: root.slotOpen && !slotHeadEdit.visible
                text: root.openSlotInfo.name
                color: Theme.text; elide: Text.ElideRight; Layout.maximumWidth: 420
                font { family: Theme.font; pixelSize: Theme.fs(20) }
            }
            TextField {
                id: slotHeadEdit
                objectName: "videoSlotNameEdit"
                visible: false
                implicitWidth: 320
                maximumLength: 60
                selectByMouse: true
                color: Theme.text
                font { family: Theme.font; pixelSize: Theme.fs(18) }
                placeholderText: "folder name"; placeholderTextColor: Theme.dim
                onAccepted: { hop.renameSlot(hop.videoSlot, text); visible = false }
                Keys.onEscapePressed: visible = false
                onActiveFocusChanged: if (!activeFocus) visible = false
                background: Rectangle { color: Theme.panel; border { width: 2; color: Theme.accent } }
            }
            AbstractButton {
                id: slotHeadRename
                objectName: "videoSlotRename"
                visible: root.slotOpen && !slotHeadEdit.visible && !hop.guest
                hoverEnabled: true
                onClicked: { slotHeadEdit.text = root.openSlotInfo.name; slotHeadEdit.visible = true; slotHeadEdit.forceActiveFocus(); slotHeadEdit.selectAll() }
                Accessible.name: "rename folder"
                contentItem: Label { text: "rename"; color: slotHeadRename.hovered ? Theme.accent : Theme.muted
                                     font { family: Theme.font; pixelSize: Theme.fs(13); underline: slotHeadRename.hovered } }
                background: Item {}
            }
            Label {
                visible: hop.videoScanning
                text: hop.videoScanTotal > 0 ? "scanning videos " + hop.videoScanDone + " / " + hop.videoScanTotal : "scanning videos…"
                color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) }
            }
            Item { Layout.fillWidth: true }
            Label {
                visible: !hop.videoScanning && hop.videos.count > 0 && !Theme.compact
                text: root.slotOpen ? hop.videos.count + (hop.videos.count === 1 ? " video" : " videos")
                                    : hop.videoSlots.length + (hop.videoSlots.length === 1 ? " folder, " : " folders, ") + hop.videos.count + (hop.videos.count === 1 ? " video" : " videos")
                color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) }
            }
        }

        ColumnLayout {
            visible: hop.videos.count === 0 && !hop.videoScanning
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.topMargin: 40
            spacing: 18
            Label {
                text: (hop.videoCategoryFolders[root.cat] || []).length === 0 ? "add a folder for " + root.labels[root.cat] : "no " + root.labels[root.cat] + " found yet"
                color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(40); weight: Font.Light }
            }
            Label {
                text: root.hints[root.cat] + " mkv, mp4, webm, mov, avi, wmv and a few others, anywhere under the folders you add here."
                color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(16) }
                wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.maximumWidth: 640
            }
            Button {
                objectName: root.cat === "movies" ? "videosChooseFolder" : "videosChooseFolder_" + root.cat
                text: "choose a folder for " + root.labels[root.cat] + "…"
                onClicked: root.addFolderRequested(root.cat)
                contentItem: Label { text: parent.text; color: Theme.ink; font { family: Theme.font; pixelSize: Theme.fs(18); weight: Font.Medium } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { implicitHeight: 52; implicitWidth: 260; color: parent.hovered ? Qt.lighter(Theme.accent, 1.08) : Theme.accent }
            }
            Item { Layout.fillHeight: true }
        }

        // each folder is a slot (like pictures): the shelf first, the folder's videos once one is open
        FolderSlots {
            objectName: "videoSlots"
            visible: hop.videos.count > 0 && !root.slotOpen
            Layout.fillWidth: true; Layout.fillHeight: true
            slots: root.slotOpen ? [] : hop.videoSlots
            prefix: "videoSlot"
            aspect: 9 / 16
            onOpened: function (dir) { hop.openVideoSlot(dir) }
            onRenamed: function (dir, name) { hop.renameSlot(dir, name) }
        }

        GridView {
            id: grid
            visible: hop.videos.count > 0 && root.slotOpen
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true
            model: hop.videos
            readonly property int columns: Theme.compact ? 2 : Math.max(2, Math.floor(width / 260))
            cellWidth: width / columns; cellHeight: cellWidth * 9 / 16 + (Theme.compact ? 44 : 70)
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            delegate: AbstractButton {
                id: tile
                required property int index
                required property string title
                required property string path
                required property string thumb
                required property string duration
                required property string resolutionBadge
                required property string codecBadge
                required property bool hdr
                required property bool customThumb
                objectName: "videoTile_" + index
                width: grid.cellWidth - 16; height: grid.cellHeight
                hoverEnabled: true
                onClicked: hop.openVideoDetail(tile.index)
                Accessible.name: title
                background: Item {}
                contentItem: ColumnLayout {
                    spacing: 4
                    VideoThumb {
                        key: tile.title; source: tile.thumb
                        resolutionBadge: tile.resolutionBadge; codecBadge: tile.codecBadge; hdr: tile.hdr
                        hovered: tile.hovered
                        Layout.fillWidth: true; Layout.preferredHeight: width * 9 / 16
                        Label {
                            anchors { right: parent.right; bottom: parent.bottom; margins: 4 }
                            text: tile.duration
                            color: "white"; font { family: Theme.font; pixelSize: Theme.fs(11) }
                        }
                    }
                    Label { text: tile.title; Layout.fillWidth: true; Layout.topMargin: Theme.compact ? 2 : 0; elide: Text.ElideRight
                            color: Theme.text; font { family: Theme.font; pixelSize: Theme.compact ? 17 : Theme.fs(14) } }
                    RowLayout {
                        visible: !hop.guest && !Theme.compact
                        spacing: 12
                        AbstractButton {
                            id: imageBtn
                            objectName: "videoImageBtn_" + tile.index
                            hoverEnabled: true
                            implicitHeight: 18
                            Accessible.name: "change video thumbnail"
                            onClicked: { thumbDialog.row = tile.index; thumbDialog.open() }
                            contentItem: Label {
                                text: "image"
                                color: imageBtn.hovered ? Theme.accent : Theme.muted
                                font { family: Theme.font; pixelSize: Theme.fs(12); underline: imageBtn.hovered }
                            }
                            background: Item {}
                        }
                        AbstractButton {
                            id: resetBtn
                            visible: tile.customThumb
                            objectName: "videoResetBtn_" + tile.index
                            hoverEnabled: true
                            implicitHeight: 18
                            Accessible.name: "use the detected thumbnail"
                            onClicked: hop.clearVideoThumb(tile.index)
                            contentItem: Label {
                                text: "reset"
                                color: resetBtn.hovered ? Theme.accent : Theme.muted
                                font { family: Theme.font; pixelSize: Theme.fs(12); underline: resetBtn.hovered }
                            }
                            background: Item {}
                        }
                    }
                }
            }
        }
    }

    VideoDetailView {
        objectName: "videoDetailView"
        anchors.fill: parent
        visible: hop.videoDetailRow >= 0
    }
}
