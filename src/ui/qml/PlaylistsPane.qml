// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// collection › music › playlists (spec §7): built-in lists, your playlists and smart
// playlists on the left; the selected list on the right.
Item {
    id: root
    signal songMenu(var paths, string path, bool hearted, int row, int count, var playFn, Item src, real x, real y)
    property string message: ""
    readonly property var info: hop.selectedPlaylistInfo

    NamePrompt {
        id: prompt
        property string mode: "new"
        onAccepted: function (name) {
            if (mode === "rename") hop.renamePlaylist(hop.selectedPlaylist, name)
            else { var k = hop.createPlaylist(name, []); if (k) hop.selectPlaylist(k) }
        }
    }
    FileDialog {
        id: importDialog
        title: "import a playlist"
        nameFilters: ["playlists (*.m3u *.m3u8 *.pls *.xspf *.zpl *.wpl)", "all files (*)"]
        onAccepted: root.message = hop.importPlaylist(selectedFile)
    }
    FileDialog {
        id: exportDialog
        title: "export playlist"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "m3u8"
        nameFilters: ["M3U8 playlist (*.m3u8)"]
        onAccepted: root.message = hop.exportPlaylist(hop.selectedPlaylist, selectedFile)
    }
    SmartRulesEditor { id: rulesEditor }

    RowLayout {
        anchors.fill: parent
        spacing: 32

        // ---- list of lists ----
        ColumnLayout {
            Layout.preferredWidth: 260; Layout.maximumWidth: 260; Layout.fillHeight: true
            spacing: 4
            ListView {
                id: lists
                objectName: "playlistList"
                Layout.fillWidth: true; Layout.fillHeight: true
                clip: true
                model: hop.playlists
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { }
                delegate: Column {
                    id: pdc
                    required property var modelData
                    required property int index
                    width: lists.width
                    Label {
                        visible: pdc.index === 0 || hop.playlists[pdc.index - 1].kind !== pdc.modelData.kind
                        text: pdc.modelData.kind === "auto" ? "AUTO" : (pdc.modelData.kind === "smart" ? "SMART" : "YOURS")
                        color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(12); letterSpacing: 2 }
                        topPadding: pdc.index === 0 ? 0 : 14; bottomPadding: 6; leftPadding: 10
                    }
                    ItemDelegate {
                        id: pd
                        readonly property bool sel: pdc.modelData.key === hop.selectedPlaylist
                        objectName: "playlist_" + pdc.modelData.name
                        width: lists.width; height: 38
                        leftPadding: 10
                        onClicked: hop.selectPlaylist(pdc.modelData.key)
                        Accessible.name: pdc.modelData.name
                        background: Rectangle { color: pd.sel || pd.hovered ? Theme.hover : "transparent" }
                        contentItem: Label {
                            text: (pdc.modelData.key === "auto:hearted" ? "♥ " : "") + pdc.modelData.name
                            elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
                            color: pd.sel ? hop.accent : Theme.text; font { family: Theme.font; pixelSize: Theme.fs(17) }
                        }
                    }
                }
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.line }
            PlainButton { objectName: "newPlaylist"; text: "+ new playlist"; Layout.fillWidth: true; onClicked: { prompt.mode = "new"; prompt.ask("new playlist", "", "create") } }
            PlainButton { objectName: "newSmartPlaylist"; text: "+ smart playlist"; Layout.fillWidth: true; onClicked: rulesEditor.edit("", "", "") }
            PlainButton { objectName: "importPlaylist"; text: "import…"; Layout.fillWidth: true; onClicked: importDialog.open() }
        }

        // ---- the selected list ----
        ColumnLayout {
            Layout.fillWidth: true; Layout.fillHeight: true
            spacing: 6
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label { text: root.info.name || ""; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(34); weight: Font.Light } elide: Text.ElideRight; Layout.fillWidth: true }
                    Label {
                        text: (root.info.count || 0) + ((root.info.count === 1) ? " song" : " songs") + (root.info.count ? " · " + root.info.length : "")
                              + (root.info.kind === "smart" ? " · smart, updates by itself" : (root.info.kind === "auto" ? " · built in" : ""))
                        color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) }
                    }
                }
                Button {
                    objectName: "playPlaylist"
                    text: "play"
                    enabled: hop.playlistSongs.count > 0
                    onClicked: hop.playPlaylist(0)
                    contentItem: Label { text: parent.text; color: Theme.ink; font { family: Theme.font; pixelSize: Theme.fs(15); weight: Font.Medium } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { implicitWidth: 100; implicitHeight: 40; color: parent.enabled ? (parent.hovered ? Qt.lighter(hop.accent, 1.08) : hop.accent) : Theme.line }
                }
                PlainButton {
                    visible: root.info.kind === "manual" || root.info.kind === "smart"
                    text: root.info.pinned ? "unpin" : "pin"
                    onClicked: {
                        var id = String(hop.selectedPlaylist).slice(3)
                        root.info.pinned ? hop.unpinTile("playlist", id) : hop.pinTile("playlist", id, root.info.name)
                    }
                }
                PlainButton {
                    text: "⋯"; implicitWidth: 52
                    onClicked: more.popup(0, height)
                    Accessible.name: "more playlist actions"
                    Menu {
                        id: more
                        background: Rectangle { implicitWidth: 220; color: Theme.panel; border { width: 1; color: Theme.line } }
                        component Item_: MenuItem {
                            id: mi
                            contentItem: Label { text: mi.text; leftPadding: 6; color: mi.enabled ? Theme.text : Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(15) } }
                            background: Rectangle { implicitHeight: 36; color: mi.highlighted ? Theme.hover : "transparent" }
                        }
                        Item_ { text: "edit rules…"; visible: root.info.kind === "smart"; height: visible ? implicitHeight : 0
                                onTriggered: rulesEditor.edit(hop.selectedPlaylist, root.info.name, root.info.rules) }
                        Item_ { text: "rename…"; visible: root.info.kind === "manual" || root.info.kind === "smart"; height: visible ? implicitHeight : 0
                                onTriggered: { prompt.mode = "rename"; prompt.ask("rename playlist", root.info.name, "rename") } }
                        Item_ { text: "export as .m3u8…"; enabled: hop.playlistSongs.count > 0
                                onTriggered: {
                                    if (hop.suggestedFolderUrl.toString()) {
                                        exportDialog.currentFolder = hop.suggestedFolderUrl
                                        exportDialog.selectedFile = hop.suggestedFolderUrl + "/" + encodeURIComponent((root.info.name || "playlist").replace(/[\/]/g, "-")) + ".m3u8"
                                    }
                                    exportDialog.open()
                                } }
                        Item_ { text: "delete playlist"; visible: root.info.kind === "manual" || root.info.kind === "smart"; height: visible ? implicitHeight : 0
                                onTriggered: confirmDelete.open() }
                    }
                }
            }
            Label { visible: root.message !== ""; text: root.message; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) } }
            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.line; Layout.topMargin: 8 }

            Label {
                visible: hop.playlistSongs.count === 0
                Layout.topMargin: 24
                text: root.info.kind === "manual" ? "this playlist is empty. right-click songs or albums and choose \\u201cadd to playlist\\u201d."
                    : root.info.kind === "smart" ? "no songs match these rules yet."
                    : root.info.key === "auto:hearted" ? "heart songs with ♡ and they collect here."
                    : "nothing here yet. it fills in as you listen."
                color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(16) }
                wrapMode: Text.WordWrap; Layout.fillWidth: true
            }
            Item { Layout.fillHeight: true; visible: hop.playlistSongs.count === 0 }
            ListView {
                id: songs
                objectName: "playlistSongList"
                Layout.fillWidth: true; Layout.fillHeight: true
                visible: hop.playlistSongs.count > 0
                clip: true
                model: hop.playlistSongs
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { }
                delegate: SongRow {
                    required property int index
                    required property var model
                    width: songs.width
                    trackId: model.trackId; title: model.title; artist: model.artist; album: model.album
                    trackNo: index + 1; duration: model.duration
                    path: model.path; hearted: model.hearted
                    draggable: root.info.kind === "manual"
                    onMoveRequested: function (steps) { hop.movePlaylistItem(index, Math.max(0, Math.min(hop.playlistSongs.count - 1, index + steps))) }
                    onPlay: hop.playPlaylist(index)
                    onMenu: function (src, x, y) {
                        root.songMenu([model.path], model.path, model.hearted,
                                      root.info.kind === "manual" ? index : -1, hop.playlistSongs.count,
                                      function () { hop.playPlaylist(index) }, src, x, y)
                    }
                }
            }
        }
    }

    Popup {
        id: confirmDelete
        modal: true
        anchors.centerIn: Overlay.overlay
        padding: 28
        background: Rectangle { color: Theme.panel; border { width: 1; color: Theme.line } }
        Overlay.modal: Rectangle { color: Theme.scrim }
        contentItem: ColumnLayout {
            spacing: 18
            Label { text: "delete \\u201c" + (root.info.name || "") + "\\u201d?"; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(26); weight: Font.Light } }
            Label { text: "only the playlist goes. the songs stay in your library."; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(15) } }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 12
                PlainButton { text: "cancel"; onClicked: confirmDelete.close() }
                PlainButton { text: "delete"; danger: true; onClicked: { hop.deletePlaylist(hop.selectedPlaylist); confirmDelete.close() } }
            }
        }
    }
}
