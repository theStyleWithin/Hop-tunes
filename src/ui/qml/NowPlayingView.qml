// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Now playing (design/screens/5): drifting art mosaic, big type, up next.
Rectangle {
    id: root
    color: Theme.compact ? Theme.bg : Theme.bgDeep
    clip: true
    readonly property var player: hop.player
    signal back()

    // The stage: the song and up-next as always ("art"), the music made visible, or one of the games (Album Fall, Cover Pinball). Desktop only.
    readonly property string stage: Theme.compact || !hop.audio.available ? "art" : hop.gameSettings.stage
    readonly property bool staged: stage !== "art"
    readonly property bool inGames: stage === "albumfall" || stage === "pinball"
    readonly property var gs: hop.gameSettings
    function setStage(s) { hop.setGameSetting("stage", s) }
    function setGame(g) { hop.setGameSetting("game", g); hop.setGameSetting("stage", g) }
    // the analysis and the controller are only listened to while they are on screen
    readonly property bool wantsAudio: root.visible && root.staged
    onWantsAudioChanged: hop.audio.want("stage", wantsAudio)
    function syncPad() { hop.gamepad.setListening(root.visible && root.inGames) }
    onStageChanged: syncPad()
    Component.onCompleted: syncPad()
    Shortcut { sequence: "G"; enabled: root.visible && !Theme.compact && hop.audio.available && !root.inGames; onActivated: root.setStage(root.stage === "art" ? "visualizer" : root.gs.game) }

    // Controls fade out after a few idle seconds while music plays (spec §4.3); any mouse
    // movement or key brings them back.
    property bool idle: false
    function wake() { idle = false; idleTimer.restart() }
    onVisibleChanged: { wake(); syncPad() }
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
        visible: !Theme.compact && root.stage === "art"
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
        visible: !Theme.compact && root.stage === "art"
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
        visible: !Theme.compact
        opacity: root.chrome
        Behavior on opacity { NumberAnimation { duration: Theme.reduceMotion ? 0 : 450 } }
        anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: Theme.pad; rightMargin: Theme.pad; topMargin: 24 }
        AbstractButton {
            implicitWidth: 44; implicitHeight: 44
            onClicked: root.back()
            Accessible.name: "back"
            background: Rectangle { radius: 22; color: "transparent"; border { width: 2; color: Theme.border } }
            contentItem: Label { text: "‹"; color: Theme.text; font.pixelSize: Theme.fs(26); horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; bottomPadding: 4 }
        }
        Item { Layout.fillWidth: true }
        Row {   // the song | visualizer | games (where the player can analyse the sound: not on Android yet)
            visible: hop.audio.available
            spacing: 6
            Layout.rightMargin: 24
            Repeater {
                model: [["art", "the song"], ["visualizer", "visualizer"], ["games", "games"]]
                delegate: AbstractButton {
                    id: chip
                    required property var modelData
                    readonly property bool active: modelData[0] === "games" ? root.inGames : root.stage === modelData[0]
                    objectName: "stage_" + modelData[0]
                    hoverEnabled: true
                    onClicked: root.setStage(modelData[0] === "games" ? root.gs.game : modelData[0])
                    Accessible.name: modelData[1]
                    contentItem: Label { text: chip.modelData[1]; color: chip.active ? Theme.ink : (chip.hovered ? Theme.text : Theme.muted)
                                         horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                         font { family: Theme.font; pixelSize: Theme.fs(15) } }
                    background: Rectangle { implicitWidth: Math.max(90, chip.contentItem.implicitWidth + 28); implicitHeight: 34
                                            color: chip.active ? Theme.accent : "transparent"
                                            border { width: chip.active ? 0 : 1; color: Theme.border } }
                }
            }
        }
        AbstractButton {   // the pretend live chat beside the song: optional, off until you want it
            id: chatChip
            objectName: "chatToggle"
            Layout.rightMargin: 24
            hoverEnabled: true
            onClicked: hop.setGameSetting("chat", !root.gs.chat)
            Accessible.name: "live chat"
            contentItem: Label { text: (root.gs.chat ? "\u2611 " : "\u2610 ") + "live chat"; color: root.gs.chat ? Theme.accent : (chatChip.hovered ? Theme.text : Theme.muted)
                                 font { family: Theme.font; pixelSize: Theme.fs(15) } }
            background: Item {}
        }
        Label { text: "NOW PLAYING"; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(16); letterSpacing: 3 } }
    }

    // ---- the stage: the visualizer, and Album Fall (optionally over the visualizer) ----
    Item {
        id: stageArea
        objectName: "nowPlayingStage"
        visible: !Theme.compact && root.staged
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom; leftMargin: Theme.pad; rightMargin: Theme.pad + (root.chatShown ? 372 : 0); topMargin: 10; bottomMargin: 18 }
        Visualizer {
            objectName: "visualizer"
            anchors { left: parent.left; right: parent.right; top: parent.top; bottom: stageOptions.top; bottomMargin: 10 }
            visible: root.stage === "visualizer" || (root.inGames && root.gs.vizBackdrop)
            style: root.gs.vizStyle; colorMode: root.gs.vizColor; intensity: root.gs.vizIntensity
            opacityScale: root.inGames ? 0.28 : 1.0
        }
        AlbumFall {
            id: fall
            anchors { left: parent.left; right: parent.right; top: parent.top; bottom: stageOptions.top; bottomMargin: 10 }
            visible: root.stage === "albumfall"
            shown: root.visible && root.stage === "albumfall"
            onLeave: root.setStage("art")
        }
        CoverPinball {
            id: pinball
            anchors { left: parent.left; right: parent.right; top: parent.top; bottom: stageOptions.top; bottomMargin: 10 }
            visible: root.stage === "pinball"
            shown: root.visible && root.stage === "pinball"
            onLeave: root.setStage("art")
        }
        Label {   // the song, small, over the visualizer
            visible: root.stage === "visualizer" && root.player.hasTrack
            anchors { left: parent.left; top: parent.top; margins: 6 }
            text: root.player.title + "  \u00b7  " + root.player.artist
            color: Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(20); weight: Font.Light }
        }
        RowLayout {   // the optional bits: look of the visualizer, behind the game, and chat
            id: stageOptions
            objectName: "stageOptions"
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            opacity: root.chrome
            Behavior on opacity { NumberAnimation { duration: Theme.reduceMotion ? 0 : 450 } }
            spacing: 14
            component OptChip: AbstractButton {
                id: oc
                property bool on: false
                hoverEnabled: true
                contentItem: Label { text: oc.text; color: oc.on ? Theme.accent : (oc.hovered ? Theme.text : Theme.muted); font { family: Theme.font; pixelSize: Theme.fs(14); underline: oc.on } }
                background: Item {}
            }
            Label { visible: root.inGames; text: "game"; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13); letterSpacing: 1 } }
            Repeater { model: root.inGames ? [["albumfall", "album fall"], ["pinball", "cover pinball"]] : []
                       delegate: OptChip { required property var modelData; objectName: "game_" + modelData[0]; text: modelData[1]; on: root.stage === modelData[0]
                                           onClicked: root.setGame(modelData[0]) } }
            Label { text: "visualizer"; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13); letterSpacing: 1 } }
            Repeater { model: ["bars", "mirror", "rings"]
                       delegate: OptChip { required property string modelData; objectName: "vizStyle_" + modelData; text: modelData; on: root.gs.vizStyle === modelData
                                           onClicked: hop.setGameSetting("vizStyle", modelData) } }
            Label { text: "colour"; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13); letterSpacing: 1 } }
            Repeater { model: ["accent", "rainbow", "white"]
                       delegate: OptChip { required property string modelData; objectName: "vizColor_" + modelData; text: modelData; on: root.gs.vizColor === modelData
                                           onClicked: hop.setGameSetting("vizColor", modelData) } }
            Label { text: "strength"; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13); letterSpacing: 1 } }
            Slider { objectName: "vizIntensity"; from: 0.3; to: 2.0; value: root.gs.vizIntensity; implicitWidth: 120
                     onMoved: hop.setGameSetting("vizIntensity", value) }
            OptChip { objectName: "vizBackdrop"; visible: root.inGames; text: root.gs.vizBackdrop ? "\u2611 visualizer behind the game" : "\u2610 visualizer behind the game"
                      on: root.gs.vizBackdrop; onClicked: hop.setGameSetting("vizBackdrop", !root.gs.vizBackdrop) }
            Item { Layout.fillWidth: true }
            Label { visible: root.inGames && hop.gamepad.connected; text: "\u25cf " + hop.gamepad.name; color: Theme.dim; elide: Text.ElideRight; Layout.maximumWidth: 260
                    font { family: Theme.font; pixelSize: Theme.fs(13) } }
        }
    }

    GridLayout {   // desktop and tablets: the song beside its up-next list
        visible: !Theme.compact && root.stage === "art"
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom; leftMargin: Theme.pad; rightMargin: Theme.pad; topMargin: 16; bottomMargin: 36 }
        columns: Theme.compact ? 1 : 2
        columnSpacing: 48; rowSpacing: 18

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignBottom
            spacing: 20

            AlbumArt { key: root.player.album; source: root.player.artUrl; Layout.preferredWidth: Theme.compact ? 240 : 200; Layout.preferredHeight: Theme.compact ? 240 : 200; Layout.alignment: Theme.compact ? Qt.AlignHCenter : Qt.AlignLeft; visible: root.player.hasTrack }
            Label { text: root.player.hasTrack ? root.player.artist : ""; color: Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(28); weight: Font.Light }
                    elide: Text.ElideRight; Layout.fillWidth: true }
            Label {
                text: root.player.hasTrack ? root.player.title : "nothing playing"
                color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(76); weight: Font.Light; letterSpacing: -2 }
                elide: Text.ElideRight; Layout.fillWidth: true
                wrapMode: Theme.compact ? Text.WordWrap : Text.NoWrap; maximumLineCount: Theme.compact ? 2 : 1
            }
            Label {
                visible: root.player.hasTrack
                wrapMode: Text.WordWrap; Layout.fillWidth: true
                text: root.player.album + (root.player.year > 0 ? " · " + root.player.year : "")
                      + " · track " + (root.player.currentIndex + 1) + " of " + root.player.queueLength
                color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(22) }
            }
            RowLayout {
                spacing: 18
                Layout.topMargin: 8
                Layout.alignment: Theme.compact ? Qt.AlignHCenter : Qt.AlignLeft
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
            Layout.fillWidth: Theme.compact
            Layout.alignment: Qt.AlignBottom
            Layout.preferredHeight: upNext.implicitHeight + 40
            color: Theme.fade(0.82)
            readonly property int rowH: 36
            ColumnLayout {
                id: upNext
                anchors { fill: parent; margins: 20 }
                spacing: 0
                Label { text: "up next"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(22) } bottomPadding: 8 }
                Label {
                    visible: root.player.currentIndex + 1 >= root.player.queueLength
                    text: root.player.repeatMode === 1 ? "the queue starts again" : "nothing after this song"
                    color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(15) }
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
                                text: "≡"; color: Theme.dim; font.pixelSize: Theme.fs(18)
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
                                color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(17) }
                                elide: Text.ElideRight; Layout.fillWidth: true
                                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.player.playQueueIndex(qrow.modelData) }
                            }
                            AbstractButton {
                                visible: rowHover.hovered
                                implicitWidth: 26; implicitHeight: 26
                                onClicked: root.player.removeFromQueue(qrow.modelData)
                                Accessible.name: "remove from queue"
                                contentItem: Label { text: "×"; color: Theme.muted; font.pixelSize: Theme.fs(18); horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                background: Item { }
                            }
                        }
                    }
                }
                Label {
                    visible: root.player.queueLength - root.player.currentIndex - 1 > 7
                    text: "+ " + (root.player.queueLength - root.player.currentIndex - 8) + " more"
                    color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) } topPadding: 4
                }
                Label {
                    text: hop.djSeed !== "" ? "smart dj · " + hop.djSeed
                        : (root.player.crossfadeMs > 0 ? "crossfade " + Math.round(root.player.crossfadeMs / 1000) + " s · albums stay gapless" : "gapless")
                          + (root.player.shuffle ? " · shuffle" : "") + (root.player.repeatMode === 1 ? " · repeat" : (root.player.repeatMode === 2 ? " · repeat this song" : ""))
                    color: Theme.tabIdle; font { family: Theme.font; pixelSize: Theme.fs(13) } topPadding: 8
                }
            }
        }
    }

    // ---- the pretend live chat (optional): about the song that is playing. Nobody in it is real. ----
    readonly property bool chatShown: !Theme.compact && gs.chat
    LiveChat {
        id: npChat
        objectName: "nowPlayingChat"
        visible: root.chatShown
        look: hop.videoChatLook
        seed: root.player.hasTrack ? root.player.title + "|" + root.player.artist + "|" + root.player.album : "idle"
        info: ({ music: true, meta: { title: root.player.title, year: root.player.year > 0 ? String(root.player.year) : "", album: root.player.album },
                 cast: root.player.artist ? [{ name: root.player.artist, role: "" }] : [] })
        position: root.player.position
        duration: root.player.duration
        onLeaveRequested: root.forceActiveFocus()
        width: 350
        x: root.width - width - Theme.pad
        y: header.y + header.height + 12
        height: Math.max(0, root.stage === "art" && upNextPanel.visible
                            ? upNextPanel.mapToItem(root, 0, 0).y - y - 14 + 0 * root.queueRev * upNextPanel.height
                            : root.height - y - 24)
        z: 60
    }

    PhoneNowPlaying {   // phones
        visible: Theme.compact
        anchors.fill: parent
        onBack: root.back()
    }
}
