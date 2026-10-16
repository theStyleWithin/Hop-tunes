// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import HopTunes.Video

// Full-window video playback (spec §5b). Only ever instantiated while hop.videoPlayerOpen
// is true — the Loader keeps MpvVideoItem (a QQuickFramebufferObject, needing a real GL
// scenegraph) out of the tree the rest of the time, so nothing pays for an mpv instance
// unless a video is actually open.
Rectangle {
    id: root
    color: "black"
    clip: true

    Loader {
        id: loader
        anchors.fill: parent
        active: hop.videoPlayerOpen
        sourceComponent: Component {
            Item {
                id: page
                anchors.fill: parent

                // Controls fade out after a few idle seconds while playing (mirrors
                // NowPlayingView); any mouse movement or key press brings them back.
                property bool idle: false
                function wake() { idle = false; idleTimer.restart() }
                readonly property real chrome: idle ? 0.0 : 1.0
                Component.onCompleted: wake()

                // Nothing (yet, or hop.videoEngine === "none") until the Loader below
                // resolves: a null-object stand-in keeps every binding below safe to
                // evaluate without guarding each one individually.
                QtObject {
                    id: nullSurface
                    readonly property bool paused: true
                    readonly property bool idle: true
                    readonly property real position: 0
                    readonly property real duration: 0
                    readonly property string hwdecActive: ""
                    readonly property string error: ""
                    readonly property var subtitleTracks: []
                    function play() {}
                    function pause() {}
                    function togglePause() {}
                    function seek(ms) {}
                    function stop() {}
                    function setSubtitleTrack(id) {}
                    function setResumePosition(ms) {}
                }
                readonly property var surface: mpvLoader.item || gstLoader.item || nullSurface

                Timer { id: idleTimer; interval: 3500; running: !page.surface.paused; onTriggered: page.idle = true }
                Timer {
                    interval: 5000; running: !page.surface.paused; repeat: true
                    onTriggered: hop.saveVideoProgress(page.surface.position, page.surface.duration)
                }
                MouseArea { anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton; onPositionChanged: page.wake(); z: 100 }
                Keys.onPressed: function (e) { page.wake(); e.accepted = false }

                // Two backing engines (spec §5b): VideoSurface (MpvVideoItem, libmpv) where
                // built with it, GstVideoSurfaceView (GStreamer's qml6glsink) otherwise —
                // see CMakeLists.txt's HOPTUNES_HAVE_MPV. Both expose the same property/
                // method surface, so everything below just talks to "surface" either way.
                //
                // gstLoader loads GstVideoSurfaceView.qml by *source* (a URL), not as an
                // inline Component: that file's "import org.freedesktop.gstreamer.Qt6GLVideoItem"
                // only resolves when a plugin providing that QML module is actually present
                // (qml6glsink, from gstreamer1.0-qt6 — not something every build/package has,
                // e.g. the AppImage, which relies on libmpv instead). An inline Component
                // referencing that type gets resolved eagerly when this whole document loads,
                // which would take the entire app down if the module is missing even though
                // hop.videoEngine never selects "gstreamer" there. Loader.source defers
                // resolution until the Loader actually activates, and reports Loader.Error
                // instead of crashing when the type can't be found.
                function onSurfaceLoaded(item) {
                    item.setResumePosition(hop.videoPlayerResumeMs)
                    // The GStreamer engine wraps Qt's own GL context on open, which needs
                    // that context to actually exist — true once the window has rendered at
                    // least one frame, which it normally has long before a video is opened,
                    // but not guaranteed (confirmed by testing: an automated/scripted launch
                    // that opens a video within the first frame or two hits "could not
                    // initialize window system"). One real rendered frame, not a guessed
                    // delay, removes the race.
                    function openIt() { item.source = hop.videoPlayerPath; item.play() }
                    const win = root.Window.window
                    if (win) {
                        function onFrame() { win.afterRendering.disconnect(onFrame); openIt() }
                        win.afterRendering.connect(onFrame)
                        win.requestUpdate()
                    } else {
                        openIt()
                    }
                }
                Loader {
                    id: mpvLoader
                    anchors.fill: parent
                    active: hop.videoEngine === "mpv"
                    sourceComponent: mpvSurface
                    onLoaded: page.onSurfaceLoaded(item)
                }
                Loader {
                    id: gstLoader
                    anchors.fill: parent
                    active: hop.videoEngine === "gstreamer"
                    source: active ? "GstVideoSurfaceView.qml" : ""
                    onLoaded: page.onSurfaceLoaded(item)
                }
                Component { id: mpvSurface; VideoSurface { objectName: "videoSurface" } }

                Label {
                    visible: hop.videoEngine === "none" || gstLoader.status === Loader.Error
                    anchors.centerIn: parent
                    text: "no video engine is available in this build"
                    color: "white"; font { family: Theme.font; pixelSize: 18 }
                }

                function closeAndBack() {
                    hop.closeVideoPlayer(surface.position, surface.duration)
                }

                RowLayout {
                    id: header
                    opacity: page.chrome
                    Behavior on opacity { NumberAnimation { duration: Theme.reduceMotion ? 0 : 450 } }
                    anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: 32; rightMargin: 32; topMargin: 20 }
                    AbstractButton {
                        objectName: "videoBack"
                        implicitWidth: 44; implicitHeight: 44
                        onClicked: closeAndBack()
                        Accessible.name: "back"
                        background: Rectangle { radius: 22; color: Theme.fade(0.5); border { width: 2; color: Theme.border } }
                        contentItem: Label { text: "‹"; color: "white"; font.pixelSize: 26; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; bottomPadding: 4 }
                    }
                    Label { text: hop.videoPlayerTitle; color: "white"; elide: Text.ElideRight; Layout.fillWidth: true; font { family: Theme.font; pixelSize: 18 } }
                    Label {
                        visible: surface.hwdecActive !== "" && surface.hwdecActive !== "no"
                        text: "hw decode: " + surface.hwdecActive
                        color: "#b0b0b8"; font { family: Theme.font; pixelSize: 13 }
                    }
                    Label {
                        visible: surface.error !== ""
                        text: surface.error; color: Theme.danger; font { family: Theme.font; pixelSize: 13 }
                    }
                }

                ColumnLayout {
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 32; rightMargin: 32; bottomMargin: 24 }
                    spacing: 8
                    opacity: page.chrome
                    Behavior on opacity { NumberAnimation { duration: Theme.reduceMotion ? 0 : 450 } }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Label { text: hop.formatTime(Math.max(1, surface.position)); color: "white"; font { family: Theme.font; pixelSize: 13 } }
                        Slider {
                            id: seek
                            Layout.fillWidth: true
                            from: 0; to: Math.max(1, surface.duration)
                            onMoved: surface.seek(value)
                            Binding { target: seek; property: "value"; value: surface.position; when: !seek.pressed }
                            Accessible.name: "seek"
                            background: Rectangle {
                                x: seek.leftPadding; y: seek.topPadding + seek.availableHeight / 2 - height / 2
                                width: seek.availableWidth; height: 4; color: "#404048"
                                Rectangle { width: seek.visualPosition * parent.width; height: parent.height; color: hop.accent }
                            }
                            handle: Rectangle {
                                x: seek.leftPadding + seek.visualPosition * (seek.availableWidth - width)
                                y: seek.topPadding + seek.availableHeight / 2 - height / 2
                                width: 12; height: 12; radius: 6; color: "white"
                                visible: seek.hovered || seek.pressed
                            }
                        }
                        Label { text: hop.formatTime(surface.duration); color: "white"; font { family: Theme.font; pixelSize: 13 } }
                    }

                    RowLayout {
                        spacing: 16
                        TransportButton { glyph: surface.paused ? "play" : "pause"; round: true; iconSize: 20; implicitWidth: 60
                                          onClicked: surface.togglePause(); Accessible.name: surface.paused ? "play" : "pause" }
                        Item { Layout.fillWidth: true }
                        AbstractButton {
                            objectName: "subtitleToggle"
                            visible: surface.subtitleTracks.length > 0
                            implicitWidth: 80; implicitHeight: 32
                            property int current: -1
                            onClicked: { current = (current + 1) % (surface.subtitleTracks.length + 1); surface.setSubtitleTrack(current >= surface.subtitleTracks.length ? -1 : surface.subtitleTracks[current].id) }
                            Accessible.name: "subtitles"
                            background: Rectangle { radius: 6; color: Theme.fade(0.5) }
                            contentItem: Label { text: "CC"; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                                 font { family: Theme.font; pixelSize: 13; weight: Font.Medium } }
                        }
                    }
                }
            }
        }
    }
}
