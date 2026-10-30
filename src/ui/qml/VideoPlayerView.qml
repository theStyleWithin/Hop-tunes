// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import HopTunes.Video

// Video playback (spec §5b), in one of three viewing styles (the profile's default is set in
// settings › video settings; the player can switch for the moment):
//   window     the video beside an up-next list, the movie's details underneath
//   theater    a wide video across the window, details and up-next below it
//   fullscreen the video fills the screen
// Only ever instantiated while hop.videoPlayerOpen is true — the Loader keeps MpvVideoItem (a
// QQuickFramebufferObject, needing a real GL scenegraph) out of the tree the rest of the time, so
// nothing pays for an mpv instance unless a video is actually open.
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
                focus: true

                // ---- viewing style -------------------------------------------------------------
                property string style: hop.videoViewStyle           // "window" | "theater" | "fullscreen"
                property string styleBeforeFullscreen: "window"
                readonly property bool full: style === "fullscreen"
                readonly property int margin: 24
                readonly property int headerH: full ? 0 : 56
                readonly property bool wide: width >= 980            // room for the up-next list
                readonly property int sideW: wide ? Math.min(360, Math.round(width * 0.27)) : 0

                // The pretend live chat (settings › video settings): shown beside the video, or over it in fullscreen.
                property bool chatOn: hop.videoChat
                readonly property bool chatFits: full ? width >= 700 : sideW > 0

                function setStyle(s) {
                    if (s === style) return
                    if (s === "fullscreen") styleBeforeFullscreen = style
                    style = s
                }
                // Esc: leave fullscreen first; otherwise go back to the movie's page.
                function stepBack() {
                    if (full) setStyle(styleBeforeFullscreen === "fullscreen" ? "window" : styleBeforeFullscreen)
                    else closeAndBack()
                }
                // A real full screen for the fullscreen style; the previous window state comes back after.
                property int windowBefore: Window.Windowed
                onFullChanged: {
                    const w = root.Window.window
                    if (!w) return
                    if (full) { windowBefore = w.visibility; w.visibility = Window.FullScreen }
                    else if (w.visibility === Window.FullScreen) w.visibility = windowBefore === Window.FullScreen ? Window.Windowed : windowBefore
                }
                Component.onDestruction: {
                    const w = root.Window.window
                    if (w && full && w.visibility === Window.FullScreen) w.visibility = windowBefore === Window.FullScreen ? Window.Windowed : windowBefore
                }

                // ---- where the video goes ------------------------------------------------------
                readonly property real availH: height - headerH
                readonly property real frameW: full ? width
                    : style === "theater" ? width
                    : Math.max(320, Math.min(width - 2 * margin - (sideW > 0 ? sideW + margin : 0),
                                              (availH - 150) * 16 / 9))
                readonly property real frameH: full ? height
                    : style === "theater" ? Math.min(width * 9 / 16, availH * (chatOn && chatFits ? 0.55 : 0.66))
                    : frameW * 9 / 16
                readonly property real frameX: full ? 0 : style === "theater" ? 0 : margin
                readonly property real frameY: full ? 0 : headerH
                readonly property real belowY: frameY + frameH + 14

                // Controls fade out after a few idle seconds in fullscreen; elsewhere they stay.
                property bool idle: false
                function wake() { idle = false; idleTimer.restart() }
                readonly property real chrome: (full && idle) ? 0.0 : 1.0
                Component.onCompleted: { wake(); forceActiveFocus() }

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
                    property int volume: 100
                    function play() {}
                    function pause() {}
                    function togglePause() {}
                    function seek(ms) {}
                    function stop() {}
                    function setSubtitleTrack(id) {}
                    function setResumePosition(ms) {}
                }
                readonly property var surface: mpvLoader.item || gstLoader.item || nullSurface

                // The movie volume: its own setting (hop.videoVolume, 0-150 %), separate from the music volume,
                // and it can go above 100 for quiet films.
                Binding { target: page.surface; property: "volume"; value: hop.videoVolume; when: page.surface !== nullSurface }
                property int volumeBeforeMute: 100
                function setVolume(v) { hop.videoVolume = Math.max(0, Math.min(150, Math.round(v))) }
                function toggleMute() {
                    if (hop.videoVolume > 0) { volumeBeforeMute = hop.videoVolume; setVolume(0) }
                    else setVolume(volumeBeforeMute > 0 ? volumeBeforeMute : 100)
                }
                readonly property var info: hop.videoPlayerInfo
                readonly property var meta: info.meta || ({})
                readonly property string shownTitle: (meta.title || "") !== "" ? meta.title : (info.fileName || hop.videoPlayerTitle)

                Timer { id: idleTimer; interval: 3500; running: !page.surface.paused; onTriggered: page.idle = true }
                Timer {
                    interval: 5000; running: !page.surface.paused; repeat: true
                    onTriggered: hop.saveVideoProgress(page.surface.position, page.surface.duration)
                }
                Keys.onPressed: function (e) {
                    page.wake()
                    if (e.key === Qt.Key_F) { setStyle(full ? (styleBeforeFullscreen === "fullscreen" ? "window" : styleBeforeFullscreen) : "fullscreen"); e.accepted = true }
                    else if (e.key === Qt.Key_T) { setStyle(style === "theater" ? "window" : "theater"); e.accepted = true }
                    else if (e.key === Qt.Key_C) { chatOn = !chatOn; e.accepted = true }
                    else if (e.key === Qt.Key_Escape) { stepBack(); e.accepted = true }
                    else if (e.key === Qt.Key_Space) { surface.togglePause(); e.accepted = true }
                    else if (e.key === Qt.Key_Up) { setVolume(hop.videoVolume + 5); e.accepted = true }
                    else if (e.key === Qt.Key_Down) { setVolume(hop.videoVolume - 5); e.accepted = true }
                    else if (e.key === Qt.Key_M) { toggleMute(); e.accepted = true }
                    else e.accepted = false
                }

                // Page background for window and theater (fullscreen is black all the way).
                Rectangle { anchors.fill: parent; color: Theme.bg; visible: !page.full }

                // ---- the engine -----------------------------------------------------------------
                // Two backing engines (spec §5b): VideoSurface (MpvVideoItem, libmpv) where built with
                // it, GstVideoSurfaceView (GStreamer's qml6glsink) otherwise — see CMakeLists.txt's
                // HOPTUNES_HAVE_MPV. Both expose the same property/method surface.
                //
                // gstLoader loads GstVideoSurfaceView.qml by *source* (a URL), not as an inline
                // Component: that file's "import org.freedesktop.gstreamer.Qt6GLVideoItem" only
                // resolves when a plugin providing that QML module is present (qml6glsink, from
                // gstreamer1.0-qt6 — not something every build has). An inline Component referencing
                // that type is resolved eagerly when this document loads, which would take the whole
                // app down if the module is missing. Loader.source defers resolution and reports
                // Loader.Error instead of crashing.
                property string loadedPath: ""
                function loadCurrent() {
                    const item = surface
                    if (item === nullSurface || !hop.videoPlayerPath) return
                    loadedPath = hop.videoPlayerPath
                    item.setResumePosition(hop.videoPlayerResumeMs)
                    item.source = hop.videoPlayerSource
                    item.play()
                }
                function onSurfaceLoaded(item) {
                    // The GStreamer engine wraps Qt's own GL context on open, which needs that context to
                    // exist — true once the window has rendered at least one frame. One real rendered
                    // frame, not a guessed delay, removes the race.
                    const win = root.Window.window
                    if (win) {
                        function onFrame() { win.afterRendering.disconnect(onFrame); page.loadCurrent() }
                        win.afterRendering.connect(onFrame)
                        win.requestUpdate()
                    } else {
                        page.loadCurrent()
                    }
                }
                // Another video chosen from the up-next list while this one is open.
                Connections {
                    target: hop
                    function onVideoPlayerChanged() {
                        if (hop.videoPlayerOpen && page.loadedPath !== "" && hop.videoPlayerPath !== page.loadedPath) page.loadCurrent()
                    }
                }

                // The frame holds the picture and its controls; its place and size follow the style.
                Item {
                    id: frame
                    x: page.frameX; y: page.frameY; width: page.frameW; height: page.frameH
                    clip: true
                    Rectangle { anchors.fill: parent; color: "black" }

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

                    MouseArea {
                        anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton
                        onPositionChanged: page.wake(); z: 100
                    }
                    // Clicking the picture hands the keyboard back to the player (f, t, c, space, esc).
                    TapHandler { onTapped: page.forceActiveFocus() }

                    // Fullscreen only: the title and back button float over the picture.
                    RowLayout {
                        visible: page.full
                        opacity: page.chrome
                        Behavior on opacity { NumberAnimation { duration: Theme.reduceMotion ? 0 : 450 } }
                        anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: 32; rightMargin: 32; topMargin: 20 }
                        AbstractButton {
                            objectName: "videoBackFull"
                            implicitWidth: 44; implicitHeight: 44
                            onClicked: page.stepBack()
                            Accessible.name: "back"
                            background: Rectangle { radius: 22; color: Theme.fade(0.5); border { width: 2; color: Theme.border } }
                            contentItem: Label { text: "‹"; color: "white"; font.pixelSize: 26; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; bottomPadding: 4 }
                        }
                        Label { text: page.shownTitle; color: "white"; elide: Text.ElideRight; Layout.fillWidth: true; font { family: Theme.font; pixelSize: 18 } }
                        Label {
                            visible: surface.error !== ""
                            text: surface.error; color: Theme.danger; font { family: Theme.font; pixelSize: 13 }
                        }
                    }

                    // Controls over the bottom of the picture.
                    Rectangle {
                        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                        height: controls.implicitHeight + 28
                        opacity: page.chrome
                        Behavior on opacity { NumberAnimation { duration: Theme.reduceMotion ? 0 : 450 } }
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "transparent" }
                            GradientStop { position: 1.0; color: "#c0000000" }
                        }
                        ColumnLayout {
                            id: controls
                            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 18; rightMargin: 18; bottomMargin: 10 }
                            spacing: 4

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
                                spacing: 12
                                TransportButton { glyph: surface.paused ? "play" : "pause"; round: true; iconSize: 18; implicitWidth: 48
                                                  onClicked: surface.togglePause(); Accessible.name: surface.paused ? "play" : "pause" }
                                AbstractButton {
                                    id: muteBtn
                                    objectName: "videoMute"
                                    implicitWidth: 48; implicitHeight: 32
                                    onClicked: page.toggleMute()
                                    Accessible.name: hop.videoVolume === 0 ? "unmute" : "mute"
                                    ToolTip.visible: hovered; ToolTip.delay: 600; ToolTip.text: "mute (m)"
                                    background: Rectangle { radius: 6; color: muteBtn.hovered ? Theme.fade(0.5) : "transparent" }
                                    contentItem: Label { text: hop.videoVolume === 0 ? "muted" : "vol"; color: hop.videoVolume === 0 ? "#ff9f43" : "white"
                                                         horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                                         font { family: Theme.font; pixelSize: 13 } }
                                }
                                Slider {
                                    id: vol
                                    objectName: "videoVolume"
                                    implicitWidth: 120
                                    from: 0; to: 150; stepSize: 5; snapMode: Slider.SnapAlways
                                    value: hop.videoVolume
                                    onMoved: page.setVolume(value)
                                    Accessible.name: "movie volume"
                                    ToolTip.visible: hovered || pressed; ToolTip.text: Math.round(hop.videoVolume) + "%" + (hop.videoVolume > 100 ? " (boosted)" : "")
                                    background: Rectangle {
                                        x: vol.leftPadding; y: vol.topPadding + vol.availableHeight / 2 - height / 2
                                        width: vol.availableWidth; height: 4; color: "#404048"
                                        Rectangle { width: Math.min(vol.visualPosition, 100 / 150) * parent.width; height: parent.height; color: hop.accent }
                                        // the boost zone, above the normal 100 %
                                        Rectangle { x: parent.width * 100 / 150; width: Math.max(0, vol.visualPosition - 100 / 150) * parent.width; height: parent.height; color: "#ff9f43" }
                                        Rectangle { x: parent.width * 100 / 150 - 1; y: -3; width: 2; height: 10; color: "#80808c" }
                                    }
                                    handle: Rectangle {
                                        x: vol.leftPadding + vol.visualPosition * (vol.availableWidth - width)
                                        y: vol.topPadding + vol.availableHeight / 2 - height / 2
                                        width: 12; height: 12; radius: 6; color: "white"
                                    }
                                }
                                Label {
                                    objectName: "videoVolumeLabel"
                                    text: hop.videoVolume + "%"
                                    Layout.preferredWidth: 40
                                    color: hop.videoVolume > 100 ? "#ff9f43" : "white"; font { family: Theme.font; pixelSize: 12 }
                                }
                                Label {
                                    visible: surface.hwdecActive !== "" && surface.hwdecActive !== "no"
                                    text: "hw decode: " + surface.hwdecActive
                                    color: "#b0b0b8"; font { family: Theme.font; pixelSize: 12 }
                                }
                                Label {
                                    visible: surface.error !== "" && !page.full
                                    text: surface.error; color: Theme.danger; font { family: Theme.font; pixelSize: 12 }
                                }
                                Item { Layout.fillWidth: true }
                                AbstractButton {
                                    objectName: "subtitleToggle"
                                    visible: surface.subtitleTracks.length > 0
                                    implicitWidth: 56; implicitHeight: 32
                                    property int current: -1
                                    onClicked: { current = (current + 1) % (surface.subtitleTracks.length + 1); surface.setSubtitleTrack(current >= surface.subtitleTracks.length ? -1 : surface.subtitleTracks[current].id) }
                                    Accessible.name: "subtitles"
                                    background: Rectangle { radius: 6; color: Theme.fade(0.5) }
                                    contentItem: Label { text: "CC"; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                                         font { family: Theme.font; pixelSize: 13; weight: Font.Medium } }
                                }
                                AbstractButton {
                                    objectName: "videoChatToggle"
                                    implicitWidth: 64; implicitHeight: 32
                                    onClicked: page.chatOn = !page.chatOn
                                    Accessible.name: "live chat"
                                    ToolTip.visible: hovered; ToolTip.delay: 600; ToolTip.text: "pretend live chat (c)"
                                    background: Rectangle { radius: 6; color: Theme.fade(0.5); border { width: page.chatOn ? 1 : 0; color: hop.accent } }
                                    contentItem: Label { text: "chat"; color: page.chatOn ? hop.accent : "white"
                                                         horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                                         font { family: Theme.font; pixelSize: 13 } }
                                }
                                AbstractButton {
                                    id: theaterBtn
                                    objectName: "videoStyleTheater"
                                    implicitWidth: 78; implicitHeight: 32
                                    onClicked: page.setStyle(page.style === "theater" ? "window" : "theater")
                                    Accessible.name: "theater mode"
                                    ToolTip.visible: hovered; ToolTip.delay: 600; ToolTip.text: "theater mode (t)"
                                    visible: !page.full
                                    background: Rectangle { radius: 6; color: Theme.fade(0.5); border { width: page.style === "theater" ? 1 : 0; color: hop.accent } }
                                    contentItem: Label { text: "theater"; color: page.style === "theater" ? hop.accent : "white"
                                                         horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                                         font { family: Theme.font; pixelSize: 13 } }
                                }
                                AbstractButton {
                                    objectName: "videoStyleFullscreen"
                                    implicitWidth: 92; implicitHeight: 32
                                    onClicked: page.setStyle(page.full ? (page.styleBeforeFullscreen === "fullscreen" ? "window" : page.styleBeforeFullscreen) : "fullscreen")
                                    Accessible.name: "fullscreen"
                                    ToolTip.visible: hovered; ToolTip.delay: 600; ToolTip.text: page.full ? "leave fullscreen (esc)" : "fullscreen (f)"
                                    background: Rectangle { radius: 6; color: Theme.fade(0.5) }
                                    contentItem: Label { text: page.full ? "exit" : "fullscreen"; color: "white"
                                                         horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                                         font { family: Theme.font; pixelSize: 13 } }
                                }
                            }
                        }
                    }
                }

                function closeAndBack() {
                    hop.closeVideoPlayer(surface.position, surface.duration)
                }

                // ---- window and theater: the page around the picture ---------------------------------
                // header: back to the movie
                RowLayout {
                    visible: !page.full
                    x: page.margin; y: 0; width: page.width - 2 * page.margin; height: page.headerH
                    spacing: 12
                    AbstractButton {
                        objectName: "videoBack"
                        implicitWidth: 40; implicitHeight: 40
                        onClicked: page.closeAndBack()
                        Accessible.name: "back"
                        background: Rectangle { radius: 20; color: parent.hovered ? Theme.hover : "transparent"; border { width: 2; color: Theme.border } }
                        contentItem: Label { text: "‹"; color: Theme.text; font.pixelSize: 24; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; bottomPadding: 3 }
                    }
                    Label { text: page.shownTitle; color: Theme.text; elide: Text.ElideRight; Layout.fillWidth: true
                            font { family: Theme.font; pixelSize: 20 } }
                }

                // details underneath: what the user filled in on the movie's page
                Flickable {
                    id: infoFlick
                    objectName: "videoInfoFlick"
                    visible: !page.full
                    x: page.style === "theater" ? page.margin : page.frameX
                    y: page.belowY
                    width: page.style === "theater" ? page.width - page.sideW - (page.sideW > 0 ? 3 : 2) * page.margin : page.frameW
                    height: Math.max(0, page.height - y - page.margin)
                    contentWidth: width
                    contentHeight: infoCol.implicitHeight + 10
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}

                    ColumnLayout {
                        id: infoCol
                        width: infoFlick.width - 12
                        spacing: 8

                        Label { text: page.shownTitle; color: Theme.text; Layout.fillWidth: true; wrapMode: Text.WordWrap
                                font { family: Theme.font; pixelSize: 26; weight: Font.Light } }
                        Label {
                            readonly property var bits: [page.meta.year, page.meta.rating, page.meta.score ? "★ " + page.meta.score : "", page.info.length].filter(function (s) { return !!s })
                            visible: bits.length > 0
                            text: bits.join("   ·   ")
                            color: Theme.muted; font { family: Theme.font; pixelSize: 15 }
                        }
                        Label {
                            text: [page.info.resolution, page.info.codec, page.info.hdr ? "HDR" : "", page.info.container, page.info.size].filter(function (s) { return !!s }).join("  ·  ")
                            visible: text !== ""
                            color: Theme.dim; font { family: Theme.font; pixelSize: 13 }
                        }
                        Label { visible: !!page.meta.tagline; text: page.meta.tagline || ""; color: Theme.text2; Layout.fillWidth: true
                                wrapMode: Text.WordWrap; font { family: Theme.font; pixelSize: 16; italic: true } }
                        Label { visible: !!page.meta.overview; text: page.meta.overview || ""; color: Theme.text; Layout.fillWidth: true
                                wrapMode: Text.WordWrap; font { family: Theme.font; pixelSize: 15 } }

                        Repeater {
                            model: [["genre", "genre"], ["director", "director"], ["writers", "writers"], ["studio", "studio"], ["tags", "tags"]]
                            delegate: RowLayout {
                                required property var modelData
                                visible: !!page.meta[modelData[0]]
                                Layout.fillWidth: true
                                spacing: 14
                                Label { text: modelData[1]; color: Theme.muted; Layout.preferredWidth: 80; font { family: Theme.font; pixelSize: 14 } }
                                Label { text: page.meta[modelData[0]] || ""; color: Theme.text; Layout.fillWidth: true
                                        wrapMode: Text.WordWrap; font { family: Theme.font; pixelSize: 14 } }
                            }
                        }
                        Label {
                            visible: !!page.info.cast && page.info.cast.length > 0
                            text: "cast   " + (page.info.cast || []).map(function (c) { return c.name + (c.role ? " (" + c.role + ")" : "") }).filter(function (s) { return s.trim() !== "" }).join(",  ")
                            color: Theme.text2; Layout.fillWidth: true; wrapMode: Text.WordWrap
                            font { family: Theme.font; pixelSize: 14 }
                        }

                        // nothing filled in yet: point at the boxes
                        Label {
                            readonly property bool empty: !page.meta.title && !page.meta.year && !page.meta.overview && !page.meta.genre && !page.meta.director
                            visible: empty
                            text: "no details yet. open the movie's page to fill them in."
                            color: Theme.dim; font { family: Theme.font; pixelSize: 14 }
                        }
                        AbstractButton {
                            id: editBtn
                            objectName: "videoEditDetails"
                            visible: page.info.row !== undefined && page.info.row >= 0
                            hoverEnabled: true
                            implicitHeight: 28
                            onClicked: { const r = page.info.row; hop.closeVideoPlayer(surface.position, surface.duration); hop.openVideoDetail(r) }
                            contentItem: Label { text: "edit details"; color: editBtn.hovered ? hop.accent : Theme.muted
                                                 font { family: Theme.font; pixelSize: 14; underline: editBtn.hovered } }
                            background: Item {}
                        }

                        // pretend comments (window and theater only; off by default)
                        VideoComments {
                            objectName: "videoComments"
                            visible: hop.videoComments !== "off"
                            Layout.fillWidth: true
                            Layout.topMargin: 14
                            Layout.bottomMargin: 20
                            mode: hop.videoComments
                            seed: hop.videoPlayerPath
                            info: page.info
                            mine: hop.myVideoComments
                            duration: page.surface.duration
                            onLeaveRequested: page.forceActiveFocus()
                        }
                    }
                }

                // up next: the rest of the library
                Rectangle {
                    id: side
                    visible: !page.full && page.sideW > 0 && !page.chatOn
                    x: page.width - page.sideW - page.margin
                    y: page.style === "theater" ? page.belowY : page.headerH
                    width: page.sideW
                    height: Math.max(0, page.height - y - page.margin)
                    color: "transparent"
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8
                        Label { text: "your videos"; color: Theme.text; font { family: Theme.font; pixelSize: 17 } }
                        ListView {
                            id: sideList
                            objectName: "videoUpNext"
                            Layout.fillWidth: true; Layout.fillHeight: true
                            clip: true
                            spacing: 10
                            model: hop.videos
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar {}
                            delegate: AbstractButton {
                                id: it
                                required property int index
                                required property string title
                                required property string thumb
                                required property string duration
                                readonly property bool current: it.index === page.info.row
                                width: sideList.width - 10; height: 72
                                hoverEnabled: true
                                onClicked: if (!current) hop.switchVideo(it.index, page.surface.position, page.surface.duration)
                                Accessible.name: title
                                background: Rectangle { color: it.hovered ? Theme.hover : "transparent"
                                                        Rectangle { visible: it.current; width: 3; height: parent.height; color: hop.accent } }
                                contentItem: RowLayout {
                                    spacing: 12
                                    Rectangle {
                                        Layout.preferredWidth: 120; Layout.preferredHeight: 68
                                        Layout.leftMargin: 6
                                        color: Theme.panel; clip: true
                                        Image {
                                            anchors.fill: parent
                                            source: it.thumb
                                            fillMode: Image.PreserveAspectCrop
                                            asynchronous: true; cache: false
                                            visible: it.thumb !== ""
                                        }
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Label { text: it.title; Layout.fillWidth: true; elide: Text.ElideRight; maximumLineCount: 2; wrapMode: Text.WordWrap
                                                color: it.current ? hop.accent : Theme.text; font { family: Theme.font; pixelSize: 14 } }
                                        Label { text: it.current ? "now playing" : it.duration; color: Theme.muted; font { family: Theme.font; pixelSize: 12 } }
                                    }
                                }
                            }
                        }
                    }
                }

                // The pretend live chat: beside the video in window and theater, over it in fullscreen.
                LiveChat {
                    id: chat
                    objectName: "liveChat"
                    visible: page.chatOn && page.chatFits
                    overlay: page.full
                    onLeaveRequested: page.forceActiveFocus()
                    look: hop.videoChatLook
                    seed: hop.videoPlayerPath
                    info: page.info
                    position: page.surface.position
                    duration: page.surface.duration
                    x: page.full ? page.width - 360 : page.width - page.sideW - page.margin
                    y: page.full ? 80 : (page.style === "theater" ? page.belowY : page.headerH)
                    width: page.full ? 340 : page.sideW
                    height: page.full ? Math.max(0, page.height - y - 130) : Math.max(0, page.height - y - page.margin)
                    opacity: page.full && page.idle ? 0.55 : 1.0
                    z: 50
                }
            }
        }
    }
}
