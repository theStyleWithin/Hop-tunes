// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// A profile's picture: its avatar image, or its initial on its accent colour.
Rectangle {
    id: root
    property var profile: ({})
    property int size: 48
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
}
