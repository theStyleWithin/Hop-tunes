// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Phones: the video player. Held upright the picture is clear (only the close arrow, the time and a thin progress
// bar are on it); play / pause sits just under it, the title row carries rotate and subtitles, and the button row
// has play, back 10 s, subtitles, forward 10 s and volume, with the next up / live chat / comments tabs below.
// Turned (or after the rotate button) the picture fills the screen: the controls show over it when you tap it and
// hide again after a few seconds, with the live chat strip on the right. The rotate button switches between the two.
Rectangle {
    id: root
    color: Theme.bg
    readonly property var info: hop.videoPlayerInfo
    readonly property var meta: info.meta || ({})
    readonly property string shownTitle: (meta.title || "") !== "" ? meta.title : (info.fileName || hop.videoPlayerTitle)
    property bool landscape: width > height
    property string tab: "nextup"           // nextup | chat | comments
    property bool chatShown: true
    // the picture: Qt Multimedia on Android (AndroidVideoSurface.qml, loaded by source so the desktop never needs it)
    readonly property bool engine: hop.videoEngine !== "none"
    readonly property var surface: surfaceLoader.item
    readonly property real position: surface ? surface.position : 0
    readonly property real duration: surface ? surface.duration : 0
    readonly property bool playing: surface ? !surface.paused : false
    property string loadedPath: ""
    function openCurrent() {
        if (!surface || !hop.videoPlayerPath) return
        loadedPath = hop.videoPlayerPath
        surface.setResumePosition(hop.videoPlayerResumeMs)
        surface.volume = hop.videoVolume
        surface.source = hop.videoPlayerPath
        surface.play()
    }
    Connections {
        target: hop
        function onVideoPlayerChanged() {
            if (hop.videoPlayerOpen && root.loadedPath !== "" && hop.videoPlayerPath !== root.loadedPath) root.openCurrent()
            if (!hop.videoPlayerOpen) root.loadedPath = ""
        }
        function onVideoVolumeChanged() { if (root.surface) root.surface.volume = hop.videoVolume }
    }

    function close() { hop.closeVideoPlayer(root.position, root.duration) }
    function rotate() { hop.requestOrientation(root.landscape ? "portrait" : "landscape") }
    Component.onCompleted: hop.requestOrientation(hop.videoOrientation)
    Component.onDestruction: hop.requestOrientation("auto")

    // landscape: the controls over the picture show on a tap and hide again while the film plays
    property bool controlsShown: true
    Timer { id: hideTimer; interval: 3500; running: root.landscape && root.controlsShown && root.playing; onTriggered: root.controlsShown = false }
    onLandscapeChanged: controlsShown = true
    readonly property bool overlay: !landscape || controlsShown

    component RoundBtn: AbstractButton {
        id: rb
        property string label: ""
        property string glyphKind: ""
        property bool accentFill: false
        property bool onPicture: true        // dark disc for use over the picture; outlined for use on the page
        implicitWidth: 44; implicitHeight: 44
        background: Rectangle {
            radius: width / 2
            color: rb.accentFill ? Theme.accent : (rb.onPicture ? "#66000000" : "transparent")
            border { width: rb.accentFill || rb.onPicture ? 0 : 1; color: Theme.borderSoft }
        }
        contentItem: Item {
            Icon { anchors.centerIn: parent; visible: rb.glyphKind !== ""; kind: rb.glyphKind || "rotate"; width: 22; height: 22
                   ink: rb.accentFill ? Theme.ink : (rb.onPicture ? "white" : Theme.text2) }
            Label { anchors.centerIn: parent; visible: rb.glyphKind === ""; text: rb.label
                    color: rb.accentFill ? Theme.ink : (rb.onPicture ? "white" : Theme.text2)
                    font { family: Theme.font; pixelSize: 15; weight: Font.Medium } }
        }
    }

    // the picture, with the controls it needs laid over it
    Item {
        id: screen
        x: 0; y: 0
        width: root.width
        height: root.landscape ? root.height : Math.round(root.width * 9 / 16) + 6
        Rectangle { anchors.fill: parent; color: "black" }
        Loader {
            id: surfaceLoader
            anchors.fill: parent
            active: hop.videoEngine === "qt" && hop.videoPlayerOpen
            source: "AndroidVideoSurface.qml"
            onLoaded: root.openCurrent()
        }
        MouseArea {   // upright: a tap plays / pauses; turned: a tap shows or hides the controls
            anchors.fill: parent
            onClicked: { if (root.landscape) root.controlsShown = !root.controlsShown; else if (root.surface) root.surface.togglePause() }
        }
        Label {
            visible: !root.engine
            anchors.centerIn: parent; width: parent.width - 80
            horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
            text: "no video engine is available in this build"
            color: "#c8c8ce"; font { family: Theme.font; pixelSize: 16 }
        }
        // top: close (and in landscape the title and the buttons)
        Rectangle { visible: root.overlay; anchors { left: parent.left; right: parent.right; top: parent.top } height: 70
                    gradient: Gradient { GradientStop { position: 0; color: "#99000000" } GradientStop { position: 1; color: "transparent" } } }
        RowLayout {
            visible: root.overlay
            anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: 14; rightMargin: 14; topMargin: 10 }
            spacing: 12
            AbstractButton {
                objectName: "phonePlayerClose"
                implicitWidth: 44; implicitHeight: 44
                onClicked: root.close()
                Accessible.name: "close"
                background: Item {}
                contentItem: Item { Icon { anchors.centerIn: parent; kind: "down"; ink: "white"; width: 26; height: 26 } }
            }
            Label { visible: root.landscape; text: root.shownTitle; color: "white"; elide: Text.ElideRight; Layout.fillWidth: true
                    font { family: Theme.font; pixelSize: 22 } }
            Item { visible: !root.landscape; Layout.fillWidth: true }
            RoundBtn { visible: root.landscape; objectName: "phonePlayerRotateLand"; glyphKind: "rotate"; accentFill: true; onClicked: root.rotate(); Accessible.name: "rotate" }
            RoundBtn { visible: root.landscape; label: "CC"; Accessible.name: "subtitles" }
            Rectangle {
                visible: root.landscape
                implicitWidth: 44; implicitHeight: 44; radius: 22; color: "#66000000"
                TransportButton { anchors.fill: parent; glyph: hop.videoVolume === 0 ? "mute" : "volume"; iconSize: 20; Accessible.name: "volume"
                                  onClicked: hop.videoVolume = hop.videoVolume >= 150 ? 50 : (hop.videoVolume >= 100 ? 150 : 100) }
            }
            RoundBtn { visible: root.landscape && hop.videoChat; label: "chat"; accentFill: root.chatShown; Accessible.name: "live chat"
                       onClicked: root.chatShown = !root.chatShown }
        }
        // landscape only: back 10 s, play / pause, forward 10 s in the middle of the picture
        Row {
            visible: root.landscape && root.overlay
            anchors.centerIn: parent
            spacing: 70
            Rectangle {
                width: 64; height: 64; radius: 32; color: "#66000000"; anchors.verticalCenter: parent.verticalCenter
                TransportButton { anchors.fill: parent; glyph: "prev"; iconSize: 24; Accessible.name: "back 10 seconds"
                                  onClicked: if (root.surface) root.surface.seek(root.position - 10000) }
            }
            TransportButton {
                objectName: "phonePlayerPlayLand"
                glyph: root.playing ? "pause" : "play"; round: true; filled: true; iconSize: 28
                implicitWidth: 80
                onClicked: if (root.surface) root.surface.togglePause()
                Accessible.name: "play or pause"
            }
            Rectangle {
                width: 64; height: 64; radius: 32; color: "#66000000"; anchors.verticalCenter: parent.verticalCenter
                TransportButton { anchors.fill: parent; glyph: "next"; iconSize: 24; Accessible.name: "forward 10 seconds"
                                  onClicked: if (root.surface) root.surface.seek(root.position + 10000) }
            }
        }
        // bottom: time and a thin progress bar (tap it to jump)
        Rectangle { visible: root.overlay; anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 70
                    gradient: Gradient { GradientStop { position: 0; color: "transparent" } GradientStop { position: 1; color: "#b3000000" } } }
        ColumnLayout {
            visible: root.overlay
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 14; rightMargin: 14; bottomMargin: 8 }
            spacing: 6
            RowLayout {
                Layout.fillWidth: true
                Label { text: hop.formatTime(Math.max(1, root.position)); color: "white"; font { family: Theme.font; pixelSize: 14 } }
                Item { Layout.fillWidth: true }
                Label { text: root.duration > 0 ? hop.formatTime(root.duration) : (root.info.length || ""); color: "white"; font { family: Theme.font; pixelSize: 14 } }
            }
            Rectangle {
                id: bar
                Layout.fillWidth: true; implicitHeight: 5; radius: 2; color: "#55ffffff"
                Rectangle { height: parent.height; radius: 2; color: Theme.accent
                            width: root.duration > 0 ? parent.width * Math.min(1, root.position / root.duration) : 0 }
                MouseArea {
                    anchors { fill: parent; topMargin: -14; bottomMargin: -14 }
                    onClicked: function (m) { if (root.surface && root.duration > 0) root.surface.seek(root.duration * m.x / bar.width) }
                }
            }
        }
        // landscape: the pretend live chat on the right edge
        LiveChat {
            visible: root.landscape && root.overlay && hop.videoChat && root.chatShown
            anchors { right: parent.right; top: parent.top; bottom: parent.bottom; topMargin: 70; bottomMargin: 70 }
            width: 240
            overlay: true
            look: hop.videoChatLook
            seed: hop.videoPlayerPath
            info: root.info
            position: root.position
            duration: root.duration
        }
    }

    // upright: everything else sits under the picture
    Flickable {
        visible: !root.landscape
        anchors { left: parent.left; right: parent.right; top: screen.bottom; bottom: parent.bottom }
        contentWidth: width
        contentHeight: below.implicitHeight + 24
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ColumnLayout {
            id: below
            x: Theme.pad; y: 10
            width: parent.width - 2 * Theme.pad
            spacing: 8
            TransportButton {   // play / pause, centred just under the picture
                objectName: "phonePlayerPlay"
                Layout.alignment: Qt.AlignHCenter
                glyph: root.playing ? "pause" : "play"; round: true; filled: true; iconSize: 22
                implicitWidth: 54
                onClicked: if (root.surface) root.surface.togglePause()
                Accessible.name: "play or pause"
            }
            RowLayout {   // the title, with rotate and subtitles at the right end
                Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.topMargin: 4
                spacing: 10
                Label { text: root.shownTitle; color: Theme.strong; elide: Text.ElideRight; Layout.fillWidth: true; Layout.minimumWidth: 0
                        font { family: Theme.font; pixelSize: 26; weight: Font.Light } }
                RoundBtn { objectName: "phonePlayerRotate"; glyphKind: "rotate"; onPicture: false; implicitWidth: 40; implicitHeight: 40
                           onClicked: root.rotate(); Accessible.name: "rotate" }
                RoundBtn { objectName: "phonePlayerCC"; label: "CC"; onPicture: false; implicitWidth: 40; implicitHeight: 40; Accessible.name: "subtitles" }
            }
            Label { text: [hop.videoCategory, root.info.length, root.info.resolution, root.info.hdr ? "HDR" : ""].filter(function (s) { return !!s }).join(" · ")
                    color: Theme.muted; font { family: Theme.font; pixelSize: 14 } Layout.fillWidth: true; Layout.minimumWidth: 0; elide: Text.ElideRight }
            Flow {   // play, back 10 s, subtitles, forward 10 s, volume
                Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 100; Layout.topMargin: 4
                spacing: 8
                AbstractButton {
                    objectName: "phonePlayerPlayRow"
                    implicitHeight: 52; implicitWidth: 92
                    onClicked: if (root.surface) root.surface.togglePause()
                    Accessible.name: "play or pause"
                    background: Rectangle { radius: 14; color: Theme.accent }
                    contentItem: Label { text: root.playing ? "❚❚  pause" : "▶  play"; color: Theme.ink
                                         horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font { family: Theme.font; pixelSize: 16 } }
                }
                TransportButton {
                    objectName: "phonePlayerBack"
                    glyph: "prev"; round: true; iconSize: 18; implicitWidth: 44
                    onClicked: if (root.surface) root.surface.seek(root.position - 10000)
                    Accessible.name: "back 10 seconds"
                }
                AbstractButton {
                    implicitHeight: 52; implicitWidth: 84
                    Accessible.name: "subtitles"
                    background: Rectangle { radius: 14; color: "transparent"; border { width: 1; color: Theme.borderSoft } }
                    contentItem: Label { text: "subtitles"; color: Theme.text2; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font { family: Theme.font; pixelSize: 15 } }
                }
                TransportButton {
                    objectName: "phonePlayerForward"
                    glyph: "next"; round: true; iconSize: 18; implicitWidth: 44
                    onClicked: if (root.surface) root.surface.seek(root.position + 10000)
                    Accessible.name: "forward 10 seconds"
                }
                AbstractButton {
                    objectName: "phonePlayerVolume"
                    implicitHeight: 52; implicitWidth: 76
                    onClicked: hop.videoVolume = hop.videoVolume >= 150 ? 50 : (hop.videoVolume >= 100 ? 150 : 100)
                    Accessible.name: "volume"
                    background: Rectangle { radius: 14; color: "transparent"; border { width: 1; color: Theme.borderSoft } }
                    contentItem: Label { text: "volume\n" + hop.videoVolume + "%"; color: Theme.text2; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                         font { family: Theme.font; pixelSize: 15 } }
                }
            }
            PillRow {
                Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.topMargin: 4
                namePrefix: "phoneTab_"
                items: [["next up", "nextup"]].concat(hop.videoChat ? [["live chat", "chat"]] : []).concat(hop.videoComments !== "off" ? [["comments", "comments"]] : [])
                current: root.tab
                onPicked: function (v) { root.tab = v }
            }
            ColumnLayout {   // next up: the other films in this category
                visible: root.tab === "nextup"
                Layout.fillWidth: true; Layout.minimumWidth: 0
                spacing: 0
                Repeater {
                    model: hop.videos
                    delegate: AbstractButton {
                        id: nx
                        required property int index
                        required property string title
                        required property string thumb
                        visible: index < 8
                        Layout.fillWidth: true; implicitHeight: visible ? 76 : 0
                        onClicked: hop.playVideo(nx.index)
                        background: Rectangle { color: "transparent"; Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 1; color: Theme.rowLine } }
                        contentItem: RowLayout {
                            spacing: 14
                            VideoThumb { key: nx.title; source: nx.thumb; Layout.preferredWidth: 110; Layout.preferredHeight: 62 }
                            Label { text: nx.title; color: Theme.text; elide: Text.ElideRight; Layout.fillWidth: true
                                    font { family: Theme.font; pixelSize: 17 } }
                        }
                    }
                }
            }
            LiveChat {
                visible: root.tab === "chat" && hop.videoChat
                Layout.fillWidth: true; Layout.preferredHeight: 320; Layout.minimumWidth: 0
                look: hop.videoChatLook; seed: hop.videoPlayerPath; info: root.info
                position: root.position; duration: root.duration
            }
            VideoComments {
                visible: root.tab === "comments" && hop.videoComments !== "off"
                Layout.fillWidth: true; Layout.minimumWidth: 0
                mode: hop.videoComments; seed: hop.videoPlayerPath; info: root.info
                mine: hop.myVideoComments; duration: root.duration
            }
        }
    }
}
