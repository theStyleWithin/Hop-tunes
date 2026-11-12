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
    property bool blocked: false
    signal retryRequested()                      // "try again" after an error: the page asks Kick for a fresh stream address

    function startStream() {
        const want = local ? file : url
        if (!surface || want === "" || loadedUrl === want) return
        loadedUrl = want
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
    Component.onDestruction: if (surface) surface.stop()

    Rectangle { anchors.fill: parent; color: "black" }
    Loader { id: mpvLoader; anchors.fill: parent; active: hop.videoEngine === "mpv" && (root.url !== "" || root.file !== ""); sourceComponent: mpvSurface; onLoaded: root.surfaceLoaded(item) }
    Loader { id: gstLoader; anchors.fill: parent; active: hop.videoEngine === "gstreamer" && (root.url !== "" || root.file !== ""); source: active ? "GstVideoSurfaceView.qml" : ""; onLoaded: root.surfaceLoaded(item) }
    Loader { id: qtLoader; anchors.fill: parent; active: hop.videoEngine === "qt" && (root.url !== "" || root.file !== ""); source: active ? "AndroidVideoSurface.qml" : ""; onLoaded: root.surfaceLoaded(item) }
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
        Label { visible: !root.local; text: "live"; color: "white"; font { family: Theme.font; pixelSize: Theme.fs(13) }
                leftPadding: 7; rightPadding: 7; topPadding: 2; bottomPadding: 2; background: Rectangle { color: "#e5484d"; radius: 3 } }
        Slider {   // a downloaded video can be moved around in
            objectName: "kickSeek"
            visible: root.local
            Layout.fillWidth: true; focusPolicy: Qt.NoFocus
            from: 0; to: root.surface && root.surface.duration > 0 ? root.surface.duration : 1
            value: root.surface ? root.surface.position : 0
            onMoved: if (root.surface) root.surface.seek(value)
        }
        Item { visible: !root.local; Layout.fillWidth: true }
        Slider {
            objectName: "kickVolume"
            from: 0; to: 100; value: root.surface ? root.surface.volume : 100
            implicitWidth: 120; focusPolicy: Qt.NoFocus
            onMoved: if (root.surface) root.surface.volume = value
        }
    }
}
