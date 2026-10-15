// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// Right-click menu for songs and albums (spec §4.2): play next, add to queue, add to
// playlist, heart, pin. Set `paths` (and optionally the pin and playlist-row fields) and popup().
Menu {
    id: m
    property var paths: []
    property string heartPath: ""          // single song: show heart toggle
    property bool hearted: false
    property string pinKind: ""            // "album" | "artist": show pin toggle
    property string pinRef: ""
    property string pinLabel: ""
    property int playlistRow: -1           // inside a manual playlist: remove / move
    property int playlistCount: 0
    signal playRequested()
    signal newPlaylistRequested(var paths)

    property bool pinnedNow: false
    onAboutToShow: pinnedNow = pinKind !== "" && hop.isPinned(pinKind, pinRef)
    readonly property var manualPlaylists: hop.playlists.filter(function (p) { return p.kind === "manual" })

    component MItem: MenuItem {
        id: mi
        implicitWidth: 260
        contentItem: Label {
            text: mi.text; leftPadding: 6
            color: mi.enabled ? (mi.highlighted ? Theme.strong : Theme.text) : Theme.dim
            font { family: Theme.font; pixelSize: 15 }
        }
        arrow: Label { x: mi.width - width - 10; anchors.verticalCenter: parent.verticalCenter; visible: !!mi.subMenu; text: "›"; color: Theme.muted; font.pixelSize: 18 }
        background: Rectangle { implicitHeight: 36; color: mi.highlighted ? Theme.hover : "transparent" }
    }
    delegate: Component { MItem { } }   // wrapped explicitly: Qt 6.4 does not auto-wrap inline components
    background: Rectangle { implicitWidth: 260; color: Theme.panel; border { width: 1; color: Theme.line } }

    MItem { text: "play"; onTriggered: m.playRequested() }
    MItem { text: "play next"; onTriggered: hop.playNextPaths(m.paths) }
    MItem { text: "add to queue"; onTriggered: hop.enqueuePaths(m.paths) }
    Menu {
        id: addMenu
        title: "add to playlist"
        delegate: m.delegate
        background: Rectangle { implicitWidth: 260; color: Theme.panel; border { width: 1; color: Theme.line } }
        Instantiator {
            model: m.manualPlaylists
            delegate: Component {
                MItem {
                    required property var modelData
                    text: modelData.name
                    onTriggered: hop.addToPlaylist(modelData.key, m.paths)
                }
            }
            onObjectAdded: function (index, object) { addMenu.insertItem(index, object) }
            onObjectRemoved: function (index, object) { addMenu.removeItem(object) }
        }
        MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.line } }
        MItem { text: "new playlist…"; onTriggered: m.newPlaylistRequested(m.paths) }
    }
    MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.line } }
    MItem {
        visible: m.heartPath !== ""; height: visible ? implicitHeight : 0
        text: m.hearted ? "remove heart" : "heart"
        onTriggered: hop.toggleHeart(m.heartPath)
    }
    MItem {
        visible: m.pinKind !== ""; height: visible ? implicitHeight : 0
        text: m.pinnedNow ? "unpin from quickplay" : "pin to quickplay"
        onTriggered: m.pinnedNow ? hop.unpinTile(m.pinKind, m.pinRef) : hop.pinTile(m.pinKind, m.pinRef, m.pinLabel)
    }
    MItem {
        visible: m.playlistRow >= 0; height: visible ? implicitHeight : 0
        text: "move up"; enabled: m.playlistRow > 0
        onTriggered: hop.movePlaylistItem(m.playlistRow, m.playlistRow - 1)
    }
    MItem {
        visible: m.playlistRow >= 0; height: visible ? implicitHeight : 0
        text: "move down"; enabled: m.playlistRow >= 0 && m.playlistRow < m.playlistCount - 1
        onTriggered: hop.movePlaylistItem(m.playlistRow, m.playlistRow + 1)
    }
    MItem {
        visible: m.playlistRow >= 0; height: visible ? implicitHeight : 0
        text: "remove from playlist"
        onTriggered: hop.removeFromPlaylist(m.playlistRow)
    }
}
