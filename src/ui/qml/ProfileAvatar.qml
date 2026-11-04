// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// A profile's picture: its avatar image, or its initial on its accent colour.
Rectangle {
    id: root
    property var profile: ({})
    property int size: 48
    property bool round: false             // phones: a circle
    property color cornerColor: Theme.bg   // what shows outside the circle
    implicitWidth: size; implicitHeight: size
    width: size; height: size
    color: profile.accent || "#4fd1c5"

    Label {
        anchors.centerIn: parent
        visible: !img.visible
        text: root.profile.initial || "?"
        color: Theme.ink
        font { family: Theme.font; pixelSize: Math.round(root.size * 0.5); weight: Font.Medium }
    }
    Image {
        id: img
        anchors.fill: parent
        visible: status === Image.Ready
        source: root.profile.avatar || ""
        sourceSize { width: 256; height: 256 }
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: false
    }
    Canvas {   // the circle
        id: mask
        anchors.fill: parent
        visible: root.round
        onPaint: {
            var g = getContext("2d")
            g.reset()
            g.fillStyle = root.cornerColor
            g.fillRect(0, 0, width, height)
            g.globalCompositeOperation = "destination-out"
            g.beginPath(); g.arc(width / 2, height / 2, Math.min(width, height) / 2, 0, 2 * Math.PI); g.fill()
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Connections { target: root; function onCornerColorChanged() { mask.requestPaint() } }
    }
}
