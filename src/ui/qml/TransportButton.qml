// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import "Theme.js" as Theme

// Icon button for prev / play / pause / next. Icons are drawn, so no icon font is needed.
AbstractButton {
    id: root
    property string icon: "play"          // prev | next | play | pause | shuffle
    property bool round: false
    property real iconSize: 18
    implicitWidth: round ? 48 : 44
    implicitHeight: implicitWidth
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: icon

    background: Rectangle {
        radius: root.round ? width / 2 : 0
        color: root.round ? "transparent" : (root.hovered ? Theme.hover : "transparent")
        border.width: root.round ? 2 : (root.visualFocus ? 1 : 0)
        border.color: root.round && root.hovered ? "#ffffff" : Theme.text
    }

    contentItem: Canvas {
        id: c
        property string kind: root.icon
        onKindChanged: requestPaint()
        onPaint: {
            var g = getContext("2d")
            g.reset()
            var s = root.iconSize, x0 = (width - s) / 2, y0 = (height - s) / 2
            g.strokeStyle = Theme.text; g.fillStyle = Theme.text
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
            }
        }
    }
}
