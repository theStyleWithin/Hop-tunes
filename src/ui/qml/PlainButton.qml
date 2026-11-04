// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// Outlined text button used on the profile and settings screens.
Button {
    id: b
    property bool primary: false
    property bool danger: false
    hoverEnabled: true
    Accessible.name: text
    contentItem: Label {
        text: b.text
        color: !b.enabled ? Theme.dim : (b.primary ? Theme.ink : (b.danger ? Theme.danger : Theme.text))
        font { family: Theme.font; pixelSize: Theme.fs(16) }
        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        implicitWidth: Math.max(120, b.contentItem.implicitWidth + 36); implicitHeight: 44
        color: b.primary ? (b.enabled ? Theme.accent : Theme.line) : (b.hovered ? Theme.hover : "transparent")
        border { width: b.primary ? 0 : 2; color: b.danger ? Theme.dangerLine : Theme.border }
    }
}
