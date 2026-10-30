// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// Who's listening: the profile's name and picture; click to switch profile (playback stops, no restart).
// It is shown in the top bar, and in the collection's tab row while that view is open.
AbstractButton {
    id: chip
    property int avatarSize: 40
    // only the one on screen answers to this name
    objectName: visible ? "profileChip" : "profileChipHidden"
    hoverEnabled: true
    onClicked: hop.leaveProfile()
    Accessible.name: "switch profile (" + (hop.profile.name || "") + ")"
    ToolTip.visible: hovered; ToolTip.delay: 600; ToolTip.text: "switch profile"
    contentItem: Row {
        spacing: 12
        Label {
            anchors.verticalCenter: parent.verticalCenter
            text: hop.profile.name || ""
            color: chip.hovered ? Theme.text : Theme.muted
            font { family: Theme.font; pixelSize: 18 }
        }
        ProfileAvatar { profile: hop.profile; size: chip.avatarSize }
    }
    background: Item { }
}
