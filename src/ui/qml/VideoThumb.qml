// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// Video tile (spec §5b): the frame-grab thumbnail when there is one, otherwise a
// placeholder derived from the title (same idea as AlbumArt), plus a badge row.
Item {
    id: root
    property string key: ""
    property string source: ""
    property string resolutionBadge: ""
    property string codecBadge: ""
    property bool hdr: false
    property bool hovered: false
    readonly property var colors: Theme.artColors(key)
    readonly property bool hasImage: img.status === Image.Ready
    implicitWidth: 160
    implicitHeight: 90

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
    }

    Image {
        id: img
        anchors.fill: parent
        source: root.source
        asynchronous: true
        cache: true
        fillMode: Image.PreserveAspectCrop
        sourceSize.width: Math.ceil(root.width * 1.5)
        sourceSize.height: Math.ceil(root.height * 1.5)
        smooth: true
    }

    // play glyph, shown on hover by the parent delegate via opacity binding on this item's parent
    Rectangle {
        anchors.centerIn: parent
        width: 40; height: 40; radius: 20
        color: Theme.fade(0.55)
        visible: root.hovered
        Label { anchors.centerIn: parent; text: "▶"; color: "white"; font.pixelSize: Theme.fs(16) }
    }

    Row {
        anchors { left: parent.left; bottom: parent.bottom; margins: 4 }
        spacing: 3
        Repeater {
            model: [root.resolutionBadge, root.hdr ? "HDR" : "", root.codecBadge].filter(function (s) { return s !== "" })
            delegate: Rectangle {
                required property string modelData
                radius: 3; color: Theme.fade(0.7)
                width: lbl.implicitWidth + 8; height: 16
                Label { id: lbl; anchors.centerIn: parent; text: modelData; color: "white"
                        font { family: Theme.font; pixelSize: Theme.fs(10); weight: Font.Medium } }
            }
        }
    }
}
