// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import org.freedesktop.gstreamer.Qt6GLVideoItem 1.0

// The GStreamer fallback video surface (spec §5b), used when libmpv isn't available
// (HOPTUNES_HAVE_MPV off — currently the Flatpak build). Exposes the same property/method
// surface as VideoSurface (MpvVideoItem) so VideoPlayerView.qml's controls don't need to
// know which engine is actually running underneath; everything here just forwards to
// hop.gstVideo, the one persistent GstVideoPlayer App owns (unlike MpvVideoItem, which
// owns its own MpvCore per instance — GstVideoPlayer is simpler to keep as one instance
// since nothing here needs more than one video open at a time).
Item {
    id: root

    property string source: ""
    property int _resumeMs: 0
    function setResumePosition(ms) { _resumeMs = ms }
    onSourceChanged: if (source !== "") hop.gstVideo.openSource(source, _resumeMs)

    readonly property bool paused: hop.gstVideo.paused
    readonly property bool idle: hop.gstVideo.idle
    readonly property real position: hop.gstVideo.position
    readonly property real duration: hop.gstVideo.duration
    readonly property string hwdecActive: hop.gstVideo.hwdecActive
    readonly property string error: hop.gstVideo.error
    readonly property var subtitleTracks: hop.gstVideo.subtitleTracks

    property int volume: hop.gstVideo.volume
    onVolumeChanged: if (hop.gstVideo.volume !== volume) hop.gstVideo.volume = volume
    function play() { hop.gstVideo.play() }
    function pause() { hop.gstVideo.pause() }
    function togglePause() { hop.gstVideo.togglePause() }
    function seek(ms) { hop.gstVideo.seek(ms) }
    function stop() { hop.gstVideo.stop() }
    function setSubtitleTrack(id) { hop.gstVideo.setSubtitleTrack(id) }

    GstGLQt6VideoItem {
        id: videoItem
        anchors.fill: parent
        // Synchronous: attachSurface() only builds the pipeline (no GL/state touched
        // yet), and it must finish before source is set below — Loader.onLoaded fires
        // right after this whole item tree is built, so a deferred attach here used to
        // lose the race and leave openSource() running against a still-null pipeline
        // (confirmed by testing: a real video opened to a silently black, stuck-at-0:00
        // player). The actual gst_element_set_state(PLAYING) call, which does need a
        // window that's already rendered at least once, happens later from openSource()
        // — by the time a video is opened, Main.qml's window has been rendering for a
        // while already, so no extra delay is needed there either.
        Component.onCompleted: hop.gstVideo.attachSurface(videoItem)
    }
}
