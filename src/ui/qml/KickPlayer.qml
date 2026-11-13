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
    readonly property bool local: file !== ""
    readonly property var surface: mpvLoader.item ? mpvLoader.item : (gstLoader.item ? gstLoader.item : qtLoader.item)
    readonly property string error: blocked ? "that stream address is not one this app may open" : (surface ? String(surface.error) : "")
    readonly property bool started: surface !== null && (url !== "" || file !== "")
    property string loadedUrl: ""
    readonly property bool wanted: url !== "" || file !== ""        // something is meant to play (even while the picture is being set up again)
    property bool recreating: false
    property real resumeAt: 0
    // The picture is dropped and set up again at the same place. Full screen, the pop-out window and turning the phone change the window's
    // surface underneath the video; on some phones the picture then stays black until it is rebuilt (like after switching apps).
    function refreshPicture() {
        if (!started) return
        resumeAt = local && surface ? surface.position : 0
        loadedUrl = ""
        recreating = true
        recreateTimer.restart()
    }
    Timer { id: recreateTimer; interval: 80; onTriggered: root.recreating = false }
    Timer { id: settleTimer; interval: 350; onTriggered: if (Qt.platform.os === "android") root.refreshPicture() }
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
            if (!hop.kick.streamAllowed(url)) { blocked = true; return }
            blocked = false
            surface.source = url
        } else surface.openStream(url)
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
        // the GStreamer engine wraps Qt's GL context, which exists once the window has drawn one frame (as in the video player)
        const win = root.Window.window
        if (win) {
            function onFrame() { win.afterRendering.disconnect(onFrame); root.startStream() }
            win.afterRendering.connect(onFrame)
            win.requestUpdate()
        } else root.startStream()
    }
    Component.onDestruction: { if (surface) surface.stop(); if (fullscreen) { hop.setImmersive(false); hop.requestOrientation("auto") } }

    // the picture and its controls live in `content`, which moves to cover the whole window for full screen and for the small pop-out window
    Item {
        id: content
        objectName: root.objectName !== "" ? root.objectName + "Stage" : "kickPlayerStage"
        anchors.fill: parent
        Rectangle { anchors.fill: parent; color: "black" }
        Loader { id: mpvLoader; anchors.fill: parent; active: hop.videoEngine === "mpv" && (root.url !== "" || root.file !== "") && !root.recreating; sourceComponent: mpvSurface; onLoaded: root.surfaceLoaded(item) }
        Loader { id: gstLoader; anchors.fill: parent; active: hop.videoEngine === "gstreamer" && (root.url !== "" || root.file !== "") && !root.recreating; source: active ? "GstVideoSurfaceView.qml" : ""; onLoaded: root.surfaceLoaded(item) }
        Loader { id: qtLoader; anchors.fill: parent; active: hop.videoEngine === "qt" && (root.url !== "" || root.file !== "") && !root.recreating; source: active ? "AndroidVideoSurface.qml" : ""; onLoaded: root.surfaceLoaded(item) }
        Component { id: mpvSurface; VideoSurface { objectName: "kickSurface" } }

        PlainButton {
            objectName: "kickPlayerRetry"
            anchors { horizontalCenter: parent.horizontalCenter; verticalCenter: parent.verticalCenter; verticalCenterOffset: 56 }
            visible: !root.local && root.url !== "" && root.error !== ""
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

        // the seek meter along the top: drag left to rewind, right to skip ahead (a downloaded video: all of it; a live stream: the stretch Kick keeps)
        SeekMeter {
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }
            visible: root.started && !hop.inPictureInPicture && root.error === ""
            position: root.surface ? root.surface.position : 0
            duration: root.surface && root.surface.duration > 0 ? root.surface.duration : 0
            live: !root.local
            onSeekRequested: ms => { if (root.surface) root.surface.seek(ms) }
        }

        // controls: pause, volume
        RowLayout {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 10 }
            visible: root.started && !hop.inPictureInPicture
            spacing: 12
            AbstractButton {
                objectName: "kickPause"
                implicitWidth: 36; implicitHeight: 36; focusPolicy: Qt.NoFocus
                onClicked: root.surface.togglePause()
                Accessible.name: "pause or play"
                background: Rectangle { radius: 18; color: Qt.rgba(0, 0, 0, 0.55); border { width: 1; color: Qt.rgba(1, 1, 1, 0.5) } }
                contentItem: Label { text: root.surface && root.surface.paused ? "▶" : "❚❚"; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 13 }
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
