// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// neko-cafe: a top-down cafe game. You are a neko maid: take orders, hand the slip to the cook, fetch the coffee and serve the
// right customer, fast. The rules live in game/NekoCafe.cpp (ported from the Game Boy original); this only draws and listens.
// Arrow keys or WASD walk, Space / Enter / Z is A, Esc pauses (and again leaves). Touch: the pad under the picture.
FocusScope {
    id: root
    objectName: "nekoCafeApp"
    readonly property var game: hop.neko
    signal leave()
    focus: true

    property int rank: 0
    readonly property bool running: game.started && !game.paused
    readonly property real s: Math.max(1, Math.floor(Math.min(stage.width / 160, stage.height / 144)))   // whole pixels per game pixel: crisp art
    readonly property var best: { hop.gameSettings; const l = hop.gameScores("nekocafe"); return l.length > 0 ? l[0].score : 0 }

    function money(cents) { return "$" + Math.floor(cents / 100) + "." + ("0" + (cents % 100)).slice(-2) }
    function goBack() {                          // Esc and the back button: pause first, then leave
        if (game.started && game.phase === 0 && !game.paused) { game.paused = true; return true }
        if (game.paused) { game.paused = false; return false }
        return false
    }
    function press() {                           // A on the title and results screens starts; in the game it is the game's key
        if (!game.started) { rank = 0; game.start(); return true }
        if (game.phase === 1) { if (game.resultsReady) { rank = 0; game.again() } return true }
        if (game.paused) { game.paused = false; return true }
        return false
    }
    Component.onDestruction: { game.quit(); game.open = false }

    Connections {
        target: game
        function onFinished(profit, served, happy) { root.rank = hop.submitScore("nekocafe", profit, served) }
    }
    Connections {
        target: hop.gamepad
        function onPressed(a) {
            if (["left", "right", "up", "down"].indexOf(a) >= 0) game.setKey(a, true)
            else if (a === "a" || a === "start") { if (!root.press()) game.setKey("a", true) }
            else if (a === "back") { if (!root.goBack()) root.leave() }
        }
        function onReleased(a) { if (["left", "right", "up", "down", "a"].indexOf(a) >= 0) game.setKey(a, false) }
    }
    Component.onCompleted: { game.open = true; hop.gamepad.setListening(true); Qt.callLater(function () { root.forceActiveFocus() }) }
    onActiveFocusChanged: if (!activeFocus) game.releaseKeys()

    readonly property var keyMap: ({ [Qt.Key_Left]: "left", [Qt.Key_A]: "left", [Qt.Key_Right]: "right", [Qt.Key_D]: "right",
                                     [Qt.Key_Up]: "up", [Qt.Key_W]: "up", [Qt.Key_Down]: "down", [Qt.Key_S]: "down",
                                     [Qt.Key_Space]: "a", [Qt.Key_Return]: "a", [Qt.Key_Enter]: "a", [Qt.Key_Z]: "a" })
    Keys.onPressed: (e) => {
        const k = keyMap[e.key]
        if (!k || e.isAutoRepeat) return
        e.accepted = true
        if (k === "a" && root.press()) return
        game.setKey(k, true)
    }
    Keys.onReleased: (e) => { const k = keyMap[e.key]; if (k && !e.isAutoRepeat) { game.setKey(k, false); e.accepted = true } }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Item {
            id: stage
            objectName: "nekoStage"
            Layout.fillWidth: true; Layout.fillHeight: true

            Rectangle { anchors.fill: parent; radius: 10; color: "#0f380f" }

            // the sprites, one 8x16 column of the sheet each; mirrored ones face left
            component Sprite: Item {
                id: sp
                property int col: 0
                property bool flip: false
                width: 8 * root.s; height: 16 * root.s
                Image {
                    anchors.fill: parent
                    source: "qrc:/images/neko-cafe-sprites.png"
                    sourceClipRect: Qt.rect(sp.col * 8, 0, 8, 16)
                    smooth: false
                    mirror: sp.flip
                }
            }
            // a 16x16 character: frame f is two sprites side by side; facing left swaps the halves and flips each
            component Character: Item {
                id: ch
                property int frame: 0
                property bool flip: false
                width: 16 * root.s; height: 16 * root.s
                Sprite { x: 0; col: ch.frame * 2 + (ch.flip ? 1 : 0); flip: ch.flip }
                Sprite { x: 8 * root.s; col: ch.frame * 2 + (ch.flip ? 0 : 1); flip: ch.flip }
            }

            Item {
                id: screen
                objectName: "nekoScreen"
                width: 160 * root.s; height: 144 * root.s
                anchors.centerIn: parent
                clip: true
                readonly property int fx: 31      // the sheet's columns for the little pictures above heads
                function px(v) { return v * root.s }

                Image { anchors.fill: parent; source: "qrc:/images/neko-cafe-bg.png"; smooth: false }
                // the day: a bar along the top wall that fills as the day goes by
                Rectangle { x: screen.px(16); y: screen.px(1); width: screen.px(128); height: screen.px(6); color: "#306230"
                    Rectangle { width: parent.width * game.dayFraction; height: parent.height; color: "#9bbc0f" } }

                Repeater {   // order bubbles ("I ORDER A COFFEE!") sit on the floor, under everybody
                    model: game.frame >= 0 ? game.customers : []
                    Image { required property var modelData
                            visible: modelData.state === 3
                            x: screen.px(modelData.bubbleX); y: screen.px(modelData.bubbleY); width: screen.px(40); height: screen.px(16)
                            source: "qrc:/images/neko-cafe-bubble.png"; smooth: false }
                }
                Repeater {   // what is above each customer's head
                    model: game.frame >= 0 ? game.customers : []
                    Sprite { required property var modelData
                             readonly property int icon: modelData.state === 4 ? (modelData.taken ? 37 : 36)
                                                         : (modelData.state === 5 || modelData.state === 6) ? (modelData.mood === 1 ? 34 : modelData.mood === 2 ? 35 : -1) : -1
                             visible: icon >= 0; col: Math.max(0, icon)
                             x: screen.px(modelData.x); y: screen.px(modelData.y - 14) }
                }
                Repeater {   // the customers
                    model: game.frame >= 0 ? game.customers : []
                    Sprite { required property var modelData
                             readonly property int tile: {
                                 const m = modelData
                                 if (m.state === 3 || m.state === 5) return 52
                                 if (m.state === 2 || m.state === 4) return m.activity === 1 ? 54 + 2 * m.step : m.activity === 2 ? 58 + 2 * m.step : 52
                                 if (m.dir === 1) return 44 + 2 * m.step
                                 if (m.dir === 0) return 40 + 2 * m.step
                                 return 48 + 2 * m.step
                             }
                             col: tile / 2; flip: modelData.state !== 2 && modelData.state !== 3 && modelData.state !== 4 && modelData.state !== 5 && modelData.dir === 3
                             x: screen.px(modelData.x); y: screen.px(modelData.y) }
                }
                Character {   // the cook
                    x: screen.px(game.cookX); y: screen.px(32)
                    frame: game.cookState === 1 || game.cookState === 2 ? 8 + game.cookStep : game.cookState === 3 ? 7 : 6
                    flip: (game.cookState === 1 || game.cookState === 2) && game.cookVx < 0
                }
                Character {   // the maid
                    objectName: "nekoMaid"
                    x: screen.px(game.maidX); y: screen.px(game.maidY)
                    frame: game.maidDir === 1 ? 2 + game.maidStep : game.maidDir === 0 ? game.maidStep : 4 + game.maidStep
                    flip: game.maidDir === 3
                }
                // above the maid: "!" for a moment after the pickup, otherwise the coffee she holds and the slip in her pocket
                Sprite { visible: game.exclaim; col: 31; x: screen.px(game.maidX + 4); y: screen.px(game.maidY - 14) }
                Sprite { visible: !game.exclaim && game.holdsFood; col: 33; x: screen.px(game.maidX + (game.holdsTicket ? 0 : 4)); y: screen.px(game.maidY - 14) }
                Sprite { visible: !game.exclaim && game.holdsTicket; col: 32; x: screen.px(game.maidX + (game.holdsFood ? 8 : 4)); y: screen.px(game.maidY - 14) }

                // ---- the title
                Rectangle {
                    objectName: "nekoTitle"
                    anchors.fill: parent; visible: !game.started; color: "#9bbc0f"
                    Column {
                        anchors.centerIn: parent; spacing: screen.px(5)
                        Character { anchors.horizontalCenter: parent.horizontalCenter; frame: 0 }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "NEKO CAFE"; color: "#0f380f"; font { family: "monospace"; bold: true; pixelSize: screen.px(14) } }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; horizontalAlignment: Text.AlignHCenter; color: "#306230"
                               text: "walk: arrows or wasd\nA: space or enter"; font { family: "monospace"; pixelSize: screen.px(8) } }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; color: "#0f380f"; text: "PRESS A TO OPEN"; font { family: "monospace"; bold: true; pixelSize: screen.px(9) } }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; visible: root.best > 0; color: "#306230"
                               text: "BEST PROFIT " + root.money(root.best); font { family: "monospace"; pixelSize: screen.px(8) } }
                    }
                    MouseArea { anchors.fill: parent; onClicked: root.press() }
                }
                // ---- paused
                Rectangle {
                    objectName: "nekoPaused"
                    anchors.fill: parent; visible: game.paused && game.started; color: "#cc0f380f"
                    Text { anchors.centerIn: parent; horizontalAlignment: Text.AlignHCenter; color: "#9bbc0f"
                           text: "PAUSED\nESC AGAIN TO LEAVE\nA TO CONTINUE"; font { family: "monospace"; bold: true; pixelSize: screen.px(9) } }
                    MouseArea { anchors.fill: parent; onClicked: game.paused = false }
                }
                // ---- the day's results
                Rectangle {
                    objectName: "nekoResults"
                    anchors.fill: parent; visible: game.started && game.phase === 1; color: "#9bbc0f"
                    Rectangle { x: 0; y: 0; width: parent.width; height: screen.px(8); color: "#306230" }
                    Rectangle { x: 0; y: parent.height - screen.px(8); width: parent.width; height: screen.px(8); color: "#306230" }
                    component Line: Item {
                        property string label; property string value; property real row: 0
                        x: screen.px(8); y: screen.px(row * 8); width: screen.px(144); height: screen.px(8)
                        Text { text: parent.label; color: "#0f380f"; font { family: "monospace"; pixelSize: screen.px(8) } }
                        Text { anchors.right: parent.right; text: parent.value; color: "#0f380f"; font { family: "monospace"; pixelSize: screen.px(8) } }
                    }
                    Text { x: screen.px(16); y: screen.px(16); text: "DAY " + game.day + " COMPLETE!"; color: "#0f380f"; font { family: "monospace"; bold: true; pixelSize: screen.px(10) } }
                    Line { row: 4; label: "ORDERS SERVED"; value: game.served }
                    Line { row: 5; label: "HAPPY CUSTOMERS"; value: game.happy }
                    Line { row: 6; label: "UPSET CUSTOMERS"; value: game.upset }
                    Line { row: 8; label: "BILLS"; value: root.money(game.bills) }
                    Line { row: 9; label: "TIPS"; value: root.money(game.tips) }
                    Line { row: 10; label: "PROFIT"; value: root.money(game.bills + game.tips) }
                    Line { row: 12; label: "HAPPINESS"; value: game.happinessPct + "%" }
                    Line { row: 13; label: "RATING"; value: game.grade }
                    Text { x: screen.px(8); y: screen.px(14.5 * 8); visible: root.rank > 0; color: "#306230"
                           text: root.rank === 1 ? "NEW BEST PROFIT!" : "BEST LIST: #" + root.rank; font { family: "monospace"; pixelSize: screen.px(8) } }
                    Text { x: screen.px(8); y: screen.px(15.5 * 8); visible: game.resultsReady; color: "#0f380f"
                           text: "PRESS A FOR THE NEXT DAY"; font { family: "monospace"; bold: true; pixelSize: screen.px(8) } }
                    MouseArea { anchors.fill: parent; onClicked: root.press() }
                }
            }
        }

        // touch: a pad and the A button (phones)
        RowLayout {
            objectName: "nekoPad"
            visible: Theme.compact
            Layout.fillWidth: true; Layout.preferredHeight: 150
            Item { Layout.fillWidth: true }
            component PadKey: Rectangle {
                id: pk
                property string key; property string glyph
                width: 52; height: 52; radius: 26; color: kp.pressed ? Theme.accent : "transparent"; border { width: 2; color: Theme.border }
                Label { anchors.centerIn: parent; text: pk.glyph; color: kp.pressed ? Theme.ink : Theme.text; font.pixelSize: Theme.fs(22) }
                MouseArea { id: kp; anchors.fill: parent; onPressed: if (pk.key !== "a" || !root.press()) game.setKey(pk.key, true); onReleased: game.setKey(pk.key, false); onCanceled: game.setKey(pk.key, false) }
            }
            Item {
                width: 160; height: 150
                PadKey { x: 54; y: 0; key: "up"; glyph: "▲" }
                PadKey { x: 0; y: 49; key: "left"; glyph: "◀" }
                PadKey { x: 108; y: 49; key: "right"; glyph: "▶" }
                PadKey { x: 54; y: 98; key: "down"; glyph: "▼" }
            }
            Item { Layout.preferredWidth: 30 }
            PadKey { key: "a"; glyph: "A"; width: 78; height: 78; radius: 39 }
            Item { Layout.fillWidth: true }
        }
    }
}
