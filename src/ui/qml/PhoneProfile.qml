// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Phones: the profile page (the bottom bar's fourth item): the private status box above the profile, then
// the library summary, quickplay and settings.
Item {
    id: root
    signal go(string view)
    Flickable {
        anchors.fill: parent
        contentHeight: col.implicitHeight + 24
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ColumnLayout {
            id: col
            x: Theme.pad; y: 12
            width: parent.width - 2 * Theme.pad
            spacing: 10
            Label { text: "profile"; color: Theme.strong; font { family: Theme.font; pixelSize: 46; weight: Font.Light } }
            Label { text: "status · only you see this"; color: Theme.muted; font { family: Theme.font; pixelSize: 14 }
                    Layout.topMargin: 6; visible: !hop.guest }
            StatusNote {
                visible: !hop.guest
                Layout.fillWidth: true
                implicitHeight: 104
                radius: 12
            }
            Label { visible: !hop.guest; text: "saved as you type · " + hop.statusNote.length + " of 300"
                    color: Theme.dim; font { family: Theme.font; pixelSize: 13 } Layout.bottomMargin: 6 }

            Rectangle {   // the profile
                Layout.fillWidth: true; implicitHeight: 112; radius: 12; color: Theme.panel
                RowLayout {
                    anchors { fill: parent; margins: 14 }
                    spacing: 12
                    ProfileAvatar { profile: hop.profile; size: 72 }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label { text: hop.profile.name || "guest"; color: Theme.strong; elide: Text.ElideRight; Layout.fillWidth: true
                                font { family: Theme.font; pixelSize: 26; weight: Font.Light } }
                        Label { text: "profile · on this device"; color: Theme.muted; elide: Text.ElideRight; Layout.fillWidth: true; font { family: Theme.font; pixelSize: 14 } }
                    }
                    AbstractButton {
                        objectName: "switchProfile"
                        implicitWidth: 72; implicitHeight: 40
                        onClicked: hop.leaveProfile()
                        Accessible.name: "switch profile"
                        background: Rectangle { radius: 20; color: "transparent"; border { width: 1; color: Theme.borderSoft } }
                        contentItem: Label { text: "switch"; color: Theme.text2; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                             font { family: Theme.font; pixelSize: 16 } }
                    }
                }
            }
            component Card: AbstractButton {
                id: card
                property string head
                property string sub
                Layout.fillWidth: true
                implicitHeight: 80
                background: Rectangle { radius: 12; color: card.pressed ? Theme.hover : Theme.panel }
                contentItem: ColumnLayout {
                    anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
                    spacing: 2
                    Item { Layout.fillHeight: true }
                    Label { text: card.head; color: Theme.strong; font { family: Theme.font; pixelSize: 20 } }
                    Label { text: card.sub; color: Theme.muted; elide: Text.ElideRight; Layout.fillWidth: true; font { family: Theme.font; pixelSize: 14 } }
                    Item { Layout.fillHeight: true }
                }
            }
            Card { head: "my library"; sub: hop.librarySize + " songs · " + hop.videos.count + " films"
                   onClicked: root.go("collection"); objectName: "profileLibrary" }
            Card { head: "quickplay"; sub: "pinned, history, new and smart dj"
                   onClicked: root.go("quickplay"); objectName: "profileQuickplay" }
            Card { head: "settings"; sub: "navigation, theme, video player, folders"
                   onClicked: root.go("settings"); objectName: "profileSettings" }
        }
    }
}
