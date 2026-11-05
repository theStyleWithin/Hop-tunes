// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// The music made visible. Bars, a mirrored skyline or rings, in your accent colour, a rainbow or white, with a
// sensitivity you choose. It only listens (hop.audio) while something shows it.
Item {
    id: root
    property string style: "bars"        // bars | mirror | rings
    property string colorMode: "accent"  // accent | rainbow | white
    property real intensity: 1.0         // 0.3 .. 2
    property real opacityScale: 1.0      // a quieter one behind a game
    readonly property var bands: hop.audio.bands
    readonly property int n: 32
    opacity: opacityScale

    function tint(i) {
        if (colorMode === "rainbow") return Qt.hsla(0.02 + 0.8 * i / n, 0.75, 0.58, 1)
        if (colorMode === "white") return Qt.rgba(1, 1, 1, 0.85)
        return Theme.accent
    }
    // a band's height 0..1: a little curve so quiet parts still move
    function amp(i) {
        const v = bands.length > i ? bands[i] : 0
        return Math.min(1, Math.pow(v, 1.6) * 1.35 * intensity)
    }

    Item {   // bars and mirror
        anchors.fill: parent
        visible: root.style !== "rings"
        Repeater {
            model: root.n
            Rectangle {
                required property int index
                readonly property real a: root.amp(index)
                width: parent.width / root.n * 0.72
                x: index * parent.width / root.n + (parent.width / root.n - width) / 2
                height: Math.max(3, a * (root.style === "mirror" ? parent.height * 0.5 : parent.height * 0.9))
                y: root.style === "mirror" ? (parent.height - height) / 2 : parent.height - height
                radius: width / 2
                color: root.tint(index)
                opacity: 0.35 + 0.65 * a
            }
        }
    }

    Item {   // rings: bass, mid and treble each get a ring; every beat sends a wave out
        anchors.fill: parent
        visible: root.style === "rings"
        readonly property real m: Math.min(width, height)
        Repeater {
            model: 3
            Rectangle {
                required property int index
                readonly property real level: index === 0 ? hop.audio.bass : (index === 1 ? hop.audio.mid : hop.audio.treble)
                anchors.centerIn: parent
                width: parent.m * (0.22 + 0.22 * index + 0.26 * Math.min(1, level * root.intensity))
                height: width; radius: width / 2
                color: "transparent"
                border { width: 3 + 7 * Math.min(1, level * root.intensity); color: root.tint(index * 10) }
                opacity: 0.35 + 0.55 * Math.min(1, level * root.intensity)
            }
        }
        Rectangle {
            id: wave
            anchors.centerIn: parent
            property real p: 1
            width: parent.m * (0.2 + 0.9 * p); height: width; radius: width / 2
            color: "transparent"
            border { width: 2; color: root.tint(16) }
            opacity: (1 - p) * 0.7
            visible: opacity > 0.01
            NumberAnimation on p { id: waveAnim; from: 0; to: 1; duration: Theme.reduceMotion ? 1 : 700; running: false }
        }
        Connections { target: hop.audio; function onBeat() { waveAnim.restart() } }
    }
}
