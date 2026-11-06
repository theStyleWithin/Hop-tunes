// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Cover Pinball: a pinball table made from the cover of the song that is playing. The bumpers stand where the cover is
// brightest (and take its colours), the beat of the song kicks them, and the bass changes how heavy the ball feels.
// Keyboard or controller. The rules live in game/PinTable.cpp; this only draws and listens.
FocusScope {
    id: root
    objectName: "coverPinball"
    readonly property var game: hop.pinball
    property bool shown: false                  // set by Now playing: the game is on screen
    property int lastRank: 0
    readonly property string cover: hop.player.artUrl
    readonly property var scores: { hop.gameSettings; return hop.gameScores("pinball") }
    readonly property bool needMusic: !hop.player.playing && game.state !== 2
    signal leave()

    function newGame() { lastRank = 0; game.start(cover) }
    function act(a) {
        if (a === "pause") { if (game.state === 2) newGame(); else game.paused = !game.paused; return }
        if (a === "exit") { leave(); return }
        if (a === "launch") { game.launch(); return }
        if (a === "left") game.flip(true, true)
        else if (a === "leftUp") game.flip(true, false)
        else if (a === "right") game.flip(false, true)
        else if (a === "rightUp") game.flip(false, false)
    }
    onShownChanged: {
        if (shown) { if (!game.running && game.state !== 2) newGame(); game.paused = false; forceActiveFocus() }
        else game.paused = true
    }
    // a new song means a new table (but never in the middle of a ball)
    onCoverChanged: if (shown && game.state === 0 && game.score === 0) newGame()
    Connections {
        target: game
        function onFinished(score, rounds, scored) { if (scored) root.lastRank = hop.submitScore("pinball", score, rounds) }
    }
    Connections {
        target: hop.gamepad
        enabled: root.shown
        function onPressed(a) {
            const map = { left: "left", lb: "left", x: "left", right: "right", rb: "right", b: "right",
                          a: "launch", down: "launch", up: "launch", start: "pause", back: "exit" }
            if (map[a]) root.act(map[a])
        }
        function onReleased(a) {
            const map = { left: "leftUp", lb: "leftUp", x: "leftUp", right: "rightUp", rb: "rightUp", b: "rightUp" }
            if (map[a]) root.act(map[a])
        }
    }
    Keys.onPressed: function (e) {
        if (e.isAutoRepeat) { e.accepted = true; return }
        const k = e.key
        let a = ""
        if (k === Qt.Key_Left || k === Qt.Key_Z || k === Qt.Key_A) a = "left"
        else if (k === Qt.Key_Right || k === Qt.Key_Slash || k === Qt.Key_D) a = "right"
        else if (k === Qt.Key_Space || k === Qt.Key_Down || k === Qt.Key_S) a = "launch"
        else if (k === Qt.Key_Shift) a = (e.nativeScanCode === 50 || e.nativeModifiers & 1) ? "left" : "right"   // either shift key flips
        else if (k === Qt.Key_P || (k === Qt.Key_Return && game.state === 2)) a = "pause"
        else if (k === Qt.Key_R) { e.accepted = true; newGame(); return }
        else if (k === Qt.Key_Escape) a = "exit"
        if (a === "") return
        e.accepted = true
        act(a)
    }
    Keys.onReleased: function (e) {
        if (e.isAutoRepeat) { e.accepted = true; return }
        const k = e.key
        if (k === Qt.Key_Left || k === Qt.Key_Z || k === Qt.Key_A) { game.flip(true, false); e.accepted = true }
        else if (k === Qt.Key_Right || k === Qt.Key_Slash || k === Qt.Key_D) { game.flip(false, false); e.accepted = true }
        else if (k === Qt.Key_Shift) { game.flip(true, false); game.flip(false, false); e.accepted = true }
    }

    readonly property real s: Math.max(14, Math.floor(Math.min((height - 24) / 17, (width - 560) / 10)))

    RowLayout {
        anchors.centerIn: parent
        spacing: 36

        // ---- left: the numbers ----
        ColumnLayout {
            Layout.preferredWidth: 220; Layout.alignment: Qt.AlignTop
            spacing: 14
            Label { text: "cover pinball"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(30); weight: Font.Light } }
            Label { text: "score"; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14); letterSpacing: 2 } }
            Label { objectName: "pbScore"; text: root.game.score; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(44); weight: Font.Light } }
            Row {
                spacing: 8
                Label { text: "balls"; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14); letterSpacing: 2 } anchors.verticalCenter: parent.verticalCenter }
                Repeater { model: 3
                           Rectangle { required property int index; width: 14; height: 14; radius: 7; anchors.verticalCenter: parent.verticalCenter
                                       color: index < root.game.ballsLeft ? Theme.accent : "transparent"; border { width: 1; color: Theme.border } } }
            }
            Label { text: "rounds " + root.game.rounds; color: Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(17) } }
            Label {
                text: root.game.hits > 0 && root.game.lastHitOnBeat ? "on the beat" : " "
                color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(17) }
            }
            Label {
                text: hop.audio.available && !root.game.pulse ? Math.round(hop.audio.bpm) + " bpm" : "keeping its own pulse"
                color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) }
            }
            Flow {   // pause, and let the song play the game for you (nothing is scored while it does)
                Layout.fillWidth: true; spacing: 8
                PlainButton { focusPolicy: Qt.NoFocus; objectName: "pbPause"; text: root.game.paused ? "go on" : "pause"; enabled: root.game.state !== 2
                              onClicked: root.act("pause") }
                PlainButton { focusPolicy: Qt.NoFocus; objectName: "pbAutoplay"; text: root.game.autoplay ? "stop autoplay" : "let the song play"; primary: root.game.autoplay
                              onClicked: root.game.autoplay = !root.game.autoplay }
            }
            Label {
                visible: root.game.autoplay
                text: "the song is playing: this game is not scored"
                color: Theme.accent; wrapMode: Text.WordWrap; Layout.fillWidth: true; font { family: Theme.font; pixelSize: Theme.fs(14) }
            }
        }

        // ---- the table ----
        Rectangle {
            id: frame
            objectName: "pbTable"
            Layout.preferredWidth: root.s * 10 + 8; Layout.preferredHeight: root.s * 17 + 8
            color: Theme.fade(0.78)
            border { width: 4; color: Theme.border }
            clip: true
            Rectangle {   // pulses with every beat
                id: pulseRing
                anchors.fill: parent; z: 5; color: "transparent"
                border { width: 4; color: Theme.accent }
                opacity: 0
                SequentialAnimation { id: pulseAnim
                    NumberAnimation { target: pulseRing; property: "opacity"; from: 0.9; to: 0; duration: Theme.reduceMotion ? 1 : 380; easing.type: Easing.OutQuad } }
            }
            Connections { target: root.game; function onBeatFlashChanged() { pulseAnim.restart() } }
            Item {
                id: field
                x: 4; y: 4; width: root.s * 10; height: root.s * 17
                // the cover is the floor of the table
                Image {
                    anchors.fill: parent
                    source: root.cover
                    sourceSize.width: 400; sourceSize.height: 680
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true; cache: true
                    opacity: 0.55
                }
                Rectangle { anchors.fill: parent; color: Theme.bgDeep; opacity: 0.35 }
                Canvas {   // the walls
                    id: wallCv
                    anchors.fill: parent
                    property var w: root.game.walls
                    onWChanged: requestPaint()
                    onWidthChanged: requestPaint()
                    onPaint: {
                        const g = getContext("2d")
                        g.reset()
                        g.strokeStyle = Theme.accent; g.lineWidth = Math.max(3, root.s * 0.14); g.lineCap = "round"; g.globalAlpha = 0.9
                        for (let i = 0; i + 3 < w.length; i += 4) {
                            g.beginPath(); g.moveTo(w[i] * root.s, w[i + 1] * root.s); g.lineTo(w[i + 2] * root.s, w[i + 3] * root.s); g.stroke()
                        }
                    }
                }
                Repeater {   // the bumpers: where the cover is brightest, in its colours; they flash with the beat
                    model: root.game.bumperCount
                    Item {
                        required property int index
                        readonly property real pulse: { root.game.frame; return root.game.bumperPulse(index) }
                        readonly property bool lit: { root.game.frame; return root.game.bumperLit(index) }
                        readonly property color tint: { root.game.bumperCount; return root.game.bumperColor(index) }
                        readonly property real r: { root.game.bumperCount; return root.game.bumperR(index) * root.s }
                        x: root.game.bumperX(index) * root.s - r; y: root.game.bumperY(index) * root.s - r
                        width: r * 2; height: r * 2
                        Rectangle { anchors.centerIn: parent; width: parent.width * (1.25 + 0.35 * pulse); height: width; radius: width / 2
                                    color: tint; opacity: 0.18 + 0.35 * pulse }
                        Rectangle { anchors.fill: parent; radius: width / 2; color: tint; opacity: lit ? 1.0 : 0.62
                                    border { width: 3; color: Qt.rgba(1, 1, 1, 0.55 + 0.4 * pulse) } }
                        Rectangle { anchors.centerIn: parent; width: parent.width * 0.4; height: width; radius: width / 2; color: Qt.rgba(1, 1, 1, 0.45) }
                    }
                }
                Repeater {   // the two flippers
                    model: 2
                    Rectangle {
                        required property int index
                        readonly property bool isLeft: index === 0
                        readonly property real ang: { root.game.frame; return isLeft ? root.game.leftAngle : root.game.rightAngle }
                        readonly property real r: root.game.flipR * root.s
                        objectName: isLeft ? "pbFlipperLeft" : "pbFlipperRight"
                        x: (isLeft ? root.game.leftPivotX : root.game.rightPivotX) * root.s - r
                        y: root.game.flipY * root.s - r
                        width: root.game.flipLen * root.s + 2 * r; height: 2 * r; radius: r
                        color: Theme.strong
                        transformOrigin: Item.Left
                        rotation: isLeft ? ang * 57.2958 : 180 - ang * 57.2958
                    }
                }
                Rectangle {   // the ball
                    id: ball
                    readonly property real br: root.game.ballR * root.s
                    x: { root.game.frame; return root.game.ballX * root.s - br }
                    y: { root.game.frame; return root.game.ballY * root.s - br }
                    width: br * 2; height: br * 2; radius: br
                    color: "#f4f4f4"
                    border { width: 2; color: Qt.rgba(0, 0, 0, 0.35) }
                }
            }
            Rectangle {   // game over, paused, waiting for music, waiting for the launch
                objectName: "pbOverlay"
                anchors.fill: parent; z: 6; color: Theme.fade(0.8)
                visible: root.game.state === 2 || root.game.paused || root.needMusic
                ColumnLayout {
                    anchors.centerIn: parent; spacing: 12; width: parent.width - 40
                    Label { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; color: Theme.strong
                            font { family: Theme.font; pixelSize: Theme.fs(34); weight: Font.Light }
                            text: root.game.state === 2 ? "game over" : (root.needMusic ? "play a song to start" : "paused") }
                    Label { visible: root.game.state === 2; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(20) }
                            text: root.game.score + " points" + (!root.game.scored ? " (not counted: the song was playing)" : root.lastRank === 1 ? " — a new best!" : (root.lastRank > 0 ? " — #" + root.lastRank + " on your list" : "")) }
                    PlainButton { focusPolicy: Qt.NoFocus; visible: root.needMusic; Layout.alignment: Qt.AlignHCenter; text: "play"; primary: true
                                  onClicked: hop.player.hasTrack ? hop.player.toggle() : hop.playSongs(0) }
                    PlainButton { focusPolicy: Qt.NoFocus; visible: root.game.state === 2; Layout.alignment: Qt.AlignHCenter; text: "play again"; primary: true; onClicked: root.newGame() }
                    Label { visible: root.game.paused && root.game.state !== 2; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; color: Theme.muted
                            text: "press P (or start on a controller) to go on"; font { family: Theme.font; pixelSize: Theme.fs(15) } wrapMode: Text.WordWrap }
                }
            }
            Label {   // the ball waits in the lane
                objectName: "pbLaunchHint"
                visible: root.game.state === 0 && !root.game.paused && !root.game.autoplay && root.game.state !== 2 && !root.needMusic
                z: 4; anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 10 }
                text: "space to launch"; color: Theme.strong
                font { family: Theme.font; pixelSize: Theme.fs(16) }
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
                    text: (index + 1) + ".  " + modelData.score + "   (" + modelData.lines + " rounds)"
                    color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(16) }
                }
            }
            Label { visible: root.scores.length === 0; text: hop.guest ? "guests keep no scores" : "no score yet"; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(15) } }
            Label {
                Layout.topMargin: 14; Layout.fillWidth: true; wrapMode: Text.WordWrap
                color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) }
                text: "the bumpers stand where this cover is brightest. Light them all for a round.\n\nkeys: Z or left for the left flipper, / or right for the right one, space to launch, P to pause, R to start over, Esc to leave.\n\n"
                      + (hop.gamepad.connected ? "controller (" + hop.gamepad.name + "): LB or X for the left flipper, RB or B for the right, A to launch, start to pause, back to leave."
                                               : "a controller works too: plug one in.")
            }
        }
    }
}
