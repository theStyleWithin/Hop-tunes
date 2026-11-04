// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// A small line icon drawn with a Canvas, so it looks the same on every phone: the symbols some phones' fonts
// lack (⌕, ⟳, ⌄) show up there as empty boxes.
Canvas {
    id: root
    property string kind: "note"          // note | now | search | person | down | rotate
    property color ink: "white"
    property real stroke: 2
    implicitWidth: 24; implicitHeight: 24
    onKindChanged: requestPaint()
    onInkChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        var g = getContext("2d")
        g.reset()
        var w = width, h = height, s = Math.min(w, h), x0 = (w - s) / 2, y0 = (h - s) / 2
        g.strokeStyle = ink; g.fillStyle = ink
        g.lineWidth = root.stroke; g.lineCap = "round"; g.lineJoin = "round"
        function P(a, b) { return [x0 + s * a, y0 + s * b] }
        g.beginPath()
        if (kind === "note") {            // a beamed eighth note
            var p = P(0.38, 0.78); g.arc(p[0], p[1], s * 0.14, 0, 2 * Math.PI); g.fill()
            g.beginPath(); var a = P(0.52, 0.76), b = P(0.52, 0.2), c = P(0.78, 0.28)
            g.moveTo(a[0], a[1]); g.lineTo(b[0], b[1]); g.lineTo(c[0], c[1]); g.stroke()
        } else if (kind === "now") {      // a ring with a dot
            var q = P(0.5, 0.5); g.arc(q[0], q[1], s * 0.38, 0, 2 * Math.PI); g.stroke()
            g.beginPath(); g.arc(q[0], q[1], s * 0.14, 0, 2 * Math.PI); g.fill()
        } else if (kind === "search") {   // a lens and its handle
            var l = P(0.44, 0.44); g.arc(l[0], l[1], s * 0.26, 0, 2 * Math.PI); g.stroke()
            g.beginPath(); var m = P(0.64, 0.64), n = P(0.84, 0.84); g.moveTo(m[0], m[1]); g.lineTo(n[0], n[1]); g.stroke()
        } else if (kind === "person") {   // head and shoulders
            var hd = P(0.5, 0.34); g.arc(hd[0], hd[1], s * 0.17, 0, 2 * Math.PI); g.stroke()
            g.beginPath(); var sh = P(0.5, 0.98); g.arc(sh[0], sh[1], s * 0.36, 1.18 * Math.PI, 1.82 * Math.PI); g.stroke()
        } else if (kind === "down") {     // a chevron pointing down
            var d1 = P(0.2, 0.36), d2 = P(0.5, 0.66), d3 = P(0.8, 0.36)
            g.moveTo(d1[0], d1[1]); g.lineTo(d2[0], d2[1]); g.lineTo(d3[0], d3[1]); g.stroke()
        } else if (kind === "rotate") {   // a circular arrow
            var r = P(0.5, 0.5); g.arc(r[0], r[1], s * 0.32, 0.25 * Math.PI, 1.85 * Math.PI); g.stroke()
            g.beginPath(); var t1 = P(0.86, 0.18), t2 = P(0.84, 0.44), t3 = P(0.6, 0.38)
            g.moveTo(t1[0], t1[1]); g.lineTo(t2[0], t2[1]); g.lineTo(t3[0], t3[1]); g.stroke()
        }
    }
}
