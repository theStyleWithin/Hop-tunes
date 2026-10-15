// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Persistent bottom bar (spec §4.6).
Rectangle {
    id: root
    signal openNowPlaying()
    readonly property var player: hop.player
    color: Theme.bar
    implicitHeight: 78

    Rectangle { anchors { left: parent.left; right: parent.right; top: parent.top } height: 1; color: Theme.line }

    RowLayout {
        anchors { fill: parent; leftMargin: 48; rightMargin: 48 }
        spacing: 20

        MouseArea {
            Layout.preferredWidth: 220
            Layout.fillHeight: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.openNowPlaying()
            Accessible.role: Accessible.Button
            Accessible.name: "open now playing"
            RowLayout {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                spacing: 14
                AlbumArt { key: player.album; source: player.artUrl; Layout.preferredWidth: 48; Layout.preferredHeight: 48; visible: player.hasTrack }
                ColumnLayout {
                    spacing: 0
                    Layout.fillWidth: true
                    Label { text: player.hasTrack ? player.title : "nothing playing"; color: Theme.text; font { family: Theme.font; pixelSize: 17 } elide: Text.ElideRight; Layout.fillWidth: true }
                    Label { text: player.artist; color: Theme.muted; font { family: Theme.font; pixelSize: 14 } elide: Text.ElideRight; Layout.fillWidth: true; visible: player.hasTrack }
                }
            }
        }

        HeartButton {
            objectName: "miniHeart"
            visible: player.hasTrack
            on: hop.currentHearted
            onClicked: hop.toggleHeartCurrent()
        }

        RowLayout {
            spacing: 8
            TransportButton {
                objectName: "shuffleButton"
                glyph: "shuffle"; iconSize: 15; active: player.shuffle
                onClicked: player.shuffle = !player.shuffle
                Accessible.name: player.shuffle ? "shuffle on" : "shuffle off"
                ToolTip.visible: hovered; ToolTip.delay: 600; ToolTip.text: player.shuffle ? "shuffle: on" : "shuffle: off"
            }
            TransportButton { glyph: "prev"; onClicked: player.previous(); enabled: player.hasTrack; Accessible.name: "previous" }
            TransportButton { glyph: player.playing ? "pause" : "play"; round: true; iconSize: 16; enabled: player.hasTrack || hop.songs.count > 0
                              onClicked: player.hasTrack ? player.toggle() : hop.playSongs(0); Accessible.name: player.playing ? "pause" : "play" }
            TransportButton { glyph: "next"; onClicked: player.next(); enabled: player.hasTrack; Accessible.name: "next" }
            TransportButton {
                objectName: "repeatButton"
                glyph: player.repeatMode === 2 ? "repeat1" : "repeat"; iconSize: 15; active: player.repeatMode !== 0
                onClicked: player.cycleRepeat()
                Accessible.name: ["repeat off", "repeat all", "repeat one"][player.repeatMode]
                ToolTip.visible: hovered; ToolTip.delay: 600; ToolTip.text: ["repeat: off", "repeat: all", "repeat: this song"][player.repeatMode]
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Label { text: player.hasTrack ? hop.formatTime(Math.max(1, player.position)) : "–"; color: Theme.muted; font { family: Theme.font; pixelSize: 13 } }
            Slider {
                id: seek
                Layout.fillWidth: true
                from: 0; to: Math.max(1, player.duration)
                enabled: player.hasTrack
                onMoved: player.seek(value)
                Binding { target: seek; property: "value"; value: player.position; when: !seek.pressed }
                Accessible.name: "seek"
                background: Rectangle {
                    x: seek.leftPadding; y: seek.topPadding + seek.availableHeight / 2 - height / 2
                    width: seek.availableWidth; height: 4; color: Theme.track
                    Rectangle { width: seek.visualPosition * parent.width; height: parent.height; color: hop.accent }
                }
                handle: Rectangle {
                    x: seek.leftPadding + seek.visualPosition * (seek.availableWidth - width)
                    y: seek.topPadding + seek.availableHeight / 2 - height / 2
                    width: 12; height: 12; radius: 6; color: Theme.text
                    visible: seek.hovered || seek.pressed
                }
            }
            Label { text: player.hasTrack ? hop.formatTime(player.duration) : ""; color: Theme.muted; font { family: Theme.font; pixelSize: 13 } }
        }

        // volume (spec §4.6)
        RowLayout {
            spacing: 2
            property real lastVolume: 1.0
            id: vol
            TransportButton {
                glyph: player.volume === 0 ? "mute" : "volume"; iconSize: 15
                onClicked: { if (player.volume > 0) { vol.lastVolume = player.volume; player.volume = 0 } else player.volume = vol.lastVolume || 1.0 }
                Accessible.name: player.volume === 0 ? "unmute" : "mute"
            }
            Slider {
                id: volSlider
                objectName: "volumeSlider"
                implicitWidth: 96
                from: 0; to: 1
                onMoved: player.volume = value
                Binding { target: volSlider; property: "value"; value: player.volume; when: !volSlider.pressed }
                Accessible.name: "volume"
                background: Rectangle {
                    x: volSlider.leftPadding; y: volSlider.topPadding + volSlider.availableHeight / 2 - height / 2
                    width: volSlider.availableWidth; height: 3; color: Theme.track
                    Rectangle { width: volSlider.visualPosition * parent.width; height: parent.height; color: Theme.muted }
                }
                handle: Rectangle {
                    x: volSlider.leftPadding + volSlider.visualPosition * (volSlider.availableWidth - width)
                    y: volSlider.topPadding + volSlider.availableHeight / 2 - height / 2
                    width: 10; height: 10; radius: 5; color: Theme.text
                    visible: volSlider.hovered || volSlider.pressed
                }
            }
        }

        Label {
            text: hop.djSeed !== "" ? "smart dj" : (player.fading ? "crossfading" : (player.crossfadeMs > 0 ? "crossfade " + Math.round(player.crossfadeMs / 1000) + " s" : "gapless"))
            color: hop.djSeed !== "" || player.fading ? hop.accent : Theme.muted
            font { family: Theme.font; pixelSize: 13 }
        }
    }
}
