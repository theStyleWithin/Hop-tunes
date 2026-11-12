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
    readonly property var surface: mpvLoader.item ? mpvLoader.item : (gstLoader.item ? gstLoader.item : qtLoader.item)
    readonly property string error: blocked ? "that stream address is not one this app may open" : (surface ? String(surface.error) : "")
    readonly property bool started: surface !== null && url !== ""
    property string loadedUrl: ""
    property bool blocked: false

    function startStream() {
        if (!surface || url === "" || loadedUrl === url) return
        loadedUrl = url
        if (hop.videoEngine === "qt") {          // Android: Qt Multimedia plays the address itself, so it is checked here first
            if (!hop.kick.streamAllowed(url)) { blocked = true; return }
            blocked = false
            surface.source = url
        } else surface.openStream(url)
        surface.play()
    }
    onUrlChanged: {
        if (url === "") { if (surface) surface.stop(); loadedUrl = "" }
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
    Component.onDestruction: if (surface) surface.stop()

    Rectangle { anchors.fill: parent; color: "black" }
    Loader { id: mpvLoader; anchors.fill: parent; active: hop.videoEngine === "mpv" && root.url !== ""; sourceComponent: mpvSurface; onLoaded: root.surfaceLoaded(item) }
    Loader { id: gstLoader; anchors.fill: parent; active: hop.videoEngine === "gstreamer" && root.url !== ""; source: active ? "GstVideoSurfaceView.qml" : ""; onLoaded: root.surfaceLoaded(item) }
    Loader { id: qtLoader; anchors.fill: parent; active: hop.videoEngine === "qt" && root.url !== ""; source: active ? "AndroidVideoSurface.qml" : ""; onLoaded: root.surfaceLoaded(item) }
    Component { id: mpvSurface; VideoSurface { objectName: "kickSurface" } }

    Label {
        anchors.centerIn: parent
        width: parent.width - 40; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
        visible: root.url !== "" && (root.error !== "" || hop.videoEngine === "none" || gstLoader.status === Loader.Error)
        text: root.error !== "" ? "the stream could not be played: " + root.error
                                : "no video engine is available in this build"
        color: "white"; font { family: Theme.font; pixelSize: Theme.fs(16) }
    }
    Label {
        anchors.centerIn: parent
        visible: root.started && root.error === "" && root.surface && root.surface.idle
        text: "connecting…"; color: Qt.rgba(1, 1, 1, 0.7); font { family: Theme.font; pixelSize: Theme.fs(18) }
    }

    // controls: pause, volume
    RowLayout {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 10 }
        visible: root.started
        spacing: 12
        AbstractButton {
            objectName: "kickPause"
            implicitWidth: 36; implicitHeight: 36; focusPolicy: Qt.NoFocus
            onClicked: root.surface.togglePause()
            Accessible.name: "pause or play"
            background: Rectangle { radius: 18; color: Qt.rgba(0, 0, 0, 0.55); border { width: 1; color: Qt.rgba(1, 1, 1, 0.5) } }
            contentItem: Label { text: root.surface && root.surface.paused ? "▶" : "❚❚"; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 13 }
        }
        Label { text: "live"; color: "white"; font { family: Theme.font; pixelSize: Theme.fs(13) }
                leftPadding: 7; rightPadding: 7; topPadding: 2; bottomPadding: 2; background: Rectangle { color: "#e5484d"; radius: 3 } }
        Item { Layout.fillWidth: true }
        Slider {
            objectName: "kickVolume"
            from: 0; to: 100; value: root.surface ? root.surface.volume : 100
            implicitWidth: 120; focusPolicy: Qt.NoFocus
            onMoved: if (root.surface) root.surface.volume = value
        }
    }
}
