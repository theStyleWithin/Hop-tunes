// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import "Theme.js" as Theme

// Outlined text button used on the profile and settings screens.
Button {
    id: b
    property bool primary: false
    property bool danger: false
    hoverEnabled: true
    Accessible.name: text
    contentItem: Label {
        text: b.text
        color: !b.enabled ? Theme.dim : (b.primary ? Theme.ink : (b.danger ? "#ff8a8a" : Theme.text))
        font { family: Theme.font; pixelSize: 16 }
        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        implicitWidth: Math.max(120, b.contentItem.implicitWidth + 36); implicitHeight: 44
        color: b.primary ? (b.enabled ? hop.accent : Theme.line) : (b.hovered ? Theme.hover : "transparent")
        border { width: b.primary ? 0 : 2; color: b.danger ? "#7a3a3a" : "#5b5b63" }
    }
}
