// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One song in a list. Double-click / Enter plays, right-click opens the song menu.
// `wide` adds artist and album columns.
ItemDelegate {
    id: row
    property bool wide: true
    property var trackId
    property string title
    property string artist
    property string album
    property int trackNo
    property string duration
    property string path
    property bool hearted: false
    signal play()
    signal menu(Item source, real x, real y)   // right-click / menu key, at that point
    property bool draggable: false             // manual playlists: drag the ≡ handle to reorder
    signal moveRequested(int steps)
    property real dragY: 0
    z: handle.pressed ? 10 : 0
    transform: Translate { y: row.dragY }

    readonly property bool current: hop.player.hasTrack && hop.player.currentTrackId === trackId
    height: 44
    leftPadding: 8; rightPadding: 8
    onDoubleClicked: row.play()
    Keys.onReturnPressed: row.play()
    Keys.onEnterPressed: row.play()
    Keys.onMenuPressed: row.menu(row, 40, row.height)
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.RightButton; onClicked: function (mouse) { row.menu(row, mouse.x, mouse.y) } }
    Accessible.name: title + (artist ? ", " + artist : "")

    background: Rectangle {
        color: row.hovered || (row.ListView.isCurrentItem && row.ListView.view && row.ListView.view.activeFocus) ? Theme.hover : "transparent"
        Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 1; color: Theme.rowLine }
    }
    contentItem: RowLayout {
        spacing: 16
        Label {
            text: row.draggable && (row.hovered || handle.pressed) ? "≡" : (row.current ? "▶" : (row.trackNo > 0 ? row.trackNo : ""))
            Layout.preferredWidth: 28
            color: row.current ? hop.accent : Theme.tabIdle; font { family: Theme.font; pixelSize: row.draggable && row.hovered ? 18 : 14 }
            MouseArea {
                id: handle
                enabled: row.draggable
                anchors.fill: parent; anchors.margins: -6
                cursorShape: row.draggable ? Qt.SizeVerCursor : Qt.ArrowCursor
                property real startY: 0
                onPressed: function (m) { startY = mapToItem(null, m.x, m.y).y }
                onPositionChanged: function (m) { row.dragY = mapToItem(null, m.x, m.y).y - startY }
                onReleased: { var steps = Math.round(row.dragY / row.height); row.dragY = 0; if (steps !== 0) row.moveRequested(steps) }
                onCanceled: row.dragY = 0
            }
        }
        Label { text: row.title; Layout.fillWidth: true; Layout.preferredWidth: 4; elide: Text.ElideRight
                color: row.current ? hop.accent : Theme.text; font { family: Theme.font; pixelSize: 16 } }
        Label { visible: row.wide; text: row.artist; Layout.fillWidth: true; Layout.preferredWidth: 3; elide: Text.ElideRight
                color: Theme.text2; font { family: Theme.font; pixelSize: 15 } }
        Label { visible: row.wide; text: row.album; Layout.fillWidth: true; Layout.preferredWidth: 3; elide: Text.ElideRight
                color: Theme.text2; font { family: Theme.font; pixelSize: 15 } }
        HeartButton {
            size: 15
            on: row.hearted
            opacity: row.hearted || row.hovered ? 1 : 0
            onClicked: hop.toggleHeart(row.path)
        }
        Label { text: row.duration; Layout.preferredWidth: 56; horizontalAlignment: Text.AlignRight
                color: Theme.muted; font { family: Theme.font; pixelSize: 14 } }
    }
}
