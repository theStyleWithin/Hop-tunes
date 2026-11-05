// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Album Fall: falling blocks, every block one of your album covers. The beat of the song decides the next piece
// (soft beat: a small piece, hard beat: a big one), the tempo and the bass set the speed, and landing on the beat
// pays extra. Keyboard or controller. The rules live in game/FallBoard.cpp; this only draws and listens.
FocusScope {
    id: root
    objectName: "albumFall"
    readonly property var game: hop.albumFall
    property bool shown: false                  // set by Now playing: the game is on screen
    property var covers: []                     // cover urls the blocks use
    property int lastRank: 0
    property bool helpOpen: false
    readonly property var scores: { hop.gameSettings; return hop.gameScores("albumfall") }
    readonly property bool needMusic: !hop.player.playing && !game.gameOver
    signal leave()

    function newGame() {
        const all = hop.mosaicArt.slice()
        for (let i = all.length - 1; i > 0; --i) { const j = Math.floor(Math.random() * (i + 1)); const t = all[i]; all[i] = all[j]; all[j] = t }
        covers = all.slice(0, 8)
        lastRank = 0
        game.start(Math.max(covers.length, 5))
    }
    function act(a) {
        if (a === "pause") { if (game.gameOver) newGame(); else game.paused = !game.paused; return }
        if (a === "exit") { leave(); return }
        game.act(a)
    }
    onShownChanged: {
        if (shown) { if (!game.running && !game.gameOver) newGame(); game.paused = false; forceActiveFocus() }
        else game.paused = true
    }
    Connections {
        target: game
        function onFinished(score, lines, scored) { if (scored) root.lastRank = hop.submitScore("albumfall", score, lines) }
    }
    Connections {
        target: hop.gamepad
        enabled: root.shown
        function onPressed(a) {
            const map = { left: "left", right: "right", down: "down", up: "rotate", a: "rotate", b: "rotateBack", rb: "rotate", lb: "rotateBack",
                          x: "drop", y: "drop", start: "pause", back: "exit" }
            if (map[a]) root.act(map[a])
        }
    }
    Keys.onPressed: function (e) {
        const k = e.key
        let a = ""
        if (k === Qt.Key_Left || k === Qt.Key_A) a = "left"
        else if (k === Qt.Key_Right || k === Qt.Key_D) a = "right"
        else if (k === Qt.Key_Down || k === Qt.Key_S) a = "down"
        else if (k === Qt.Key_Up || k === Qt.Key_W || k === Qt.Key_X) a = "rotate"
        else if (k === Qt.Key_Z) a = "rotateBack"
        else if (k === Qt.Key_Space) a = "drop"
        else if (k === Qt.Key_P || (k === Qt.Key_Return && game.gameOver) || (k === Qt.Key_R)) a = k === Qt.Key_R ? "restart" : "pause"
        else if (k === Qt.Key_Escape) a = "exit"
        if (a === "") return
        e.accepted = true
        if (a === "restart") newGame(); else act(a)
    }

    // ---- one block: a cover on a coloured tile (the colour shows while the cover loads, or when there is none) ----
    component Block: Item {
        id: blk
        property int art: 0
        property bool ghost: false
        readonly property var tint: Theme.artColors("blk" + art)
        Rectangle { anchors.fill: parent; anchors.margins: 1; color: blk.ghost ? "transparent" : blk.tint[1]
                    border { width: blk.ghost ? 2 : 0; color: Theme.accent } opacity: blk.ghost ? 0.35 : 1 }
        Image {
            anchors.fill: parent; anchors.margins: 1
            visible: !blk.ghost && root.covers.length > 0
            source: root.covers.length > 0 ? root.covers[blk.art % root.covers.length] : ""
            sourceSize.width: 96; sourceSize.height: 96
            fillMode: Image.PreserveAspectCrop
            asynchronous: true; cache: true
        }
        Rectangle { anchors.fill: parent; anchors.margins: 1; visible: !blk.ghost; color: "transparent"; border { width: 1; color: Qt.rgba(0, 0, 0, 0.45) } }
    }

    readonly property real cs: Math.max(12, Math.floor(Math.min((height - 24) / 20, (width - 560) / 10)))

    RowLayout {
        anchors.centerIn: parent
        spacing: 36

        // ---- left: next piece and the numbers ----
        ColumnLayout {
            Layout.preferredWidth: 220; Layout.alignment: Qt.AlignTop
            spacing: 14
            Label { text: "album fall"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(30); weight: Font.Light } }
            Label { text: "next"; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14); letterSpacing: 2 } }
            Rectangle {
                Layout.preferredWidth: 132; Layout.preferredHeight: 132
                color: Theme.fade(0.7); border { width: 1; color: Theme.line }
                Item {
                    anchors.centerIn: parent
                    Repeater {
                        model: root.game.next.length / 2
                        Block {
                            required property int index
                            x: root.game.next[index * 2] * 26 - 13; y: root.game.next[index * 2 + 1] * 26 - 13
                            width: 26; height: 26; art: root.game.nextArt
                        }
                    }
                }
            }
            Label { text: "score"; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14); letterSpacing: 2 } }
            Label { objectName: "afScore"; text: root.game.score; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(44); weight: Font.Light } }
            Label { text: "lines " + root.game.lines + "   level " + root.game.level; color: Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(17) } }
            Label {
                text: root.game.streak > 1 ? "on the beat x" + root.game.streak : (root.game.streak === 1 ? "on the beat" : " ")
                color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(17) }
            }
            Label {
                text: hop.audio.available && !root.game.pulse ? Math.round(hop.audio.bpm) + " bpm" : "keeping its own pulse"
                color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) }
            }
            Flow {   // pause, and let the song play the game for you (nothing is scored while it does)
                Layout.fillWidth: true; spacing: 8
                PlainButton { objectName: "afPause"; text: root.game.paused ? "go on" : "pause"; enabled: !root.game.gameOver
                              onClicked: root.act("pause") }
                PlainButton { objectName: "afAutoplay"; text: root.game.autoplay ? "stop autoplay" : "let the song play"; primary: root.game.autoplay
                              onClicked: root.game.autoplay = !root.game.autoplay }
            }
            Label {
                visible: root.game.autoplay
                text: "the song is playing: this game is not scored"
                color: Theme.accent; wrapMode: Text.WordWrap; Layout.fillWidth: true; font { family: Theme.font; pixelSize: Theme.fs(14) }
            }
        }

        // ---- the board ----
        Rectangle {
            id: boardFrame
            Layout.preferredWidth: root.cs * 10 + 8; Layout.preferredHeight: root.cs * 20 + 8
            color: Theme.fade(0.78)
            border { width: 4; color: Theme.border }
            Rectangle {   // pulses with every beat
                id: pulseRing
                anchors.fill: parent; color: "transparent"
                border { width: 4; color: Theme.accent }
                opacity: 0
                SequentialAnimation { id: pulseAnim
                    NumberAnimation { target: pulseRing; property: "opacity"; from: 0.9; to: 0; duration: Theme.reduceMotion ? 1 : 380; easing.type: Easing.OutQuad } }
            }
            Connections { target: root.game; function onBeatFlashChanged() { pulseAnim.restart() } }
            Item {
                id: grid
                x: 4; y: 4; width: root.cs * 10; height: root.cs * 20
                Repeater {   // what has landed
                    model: 200
                    Block {
                        required property int index
                        readonly property int v: { root.game.rev; return root.game.cell(index % 10, Math.floor(index / 10)) }
                        visible: v > 0
                        x: (index % 10) * root.cs; y: Math.floor(index / 10) * root.cs
                        width: root.cs; height: root.cs; art: v - 1
                    }
                }
                Repeater {   // where the piece would land
                    model: root.game.ghost.length / 2
                    Block {
                        required property int index
                        x: root.game.ghost[index * 2] * root.cs; y: root.game.ghost[index * 2 + 1] * root.cs
                        width: root.cs; height: root.cs; ghost: true
                    }
                }
                Repeater {   // the falling piece
                    model: root.game.piece.length / 2
                    Block {
                        required property int index
                        visible: root.game.piece[index * 2 + 1] >= 0
                        x: root.game.piece[index * 2] * root.cs; y: root.game.piece[index * 2 + 1] * root.cs
                        width: root.cs; height: root.cs; art: root.game.pieceArt
                    }
                }
            }
            Rectangle {   // game over, paused, waiting for music
                objectName: "afOverlay"
                anchors.fill: parent; color: Theme.fade(0.8)
                visible: root.game.gameOver || root.game.paused || root.needMusic
                ColumnLayout {
                    anchors.centerIn: parent; spacing: 12; width: parent.width - 40
                    Label { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; color: Theme.strong
                            font { family: Theme.font; pixelSize: Theme.fs(34); weight: Font.Light }
                            text: root.game.gameOver ? "game over" : (root.needMusic ? "play a song to start" : "paused") }
                    Label { visible: root.game.gameOver; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(20) }
                            text: root.game.score + " points" + (!root.game.scored ? " (not counted: the song was playing)" : root.lastRank === 1 ? " — a new best!" : (root.lastRank > 0 ? " — #" + root.lastRank + " on your list" : "")) }
                    PlainButton { visible: root.needMusic; Layout.alignment: Qt.AlignHCenter; text: "play"; primary: true
                                  onClicked: hop.player.hasTrack ? hop.player.toggle() : hop.playSongs(0) }
                    PlainButton { visible: root.game.gameOver; Layout.alignment: Qt.AlignHCenter; text: "play again"; primary: true; onClicked: root.newGame() }
                    Label { visible: root.game.paused && !root.game.gameOver; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; color: Theme.muted
                            text: "press P (or start on a controller) to go on"; font { family: Theme.font; pixelSize: Theme.fs(15) } wrapMode: Text.WordWrap }
                }
            }
        }

        // ---- right: best scores and how to play ----
        ColumnLayout {
            Layout.preferredWidth: 240; Layout.alignment: Qt.AlignTop
            spacing: 10
            Label { text: "your best"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(22) } }
            Repeater {
                model: root.scores
                Label {
                    required property var modelData
                    required property int index
                    text: (index + 1) + ".  " + modelData.score + "   (" + modelData.lines + " lines)"
                    color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(16) }
                }
            }
            Label { visible: root.scores.length === 0; text: hop.guest ? "guests keep no scores" : "no score yet"; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(15) } }
            Label {
                Layout.topMargin: 14; Layout.fillWidth: true; wrapMode: Text.WordWrap
                color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) }
                text: "keys: arrows or WASD to move, up / W / X to turn, Z to turn back, space to drop, P to pause, Esc to leave.\n\n"
                      + (hop.gamepad.connected ? "controller (" + hop.gamepad.name + "): d-pad or stick to move, A or RB to turn, B or LB to turn back, X to drop, start to pause, back to leave."
                                               : "a controller works too: plug one in.")
            }
        }
    }
}
