// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// collection › music › songs (M1). Artists/albums panes arrive in M2 (see design/screens/4).
Item {
    id: root
    signal addFolderRequested()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // music · videos · podcasts
        RowLayout {
            Layout.leftMargin: 48; Layout.rightMargin: 48; Layout.topMargin: 14
            spacing: 28
            Label { text: "music"; color: Theme.accent; font { family: Theme.font; pixelSize: 22 } }
            Label { text: "videos"; color: Theme.dim; font { family: Theme.font; pixelSize: 22 } }
            Label { text: "podcasts"; color: Theme.dim; font { family: Theme.font; pixelSize: 22 } }
            Item { Layout.fillWidth: true }
            Label {
                visible: hop.scanning
                text: hop.scanTotal > 0 ? "scanning " + hop.scanDone + " / " + hop.scanTotal : "scanning…"
                color: Theme.muted; font { family: Theme.font; pixelSize: 14 }
            }
            Label {
                visible: !hop.scanning && hop.songs.count > 0
                text: hop.songs.count + (hop.songs.count === 1 ? " song" : " songs")
                color: Theme.muted; font { family: Theme.font; pixelSize: 14 }
            }
        }

        // ARTISTS · ALBUMS · SONGS · GENRES · PLAYLISTS
        RowLayout {
            Layout.leftMargin: 48; Layout.topMargin: 10
            spacing: 24
            Repeater {
                model: ["ARTISTS", "ALBUMS", "SONGS", "GENRES", "PLAYLISTS"]
                delegate: Label {
                    required property string modelData
                    readonly property bool on: modelData === "SONGS"
                    text: modelData
                    color: on ? "#ffffff" : Theme.faint
                    font { family: Theme.font; pixelSize: 15; letterSpacing: 1 }
                    bottomPadding: 4
                    Rectangle { visible: parent.on; anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 2; color: Theme.accent }
                }
            }
        }

        // empty state
        ColumnLayout {
            visible: hop.songs.count === 0 && !hop.scanning
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.leftMargin: 48; Layout.topMargin: 40
            spacing: 18
            Label {
                text: hop.folders.length === 0 ? "add your music" : "no songs found yet"
                color: Theme.text; font { family: Theme.font; pixelSize: 56; weight: Font.Light }
            }
            Label {
                text: hop.folders.length === 0
                      ? "pick a folder and hop-tunes will find your songs. nothing is uploaded anywhere."
                      : "hop-tunes looked in " + hop.folders.join(", ") + " and found no audio files."
                color: Theme.muted; font { family: Theme.font; pixelSize: 18 }
                wrapMode: Text.WordWrap; Layout.maximumWidth: 640
            }
            Button {
                text: "choose a music folder…"
                onClicked: root.addFolderRequested()
                contentItem: Label { text: parent.text; color: Theme.ink; font { family: Theme.font; pixelSize: 18; weight: Font.Medium } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { implicitHeight: 52; implicitWidth: 260; color: parent.hovered ? Qt.lighter(Theme.accent, 1.08) : Theme.accent }
            }
            Item { Layout.fillHeight: true }
        }

        // song list
        ListView {
            id: list
            visible: hop.songs.count > 0
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.leftMargin: 48; Layout.rightMargin: 48; Layout.topMargin: 18
            clip: true
            model: hop.songs
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { }
            keyNavigationEnabled: true
            focus: true

            header: Rectangle {
                width: list.width; height: 34; color: "transparent"
                RowLayout {
                    anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                    spacing: 16
                    Label { text: "#"; Layout.preferredWidth: 28; color: Theme.dim; font { family: Theme.font; pixelSize: 13 } }
                    Label { text: "title"; Layout.fillWidth: true; Layout.preferredWidth: 4; color: Theme.dim; font { family: Theme.font; pixelSize: 13 } }
                    Label { text: "artist"; Layout.fillWidth: true; Layout.preferredWidth: 3; color: Theme.dim; font { family: Theme.font; pixelSize: 13 } }
                    Label { text: "album"; Layout.fillWidth: true; Layout.preferredWidth: 3; color: Theme.dim; font { family: Theme.font; pixelSize: 13 } }
                    Label { text: "length"; Layout.preferredWidth: 56; horizontalAlignment: Text.AlignRight; color: Theme.dim; font { family: Theme.font; pixelSize: 13 } }
                }
                Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 1; color: Theme.line }
            }

            delegate: ItemDelegate {
                id: row
                required property int index
                required property var trackId
                required property string title
                required property string artist
                required property string album
                required property int trackNo
                required property string duration
                readonly property bool current: hop.player.hasTrack && hop.player.currentTrackId === trackId
                width: list.width
                height: 44
                leftPadding: 8; rightPadding: 8
                onDoubleClicked: hop.playSongs(index)
                Keys.onReturnPressed: hop.playSongs(index)
                Accessible.name: title + ", " + artist
                background: Rectangle {
                    color: row.hovered || row.ListView.isCurrentItem && list.activeFocus ? Theme.hover : "transparent"
                    Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 1; color: "#1d1d22" }
                }
                contentItem: RowLayout {
                    spacing: 16
                    Label { text: row.current ? "▶" : (row.trackNo > 0 ? row.trackNo : ""); Layout.preferredWidth: 28
                            color: row.current ? Theme.accent : "#8a8a93"; font { family: Theme.font; pixelSize: 14 } }
                    Label { text: row.title; Layout.fillWidth: true; Layout.preferredWidth: 4; elide: Text.ElideRight
                            color: row.current ? Theme.accent : Theme.text; font { family: Theme.font; pixelSize: 16 } }
                    Label { text: row.artist; Layout.fillWidth: true; Layout.preferredWidth: 3; elide: Text.ElideRight
                            color: Theme.text2; font { family: Theme.font; pixelSize: 15 } }
                    Label { text: row.album; Layout.fillWidth: true; Layout.preferredWidth: 3; elide: Text.ElideRight
                            color: Theme.text2; font { family: Theme.font; pixelSize: 15 } }
                    Label { text: row.duration; Layout.preferredWidth: 56; horizontalAlignment: Text.AlignRight
                            color: Theme.muted; font { family: Theme.font; pixelSize: 14 } }
                }
            }
        }
    }
}
