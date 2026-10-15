// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// Now playing (design/screens/5): drifting art mosaic, big type, up next.
Rectangle {
    id: root
    color: Theme.bgDeep
    clip: true
    readonly property var player: hop.player
    signal back()

    // Background mosaic of placeholder tiles from the queue's albums.
    Grid {
        id: mosaic
        columns: 8
        spacing: 6
        opacity: 0.38
        rotation: -4
        width: 1100
        x: root.width - width + 80 + drift
        y: -40
        property real drift: 0
        Repeater {
            model: 40
            AlbumArt {
                required property int index
                width: (mosaic.width - 7 * 6) / 8; height: width
                key: "tile" + ((index * 7 + 3) % 11) + root.player.album
                source: hop.mosaicArt.length > 0 ? hop.mosaicArt[(index * 7 + 3) % hop.mosaicArt.length] : ""
            }
        }
        SequentialAnimation on drift {
            running: root.visible && root.player.playing
            loops: Animation.Infinite
            NumberAnimation { to: -120; duration: 60000; easing.type: Easing.InOutSine }
            NumberAnimation { to: 0; duration: 60000; easing.type: Easing.InOutSine }
        }
    }
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#ff0b0b0d" }
            GradientStop { position: 0.38; color: "#eb0b0b0d" }
            GradientStop { position: 0.70; color: "#8c0b0b0d" }
            GradientStop { position: 1.0; color: "#bf0b0b0d" }
        }
    }

    RowLayout {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: 48; rightMargin: 48; topMargin: 24 }
        AbstractButton {
            implicitWidth: 44; implicitHeight: 44
            onClicked: root.back()
            Accessible.name: "back"
            background: Rectangle { radius: 22; color: "transparent"; border { width: 2; color: "#5b5b63" } }
            contentItem: Label { text: "‹"; color: Theme.text; font.pixelSize: 26; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; bottomPadding: 4 }
        }
        Item { Layout.fillWidth: true }
        Label { text: "NOW PLAYING"; color: Theme.muted; font { family: Theme.font; pixelSize: 16; letterSpacing: 3 } }
    }

    RowLayout {
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom; leftMargin: 48; rightMargin: 48; topMargin: 16; bottomMargin: 36 }
        spacing: 48

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignBottom
            spacing: 20

            AlbumArt { key: root.player.album; source: root.player.artUrl; Layout.preferredWidth: 200; Layout.preferredHeight: 200; visible: root.player.hasTrack }
            Label { text: root.player.hasTrack ? root.player.artist : ""; color: Theme.text2; font { family: Theme.font; pixelSize: 28; weight: Font.Light } }
            Label {
                text: root.player.hasTrack ? root.player.title : "nothing playing"
                color: Theme.text; font { family: Theme.font; pixelSize: 76; weight: Font.Light; letterSpacing: -2 }
                elide: Text.ElideRight; Layout.fillWidth: true
            }
            Label {
                visible: root.player.hasTrack
                text: root.player.album + (root.player.year > 0 ? " · " + root.player.year : "")
                      + " · track " + (root.player.currentIndex + 1) + " of " + root.player.queueLength
                color: Theme.muted; font { family: Theme.font; pixelSize: 22 }
            }
            RowLayout {
                spacing: 18
                Layout.topMargin: 8
                TransportButton { glyph: "prev"; iconSize: 24; implicitWidth: 52; onClicked: root.player.previous(); Accessible.name: "previous" }
                TransportButton { glyph: root.player.playing ? "pause" : "play"; round: true; iconSize: 24; implicitWidth: 76
                                  onClicked: root.player.hasTrack ? root.player.toggle() : hop.playSongs(0)
                                  Accessible.name: root.player.playing ? "pause" : "play" }
                TransportButton { glyph: "next"; iconSize: 24; implicitWidth: 52; onClicked: root.player.next(); Accessible.name: "next" }
            }
        }

        Rectangle {
            visible: root.player.hasTrack
            Layout.preferredWidth: 340
            Layout.alignment: Qt.AlignBottom
            Layout.preferredHeight: upNext.implicitHeight + 40
            color: "#d1141418"
            ColumnLayout {
                id: upNext
                anchors { fill: parent; margins: 20 }
                spacing: 4
                Label { text: "up next"; color: Theme.accent; font { family: Theme.font; pixelSize: 22 } bottomPadding: 8 }
                Repeater {
                    model: {
                        var q = [], n = root.player.queueLength, i = root.player.currentIndex
                        for (var k = i + 1; k < Math.min(n, i + 6); ++k) q.push(k)
                        return q
                    }
                    delegate: Label {
                        required property int modelData
                        text: (modelData + 1) + "   " + hop.player.queueTitle(modelData)
                        color: Theme.text; font { family: Theme.font; pixelSize: 17 }
                        elide: Text.ElideRight; Layout.fillWidth: true
                        topPadding: 6; bottomPadding: 6
                    }
                }
                Label { text: "gapless · crossfade off"; color: "#8a8a93"; font { family: Theme.font; pixelSize: 13 } topPadding: 8 }
            }
        }
    }
}
