// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

// Phones: now playing as in the mockups: a big cover, the song, a seek bar, the transport with a solid play
// button, and heart / queue / volume underneath. Turned sideways (landscape) the cover moves to the left and
// everything else to the right, so nothing is squeezed.
Item {
    id: root
    signal back()
    readonly property var player: hop.player
    property bool showQueue: false
    property bool showVolume: false
    // the visualizer takes the cover's place (portrait and landscape): the same one as on the desktop, with its own choices
    readonly property bool showViz: hop.audio.available && hop.gameSettings.phoneViz && !showQueue
    readonly property bool wantsAudio: root.visible && showViz
    onWantsAudioChanged: hop.audio.want("phoneviz", wantsAudio)
    readonly property var gs: hop.gameSettings
    property int queueRev: 0
    readonly property bool land: width > height * 1.1   // the phone is turned sideways
    Connections { target: root.player; function onQueueChanged() { root.queueRev++ } }

    ColumnLayout {
        anchors { fill: parent; leftMargin: root.land ? 32 : 40; rightMargin: root.land ? 32 : 40
                  topMargin: root.land ? 4 : 10; bottomMargin: root.land ? 8 : 20 }
        spacing: root.land ? 2 : 8

        RowLayout {
            Layout.fillWidth: true; Layout.minimumWidth: 0
            spacing: 6
            AbstractButton {
                objectName: "phoneNpBack"
                implicitWidth: 44; implicitHeight: root.land ? 36 : 44
                onClicked: root.back()
                Accessible.name: "back"
                background: Item {}
                contentItem: Item { Icon { anchors.centerIn: parent; kind: "down"; ink: Theme.text2; width: 26; height: 26 } }
            }
            Label { visible: root.land || root.width >= 420; text: "now playing"; color: Theme.muted; font { family: Theme.font; pixelSize: root.land ? 17 : 20 } }
            Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
            AbstractButton {   // the visualizer in place of the cover
                objectName: "phoneVizToggle"
                visible: hop.audio.available
                implicitHeight: root.land ? 32 : 36; implicitWidth: vizLabel.implicitWidth + 22
                onClicked: hop.setGameSetting("phoneViz", !hop.gameSettings.phoneViz)
                Accessible.name: "visualizer"
                background: Rectangle { radius: height / 2; color: root.showViz ? Theme.accent : "transparent"; border { width: 1; color: Theme.accent } }
                contentItem: Label { id: vizLabel; text: root.showViz ? "cover" : "visualizer"; color: root.showViz ? Theme.ink : Theme.accent
                                     horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font { family: Theme.font; pixelSize: 15 } }
            }
            AbstractButton {   // the game: a music note flying through your album covers
                objectName: "phoneGames"
                visible: hop.audio.available
                implicitHeight: root.land ? 32 : 36; implicitWidth: gamesLabel.implicitWidth + 22
                onClicked: if (Window.window && Window.window.openMusyNote) Window.window.openMusyNote()
                Accessible.name: "games"
                background: Rectangle { radius: height / 2; color: "transparent"; border { width: 1; color: Theme.accent } }
                contentItem: Label { id: gamesLabel; text: "\u266a games"; color: Theme.accent; horizontalAlignment: Text.AlignHCenter
                                     verticalAlignment: Text.AlignVCenter; font { family: Theme.font; pixelSize: 15 } }
            }
        }

        GridLayout {   // portrait: the cover above the rest. landscape: the cover left, the rest right
            Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumWidth: 0; Layout.minimumHeight: 0
            columns: root.land ? 2 : 1
            columnSpacing: 28; rowSpacing: root.land ? 0 : 8

            Item {   // the cover, or the up-next list
                id: coverBox
                Layout.fillWidth: !root.land
                Layout.minimumWidth: 0
                Layout.fillHeight: true                    // the cover gives way when the phone is short, the controls never do
                Layout.preferredWidth: root.land ? Math.min(height, root.width * 0.42) : -1
                Layout.preferredHeight: root.land ? -1 : Math.min(width, 340) + 12
                Layout.minimumHeight: root.land ? 80 : 70
                Item {   // the visualizer, as big as the cover's slot, with its look one tap away
                    id: vizBox
                    objectName: "phoneViz"
                    visible: root.showViz
                    anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: root.land ? 0 : 6 }
                    width: root.land ? parent.width : parent.width; height: parent.height - (root.land ? 0 : 6)
                    Rectangle { anchors.fill: parent; radius: 10; color: Qt.rgba(1, 1, 1, 0.04) }
                    Visualizer {
                        objectName: "phoneVisualizer"
                        anchors { fill: parent; margins: 8; bottomMargin: 40 }
                        style: root.gs.vizStyle; colorMode: root.gs.vizColor; intensity: root.gs.vizIntensity
                    }
                    Flow {   // the look: shape and colour, one tap each
                        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 8 }
                        spacing: 4
                        Repeater {
                            model: ["bars", "mirror", "rings", "accent", "rainbow", "white"]
                            delegate: AbstractButton {
                                id: vchip
                                required property string modelData
                                readonly property bool isShape: ["bars", "mirror", "rings"].indexOf(modelData) >= 0
                                readonly property bool on: isShape ? root.gs.vizStyle === modelData : root.gs.vizColor === modelData
                                objectName: "phoneViz_" + modelData
                                implicitHeight: 28; implicitWidth: vlab.implicitWidth + 16
                                onClicked: hop.setGameSetting(isShape ? "vizStyle" : "vizColor", modelData)
                                background: Rectangle { radius: 14; color: vchip.on ? Theme.accent : "transparent"; border { width: 1; color: vchip.on ? Theme.accent : Theme.border } }
                                contentItem: Label { id: vlab; text: vchip.modelData; color: vchip.on ? Theme.ink : Theme.muted
                                                     horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font { family: Theme.font; pixelSize: 12 } }
                            }
                        }
                    }
                }
                AlbumArt {
                    objectName: "phoneCover"
                    visible: !root.showQueue && !root.showViz
                    anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: root.land ? 0 : 6 }
                    width: Math.min(parent.width, parent.height - (root.land ? 0 : 6), 340); height: width
                    key: root.player.album; source: root.player.artUrl; radius: 10; cornerColor: Theme.bg
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
                        width: queueList.width; height: root.land ? 40 : 48
                        background: Rectangle { color: index === root.player.currentIndex ? Theme.hover : "transparent" }
                        onClicked: root.player.playQueueIndex(index)
                        contentItem: Label {
                            text: { root.queueRev; return hop.player.queueTitle(index) }
                            elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
                            color: index === root.player.currentIndex ? Theme.accent : Theme.text
                            font { family: Theme.font; pixelSize: 17 }
                        }
                    }
                }
            }

            ColumnLayout {   // the song, the seek bar and the buttons
                Layout.fillWidth: true; Layout.minimumWidth: 0
                Layout.fillHeight: root.land
                Layout.alignment: Qt.AlignVCenter
                spacing: root.land ? 2 : 8

                Label {
                    text: root.player.hasTrack ? root.player.title : "nothing playing"
                    Layout.topMargin: root.land ? 0 : 14
                    color: Theme.strong; elide: Text.ElideRight; Layout.fillWidth: true
                    font { family: Theme.font; pixelSize: root.land ? 24 : 30; weight: Font.Light }
                }
                Label {
                    visible: root.player.hasTrack
                    text: root.player.artist + (root.player.album ? " · " + root.player.album : "")
                    color: Theme.muted; elide: Text.ElideRight; Layout.fillWidth: true
                    font { family: Theme.font; pixelSize: root.land ? 16 : 19 }
                }

                Slider {
                    id: seek
                    objectName: "phoneSeek"
                    Layout.fillWidth: true; Layout.topMargin: root.land ? 4 : 10
                    from: 0; to: Math.max(1, root.player.duration)
                    enabled: root.player.hasTrack
                    onMoved: root.player.seek(value)
                    Binding { target: seek; property: "value"; value: root.player.position; when: !seek.pressed }
                    background: Rectangle {
                        x: seek.leftPadding; y: seek.topPadding + seek.availableHeight / 2 - height / 2
                        width: seek.availableWidth; height: 5; radius: 2; color: Theme.track
                        Rectangle { width: seek.visualPosition * parent.width; height: parent.height; radius: 2; color: Theme.accent }
                    }
                    handle: Item { width: 0; height: 0 }   // the design has no knob, only the filled track
                }
                RowLayout {
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                    Label { text: root.player.hasTrack ? hop.formatTime(Math.max(1, root.player.position)) : "–"; color: Theme.muted; font { family: Theme.font; pixelSize: 14 } }
                    Item { Layout.fillWidth: true }
                    Label { text: root.player.hasTrack ? hop.formatTime(root.player.duration) : ""; color: Theme.muted; font { family: Theme.font; pixelSize: 14 } }
                }

                RowLayout {
                    Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.topMargin: root.land ? 0 : 4
                    spacing: 0
                    Item { Layout.fillWidth: true }
                    TransportButton { glyph: "shuffle"; iconSize: 20; active: root.player.shuffle; onClicked: root.player.shuffle = !root.player.shuffle
                                      Accessible.name: root.player.shuffle ? "shuffle on" : "shuffle off" }
                    Item { Layout.fillWidth: true }
                    TransportButton { glyph: "prev"; iconSize: 26; enabled: root.player.hasTrack; onClicked: root.player.previous(); Accessible.name: "previous" }
                    Item { Layout.fillWidth: true }
                    TransportButton {
                        objectName: "phonePlay"
                        glyph: root.player.playing ? "pause" : "play"; round: true; filled: true; iconSize: root.land ? 24 : 28
                        implicitWidth: root.land ? 64 : 76
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
                    Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.topMargin: 2
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
                Item { Layout.fillHeight: !root.land }
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
    }
}
