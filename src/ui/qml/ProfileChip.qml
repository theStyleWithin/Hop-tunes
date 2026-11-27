// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// Who's listening: the profile's name and picture; click to switch profile (playback stops, no restart). A right click (or a long press on a touch
// screen) opens a small menu: switch, eject (not for a guest) and quick settings (just the settings page).
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
    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight
        Row {
            id: row
            anchors.verticalCenter: parent.verticalCenter
            spacing: 12
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: hop.profile.name || ""
                color: chip.hovered ? Theme.text : Theme.muted
                font { family: Theme.font; pixelSize: Theme.fs(18) }
            }
            ProfileAvatar { profile: hop.profile; size: chip.avatarSize }
        }
    }
    background: Item { }

    MouseArea { anchors.fill: parent; acceptedButtons: Qt.RightButton; onClicked: chipMenu.popup(chip.width - 220, chip.height + 4) }
    TapHandler { acceptedDevices: PointerDevice.TouchScreen; onLongPressed: chipMenu.popup(chip.width - 220, chip.height + 4) }
    Menu {
        id: chipMenu
        objectName: visible ? "chipMenu" : "chipMenuClosed"
        background: Rectangle { implicitWidth: 220; color: Theme.panel; border { width: 1; color: Theme.line } }
        MenuItem {
            objectName: "chipMenuSwitch"
            text: "switch"
            onTriggered: hop.leaveProfile()
            contentItem: Label { text: parent.text; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(15) } leftPadding: 6 }
            background: Rectangle { implicitHeight: 36; color: parent.highlighted ? Theme.hover : "transparent" }
        }
        MenuItem {
            objectName: "chipMenuEject"
            visible: !hop.guest
            height: visible ? implicitHeight : 0
            text: "eject"
            onTriggered: hop.ejectCurrentProfile()
            contentItem: Label { text: parent.text; color: Theme.danger; font { family: Theme.font; pixelSize: Theme.fs(15) } leftPadding: 6 }
            background: Rectangle { implicitHeight: 36; color: parent.highlighted ? Theme.hover : "transparent" }
        }
        MenuItem {
            objectName: "chipMenuSettings"
            text: "quick settings"
            onTriggered: { if (Window.window) Window.window.view = "settings" }
            contentItem: Label { text: parent.text; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(15) } leftPadding: 6 }
            background: Rectangle { implicitHeight: 36; color: parent.highlighted ? Theme.hover : "transparent" }
        }
    }
}
