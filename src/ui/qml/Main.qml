// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1280
    height: 800
    // phones and tablets: bottom bar / side drawer (MobileNav.qml) instead of the desktop's top pivots
    property bool forceMobile: false   // tests and screenshots
    readonly property bool mobileNav: forceMobile || Qt.platform.os === "android"
    readonly property bool tablet: width >= 600
    Binding { target: Theme; property: "compact"; value: win.mobileNav && !win.tablet }
    readonly property int railWidth: !mobileNav || !tablet ? 0 : (hop.navStyle === "drawer" ? 240 : 84)
    minimumWidth: mobileNav ? 0 : 900
    minimumHeight: mobileNav ? 0 : 600
    visible: true
    title: hop.player.hasTrack ? hop.player.title + " · Hop-tunes" : "Hop-tunes"
    color: view === "now playing" ? Theme.bgDeep : Theme.bg

    readonly property var views: ["collection", "now playing", "quickplay", "settings"]
    // phones and tablets: the bottom bar / drawer shows these four, as in the mockups; quickplay and settings are reached from profile
    readonly property var navViews: ["collection", "now playing", "search", "profile"]
    readonly property var allViews: views.concat(["search", "profile"])
    property string view: {
        var v = String(initialView || "").replace("nowplaying", "now playing")
        if (v === "albums" || v === "songs" || v === "artists" || v === "playlists") v = "collection"
        return views.indexOf(v) >= 0 ? v : "collection"
    }

    FolderDialog {
        id: videoFolderDialog
        title: "choose a video folder"
        onAccepted: hop.addVideoFolder(selectedFolder)
    }
    // Folder picker for the videos tab: movies, shows, cartoons, anime and other videos each have their own.
    FolderDialog {
        id: videoCategoryFolderDialog
        property string category: "movies"
        title: "choose a folder"
        onAccepted: hop.addVideoCategoryFolder(category, selectedFolder)
    }
    function openVideoCategoryFolderDialog(c) {
        videoCategoryFolderDialog.category = c
        videoCategoryFolderDialog.title = "choose a folder for " + (c === "other" ? "other videos" : c)
        videoCategoryFolderDialog.open()
    }
    function openSectionFolderDialog(k) {
        sectionFolderDialog.kind = k
        const noun = k.replace(/s$/, "")
        sectionFolderDialog.title = "choose " + (/^[aeiou]/.test(noun) ? "an " : "a ") + noun + " folder"
        sectionFolderDialog.open()
    }
    // Folder picker for the pictures / audiobooks / ebooks / podcasts sections.
    FolderDialog {
        id: sectionFolderDialog
        property string kind: ""
        title: "choose a folder"
        onAccepted: hop.addSectionFolder(kind, selectedFolder)
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

    VideoPlayerView {
        anchors.fill: parent
        visible: hop.profileReady && hop.videoPlayerOpen
        z: 20
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        visible: hop.profileReady

        // pivots, with the private status box at the right end (in the collection view)
        Item {   // phones with the side drawer: room for the ☰ header
            visible: win.mobileNav && !win.tablet && hop.navStyle === "drawer"
            implicitHeight: 56
        }
        Item {
            visible: win.view !== "now playing" && !win.mobileNav
            Layout.fillWidth: true
            Layout.leftMargin: 48; Layout.rightMargin: 48; Layout.topMargin: 24
            implicitHeight: pivots.implicitHeight
            // it gives way to the pivots on a narrow window rather than overlap them
            StatusNote {
                readonly property real room: parent.width - 880
                visible: win.view === "collection" && !hop.guest && room >= 200
                width: Math.min(460, Math.max(0, room))
                anchors { right: parent.right; verticalCenter: parent.verticalCenter }
            }
        RowLayout {
            id: pivots
            anchors.fill: parent
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
                        color: pivot.on ? Theme.strong : (pivot.hovered ? Theme.text2 : Theme.dim)
                        font { family: Theme.font; pixelSize: Theme.fs(44); weight: pivot.on ? Font.Normal : Font.Light }
                    }
                    background: Item { }
                }
            }
            Item { Layout.fillWidth: true }
            // who's listening (in the collection view it sits in the tab row below instead)
            ProfileChip { visible: win.view !== "collection" }
        }
        }

        StackLayout {
            id: stack
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: win.railWidth
            currentIndex: win.allViews.indexOf(win.view)
            // Slide-and-fade between pivots (spec §4.5: ≤ 200 ms, off with "reduce motion").
            transform: Translate { id: slide }
            onCurrentIndexChanged: if (Theme.motion > 0) pivotAnim.restart()
            ParallelAnimation {
                id: pivotAnim
                NumberAnimation { target: stack; property: "opacity"; from: 0.0; to: 1.0; duration: Theme.motion; easing.type: Easing.OutCubic }
                NumberAnimation { target: slide; property: "x"; from: 28; to: 0; duration: Theme.motion; easing.type: Easing.OutCubic }
            }
            CollectionView { id: collectionView; onAddFolderRequested: folderDialog.open(); onAddVideoFolderRequested: videoFolderDialog.open(); onAddVideoCategoryFolderRequested: function (c) { win.openVideoCategoryFolderDialog(c) }; onAddSectionFolderRequested: function (k) { win.openSectionFolderDialog(k) } }
            NowPlayingView { onBack: win.view = "collection" }
            QuickplayView { onOpenAlbum: { collectionView.showArtists(); win.view = "collection" } }
            SettingsView { objectName: "settingsView"; onBack: win.view = "profile"; onAddFolderRequested: folderDialog.open(); onAddVideoFolderRequested: videoFolderDialog.open(); onAddVideoCategoryFolderRequested: function (c) { win.openVideoCategoryFolderDialog(c) }; onAddSectionFolderRequested: function (k) { win.openSectionFolderDialog(k) } }
            PhoneSearch { }
            PhoneProfile { onGo: function (v) { win.view = v } }
        }

        MiniPlayer {
            compact: win.mobileNav && !win.tablet
            Layout.fillWidth: true
            Layout.leftMargin: win.railWidth + (compact ? 12 : 0)
            Layout.rightMargin: compact ? 12 : 0
            Layout.bottomMargin: compact ? 6 : 0
            visible: win.view !== "now playing"
            onOpenNowPlaying: win.view = "now playing"
        }
        Item {   // room for the phone's bottom bar
            visible: win.mobileNav && !win.tablet && hop.navStyle !== "drawer"
            implicitHeight: 72
        }
    }

    MobileNav {
        anchors.fill: parent
        visible: win.mobileNav && hop.profileReady && !hop.videoPlayerOpen
        views: win.navViews
        view: win.view === "quickplay" || win.view === "settings" ? "profile" : win.view
        tablet: win.tablet
        onPicked: function (v) { win.view = v }
    }
}
