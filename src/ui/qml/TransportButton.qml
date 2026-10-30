// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// Icon button for the transport controls. Icons are drawn, so no icon font is needed.
// `active` tints the icon with the accent (shuffle on, repeat on).
AbstractButton {
    id: root
    property string glyph: "play"         // prev | next | play | pause | shuffle | repeat | repeat1 | volume | mute
    property bool active: false
    property bool round: false
    property bool filled: false           // a solid accent circle with a dark icon (the phone's big play button)
    property real iconSize: 18
    implicitWidth: round ? 48 : 44
    implicitHeight: implicitWidth
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: glyph

    background: Rectangle {
        radius: root.round ? width / 2 : 0
        color: root.filled ? hop.accent : (root.round ? "transparent" : (root.hovered ? Theme.hover : "transparent"))
        border.width: root.filled ? 0 : (root.round ? 2 : (root.visualFocus ? 1 : 0))
        border.color: root.round && root.hovered ? Theme.strong : Theme.text
    }

    contentItem: Canvas {
        id: c
        property string kind: root.glyph
        property color ink: root.filled ? Theme.ink : (root.active ? hop.accent : (root.enabled ? Theme.text : Theme.dim))
        onKindChanged: requestPaint()
        onInkChanged: requestPaint()
        onPaint: {
            var g = getContext("2d")
            g.reset()
            var s = root.iconSize, x0 = (width - s) / 2, y0 = (height - s) / 2
            g.strokeStyle = ink; g.fillStyle = ink
            g.lineWidth = 2; g.lineJoin = "round"
            g.beginPath()
            if (kind === "play") {
                g.moveTo(x0 + s * 0.2, y0); g.lineTo(x0 + s, y0 + s / 2); g.lineTo(x0 + s * 0.2, y0 + s); g.closePath(); g.fill()
            } else if (kind === "pause") {
                g.fillRect(x0 + s * 0.18, y0, s * 0.2, s); g.fillRect(x0 + s * 0.62, y0, s * 0.2, s)
            } else if (kind === "prev") {
                g.moveTo(x0 + s, y0 + s * 0.1); g.lineTo(x0 + s * 0.3, y0 + s / 2); g.lineTo(x0 + s, y0 + s * 0.9); g.closePath(); g.stroke()
                g.beginPath(); g.moveTo(x0 + s * 0.15, y0 + s * 0.1); g.lineTo(x0 + s * 0.15, y0 + s * 0.9); g.stroke()
            } else if (kind === "next") {
                g.moveTo(x0, y0 + s * 0.1); g.lineTo(x0 + s * 0.7, y0 + s / 2); g.lineTo(x0, y0 + s * 0.9); g.closePath(); g.stroke()
                g.beginPath(); g.moveTo(x0 + s * 0.85, y0 + s * 0.1); g.lineTo(x0 + s * 0.85, y0 + s * 0.9); g.stroke()
            } else if (kind === "shuffle") {
                g.moveTo(x0, y0 + s * 0.25); g.lineTo(x0 + s * 0.3, y0 + s * 0.25); g.lineTo(x0 + s * 0.7, y0 + s * 0.75); g.lineTo(x0 + s, y0 + s * 0.75)
                g.moveTo(x0, y0 + s * 0.75); g.lineTo(x0 + s * 0.3, y0 + s * 0.75); g.lineTo(x0 + s * 0.7, y0 + s * 0.25); g.lineTo(x0 + s, y0 + s * 0.25)
                g.stroke()
            } else if (kind === "repeat" || kind === "repeat1") {
                // a rounded loop with an arrowhead
                g.moveTo(x0 + s * 0.15, y0 + s * 0.55); g.lineTo(x0 + s * 0.15, y0 + s * 0.3); g.lineTo(x0 + s * 0.85, y0 + s * 0.3)
                g.moveTo(x0 + s * 0.85, y0 + s * 0.45); g.lineTo(x0 + s * 0.85, y0 + s * 0.7); g.lineTo(x0 + s * 0.15, y0 + s * 0.7)
                g.moveTo(x0 + s * 0.7, y0 + s * 0.15); g.lineTo(x0 + s * 0.85, y0 + s * 0.3); g.lineTo(x0 + s * 0.7, y0 + s * 0.45)
                g.stroke()
                if (kind === "repeat1") {
                    g.font = Math.round(s * 0.42) + "px sans-serif"; g.textAlign = "center"; g.textBaseline = "middle"
                    g.fillText("1", x0 + s * 0.5, y0 + s * 0.52)
                }
            } else if (kind === "queue") {   // three lines: the up-next list
                for (var q = 0; q < 3; ++q) { g.moveTo(x0, y0 + s * (0.2 + 0.3 * q)); g.lineTo(x0 + s, y0 + s * (0.2 + 0.3 * q)) }
                g.lineWidth = 2.5; g.stroke()
            } else if (kind === "volume" || kind === "mute") {
                g.moveTo(x0, y0 + s * 0.35); g.lineTo(x0 + s * 0.25, y0 + s * 0.35); g.lineTo(x0 + s * 0.5, y0 + s * 0.1)
                g.lineTo(x0 + s * 0.5, y0 + s * 0.9); g.lineTo(x0 + s * 0.25, y0 + s * 0.65); g.lineTo(x0, y0 + s * 0.65); g.closePath(); g.fill()
                g.beginPath()
                if (kind === "mute") {
                    g.moveTo(x0 + s * 0.65, y0 + s * 0.35); g.lineTo(x0 + s * 0.95, y0 + s * 0.65)
                    g.moveTo(x0 + s * 0.95, y0 + s * 0.35); g.lineTo(x0 + s * 0.65, y0 + s * 0.65)
                } else {
                    g.arc(x0 + s * 0.5, y0 + s * 0.5, s * 0.25, -0.8, 0.8)
                    g.moveTo(x0 + s * 0.5 + s * 0.42 * Math.cos(-0.8), y0 + s * 0.5 + s * 0.42 * Math.sin(-0.8))
                    g.arc(x0 + s * 0.5, y0 + s * 0.5, s * 0.42, -0.8, 0.8)
                }
                g.stroke()
            }
        }
    }
}
