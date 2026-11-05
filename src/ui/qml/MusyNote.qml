// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Musy Note: a music note flies through gaps in walls made of your album covers; the song makes the course. Walls reach
// the note on the beat, the tempo sets the pace and the pitch decides where the gap is. Tap, space, or any button on a
// game controller flaps. The rules live in game/MusyNote.cpp; this only draws and listens.
FocusScope {
    id: root
    objectName: "musyNote"
    readonly property var game: hop.musyNote
    property bool active: false                 // set by the window: the game is on screen
    property var covers: []                     // cover urls the blocks use
    property int rank: 0
    signal leave()

    visible: active
    z: 1000
    readonly property real s: Math.min(width / game.worldW, height / game.worldH)   // pixels per world unit
    readonly property real ox: (width - s * game.worldW) / 2
    readonly property real oy: (height - s * game.worldH) / 2
    readonly property bool needMusic: !hop.player.playing && game.state !== 2
    readonly property var best: { hop.gameSettings; const l = hop.gameScores("musynote"); return l.length > 0 ? l[0].score : 0 }

    property real openedAt: 0                   // a tap that is still part of opening the game must not start it
    function newGame() {
        openedAt = Date.now()
        const all = hop.mosaicArt.slice()
        for (let i = all.length - 1; i > 0; --i) { const j = Math.floor(Math.random() * (i + 1)); const t = all[i]; all[i] = all[j]; all[j] = t }
        covers = all.slice(0, 150)
        rank = 0
        game.start(Math.max(covers.length, 1))
    }
    function act(a, src) {
        // a press that is still part of opening the game (a lingering tap or button) never starts it
        if (a === "flap" && Date.now() - openedAt < 450) return
        if (a === "flap") { if (game.state === 2) newGame(); else game.flap(); return }
        if (a === "pause") { if (game.state === 2) newGame(); else game.paused = !game.paused; return }
        if (a === "exit") { leave(); return }
    }
    onActiveChanged: {
        hop.audio.want("musynote", active)
        hop.setGameScreen(active)
        hop.gamepad.setListening(active)
        if (active) { newGame(); forceActiveFocus() }
        else game.paused = true
    }
    Connections {
        target: game
        function onFinished(score, passed, scored) { if (scored) root.rank = hop.submitScore("musynote", score, passed) }
    }
    Connections {
        target: hop.gamepad
        enabled: root.active
        function onPressed(a) {
            if (["a", "b", "x", "y", "lb", "rb", "lt", "rt", "up"].indexOf(a) >= 0) root.act("flap", "pad " + a)
            else if (a === "start") root.act("pause")
            else if (a === "back") root.act("exit")
        }
    }
    Keys.onPressed: function (e) {
        const k = e.key
        if (k === Qt.Key_Space || k === Qt.Key_Up || k === Qt.Key_W || k === Qt.Key_Return) { e.accepted = true; act("flap", "key " + k) }
        else if (k === Qt.Key_P) { e.accepted = true; act("pause") }
        else if (k === Qt.Key_Escape || k === Qt.Key_Back) { e.accepted = true; act("exit") }
    }

    Rectangle {   // the night sky; it pulses on every beat
        anchors.fill: parent
        color: "#0b0b12"
        Rectangle {
            id: pulseGlow
            anchors.fill: parent; color: Theme.accent; opacity: 0
            SequentialAnimation { id: pulseAnim
                NumberAnimation { target: pulseGlow; property: "opacity"; from: 0.16; to: 0; duration: Theme.reduceMotion ? 1 : 420; easing.type: Easing.OutQuad } }
        }
        Connections { target: root.game; function onBeatFlashChanged() { pulseAnim.restart() } }
    }
    Visualizer {   // a faint skyline of the sound behind everything
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: parent.height * 0.4
        style: "bars"; colorMode: "accent"; intensity: 0.9; opacityScale: 0.12
    }

    // ---- one cover block ----
    component Block: Item {
        id: blk
        property int art: 0
        readonly property var tint: Theme.artColors("mn" + art)
        Rectangle { anchors.fill: parent; anchors.margins: 1; color: blk.tint[1] }
        Image {
            anchors.fill: parent; anchors.margins: 1
            visible: root.covers.length > 0
            source: root.covers.length > 0 ? root.covers[blk.art % root.covers.length] : ""
            sourceSize.width: 128; sourceSize.height: 128
            fillMode: Image.PreserveAspectCrop
            asynchronous: true; cache: true
        }
        Rectangle { anchors.fill: parent; anchors.margins: 1; color: "transparent"; border { width: 1; color: Qt.rgba(0, 0, 0, 0.5) } }
    }

    Item {   // the world, centred and scaled
        x: root.ox; y: root.oy; width: root.s * game.worldW; height: root.s * game.worldH
        clip: true

        Repeater {
            model: game.wallIds
            delegate: Item {
                id: wall
                required property var modelData
                readonly property int wid: modelData
                readonly property real gapTop: game.wallGapTop(wid)
                readonly property real gapBottom: game.wallGapBottom(wid)
                readonly property var topArts: game.wallTopArts(wid)
                readonly property var bottomArts: game.wallBottomArts(wid)
                x: { game.frame; return game.wallX(wid) * root.s }
                width: game.wallW * root.s; height: parent.height
                Repeater {   // the top part grows up from the gap
                    model: wall.topArts
                    Block { required property var modelData; required property int index
                            art: modelData; width: wall.width; height: root.s
                            y: wall.gapTop * root.s - (index + 1) * root.s }
                }
                Repeater {   // the bottom part grows down from the gap
                    model: wall.bottomArts
                    Block { required property var modelData; required property int index
                            art: modelData; width: wall.width; height: root.s
                            y: wall.gapBottom * root.s + index * root.s }
                }
            }
        }

        // The note: a round, chubby little music note with big shiny eyes, rosy cheeks and tiny wings. It blinks, beams
        // when it flies up, gasps when it falls fast and sees stars when it crashes. (Drawn a little bigger than the part
        // that can be hit, so it is forgiving and easy to love.)
        Canvas {
            id: noteCv
            objectName: "musyNoteSprite"
            readonly property real rad: game.noteR * 1.45 * root.s            // the body's radius on screen
            readonly property real d: rad * 4.6
            property bool blink: false
            readonly property string mood: game.state === 2 ? "dizzy" : (game.state === 0 ? "happy"
                                         : (game.noteVy > 5.5 ? "scared" : (game.noteVy < -2.5 ? "beam" : "happy")))
            readonly property int wing: game.state === 1 ? (game.noteVy < -1 ? 1 : (game.noteVy > 3 ? -1 : 0)) : 0
            width: d; height: d
            x: game.noteX * root.s - width / 2
            y: { game.frame; return game.noteY * root.s - height * 0.56 }
            rotation: { game.frame; return game.state === 2 ? 25 : Math.max(-28, Math.min(40, game.noteVy * 4)) }
            transformOrigin: Item.Center
            onMoodChanged: requestPaint()
            onBlinkChanged: requestPaint()
            onWingChanged: requestPaint()
            onRadChanged: requestPaint()
            Timer { interval: 2600; running: root.active; repeat: true; onTriggered: { noteCv.blink = true; blinkOff.start() } }
            Timer { id: blinkOff; interval: 130; onTriggered: noteCv.blink = false }
            onPaint: {
                const g = getContext("2d")
                g.reset()
                const cx = width / 2, cy = height * 0.56, r = rad
                const body = Theme.accent
                const dark = "#2b2140"
                // soft glow
                const glow = g.createRadialGradient(cx, cy, r * 0.8, cx, cy, r * 2.0)
                glow.addColorStop(0, Qt.rgba(1, 0.9, 0.4, 0.28)); glow.addColorStop(1, Qt.rgba(1, 0.9, 0.4, 0))
                g.fillStyle = glow; g.beginPath(); g.arc(cx, cy, r * 2.0, 0, 6.2832); g.fill()
                // wings: two little white ovals, up when flying up, down when falling
                const wa = wing > 0 ? -0.9 : (wing < 0 ? 0.55 : -0.25)
                for (const side of [-1, 1]) {
                    g.save()
                    g.translate(cx + side * r * 0.95, cy + r * 0.05)
                    g.rotate(side * wa)
                    g.fillStyle = "#ffffff"; g.globalAlpha = 0.92
                    g.beginPath(); g.ellipse(side > 0 ? 0 : -r * 0.95, -r * 0.3, r * 0.95, r * 0.62); g.fill()
                    g.globalAlpha = 1
                    g.restore()
                }
                // the stem and the flag (the note's little tuft)
                g.strokeStyle = body; g.fillStyle = body; g.lineWidth = r * 0.3; g.lineCap = "round"
                g.beginPath(); g.moveTo(cx + r * 0.78, cy - r * 0.55); g.lineTo(cx + r * 0.78, cy - r * 2.0); g.stroke()
                g.beginPath()
                g.moveTo(cx + r * 0.78, cy - r * 2.0)
                g.bezierCurveTo(cx + r * 1.7, cy - r * 1.95, cx + r * 1.95, cy - r * 1.3, cx + r * 1.4, cy - r * 0.75)
                g.bezierCurveTo(cx + r * 1.55, cy - r * 1.35, cx + r * 1.25, cy - r * 1.6, cx + r * 0.78, cy - r * 1.6)
                g.closePath(); g.fill()
                // the round body with a little shine and a darker rim
                const bg = g.createRadialGradient(cx - r * 0.35, cy - r * 0.4, r * 0.15, cx, cy, r * 1.05)
                bg.addColorStop(0, Qt.lighter(body, 1.35)); bg.addColorStop(0.7, body); bg.addColorStop(1, Qt.darker(body, 1.25))
                g.fillStyle = bg
                g.beginPath(); g.arc(cx, cy, r, 0, 6.2832); g.fill()
                // rosy cheeks
                g.fillStyle = "#ff7aa2"; g.globalAlpha = 0.55
                g.beginPath(); g.ellipse(cx - r * 0.82, cy + r * 0.18, r * 0.34, r * 0.22); g.fill()
                g.beginPath(); g.ellipse(cx + r * 0.48, cy + r * 0.18, r * 0.34, r * 0.22); g.fill()
                g.globalAlpha = 1
                // eyes
                const ex = r * 0.36, ey = cy - r * 0.08
                if (mood === "dizzy") {                                    // x x
                    g.strokeStyle = dark; g.lineWidth = r * 0.12; g.lineCap = "round"
                    for (const sx of [-1, 1]) {
                        const x = cx + sx * ex, y = ey
                        g.beginPath(); g.moveTo(x - r * 0.16, y - r * 0.16); g.lineTo(x + r * 0.16, y + r * 0.16); g.stroke()
                        g.beginPath(); g.moveTo(x + r * 0.16, y - r * 0.16); g.lineTo(x - r * 0.16, y + r * 0.16); g.stroke()
                    }
                } else if (blink) {                                         // a happy closed-eye curve
                    g.strokeStyle = dark; g.lineWidth = r * 0.1; g.lineCap = "round"
                    for (const sx of [-1, 1]) { g.beginPath(); g.arc(cx + sx * ex, ey + r * 0.04, r * 0.17, 3.4, 6.0); g.stroke() }
                } else {
                    const big = mood === "scared" ? 1.22 : 1.0
                    for (const sx of [-1, 1]) {
                        const x = cx + sx * ex
                        g.fillStyle = dark
                        g.beginPath(); g.ellipse(x - r * 0.2 * big, ey - r * 0.27 * big, r * 0.4 * big, r * 0.54 * big); g.fill()
                        g.fillStyle = "#ffffff"                                // big and small sparkles
                        g.beginPath(); g.arc(x - r * 0.07, ey - r * 0.14 * big, r * 0.12 * big, 0, 6.2832); g.fill()
                        g.beginPath(); g.arc(x + r * 0.08, ey + r * 0.12 * big, r * 0.055 * big, 0, 6.2832); g.fill()
                    }
                }
                // the mouth
                g.strokeStyle = dark; g.fillStyle = dark; g.lineWidth = r * 0.1; g.lineCap = "round"
                const my = cy + r * 0.36
                if (mood === "scared") {                                    // a little "o"
                    g.beginPath(); g.ellipse(cx - r * 0.1, my - r * 0.08, r * 0.2, r * 0.26); g.fill()
                } else if (mood === "dizzy") {                              // a wobbly line
                    g.beginPath(); g.moveTo(cx - r * 0.22, my); g.quadraticCurveTo(cx - r * 0.1, my - r * 0.1, cx, my); g.quadraticCurveTo(cx + r * 0.1, my + r * 0.1, cx + r * 0.22, my); g.stroke()
                } else if (mood === "beam") {                               // an open happy smile
                    g.beginPath(); g.moveTo(cx - r * 0.24, my - r * 0.08); g.quadraticCurveTo(cx, my + r * 0.34, cx + r * 0.24, my - r * 0.08); g.closePath(); g.fill()
                } else {                                                    // a gentle smile
                    g.beginPath(); g.moveTo(cx - r * 0.2, my - r * 0.02); g.quadraticCurveTo(cx, my + r * 0.2, cx + r * 0.2, my - r * 0.02); g.stroke()
                }
                if (mood === "dizzy") {                                     // little stars circling its head
                    g.fillStyle = "#ffe27a"
                    for (let i = 0; i < 3; ++i) {
                        const a = i * 2.094 + 0.6, x = cx + Math.cos(a) * r * 1.35, y = cy - r * 1.3 + Math.sin(a) * r * 0.35, k = r * 0.16
                        g.beginPath(); g.moveTo(x, y - k * 1.6); g.lineTo(x + k * 0.5, y - k * 0.5); g.lineTo(x + k * 1.6, y); g.lineTo(x + k * 0.5, y + k * 0.5)
                        g.lineTo(x, y + k * 1.6); g.lineTo(x - k * 0.5, y + k * 0.5); g.lineTo(x - k * 1.6, y); g.lineTo(x - k * 0.5, y - k * 0.5); g.closePath(); g.fill()
                    }
                }
            }
            Component.onCompleted: requestPaint()
        }
    }

    // ---- a tap anywhere flaps ----
    MouseArea {
        anchors.fill: parent
        onPressed: if (Date.now() - root.openedAt > 450) root.act("flap", "mouse")
    }

    // ---- HUD ----
    Label {
        objectName: "musyScore"
        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 8 }
        text: game.score
        color: "white"; opacity: 0.92
        font { family: Theme.font; pixelSize: Math.max(28, root.height * 0.13); weight: Font.Light }
    }
    Label {
        anchors { top: parent.top; right: parent.right; topMargin: 12; rightMargin: 78 }
        text: "best " + root.best + "   level " + game.level
        color: Theme.muted; font { family: Theme.font; pixelSize: 15 }
    }
    RowLayout {   // top-left: leave; top-right: pause
        anchors { top: parent.top; left: parent.left; margins: 10 }
        AbstractButton {
            objectName: "musyExit"
            implicitWidth: 44; implicitHeight: 44
            onClicked: root.act("exit")
            Accessible.name: "leave the game"
            background: Rectangle { radius: 22; color: Qt.rgba(1, 1, 1, 0.08); border { width: 1; color: Qt.rgba(1, 1, 1, 0.25) } }
            contentItem: Label { text: "✕"; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 18 }
        }
    }
    AbstractButton {
        objectName: "musyPause"
        anchors { top: parent.top; right: parent.right; margins: 10 }
        implicitWidth: 44; implicitHeight: 44
        onClicked: root.act("pause")
        Accessible.name: game.paused ? "go on" : "pause"
        background: Rectangle { radius: 22; color: Qt.rgba(1, 1, 1, 0.08); border { width: 1; color: Qt.rgba(1, 1, 1, 0.25) } }
        contentItem: Label { text: game.paused ? "▶" : "❙❙"; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 16 }
    }
    AbstractButton {   // let the song fly it: nothing is scored while it does
        id: autoBtn
        objectName: "musyAutoplay"
        anchors { bottom: parent.bottom; right: parent.right; margins: 10 }
        implicitHeight: 38; implicitWidth: autoLabel.implicitWidth + 28
        onClicked: game.autoplay = !game.autoplay
        Accessible.name: "let the song play"
        background: Rectangle { radius: 19; color: game.autoplay ? Theme.accent : Qt.rgba(1, 1, 1, 0.08); border { width: 1; color: Qt.rgba(1, 1, 1, 0.25) } }
        contentItem: Label { id: autoLabel; text: game.autoplay ? "the song is playing (no score)" : "let the song play"
                             color: game.autoplay ? Theme.ink : "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 14 }
    }
    Label {
        anchors { bottom: parent.bottom; left: parent.left; margins: 12 }
        text: (hop.gamepad.connected ? "● " + hop.gamepad.name + "    " : "") + (game.pulse ? "no beat heard: keeping its own pulse" : Math.round(hop.audio.bpm) + " bpm")
        color: Theme.dim; font { family: Theme.font; pixelSize: 13 }
    }

    // ---- ready, paused, game over, waiting for music ----
    Rectangle {
        objectName: "musyOverlay"
        anchors.centerIn: parent
        visible: game.state === 0 || game.paused || game.state === 2 || root.needMusic
        width: Math.min(parent.width - 40, 520); height: msgCol.implicitHeight + 36
        radius: 12; color: Qt.rgba(0.05, 0.05, 0.09, 0.88); border { width: 1; color: Qt.rgba(1, 1, 1, 0.2) }
        ColumnLayout {
            id: msgCol
            anchors { fill: parent; margins: 18 }
            spacing: 8
            Label {
                Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; color: "white"
                font { family: Theme.font; pixelSize: 30; weight: Font.Light }
                text: game.state === 2 ? "game over" : (root.needMusic ? "play a song to start" : (game.paused ? "paused" : "musy note"))
            }
            Label {
                Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; color: Theme.text2
                font { family: Theme.font; pixelSize: 16 }
                text: game.state === 2
                      ? (game.score + " points" + (!game.scored ? " (not counted: the song was playing)" : (root.rank === 1 ? ", a new best!" : (root.rank > 0 ? ", #" + root.rank + " on your list" : ""))))
                      : (root.needMusic ? "the song makes the walls: start one first"
                                        : (game.paused ? "tap, space or start to go on"
                                                       : "tap, press space or any button on your controller to fly. the walls come on the beat."))
            }
            PlainButton { Layout.alignment: Qt.AlignHCenter; visible: root.needMusic; text: "play"; primary: true
                          onClicked: hop.player.hasTrack ? hop.player.toggle() : hop.playSongs(0) }
            PlainButton { Layout.alignment: Qt.AlignHCenter; visible: game.state === 2; text: "play again"; primary: true; onClicked: root.newGame() }
        }
    }
}
