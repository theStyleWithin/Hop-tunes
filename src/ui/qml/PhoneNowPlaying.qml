// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Phones: now playing as in the mockups: a big cover, the song, a seek bar, the transport with a solid play
// button, and heart / queue / volume underneath.
Item {
    id: root
    signal back()
    readonly property var player: hop.player
    property bool showQueue: false
    property bool showVolume: false
    property int queueRev: 0
    Connections { target: root.player; function onQueueChanged() { root.queueRev++ } }

    ColumnLayout {
        anchors { fill: parent; leftMargin: 28; rightMargin: 28; topMargin: 10; bottomMargin: 12 }
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            AbstractButton {
                objectName: "phoneNpBack"
                implicitWidth: 44; implicitHeight: 44
                onClicked: root.back()
                Accessible.name: "back"
                background: Item {}
                contentItem: Label { text: "⌄"; color: Theme.text2; font.pixelSize: 26; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
            Label { text: "now playing"; color: Theme.muted; font { family: Theme.font; pixelSize: 20 } }
            Item { Layout.fillWidth: true }
        }

        Item {   // the cover, or the up-next list
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.minimumHeight: 120
            AlbumArt {
                visible: !root.showQueue
                anchors.centerIn: parent
                width: Math.min(parent.width, parent.height); height: width
                key: root.player.album; source: root.player.artUrl
            }
            ListView {
                id: queueList
                objectName: "phoneQueue"
                visible: root.showQueue
                anchors.fill: parent
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: { root.queueRev; return root.player.queueLength }
                delegate: ItemDelegate {
                    required property int index
                    width: queueList.width; height: 48
                    background: Rectangle { color: index === root.player.currentIndex ? Theme.hover : "transparent" }
                    onClicked: root.player.playQueueIndex(index)
                    contentItem: Label {
                        text: { root.queueRev; return hop.player.queueTitle(index) }
                        elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
                        color: index === root.player.currentIndex ? hop.accent : Theme.text
                        font { family: Theme.font; pixelSize: 17 }
                    }
                }
            }
        }

        Label {
            text: root.player.hasTrack ? root.player.title : "nothing playing"
            color: Theme.strong; elide: Text.ElideRight; Layout.fillWidth: true
            font { family: Theme.font; pixelSize: 30; weight: Font.Light }
        }
        Label {
            visible: root.player.hasTrack
            text: root.player.artist + (root.player.album ? " · " + root.player.album : "")
            color: Theme.muted; elide: Text.ElideRight; Layout.fillWidth: true
            font { family: Theme.font; pixelSize: 19 }
        }

        Slider {
            id: seek
            objectName: "phoneSeek"
            Layout.fillWidth: true; Layout.topMargin: 10
            from: 0; to: Math.max(1, root.player.duration)
            enabled: root.player.hasTrack
            onMoved: root.player.seek(value)
            Binding { target: seek; property: "value"; value: root.player.position; when: !seek.pressed }
            background: Rectangle {
                x: seek.leftPadding; y: seek.topPadding + seek.availableHeight / 2 - height / 2
                width: seek.availableWidth; height: 5; radius: 2; color: Theme.track
                Rectangle { width: seek.visualPosition * parent.width; height: parent.height; radius: 2; color: hop.accent }
            }
            handle: Rectangle {
                x: seek.leftPadding + seek.visualPosition * (seek.availableWidth - width); y: seek.topPadding + seek.availableHeight / 2 - height / 2
                width: 16; height: 16; radius: 8; color: Theme.strong
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label { text: root.player.hasTrack ? hop.formatTime(Math.max(1, root.player.position)) : "–"; color: Theme.muted; font { family: Theme.font; pixelSize: 14 } }
            Item { Layout.fillWidth: true }
            Label { text: root.player.hasTrack ? hop.formatTime(root.player.duration) : ""; color: Theme.muted; font { family: Theme.font; pixelSize: 14 } }
        }

        RowLayout {
            Layout.fillWidth: true; Layout.topMargin: 4
            spacing: 0
            Item { Layout.fillWidth: true }
            TransportButton { glyph: "shuffle"; iconSize: 20; active: root.player.shuffle; onClicked: root.player.shuffle = !root.player.shuffle
                              Accessible.name: root.player.shuffle ? "shuffle on" : "shuffle off" }
            Item { Layout.fillWidth: true }
            TransportButton { glyph: "prev"; iconSize: 26; enabled: root.player.hasTrack; onClicked: root.player.previous(); Accessible.name: "previous" }
            Item { Layout.fillWidth: true }
            TransportButton {
                objectName: "phonePlay"
                glyph: root.player.playing ? "pause" : "play"; round: true; filled: true; iconSize: 28
                implicitWidth: 76
                onClicked: root.player.hasTrack ? root.player.toggle() : hop.playSongs(0)
                Accessible.name: root.player.playing ? "pause" : "play"
            }
            Item { Layout.fillWidth: true }
            TransportButton { glyph: "next"; iconSize: 26; enabled: root.player.hasTrack; onClicked: root.player.next(); Accessible.name: "next" }
            Item { Layout.fillWidth: true }
            TransportButton { glyph: root.player.repeatMode === 2 ? "repeat1" : "repeat"; iconSize: 20; active: root.player.repeatMode !== 0
                              onClicked: root.player.cycleRepeat(); Accessible.name: "repeat" }
            Item { Layout.fillWidth: true }
        }

        RowLayout {
            Layout.fillWidth: true; Layout.topMargin: 2
            spacing: 0
            Item { Layout.fillWidth: true }
            HeartButton { size: 28; visible: root.player.hasTrack; on: hop.currentHearted; onClicked: hop.toggleHeartCurrent() }
            Item { Layout.fillWidth: true }
            TransportButton { objectName: "phoneQueueButton"; glyph: "queue"; iconSize: 22; active: root.showQueue
                              onClicked: root.showQueue = !root.showQueue; Accessible.name: "up next" }
            Item { Layout.fillWidth: true }
            TransportButton { objectName: "phoneVolumeButton"; glyph: root.player.volume === 0 ? "mute" : "volume"; iconSize: 22; active: root.showVolume
                              onClicked: root.showVolume = !root.showVolume; Accessible.name: "volume" }
            Item { Layout.fillWidth: true }
        }
        Slider {
            visible: root.showVolume
            objectName: "phoneVolume"
            Layout.fillWidth: true
            from: 0; to: 1
            onMoved: root.player.volume = value
            Binding on value { value: root.player.volume }
        }
    }
}
