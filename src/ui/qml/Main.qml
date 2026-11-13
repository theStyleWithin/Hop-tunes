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
    property bool miniHidden: false                // the phone's mini player pushed away to the right for now
    // what is "playing" besides music (the mini player shows it when no song is): a live stream in the dock, even with Kick closed, otherwise what the open app says
    readonly property bool dockOn: !!hop.dock && hop.dock.active
    readonly property string appNowTitle: dockOn ? "now playing " + hop.dock.name : (view === "collection" ? collectionView.appNowTitle : "")
    readonly property string appNowSub: dockOn ? "live on kick" : (view === "collection" ? collectionView.appNowSub : "")
    readonly property bool theatre: view === "collection" && collectionView.appTheatre      // an app has the whole window (Kick, phone sideways)
    readonly property bool tablet: Math.min(width, height) >= 600   // a phone turned sideways is still a phone
    Binding { target: Theme; property: "compact"; value: win.mobileNav && !win.tablet }
    readonly property int railWidth: !mobileNav || !tablet ? 0 : (hop.navStyle === "drawer" ? 240 : 84)
    minimumWidth: mobileNav ? 0 : 900
    minimumHeight: mobileNav ? 0 : 600
    visible: true
    title: hop.player.hasTrack ? hop.player.title + " · Hop-tunes" : "Hop-tunes"
    color: view === "now playing" && !Theme.compact ? Theme.bgDeep : Theme.bg

    // The Android back button (and a phone's back gesture) steps back through the app and never leaves it: leaving
    // by "back" closed the screen and left a grey window behind. At the top of the music collection it does nothing.
    // Returns true when it did something. Desktop windows close as usual.
    function goBack() {
        if (!hop.profileReady) {
            if (gate.mode === "create" && hop.profiles.length > 0) gate.show("pick")
            else if (gate.mode === "pin") gate.show("pick")
            return true
        }
        if (musyNote.active) { musyNote.active = false; return true }
        if (hop.videoPlayerOpen) { videoView.goBack(); return true }
        if (hop.videoDetailRow >= 0) { hop.closeVideoDetail(); return true }
        if (view === "settings" || view === "quickplay") { view = "profile"; return true }
        if (view !== "collection") { view = "collection"; return true }
        collectionView.goBack()
        return true
    }
    onClosing: function (close) {
        if (!mobileNav) return
        close.accepted = false
        goBack()
    }

    readonly property var views: ["collection", "now playing", "quickplay", "settings"]
    // phones and tablets: the bottom bar / drawer shows these four, as in the mockups; quickplay and settings are reached from profile
    readonly property var navViews: ["collection", "now playing", "search", "profile"]
    readonly property var allViews: views.concat(["search", "profile"])
    property string view: {
        var v = String(initialView || "").replace("nowplaying", "now playing")
        if (v === "albums" || v === "songs" || v === "artists" || v === "playlists") v = "collection"
        return views.indexOf(v) >= 0 ? v : "collection"
    }

    // With a game controller connected (plugged in, or clipped onto the phone), the collection hands over to Now playing,
    // where the controller's games are: when a song starts while a controller is there, and when a controller shows up
    // while a song is already playing. Only from the collection (never yanks you out of another screen) and it can be
    // switched off in settings.
    function maybeOpenNowPlaying() {
        if (win.view === "collection" && hop.profileReady && hop.player.playing && hop.gamepad.connected
            && hop.gameSettings.autoNowPlaying && !hop.videoPlayerOpen && !hop.neko.open)   // (neko cafe, which listens to the controller itself, keeps the screen)
            win.view = "now playing"
    }
    Connections { target: hop.player; function onPlayingChanged() { win.maybeOpenNowPlaying() } }
    Connections { target: hop.gamepad; function onConnectedChanged() { win.maybeOpenNowPlaying() } }

    // Musy Note, the phone game: fullscreen over everything, opened from Now playing > games
    function openMusyNote() { musyNote.active = true }
    MusyNote {
        id: musyNote
        anchors.fill: parent
        onLeave: active = false
    }

    FolderPick {
        id: videoFolderDialog
        title: "choose a video folder"
        onAccepted: hop.addVideoFolder(selectedFolder)
    }
    // Folder picker for the videos tab: movies, shows, cartoons, anime and other videos each have their own.
    FolderPick {
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
    FolderPick {
        id: sectionFolderDialog
        property string kind: ""
        title: "choose a folder"
        onAccepted: hop.addSectionFolder(kind, selectedFolder)
    }
    FolderPick {
        id: folderDialog
        objectName: "musicFolderPick"
        title: "choose a music folder"
        onAccepted: hop.addFolder(selectedFolder)
    }

    Shortcut { sequence: "Space"; enabled: hop.profileReady && !musyNote.active && !hop.neko.open && !(win.view === "now playing" && (hop.gameSettings.stage === "albumfall" || hop.gameSettings.stage === "pinball")); onActivated: hop.player.hasTrack ? hop.player.toggle() : hop.playSongs(0) }
    Shortcut { sequences: ["Ctrl+Right", "Media Next"]; enabled: hop.profileReady; onActivated: hop.player.next() }
    Shortcut { sequences: ["Ctrl+Left", "Media Previous"]; enabled: hop.profileReady; onActivated: hop.player.previous() }
    Shortcut { sequence: "Ctrl+1"; enabled: hop.profileReady; onActivated: win.view = "collection" }
    Shortcut { sequence: "Ctrl+2"; enabled: hop.profileReady; onActivated: win.view = "now playing" }
    Shortcut { sequence: "Ctrl+3"; enabled: hop.profileReady; onActivated: win.view = "quickplay" }
    Shortcut { sequence: "Ctrl+,"; enabled: hop.profileReady; onActivated: win.view = "settings" }

    ProfileGate {
        id: gate
        anchors.fill: parent
        visible: !hop.profileReady
        z: 10
    }

    VideoPlayerView {
        id: videoView
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
            visible: win.mobileNav && !win.tablet && hop.navStyle === "drawer" && !win.theatre
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
            CollectionView { id: collectionView; objectName: "collectionView"; onAddFolderRequested: folderDialog.open(); onAddVideoFolderRequested: videoFolderDialog.open(); onAddVideoCategoryFolderRequested: function (c) { win.openVideoCategoryFolderDialog(c) }; onAddSectionFolderRequested: function (k) { win.openSectionFolderDialog(k) } }
            NowPlayingView { onBack: win.view = "collection" }
            QuickplayView { onOpenAlbum: { collectionView.showArtists(); win.view = "collection" } }
            SettingsView { objectName: "settingsView"; onBack: win.view = "profile"; onAddFolderRequested: folderDialog.open(); onAddVideoFolderRequested: videoFolderDialog.open(); onAddVideoCategoryFolderRequested: function (c) { win.openVideoCategoryFolderDialog(c) }; onAddSectionFolderRequested: function (k) { win.openSectionFolderDialog(k) } }
            PhoneSearch { }
            PhoneProfile { onGo: function (v) { win.view = v } }
        }

        Item {   // the mini player; on a phone, press and slide it to the right to push it away (more room for the chat), tap the little tab to bring it back
            id: miniWrap
            objectName: "miniWrap"
            readonly property bool compactBar: win.mobileNav && !win.tablet
            Layout.fillWidth: true
            Layout.leftMargin: win.railWidth + (compactBar ? 12 : 0)
            Layout.rightMargin: compactBar ? 12 : 0
            Layout.bottomMargin: compactBar ? 6 : 0
            implicitHeight: mini.implicitHeight
            visible: win.view !== "now playing" && !(compactBar && win.miniHidden) && !win.theatre
            MiniPlayer {
                id: mini
                objectName: "miniPlayer"
                width: parent.width; height: parent.height
                compact: miniWrap.compactBar
                appTitle: win.appNowTitle
                appSub: win.appNowSub
                onOpenNowPlaying: win.view = "now playing"
                opacity: 1 - Math.min(0.6, Math.max(0, x) / Math.max(1, width))
                DragHandler {
                    id: miniDrag
                    enabled: miniWrap.compactBar
                    target: mini
                    xAxis { enabled: true; minimum: 0 }
                    yAxis.enabled: false
                    onActiveChanged: {
                        if (active) return
                        if (mini.x > miniWrap.width * 0.3) slideOut.start()           // far enough: away it goes
                        else slideBack.start()                                           // not far: it comes back
                    }
                }
                NumberAnimation { id: slideBack; target: mini; property: "x"; to: 0; duration: Theme.reduceMotion ? 0 : 160; easing.type: Easing.OutCubic }
                SequentialAnimation {
                    id: slideOut
                    NumberAnimation { target: mini; property: "x"; to: miniWrap.width + 24; duration: Theme.reduceMotion ? 0 : 160; easing.type: Easing.InCubic }
                    ScriptAction { script: { win.miniHidden = true; mini.x = 0 } }
                }
            }
        }
        Item {   // room for the phone's bottom bar
            visible: win.mobileNav && !win.tablet && hop.navStyle !== "drawer" && win.view !== "now playing" && !win.theatre
            implicitHeight: 72
        }
    }

    MobileNav {
        anchors.fill: parent
        visible: win.mobileNav && hop.profileReady && !hop.videoPlayerOpen && !win.theatre && !(win.view === "now playing" && !win.tablet)
        views: win.navViews
        view: win.view === "quickplay" || win.view === "settings" ? "profile" : (win.view === "collection" && collectionView.appSearching ? "search" : win.view)
        tablet: win.tablet
        onPicked: function (v) {
            // inside an app that has its own search (Kick), the search button searches that app, not the music
            if (v === "search" && win.view === "collection" && collectionView.appSearchable && collectionView.appSearch()) return
            win.view = v
        }
    }

    // the live stream that keeps playing after the Kick app is left: a small window you can drag anywhere, with a ✕ to close it
    StreamDock {
        id: streamDock
        anchors.fill: parent
        slot: win.view === "collection" ? collectionView.appVideoSlot : null
        bottomInset: miniWrap.visible ? miniWrap.height + 16 : 16
    }
    // ⤢ on the small window: back to the channel (the Kick app is opened if it was closed; an open one answers by itself)
    Connections {
        target: hop.dock
        enabled: !!hop.dock
        function onWantsOpenChanged() {
            if (hop.dock.wantsOpen && !collectionView.appActive) { win.view = "collection"; collectionView.openApp("kick") }
        }
    }
    Item {   // the little tab that brings a pushed-away mini player back: it floats at the right edge above the bottom bar and takes no room of its own
        id: miniTab
        objectName: "miniPullTab"
        visible: win.mobileNav && !win.tablet && win.miniHidden && win.view !== "now playing" && hop.profileReady && !hop.videoPlayerOpen && !win.theatre
        width: 14; height: 64                              // slimmer than the page's side margin: it never sits on top of a page's content
        anchors { right: parent.right; bottom: parent.bottom; bottomMargin: (hop.navStyle !== "drawer" ? 72 : 0) + 16 }
        z: 50
        Rectangle { anchors.fill: parent; radius: 12; color: Theme.panel; border { width: 1; color: Theme.line } }
        Rectangle { anchors { right: parent.right; top: parent.top; bottom: parent.bottom } width: 7; color: Theme.panel }        // (square on the screen's edge side)
        Icon { anchors.centerIn: parent; kind: "down"; ink: Theme.text2; width: 11; height: 11; rotation: 90 }              // pointing back to the left
        TapHandler { onTapped: win.miniHidden = false }
        DragHandler { xAxis { enabled: true; maximum: 0 } yAxis.enabled: false; onActiveChanged: if (!active && centroid.position.x < -20) win.miniHidden = false }
        Accessible.role: Accessible.Button
        Accessible.name: "show the player"
    }
}
