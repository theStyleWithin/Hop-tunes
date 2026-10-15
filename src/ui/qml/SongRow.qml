// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// One song in a list. Double-click / Enter plays. `wide` adds artist and album columns.
ItemDelegate {
    id: row
    property bool wide: true
    property var trackId
    property string title
    property string artist
    property string album
    property int trackNo
    property string duration
    signal play()

    readonly property bool current: hop.player.hasTrack && hop.player.currentTrackId === trackId
    height: 44
    leftPadding: 8; rightPadding: 8
    onDoubleClicked: row.play()
    Keys.onReturnPressed: row.play()
    Keys.onEnterPressed: row.play()
    Accessible.name: title + (artist ? ", " + artist : "")

    background: Rectangle {
        color: row.hovered || (row.ListView.isCurrentItem && row.ListView.view && row.ListView.view.activeFocus) ? Theme.hover : "transparent"
        Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 1; color: "#1d1d22" }
    }
    contentItem: RowLayout {
        spacing: 16
        Label { text: row.current ? "▶" : (row.trackNo > 0 ? row.trackNo : ""); Layout.preferredWidth: 28
                color: row.current ? hop.accent : "#8a8a93"; font { family: Theme.font; pixelSize: 14 } }
        Label { text: row.title; Layout.fillWidth: true; Layout.preferredWidth: 4; elide: Text.ElideRight
                color: row.current ? hop.accent : Theme.text; font { family: Theme.font; pixelSize: 16 } }
        Label { visible: row.wide; text: row.artist; Layout.fillWidth: true; Layout.preferredWidth: 3; elide: Text.ElideRight
                color: Theme.text2; font { family: Theme.font; pixelSize: 15 } }
        Label { visible: row.wide; text: row.album; Layout.fillWidth: true; Layout.preferredWidth: 3; elide: Text.ElideRight
                color: Theme.text2; font { family: Theme.font; pixelSize: 15 } }
        Label { text: row.duration; Layout.preferredWidth: 56; horizontalAlignment: Text.AlignRight
                color: Theme.muted; font { family: Theme.font; pixelSize: 14 } }
    }
}
