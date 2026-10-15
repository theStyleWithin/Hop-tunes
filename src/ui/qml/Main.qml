// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "Theme.js" as Theme

ApplicationWindow {
    id: win
    width: 1280
    height: 800
    minimumWidth: 900
    minimumHeight: 600
    visible: true
    title: hop.player.hasTrack ? hop.player.title + " · Hop-tunes" : "Hop-tunes"
    color: view === "now playing" ? Theme.bgDeep : Theme.bg

    readonly property var views: ["collection", "now playing", "quickplay", "settings"]
    property string view: {
        var v = String(initialView || "").replace("nowplaying", "now playing")
        if (v === "albums" || v === "songs" || v === "artists" || v === "playlists") v = "collection"
        return views.indexOf(v) >= 0 ? v : "collection"
    }

    FolderDialog {
        id: folderDialog
        title: "choose a music folder"
        onAccepted: hop.addFolder(selectedFolder)
    }

    Shortcut { sequence: "Space"; enabled: hop.profileReady; onActivated: hop.player.hasTrack ? hop.player.toggle() : hop.playSongs(0) }
    Shortcut { sequences: ["Ctrl+Right", "Media Next"]; enabled: hop.profileReady; onActivated: hop.player.next() }
    Shortcut { sequences: ["Ctrl+Left", "Media Previous"]; enabled: hop.profileReady; onActivated: hop.player.previous() }
    Shortcut { sequence: "Ctrl+1"; enabled: hop.profileReady; onActivated: win.view = "collection" }
    Shortcut { sequence: "Ctrl+2"; enabled: hop.profileReady; onActivated: win.view = "now playing" }
    Shortcut { sequence: "Ctrl+3"; enabled: hop.profileReady; onActivated: win.view = "quickplay" }
    Shortcut { sequence: "Ctrl+,"; enabled: hop.profileReady; onActivated: win.view = "settings" }

    ProfileGate {
        anchors.fill: parent
        visible: !hop.profileReady
        z: 10
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        visible: hop.profileReady

        // pivots
        RowLayout {
            visible: win.view !== "now playing"
            Layout.fillWidth: true
            Layout.leftMargin: 48; Layout.rightMargin: 48; Layout.topMargin: 24
            spacing: 36
            Repeater {
                model: win.views
                delegate: AbstractButton {
                    id: pivot
                    required property string modelData
                    readonly property bool on: win.view === modelData
                    onClicked: win.view = modelData
                    hoverEnabled: true
                    Accessible.name: modelData
                    Accessible.role: Accessible.PageTab
                    contentItem: Label {
                        text: pivot.modelData
                        color: pivot.on ? "#ffffff" : (pivot.hovered ? Theme.text2 : Theme.dim)
                        font { family: Theme.font; pixelSize: 44; weight: pivot.on ? Font.Normal : Font.Light }
                    }
                    background: Item { }
                }
            }
            Item { Layout.fillWidth: true }
            // who's listening: click to switch profile (playback stops, no restart)
            AbstractButton {
                id: chip
                objectName: "profileChip"
                hoverEnabled: true
                onClicked: hop.leaveProfile()
                Accessible.name: "switch profile (" + (hop.profile.name || "") + ")"
                ToolTip.visible: hovered; ToolTip.delay: 600; ToolTip.text: "switch profile"
                contentItem: Row {
                    spacing: 12
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: hop.profile.name || ""
                        color: chip.hovered ? Theme.text : Theme.muted
                        font { family: Theme.font; pixelSize: 18 }
                    }
                    ProfileAvatar { profile: hop.profile; size: 40 }
                }
                background: Item { }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: win.views.indexOf(win.view)
            CollectionView { id: collectionView; onAddFolderRequested: folderDialog.open() }
            NowPlayingView { onBack: win.view = "collection" }
            QuickplayView { onOpenAlbum: { collectionView.showArtists(); win.view = "collection" } }
            SettingsView { onAddFolderRequested: folderDialog.open() }
        }

        MiniPlayer {
            Layout.fillWidth: true
            visible: win.view !== "now playing"
            onOpenNowPlaying: win.view = "now playing"
        }
    }
}
