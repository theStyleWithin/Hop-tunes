// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Now playing (design/screens/5): drifting art mosaic, big type, up next.
Rectangle {
    id: root
    color: Theme.bgDeep
    clip: true
    readonly property var player: hop.player
    signal back()

    // Controls fade out after a few idle seconds while music plays (spec §4.3); any mouse
    // movement or key brings them back.
    property bool idle: false
    function wake() { idle = false; idleTimer.restart() }
    onVisibleChanged: wake()
    Timer { id: idleTimer; interval: 3500; running: root.visible && root.player.playing; onTriggered: root.idle = true }
    Connections { target: root.player; function onPlayingChanged() { root.wake() } }
    MouseArea { anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton; onPositionChanged: root.wake(); z: 100 }
    Keys.onPressed: function (e) { root.wake(); e.accepted = false }
    readonly property real chrome: idle ? 0.0 : 1.0
    // Bumps whenever the queue changes (reorder, remove, add) so "up next" redraws.
    property int queueRev: 0
    Connections { target: root.player; function onQueueChanged() { root.queueRev++ } }

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
            GradientStop { position: 0.0; color: Theme.fade(1.0) }
            GradientStop { position: 0.38; color: Theme.fade(0.92) }
            GradientStop { position: 0.70; color: Theme.fade(0.55) }
            GradientStop { position: 1.0; color: Theme.fade(0.75) }
        }
    }

    RowLayout {
        id: header
        opacity: root.chrome
        Behavior on opacity { NumberAnimation { duration: Theme.reduceMotion ? 0 : 450 } }
        anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: Theme.pad; rightMargin: Theme.pad; topMargin: 24 }
        AbstractButton {
            implicitWidth: 44; implicitHeight: 44
            onClicked: root.back()
            Accessible.name: "back"
            background: Rectangle { radius: 22; color: "transparent"; border { width: 2; color: Theme.border } }
            contentItem: Label { text: "‹"; color: Theme.text; font.pixelSize: 26; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; bottomPadding: 4 }
        }
        Item { Layout.fillWidth: true }
        Label { text: "NOW PLAYING"; color: Theme.muted; font { family: Theme.font; pixelSize: 16; letterSpacing: 3 } }
    }

    RowLayout {
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom; leftMargin: Theme.pad; rightMargin: Theme.pad; topMargin: 16; bottomMargin: 36 }
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
                opacity: root.chrome
                Behavior on opacity { NumberAnimation { duration: Theme.reduceMotion ? 0 : 450 } }
                TransportButton { glyph: "prev"; iconSize: 24; implicitWidth: 52; onClicked: root.player.previous(); Accessible.name: "previous" }
                TransportButton { glyph: root.player.playing ? "pause" : "play"; round: true; iconSize: 24; implicitWidth: 76
                                  onClicked: root.player.hasTrack ? root.player.toggle() : hop.playSongs(0)
                                  Accessible.name: root.player.playing ? "pause" : "play" }
                TransportButton { glyph: "next"; iconSize: 24; implicitWidth: 52; onClicked: root.player.next(); Accessible.name: "next" }
                HeartButton { size: 26; visible: root.player.hasTrack; on: hop.currentHearted; onClicked: hop.toggleHeartCurrent() }
            }
        }

        Rectangle {
            id: upNextPanel
            visible: root.player.hasTrack
            opacity: root.chrome
            Behavior on opacity { NumberAnimation { duration: Theme.reduceMotion ? 0 : 450 } }
            Layout.preferredWidth: 360
            Layout.alignment: Qt.AlignBottom
            Layout.preferredHeight: upNext.implicitHeight + 40
            color: Theme.fade(0.82)
            readonly property int rowH: 36
            ColumnLayout {
                id: upNext
                anchors { fill: parent; margins: 20 }
                spacing: 0
                Label { text: "up next"; color: hop.accent; font { family: Theme.font; pixelSize: 22 } bottomPadding: 8 }
                Label {
                    visible: root.player.currentIndex + 1 >= root.player.queueLength
                    text: root.player.repeatMode === 1 ? "the queue starts again" : "nothing after this song"
                    color: Theme.muted; font { family: Theme.font; pixelSize: 15 }
                }
                Repeater {
                    // the next 7 songs; drag the handle to reorder, click to jump, × to remove
                    model: {
                        root.queueRev
                        var q = [], n = root.player.queueLength, i = root.player.currentIndex
                        for (var k = i + 1; k < Math.min(n, i + 8); ++k) q.push(k)
                        return q
                    }
                    delegate: Item {
                        id: qrow
                        required property int modelData
                        required property int index
                        objectName: "upNext_" + index
                        Layout.fillWidth: true
                        Layout.preferredHeight: upNextPanel.rowH
                        z: drag.active ? 10 : 0
                        property real dy: 0
                        transform: Translate { y: qrow.dy }
                        Rectangle { anchors.fill: parent; color: drag.active ? Theme.hover : (rowHover.hovered ? Theme.fade(0.6) : "transparent") }
                        HoverHandler { id: rowHover }
                        RowLayout {
                            anchors.fill: parent
                            spacing: 10
                            Label {
                                text: "≡"; color: Theme.dim; font.pixelSize: 18
                                Layout.preferredWidth: 18
                                MouseArea {
                                    id: drag
                                    property bool active: pressed
                                    anchors.fill: parent; anchors.margins: -8
                                    cursorShape: Qt.SizeVerCursor
                                    property real startY: 0
                                    onPressed: function (m) { startY = mapToItem(upNext, m.x, m.y).y }
                                    onPositionChanged: function (m) { qrow.dy = mapToItem(upNext, m.x, m.y).y - startY }
                                    onReleased: {
                                        var steps = Math.round(qrow.dy / upNextPanel.rowH)
                                        var to = Math.max(root.player.currentIndex + 1, Math.min(root.player.queueLength - 1, qrow.modelData + steps))
                                        qrow.dy = 0
                                        if (to !== qrow.modelData) root.player.moveQueueItem(qrow.modelData, to)
                                    }
                                    Accessible.name: "drag to reorder"
                                }
                            }
                            Label {
                                objectName: "upNextTitle_" + qrow.index
                                text: { root.queueRev; return hop.player.queueTitle(qrow.modelData) }
                                color: Theme.text; font { family: Theme.font; pixelSize: 17 }
                                elide: Text.ElideRight; Layout.fillWidth: true
                                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.player.playQueueIndex(qrow.modelData) }
                            }
                            AbstractButton {
                                visible: rowHover.hovered
                                implicitWidth: 26; implicitHeight: 26
                                onClicked: root.player.removeFromQueue(qrow.modelData)
                                Accessible.name: "remove from queue"
                                contentItem: Label { text: "×"; color: Theme.muted; font.pixelSize: 18; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                background: Item { }
                            }
                        }
                    }
                }
                Label {
                    visible: root.player.queueLength - root.player.currentIndex - 1 > 7
                    text: "+ " + (root.player.queueLength - root.player.currentIndex - 8) + " more"
                    color: Theme.dim; font { family: Theme.font; pixelSize: 13 } topPadding: 4
                }
                Label {
                    text: hop.djSeed !== "" ? "smart dj · " + hop.djSeed
                        : (root.player.crossfadeMs > 0 ? "crossfade " + Math.round(root.player.crossfadeMs / 1000) + " s · albums stay gapless" : "gapless")
                          + (root.player.shuffle ? " · shuffle" : "") + (root.player.repeatMode === 1 ? " · repeat" : (root.player.repeatMode === 2 ? " · repeat this song" : ""))
                    color: Theme.tabIdle; font { family: Theme.font; pixelSize: 13 } topPadding: 8
                }
            }
        }
    }
}
