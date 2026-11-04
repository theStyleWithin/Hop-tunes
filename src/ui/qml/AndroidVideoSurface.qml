// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtMultimedia

// The video picture on Android: Qt Multimedia. It exposes the same properties and functions as the desktop
// engines (VideoSurface.qml / GstVideoSurfaceView.qml), so the player screen does not care which one it has.
// Loaded only on Android (by source), because the desktop builds do not have the QtMultimedia QML module.
Item {
    id: root
    property string source: ""
    property int volume: 100                     // percent; Android's player stops at 100
    readonly property bool paused: player.playbackState !== MediaPlayer.PlayingState
    readonly property bool idle: player.source.toString() === ""
    readonly property real position: player.position
    readonly property real duration: player.duration
    readonly property string hwdecActive: ""
    readonly property string error: player.errorString
    readonly property var subtitleTracks: []
    property real resumeMs: 0

    function setResumePosition(ms) { resumeMs = ms > 0 ? ms : 0 }
    function play() { player.play() }
    function pause() { player.pause() }
    function togglePause() { player.playbackState === MediaPlayer.PlayingState ? player.pause() : player.play() }
    function seek(ms) { player.position = Math.max(0, ms) }
    function stop() { player.stop() }
    function setSubtitleTrack(id) {}

    onSourceChanged: {
        player.stop()
        player.source = root.source === "" ? "" : (root.source.indexOf("://") > 0 ? root.source : "file://" + root.source)
    }

    MediaPlayer {
        id: player
        videoOutput: output
        audioOutput: AudioOutput { volume: Math.min(1.0, root.volume / 100) }
        onMediaStatusChanged: {
            if (mediaStatus === MediaPlayer.LoadedMedia && root.resumeMs > 0) {
                position = root.resumeMs
                root.resumeMs = 0
            }
        }
    }
    VideoOutput {
        id: output
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
    }
}
