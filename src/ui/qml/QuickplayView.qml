// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// quickplay (design/screens/3). Pins, history and Smart DJ arrive in M4; for now: shuffle everything.
Item {
    ColumnLayout {
        anchors { fill: parent; leftMargin: 48; topMargin: 36 }
        spacing: 16
        Label { text: "smart dj"; color: hop.accent; font { family: Theme.font; pixelSize: 24 } }
        Rectangle {
            color: Theme.panel
            implicitWidth: 320; implicitHeight: col.implicitHeight + 40
            ColumnLayout {
                id: col
                anchors { fill: parent; margins: 20 }
                spacing: 14
                Label { text: "play your whole library in random order"; color: Theme.muted; font { family: Theme.font; pixelSize: 15 } wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Label { text: hop.songs.count + " songs"; color: Theme.text; font { family: Theme.font; pixelSize: 30; weight: Font.Light } }
                Button {
                    text: "shuffle all"
                    enabled: hop.songs.count > 0
                    onClicked: hop.shuffleAll()
                    contentItem: Label { text: parent.text; color: Theme.ink; font { family: Theme.font; pixelSize: 17; weight: Font.Medium } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { implicitWidth: 140; implicitHeight: 44; color: parent.enabled ? hop.accent : Theme.dim }
                }
            }
        }
        Label { text: "pinned albums, history and new arrivals are coming in a later milestone."; color: Theme.faint; font { family: Theme.font; pixelSize: 14 } }
        Item { Layout.fillHeight: true }
    }
}
