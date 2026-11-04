// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// Album cover: the real image when there is one, otherwise a two-colour placeholder
// derived from the album name (so every album still has a recognisable tile).
Item {
    id: root
    property string key: ""          // album name, for the placeholder colours
    property string source: ""       // file:// URL of the cover image, or ""
    readonly property var colors: Theme.artColors(key)
    readonly property bool hasImage: img.status === Image.Ready
    property real radius: Theme.compact ? 6 : 0   // phones: softly rounded covers
    property color cornerColor: Theme.bg          // what shows behind the rounded corners
    implicitWidth: 48
    implicitHeight: 48

    Canvas {
        id: cv
        anchors.fill: parent
        visible: !root.hasImage
        onPaint: {
            var ctx = getContext("2d")
            if (Theme.compact) {   // phones: a soft diagonal blend of the same two colours
                var gr = ctx.createLinearGradient(0, 0, width, height)
                gr.addColorStop(0, root.colors[0]); gr.addColorStop(1, root.colors[1])
                ctx.fillStyle = gr
                ctx.fillRect(0, 0, width, height)
                return
            }
            ctx.fillStyle = root.colors[1]
            ctx.fillRect(0, 0, width, height)
            ctx.fillStyle = root.colors[0]
            ctx.beginPath()
            ctx.moveTo(0, 0); ctx.lineTo(width, 0); ctx.lineTo(0, height)
            ctx.closePath()
            ctx.fill()
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Connections { target: root; function onColorsChanged() { cv.requestPaint() } }
    }

    Image {
        id: img
        anchors.fill: parent
        source: root.source
        asynchronous: true
        cache: true
        fillMode: Image.PreserveAspectCrop
        // Decode at display size (x2 for HiDPI) so big covers stay cheap in memory.
        sourceSize.width: Math.ceil(root.width * 2)
        sourceSize.height: Math.ceil(root.height * 2)
        smooth: true
        mipmap: true
    }

    Canvas {   // the four rounded corners
        id: corners
        anchors.fill: parent
        visible: root.radius > 0
        onPaint: {
            var g = getContext("2d"), r = root.radius, w = width, h = height
            g.reset()
            g.fillStyle = root.cornerColor
            var P = Math.PI
            g.beginPath(); g.moveTo(0, 0); g.lineTo(r, 0); g.arc(r, r, r, 1.5 * P, P, true); g.closePath(); g.fill()
            g.beginPath(); g.moveTo(w, 0); g.lineTo(w, r); g.arc(w - r, r, r, 0, 1.5 * P, true); g.closePath(); g.fill()
            g.beginPath(); g.moveTo(w, h); g.lineTo(w - r, h); g.arc(w - r, h - r, r, 0.5 * P, 0, true); g.closePath(); g.fill()
            g.beginPath(); g.moveTo(0, h); g.lineTo(0, h - r); g.arc(r, h - r, r, P, 0.5 * P, true); g.closePath(); g.fill()
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Connections { target: root; function onCornerColorChanged() { corners.requestPaint() } function onRadiusChanged() { corners.requestPaint() } }
    }
}
