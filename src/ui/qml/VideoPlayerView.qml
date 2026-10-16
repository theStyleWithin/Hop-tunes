// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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

                Timer { id: idleTimer; interval: 3500; running: !surface.paused; onTriggered: page.idle = true }
                Timer { interval: 5000; running: !surface.paused; repeat: true; onTriggered: hop.saveVideoProgress(surface.position, surface.duration) }
                MouseArea { anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton; onPositionChanged: page.wake(); z: 100 }
                Keys.onPressed: function (e) { page.wake(); e.accepted = false }

                VideoSurface {
                    id: surface
                    objectName: "videoSurface"
                    anchors.fill: parent
                    Component.onCompleted: {
                        setResumePosition(hop.videoPlayerResumeMs)
                        source = hop.videoPlayerPath
                        play()
                    }
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
