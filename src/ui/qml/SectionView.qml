// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One collection section (pictures, audiobooks, ebooks, podcasts): its own folders and its
// own "choose a folder" button. Pictures show a grid with a viewer; the others show a list.
Item {
    id: root

    property string kind: "pictures"
    property var model
    signal addFolderRequested(string kind)
    signal itemActivated(int row)

    readonly property var info: ({
        "pictures":   { noun: "picture",   plural: "pictures",   hint: "jpg, png, webp, gif and a few others, anywhere under your picture folders." },
        "audiobooks": { noun: "audiobook", plural: "audiobooks", hint: "m4b, mp3, m4a and other audio files, anywhere under your audiobook folders." },
        "ebooks":     { noun: "book",      plural: "books",      hint: "epub and pdf files, anywhere under your book folders." },
        "podcasts":   { noun: "podcast",   plural: "podcasts",   hint: "audio episodes, anywhere under your podcast folders. each folder is a show." }
    })
    readonly property var meta: info[kind]
    readonly property var status: hop.sectionStatus[kind] || ({ scanning: false, done: 0, total: 0 })
    readonly property var folders: hop.sectionFolders[kind] || []

    // Pictures are shown as albums, one per folder; an open album shows its own pictures.
    readonly property bool albumList: kind === "pictures" && hop.pictureAlbum === ""
    readonly property bool albumOpen: kind === "pictures" && hop.pictureAlbum !== ""
    readonly property var openAlbum: {
        const list = hop.pictureAlbums
        for (let i = 0; i < list.length; ++i) if (list[i].dir === hop.pictureAlbum) return list[i]
        return ({ name: "", count: 0 })
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Label {
                visible: root.status.scanning
                text: root.status.total > 0 ? "scanning " + root.status.done + " / " + root.status.total : "scanning…"
                color: Theme.muted; font { family: Theme.font; pixelSize: 14 }
            }
            AbstractButton {
                id: albumBack
                objectName: "pictureAlbumBack"
                visible: root.albumOpen
                hoverEnabled: true
                onClicked: hop.closePictureAlbum()
                Accessible.name: "back to albums"
                contentItem: Label { text: "‹ albums"; color: albumBack.hovered ? hop.accent : Theme.text2; font { family: Theme.font; pixelSize: 16 } }
                background: Item {}
            }
            Label {
                visible: root.albumOpen && !headEdit.visible
                text: root.openAlbum.name
                color: Theme.text; elide: Text.ElideRight; font { family: Theme.font; pixelSize: 20 }
            }
            TextField {
                id: headEdit
                objectName: "pictureAlbumNameEdit"
                visible: false
                implicitWidth: 320
                maximumLength: 60
                selectByMouse: true
                color: Theme.text
                font { family: Theme.font; pixelSize: 18 }
                placeholderText: "album name"; placeholderTextColor: Theme.dim
                onAccepted: { hop.renamePictureAlbum(hop.pictureAlbum, text); visible = false }
                Keys.onEscapePressed: visible = false
                onActiveFocusChanged: if (!activeFocus) visible = false
                background: Rectangle { color: Theme.panel; border { width: 2; color: hop.accent } }
            }
            AbstractButton {
                id: headRename
                objectName: "pictureAlbumRename"
                visible: root.albumOpen && !headEdit.visible && !hop.guest
                hoverEnabled: true
                onClicked: { headEdit.text = root.openAlbum.name; headEdit.visible = true; headEdit.forceActiveFocus(); headEdit.selectAll() }
                Accessible.name: "rename album"
                contentItem: Label { text: "rename"; color: headRename.hovered ? hop.accent : Theme.muted
                                     font { family: Theme.font; pixelSize: 13; underline: headRename.hovered } }
                background: Item {}
            }
            Item { Layout.fillWidth: true }
            Label {
                visible: !root.status.scanning && root.albumList && root.model.count > 0
                text: hop.pictureAlbums.length + (hop.pictureAlbums.length === 1 ? " album, " : " albums, ") + root.model.count + " " + (root.model.count === 1 ? root.meta.noun : root.meta.plural)
                color: Theme.muted; font { family: Theme.font; pixelSize: 14 }
            }
            Label {
                visible: !root.status.scanning && root.model.count > 0 && !root.albumList
                text: (root.albumOpen ? root.openAlbum.count : root.model.count) + " " + (root.model.count === 1 ? root.meta.noun : root.meta.plural)
                color: Theme.muted; font { family: Theme.font; pixelSize: 14 }
            }
            AbstractButton {
                id: addMore
                objectName: "sectionAddFolder_" + root.kind
                visible: root.model.count > 0 && !hop.guest
                hoverEnabled: true
                onClicked: root.addFolderRequested(root.kind)
                contentItem: Label {
                    text: "+ add folder"
                    color: addMore.hovered ? hop.accent : Theme.muted
                    font { family: Theme.font; pixelSize: 14; underline: addMore.hovered }
                }
                background: Item {}
            }
        }

        // nothing found yet: the section's own folder button
        ColumnLayout {
            visible: root.model.count === 0 && !root.status.scanning
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.topMargin: 40
            spacing: 18
            Label {
                text: root.folders.length === 0 ? "add a folder to find " + root.meta.plural : "no " + root.meta.plural + " found yet"
                color: Theme.text; font { family: Theme.font; pixelSize: 40; weight: Font.Light }
            }
            Label {
                text: root.meta.hint
                color: Theme.muted; font { family: Theme.font; pixelSize: 16 }
                wrapMode: Text.WordWrap; Layout.maximumWidth: 640
            }
            Button {
                objectName: "sectionChooseFolder_" + root.kind
                text: "choose a " + root.meta.noun + " folder…"
                onClicked: root.addFolderRequested(root.kind)
                contentItem: Label { text: parent.text; color: Theme.ink; font { family: Theme.font; pixelSize: 18; weight: Font.Medium } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { implicitHeight: 52; implicitWidth: 300; color: parent.hovered ? Qt.lighter(hop.accent, 1.08) : hop.accent }
            }
            Item { Layout.fillHeight: true }
        }

        // pictures: one slot per folder, like a photo album shelf
        GridView {
            id: albums
            objectName: "pictureAlbums"
            visible: root.albumList && root.model.count > 0
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true
            model: root.albumList ? hop.pictureAlbums : []
            readonly property int columns: Math.max(2, Math.floor(width / 260))
            cellWidth: width / columns; cellHeight: cellWidth * 3 / 4 + 52
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            delegate: AbstractButton {
                id: slot
                required property int index
                required property var modelData
                objectName: "pictureAlbum_" + index
                width: albums.cellWidth - 16; height: albums.cellHeight - 8
                hoverEnabled: true
                onClicked: hop.openPictureAlbum(slot.modelData.dir)
                Accessible.name: slot.modelData.name
                background: Item {}
                contentItem: ColumnLayout {
                    spacing: 6
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: width * 3 / 4
                        color: Theme.panel
                        clip: true
                        Image {
                            anchors.fill: parent
                            source: slot.modelData.cover
                            asynchronous: true
                            cache: false
                            fillMode: Image.PreserveAspectCrop
                            sourceSize.width: 520
                            opacity: slot.hovered ? 0.85 : 1.0
                        }
                        Rectangle {
                            anchors { right: parent.right; bottom: parent.bottom; margins: 8 }
                            width: countLabel.implicitWidth + 16; height: 24; radius: 12
                            color: Theme.fade(0.6)
                            Label { id: countLabel; anchors.centerIn: parent; text: slot.modelData.count
                                    color: "white"; font { family: Theme.font; pixelSize: 13 } }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Label { visible: !slotEdit.visible; text: slot.modelData.name; Layout.fillWidth: true; elide: Text.ElideRight
                                color: slot.hovered ? hop.accent : Theme.text; font { family: Theme.font; pixelSize: 16 } }
                        TextField {
                            id: slotEdit
                            objectName: "pictureAlbumEdit_" + slot.index
                            visible: false
                            Layout.fillWidth: true
                            maximumLength: 60
                            selectByMouse: true
                            color: Theme.text
                            font { family: Theme.font; pixelSize: 15 }
                            onAccepted: { hop.renamePictureAlbum(slot.modelData.dir, text); visible = false }
                            Keys.onEscapePressed: visible = false
                            onActiveFocusChanged: if (!activeFocus) visible = false
                            background: Rectangle { color: Theme.panel; border { width: 2; color: hop.accent } }
                        }
                        AbstractButton {
                            id: slotRename
                            objectName: "pictureAlbumRename_" + slot.index
                            visible: (slot.hovered || slotRename.hovered) && !slotEdit.visible && !hop.guest
                            hoverEnabled: true
                            onClicked: { slotEdit.text = slot.modelData.name; slotEdit.visible = true; slotEdit.forceActiveFocus(); slotEdit.selectAll() }
                            Accessible.name: "rename album"
                            contentItem: Label { text: "rename"; color: slotRename.hovered ? hop.accent : Theme.muted
                                                 font { family: Theme.font; pixelSize: 12; underline: slotRename.hovered } }
                            background: Item {}
                        }
                    }
                }
            }
        }

        // an open album: a grid of thumbnails
        GridView {
            id: grid
            visible: root.albumOpen && root.model.count > 0
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true
            model: root.albumOpen ? hop.pictureAlbumItems : null
            readonly property int columns: Math.max(2, Math.floor(width / 220))
            cellWidth: width / columns; cellHeight: cellWidth * 3 / 4 + 28
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            delegate: AbstractButton {
                id: tile
                required property int index
                required property string title
                required property string fileUrl
                objectName: "pictureTile_" + index
                width: grid.cellWidth - 12; height: grid.cellHeight - 4
                hoverEnabled: true
                onClicked: root.itemActivated(tile.index)
                Accessible.name: title
                background: Item {}
                contentItem: ColumnLayout {
                    spacing: 4
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: width * 3 / 4
                        color: Theme.panel
                        clip: true
                        Image {
                            anchors.fill: parent
                            source: tile.fileUrl
                            asynchronous: true
                            cache: false
                            fillMode: Image.PreserveAspectCrop
                            sourceSize.width: 440
                            opacity: tile.hovered ? 0.85 : 1.0
                        }
                    }
                    Label { text: tile.title; Layout.fillWidth: true; elide: Text.ElideRight
                            color: Theme.text; font { family: Theme.font; pixelSize: 13 } }
                }
            }
        }

        // audiobooks, ebooks, podcasts: a list with progress
        ListView {
            id: list
            visible: root.kind !== "pictures" && root.model.count > 0
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true
            model: root.kind !== "pictures" ? root.model : null
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            delegate: AbstractButton {
                id: row
                required property int index
                required property string title
                required property string group
                required property string duration
                required property string sizeText
                required property real progress
                required property bool played
                required property string path
                objectName: "sectionRow_" + root.kind + "_" + index
                width: list.width; height: 64
                hoverEnabled: true
                onClicked: root.itemActivated(row.index)
                Accessible.name: title
                background: Rectangle { color: row.hovered ? Theme.hover : "transparent" }
                contentItem: ColumnLayout {
                    spacing: 2
                    Item { Layout.preferredHeight: 6 }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: row.title; Layout.fillWidth: true; elide: Text.ElideRight
                                color: row.played ? Theme.muted : Theme.text; font { family: Theme.font; pixelSize: 17 } }
                        AbstractButton {
                            id: playedBtn
                            objectName: "sectionPlayed_" + root.kind + "_" + row.index
                            visible: (root.kind === "audiobooks" || root.kind === "podcasts") && !hop.guest && (row.hovered || row.played)
                            hoverEnabled: true
                            implicitHeight: 18
                            Accessible.name: row.played ? "mark unplayed" : "mark played"
                            onClicked: hop.toggleItemPlayed(row.path)
                            contentItem: Label {
                                text: row.played ? "played ✓" : "mark played"
                                color: playedBtn.hovered ? hop.accent : Theme.muted
                                font { family: Theme.font; pixelSize: 12; underline: playedBtn.hovered }
                            }
                            background: Item {}
                        }
                        Label { text: row.duration !== "" ? row.duration : row.sizeText
                                color: Theme.muted; font { family: Theme.font; pixelSize: 14 } }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: row.group; Layout.fillWidth: true; elide: Text.ElideRight
                                color: Theme.muted; font { family: Theme.font; pixelSize: 13 } }
                        Label { visible: row.progress > 0
                                text: Math.round(row.progress * 100) + "%"
                                color: hop.accent; font { family: Theme.font; pixelSize: 13 } }
                    }
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 2
                        color: Theme.border
                        visible: row.progress > 0
                        Rectangle { width: parent.width * Math.min(1, row.progress); height: parent.height; color: hop.accent }
                    }
                    Item { Layout.fillHeight: true }
                }
            }
        }
    }
}
