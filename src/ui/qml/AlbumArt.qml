// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import "Theme.js" as Theme

// Album cover: the real image when there is one, otherwise a two-colour placeholder
// derived from the album name (so every album still has a recognisable tile).
Item {
    id: root
    property string key: ""          // album name, for the placeholder colours
    property string source: ""       // file:// URL of the cover image, or ""
    readonly property var colors: Theme.artColors(key)
    readonly property bool hasImage: img.status === Image.Ready
    implicitWidth: 48
    implicitHeight: 48

    Canvas {
        id: cv
        anchors.fill: parent
        visible: !root.hasImage
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
}
