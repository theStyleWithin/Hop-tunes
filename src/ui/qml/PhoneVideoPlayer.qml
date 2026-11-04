// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Phones: the video player as in the mockups. Held upright it is the picture on top with the title, the
// buttons and the next up / live chat / comments tabs below; turned (or after the rotate button) it fills the
// screen with a chat strip on the right. The rotate button switches between the two for this video.
Rectangle {
    id: root
    color: Theme.bg
    readonly property var info: hop.videoPlayerInfo
    readonly property var meta: info.meta || ({})
    readonly property string shownTitle: (meta.title || "") !== "" ? meta.title : (info.fileName || hop.videoPlayerTitle)
    property bool landscape: width > height
    property string tab: "nextup"           // nextup | chat | comments
    property bool chatShown: true
    property real position: 0
    property real duration: 0
    readonly property bool engine: hop.videoEngine !== "none"

    function close() { hop.closeVideoPlayer(root.position, root.duration) }
    function rotate() { hop.requestOrientation(root.landscape ? "portrait" : "landscape") }
    Component.onCompleted: hop.requestOrientation(hop.videoOrientation)
    Component.onDestruction: hop.requestOrientation("auto")

    component RoundBtn: AbstractButton {
        id: rb
        property string label: ""
        property bool accentFill: false
        implicitWidth: 46; implicitHeight: 46
        background: Rectangle { radius: width / 2; color: rb.accentFill ? Theme.accent : "#66000000" }
        contentItem: Label { text: rb.label; color: rb.accentFill ? Theme.ink : "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                             font { family: Theme.font; pixelSize: rb.label.length > 1 ? 15 : 22; weight: Font.Medium } }
    }

    // the picture, with its controls laid over it
    Item {
        id: screen
        x: 0; y: 0
        width: root.width
        height: root.landscape ? root.height : Math.round(root.width * 9 / 16) + 6
        Rectangle { anchors.fill: parent; color: "black" }
        Label {
            visible: !root.engine
            anchors.centerIn: parent; width: parent.width - 80
            horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
            text: "video playback is not on Android yet"
            color: "#c8c8ce"; font { family: Theme.font; pixelSize: 16 }
        }
        // top: close, title (landscape), rotate, subtitles
        Rectangle { anchors { left: parent.left; right: parent.right; top: parent.top } height: 70
                    gradient: Gradient { GradientStop { position: 0; color: "#99000000" } GradientStop { position: 1; color: "transparent" } } }
        RowLayout {
            anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: 14; rightMargin: 14; topMargin: 10 }
            spacing: 12
            AbstractButton {
                objectName: "phonePlayerClose"
                implicitWidth: 44; implicitHeight: 44
                onClicked: root.close()
                Accessible.name: "close"
                background: Item {}
                contentItem: Label { text: "⌄"; color: "white"; font.pixelSize: 26; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
            Label { visible: root.landscape; text: root.shownTitle; color: "white"; elide: Text.ElideRight; Layout.fillWidth: true
                    font { family: Theme.font; pixelSize: 22 } }
            Item { visible: !root.landscape; Layout.fillWidth: true }
            RoundBtn { objectName: "phonePlayerRotate"; label: "⟳"; accentFill: true; onClicked: root.rotate(); Accessible.name: "rotate" }
            RoundBtn { objectName: "phonePlayerCC"; label: "CC"; Accessible.name: "subtitles" }
            Rectangle {
                visible: root.landscape
                implicitWidth: 46; implicitHeight: 46; radius: 23; color: "#66000000"
                TransportButton { anchors.fill: parent; glyph: hop.videoVolume === 0 ? "mute" : "volume"; iconSize: 20; Accessible.name: "volume"
                                  onClicked: hop.videoVolume = hop.videoVolume >= 150 ? 50 : (hop.videoVolume >= 100 ? 150 : 100) }
            }
            RoundBtn { visible: root.landscape && hop.videoChat; label: "chat"; accentFill: root.chatShown; Accessible.name: "live chat"
                       onClicked: root.chatShown = !root.chatShown }
        }
        // centre: back 10 s, play / pause, forward 10 s
        Row {
            anchors.centerIn: parent
            spacing: root.landscape ? 70 : 34
            Rectangle {
                width: 64; height: 64; radius: 32; color: "#66000000"; anchors.verticalCenter: parent.verticalCenter
                TransportButton { anchors.fill: parent; glyph: "prev"; iconSize: 24; Accessible.name: "back 10 seconds"
                                  onClicked: root.position = Math.max(0, root.position - 10000) }
            }
            TransportButton {
                objectName: "phonePlayerPlay"
                glyph: "pause"; round: true; filled: true; iconSize: 26
                implicitWidth: root.landscape ? 80 : 64
                Accessible.name: "play or pause"
            }
            Rectangle {
                width: 64; height: 64; radius: 32; color: "#66000000"; anchors.verticalCenter: parent.verticalCenter
                TransportButton { anchors.fill: parent; glyph: "next"; iconSize: 24; Accessible.name: "forward 10 seconds"
                                  onClicked: root.position = root.position + 10000 }
            }
        }
        // bottom: time and a thin progress bar
        Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 70
                    gradient: Gradient { GradientStop { position: 0; color: "transparent" } GradientStop { position: 1; color: "#b3000000" } } }
        ColumnLayout {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 14; rightMargin: 14; bottomMargin: 8 }
            spacing: 6
            RowLayout {
                Layout.fillWidth: true
                Label { text: hop.formatTime(Math.max(1, root.position)); color: "white"; font { family: Theme.font; pixelSize: 14 } }
                Item { Layout.fillWidth: true }
                Label { text: root.info.length || ""; color: "white"; font { family: Theme.font; pixelSize: 14 } }
            }
            Rectangle {
                Layout.fillWidth: true; implicitHeight: 5; radius: 2; color: "#55ffffff"
                Rectangle { height: parent.height; radius: 2; color: Theme.accent
                            width: root.duration > 0 ? parent.width * Math.min(1, root.position / root.duration) : 0 }
            }
        }
        // landscape: the pretend live chat on the right edge
        LiveChat {
            visible: root.landscape && hop.videoChat && root.chatShown
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

    // upright: the details under the picture
    Flickable {
        visible: !root.landscape
        anchors { left: parent.left; right: parent.right; top: screen.bottom; bottom: parent.bottom }
        contentWidth: width
        contentHeight: below.implicitHeight + 24
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ColumnLayout {
            id: below
            x: Theme.pad; y: 14
            width: parent.width - 2 * Theme.pad
            spacing: 8
            Label { text: root.shownTitle; color: Theme.strong; elide: Text.ElideRight; Layout.fillWidth: true; Layout.minimumWidth: 0
                    font { family: Theme.font; pixelSize: 26; weight: Font.Light } }
            Label { text: [hop.videoCategory, root.info.length, root.info.resolution, root.info.hdr ? "HDR" : ""].filter(function (s) { return !!s }).join(" · ")
                    color: Theme.muted; font { family: Theme.font; pixelSize: 14 } Layout.fillWidth: true; Layout.minimumWidth: 0; elide: Text.ElideRight }
            Flow {
                Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 100; Layout.topMargin: 4
                spacing: 10
                AbstractButton {
                    implicitHeight: 52; implicitWidth: 96
                    Accessible.name: "resume"
                    background: Rectangle { radius: 14; color: Theme.accent }
                    contentItem: Label { text: root.info.resumeText ? "▶ resume\n" + root.info.resumeText : "▶ play"; color: Theme.ink
                                         horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font { family: Theme.font; pixelSize: 15 } }
                }
                AbstractButton {
                    implicitHeight: 52; implicitWidth: 80
                    Accessible.name: "subtitles"
                    background: Rectangle { radius: 14; color: "transparent"; border { width: 1; color: Theme.borderSoft } }
                    contentItem: Label { text: "subtitles"; color: Theme.text2; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font { family: Theme.font; pixelSize: 15 } }
                }
                AbstractButton {
                    objectName: "phonePlayerVolume"
                    implicitHeight: 52; implicitWidth: 90
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
