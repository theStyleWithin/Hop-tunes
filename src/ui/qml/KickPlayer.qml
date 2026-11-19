// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HopTunes.Video 1.0

// The picture of a live stream inside the Kick app, on the same two video engines as the video player (libmpv, or GStreamer where
// libmpv is not built). Nothing is loaded until `url` is set, and `url` is only ever set when the person picked a stream.
Item {
    id: root
    objectName: "kickPlayer"
    property string url: ""                      // the live stream's address ("" = nothing playing, no engine started)
    property string file: ""                     // a downloaded video on this device (plays from disk, nothing is fetched)
    property bool showControls: true             // the small dock window hides the control row and has its own buttons
    property string appId: "kick"               // whose gateway checks the address (the Internet Archive app plays its music and videos here too)
    property bool vod: false                     // a recording from the internet (music, a film): it has a length, so it gets the seek meter, no "live" tag
    readonly property bool local: file !== ""
    readonly property bool showSeekMeter: (local || vod) && !hop.inPictureInPicture && error === ""   // recordings only, never a live stream
    readonly property bool showLiveTag: !local && !vod
    readonly property var surface: mpvLoader.item ? mpvLoader.item : (gstLoader.item ? gstLoader.item : qtLoader.item)
    readonly property string error: blocked ? "that stream address is not one this app may open" : (surface ? String(surface.error) : "")
    readonly property bool started: surface !== null && (url !== "" || file !== "")
    property string loadedUrl: ""
    readonly property bool wanted: url !== "" || file !== ""        // something is meant to play (even while the picture is being set up again)
    // Android: the picture's surface is never taken apart in the middle of a window change. When nothing is meant to play any more, the surface is
    // kept (stopped) a moment, and for as long as the window is stopped (the small pop-out window was closed, or the app was left): tearing a video
    // surface down underneath a stopping window is what froze and closed the app. Elsewhere it goes at once.
    readonly property bool windowAway: Qt.platform.os === "android" && (Qt.application.state === Qt.ApplicationSuspended || Qt.application.state === Qt.ApplicationHidden)
    property bool surfaceOn: url !== "" || file !== ""
    function syncSurface() {
        if (wanted) { holdTimer.stop(); surfaceOn = true }
        else if (Qt.platform.os !== "android") surfaceOn = false
        else if (!windowAway) holdTimer.restart()
    }
    onWantedChanged: syncSurface()
    Timer { id: holdTimer; interval: 1500; onTriggered: if (!root.wanted && !root.windowAway) root.surfaceOn = false }
    property bool recreating: false
    property real resumeAt: 0
    // The picture is dropped and set up again at the same place. Full screen, the pop-out window and turning the phone change the window's
    // surface underneath the video; on some phones the picture then stays black until it is rebuilt (like after switching apps).
    function refreshPicture() {
        if (!started) return
        resumeAt = (local || vod) && surface ? surface.position : 0
        loadedUrl = ""
        recreating = true
        recreateTimer.restart()
    }
    Timer { id: recreateTimer; interval: 80; onTriggered: root.recreating = false }
    Timer { id: settleTimer; interval: 350; onTriggered: if (Qt.platform.os === "android") root.refreshPicture() }
    // popping out (and back) resizes the window in steps: once more a moment later, for the picture that is still black after the first rebuild
    Timer { id: lateTimer; interval: 1400; onTriggered: if (Qt.platform.os === "android" && root.started && !root.windowAway) root.refreshPicture() }
    Connections {
        target: hop
        enabled: Qt.platform.os === "android"
        function onInPictureInPictureChanged() { if (root.started) { settleTimer.restart(); lateTimer.restart() } }
    }
    Connections {
        target: root.Window.window
        enabled: Qt.platform.os === "android" && root.started
        function onWidthChanged() { settleTimer.restart() }
        function onHeightChanged() { if (root.staged) settleTimer.restart() }       // (the keyboard changes the height too: not a reason to rebuild the inline picture)
    }
    // Full screen turns with the phone for a normal (wide) video and stays upright for a tall, shorts-style one
    readonly property real videoAspect: surface && surface.videoAspect !== undefined ? Number(surface.videoAspect) : 0
    function orientationFor(aspect) { return aspect > 1 ? "sensor" : "portrait" }
    function applyOrientation() { hop.requestOrientation(root.fullscreen ? orientationFor(videoAspect) : "auto") }
    onVideoAspectChanged: if (fullscreen) applyOrientation()
    property bool blocked: false
    signal retryRequested()                      // "try again" after an error: the page asks Kick for a fresh stream address

    function startStream() {
        const want = local ? file : url
        if (!surface || want === "" || loadedUrl === want) return
        loadedUrl = want
        if (resumeAt > 0 && typeof surface.setResumePosition === "function") { surface.setResumePosition(resumeAt); resumeAt = 0 }
        if (local) { surface.source = file; surface.play(); return }
        if (hop.videoEngine === "qt") {          // Android: Qt Multimedia plays the address itself, so it is checked here first
            if (!(appId === "kick" ? hop.kick.streamAllowed(url) : hop.streamAllowed(appId, url))) { blocked = true; return }
            blocked = false
            surface.source = url
        } else surface.openStream(url, appId)
        surface.play()
    }
    onUrlChanged: {
        if (local) return
        if (url === "") { if (surface) surface.stop(); loadedUrl = "" }
        else startStream()
    }
    onFileChanged: {
        if (file === "") { if (surface) surface.stop(); loadedUrl = "" }
        else startStream()
    }
    function surfaceLoaded(item) {
        hop.diagNote("picture: " + hop.videoEngine + " surface built (window " + Math.round(root.Window.width) + "x" + Math.round(root.Window.height) + ", in small window " + hop.inPictureInPicture + ")")
        startCheck.restart()
        // Qt Multimedia (Android) needs no drawn frame, and the small pop-out window may never draw one: waiting for it left that window black
        if (hop.videoEngine === "qt") { root.startStream(); return }
        // the GStreamer engine wraps Qt's GL context, which exists once the window has drawn one frame (as in the video player)
        const win = root.Window.window
        if (win) {
            function onFrame() { win.afterRendering.disconnect(onFrame); root.startStream() }
            win.afterRendering.connect(onFrame)
            win.requestUpdate()
        } else root.startStream()
    }
    // a picture that was built but is still not playing a moment later is started again (and the person's phone log says so)
    Timer { id: startCheck; interval: 1200; onTriggered: {
        if (root.wanted && root.surface && root.surface.idle) { hop.diagNote("picture: still idle 1.2 s after it was built, starting it again"); root.loadedUrl = ""; root.startStream() }
        else if (root.wanted && root.surface) hop.diagNote("picture: playing=" + !root.surface.paused + " error='" + root.error + "'")
    } }
    // what the small window's button shows, and the sound that keeps the app alive in the background
    readonly property bool playing: started && !!surface && !surface.idle && !surface.paused && error === ""
    onPlayingChanged: { hop.setPictureInPicturePlaying(playing); reportTimer.restart() }
    Timer { id: reportTimer; interval: 1500; onTriggered: hop.setStreamPlaying(root.playing) }
    onErrorChanged: if (error !== "" && wanted) hop.diagNote("picture: error '" + error + "'")
    Connections {
        target: hop
        enabled: root.staged                       // only the picture that is in the small window (or full screen) answers its button
        function onPictureInPictureToggle() { if (root.surface) root.surface.togglePause() }
    }
    onWindowAwayChanged: { if (!windowAway && !wanted) holdTimer.restart(); if (!windowAway && started) { settleTimer.restart(); lateTimer.restart() } }   // back from away (or the small window closed): the picture is built again
    // what the picture is doing in the small window, a moment after it was set up: if it stays black, "copy this run" (settings > diagnostics) says why
    Timer { id: infoTimer; interval: 2500; onTriggered: {
        const w = root.Window.window
        hop.diagNote("picture (small window " + hop.inPictureInPicture + ", full screen " + root.fullscreen + "): wanted=" + root.wanted + " started=" + root.started + " playing=" + root.playing
                     + " idle=" + (root.surface ? root.surface.idle : "-") + " paused=" + (root.surface ? root.surface.paused : "-") + " position=" + (root.surface ? Math.round(root.surface.position) : "-")
                     + " error='" + root.error + "' stage=" + Math.round(content.width) + "x" + Math.round(content.height) + " window=" + (w ? Math.round(w.width) + "x" + Math.round(w.height) + " visible=" + w.visible + " active=" + w.active : "none")
                     + " appState=" + Qt.application.state)
    } }
    Connections { target: hop; function onInPictureInPictureChanged() { infoTimer.restart() } }
    Component.onDestruction: { if (playing) hop.setStreamPlaying(false); if (surface) surface.stop(); if (fullscreen) { hop.setImmersive(false); hop.requestOrientation("auto") } }

    // the picture and its controls live in `content`, which moves to cover the whole window for full screen and for the small pop-out window
    Item {
        id: content
        objectName: root.objectName !== "" ? root.objectName + "Stage" : "kickPlayerStage"
        anchors.fill: parent
        Rectangle { anchors.fill: parent; color: "black" }
        Loader { id: mpvLoader; anchors.fill: parent; active: hop.videoEngine === "mpv" && root.surfaceOn && !root.recreating; sourceComponent: mpvSurface; onLoaded: root.surfaceLoaded(item) }
        Loader { id: gstLoader; anchors.fill: parent; active: hop.videoEngine === "gstreamer" && root.surfaceOn && !root.recreating; source: active ? "GstVideoSurfaceView.qml" : ""; onLoaded: root.surfaceLoaded(item) }
        Loader { id: qtLoader; anchors.fill: parent; active: hop.videoEngine === "qt" && root.surfaceOn && !root.recreating; source: active ? "AndroidVideoSurface.qml" : ""; onLoaded: root.surfaceLoaded(item) }
        Component { id: mpvSurface; VideoSurface { objectName: "kickSurface" } }

        PlainButton {
            objectName: "kickPlayerRetry"
            anchors { horizontalCenter: parent.horizontalCenter; verticalCenter: parent.verticalCenter; verticalCenterOffset: 56 }
            visible: !root.local && root.url !== "" && root.error !== "" && !root.vod
            text: "try again"; implicitHeight: 38
            onClicked: root.retryRequested()
        }
        Label {
            anchors.centerIn: parent
            width: parent.width - 40; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
            visible: (root.url !== "" || root.file !== "") && (root.error !== "" || hop.videoEngine === "none" || gstLoader.status === Loader.Error)
            text: root.error !== "" ? "the stream could not be played: " + root.error
                                    : "no video engine is available in this build"
            color: "white"; font { family: Theme.font; pixelSize: Theme.fs(16) }
        }
        Label {
            anchors.centerIn: parent
            visible: root.started && root.error === "" && root.surface && root.surface.idle
            text: "connecting…"; color: Qt.rgba(1, 1, 1, 0.7); font { family: Theme.font; pixelSize: Theme.fs(18) }
        }

        // the seek meter along the top: drag left to rewind, right to skip ahead. Only for a video kept on this device (a download or a recording):
        // a live stream is just watched, so it has no meter (it has the "live" tag below instead)
        SeekMeter {
            objectName: "kickSeekMeter"
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }
            visible: root.started && root.showSeekMeter
            position: root.surface ? root.surface.position : 0
            duration: root.surface && root.surface.duration > 0 ? root.surface.duration : 0
            onSeekRequested: ms => { if (root.surface) root.surface.seek(ms) }
        }

        // controls: pause, volume
        RowLayout {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 10 }
            visible: root.started && root.showControls && !hop.inPictureInPicture
            spacing: 12
            AbstractButton {
                objectName: "kickPause"
                implicitWidth: 36; implicitHeight: 36; focusPolicy: Qt.NoFocus
                onClicked: root.surface.togglePause()
                Accessible.name: "pause or play"
                background: Rectangle { radius: 18; color: Qt.rgba(0, 0, 0, 0.55); border { width: 1; color: Qt.rgba(1, 1, 1, 0.5) } }
                contentItem: Item {
                    // drawn, not typed: the phone's font has no glyph for the pause bars (they showed as empty boxes)
                    readonly property bool isPaused: !!root.surface && root.surface.paused
                    Rectangle { visible: !parent.isPaused; x: parent.width / 2 - 6; y: parent.height / 2 - 6; width: 4; height: 12; color: "white" }
                    Rectangle { visible: !parent.isPaused; x: parent.width / 2 + 2; y: parent.height / 2 - 6; width: 4; height: 12; color: "white" }
                    Canvas {
                        visible: parent.isPaused
                        anchors.centerIn: parent; width: 14; height: 14
                        onPaint: { const c = getContext("2d"); c.clearRect(0, 0, width, height); c.fillStyle = "white"; c.beginPath(); c.moveTo(2, 0); c.lineTo(14, 7); c.lineTo(2, 14); c.closePath(); c.fill() }
                    }
                }
            }
            Label {
                objectName: "kickLiveTag"
                visible: root.showLiveTag
                text: "live"; color: "white"; font { family: Theme.font; pixelSize: Theme.fs(13) }
                leftPadding: 7; rightPadding: 7; topPadding: 2; bottomPadding: 2; background: Rectangle { color: "#e5484d"; radius: 3 }
            }
            Item { Layout.fillWidth: true }
            AbstractButton {   // a small window over the home screen and other apps (Android)
                objectName: "kickPopOut"
                visible: hop.pictureInPictureSupported()
                implicitWidth: 36; implicitHeight: 36; focusPolicy: Qt.NoFocus
                onClicked: hop.enterPictureInPicture()
                Accessible.name: "pop out into a small window"
                background: Rectangle { radius: 18; color: Qt.rgba(0, 0, 0, 0.55); border { width: 1; color: Qt.rgba(1, 1, 1, 0.5) } }
                contentItem: Item {
                    Rectangle { anchors.centerIn: parent; width: 16; height: 12; color: "transparent"; border { width: 1.5; color: "white" }
                                Rectangle { anchors { right: parent.right; bottom: parent.bottom; margins: 2 } width: 6; height: 4; color: "white" } }
                }
            }
            AbstractButton {   // turn between portrait and landscape (for when the phone's own rotation is off)
                objectName: "kickRotate"
                visible: root.fullscreen && Qt.platform.os === "android"
                implicitWidth: 36; implicitHeight: 36; focusPolicy: Qt.NoFocus
                onClicked: hop.requestOrientation(content.width > content.height ? "portrait" : "landscape")
                Accessible.name: "rotate"
                background: Rectangle { radius: 18; color: Qt.rgba(0, 0, 0, 0.55); border { width: 1; color: Qt.rgba(1, 1, 1, 0.5) } }
                contentItem: Icon { kind: "rotate"; ink: "white"; width: 18; height: 18 }
            }
            AbstractButton {   // full screen (the page's back button, or this, brings it back)
                objectName: "kickFullscreen"
                implicitWidth: 36; implicitHeight: 36; focusPolicy: Qt.NoFocus
                onClicked: root.fullscreenToggled()
                Accessible.name: root.fullscreen ? "leave full screen" : "full screen"
                background: Rectangle { radius: 18; color: Qt.rgba(0, 0, 0, 0.55); border { width: 1; color: Qt.rgba(1, 1, 1, 0.5) } }
                contentItem: Item {
                    Rectangle { anchors.centerIn: parent; width: 14; height: 14; color: "transparent"; border { width: 1.5; color: "white" }
                                Rectangle { anchors.centerIn: parent; width: root.fullscreen ? 8 : 2; height: root.fullscreen ? 8 : 2; color: root.fullscreen ? "transparent" : "white"
                                            border { width: root.fullscreen ? 1.5 : 0; color: "white" } } }
                }
            }
            Slider {
                objectName: "kickVolume"
                from: 0; to: 100; value: root.surface ? root.surface.volume : 100
                implicitWidth: 120; focusPolicy: Qt.NoFocus
                onMoved: if (root.surface) root.surface.volume = value
            }
        }
    }

    property bool fullscreen: false              // the picture fills the whole window (the page sets it from the button's signal)
    signal fullscreenToggled()
    readonly property bool staged: fullscreen || hop.inPictureInPicture
    onStagedChanged: {
        const win = root.Window.window
        if (staged && win) { content.parent = win.contentItem; content.z = 100000 }
        else { content.parent = root; content.z = 0 }
        settleTimer.restart()                                  // (Android) rebuild the picture once the window has settled
    }
    onFullscreenChanged: { hop.setImmersive(fullscreen); applyOrientation() }
}
