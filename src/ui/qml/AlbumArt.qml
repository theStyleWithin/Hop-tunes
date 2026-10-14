// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import "Theme.js" as Theme

// Placeholder cover: two colour triangles chosen from the album name.
Item {
    id: root
    property string key: ""
    readonly property var colors: Theme.artColors(key)
    implicitWidth: 48
    implicitHeight: 48

    Canvas {
        id: cv
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d")
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
}
