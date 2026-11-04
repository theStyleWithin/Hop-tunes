// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// ♥ toggle. Hearts are per profile and feed the "hearted" list and the smart DJ.
AbstractButton {
    id: b
    property bool on: false
    property int size: 18
    hoverEnabled: true
    implicitWidth: size + 12; implicitHeight: size + 12
    Accessible.name: on ? "remove heart" : "heart"
    ToolTip.visible: hovered; ToolTip.delay: 700; ToolTip.text: on ? "hearted" : "heart this song"
    contentItem: Label {
        text: b.on ? "♥" : "♡"
        color: b.on ? Theme.accent : (b.hovered ? Theme.text : Theme.dim)
        font { pixelSize: b.size }
        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
    }
    background: Item { }
}
