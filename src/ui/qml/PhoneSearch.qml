// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Phones: the search page (the bottom bar's third item). Types into the same search as the collection.
Item {
    id: root
    onVisibleChanged: if (visible) field.forceActiveFocus()
    ColumnLayout {
        anchors { fill: parent; leftMargin: Theme.pad; rightMargin: Theme.pad; topMargin: 12 }
        spacing: 10
        Label { text: "search"; color: Theme.strong; font { family: Theme.font; pixelSize: 46; weight: Font.Light } }
        TextField {
            id: field
            objectName: "phoneSearchField"
            Layout.fillWidth: true
            placeholderText: "titles, artists, albums, genres"
            placeholderTextColor: Theme.dim
            color: Theme.text
            font { family: Theme.font; pixelSize: 18 }
            leftPadding: 38
            text: hop.searchText
            onTextEdited: delay.restart()
            Timer { id: delay; interval: 120; onTriggered: hop.searchText = field.text }
            Connections { target: hop; function onSearchChanged() { if (field.text !== hop.searchText) field.text = hop.searchText } }
            background: Rectangle {
                implicitHeight: 48; radius: 24; color: Theme.panel
                border { width: field.activeFocus ? 2 : 1; color: field.activeFocus ? Theme.accent : Theme.line }
                Label { x: 14; anchors.verticalCenter: parent.verticalCenter; text: "⌕"; color: Theme.dim; font.pixelSize: 20 }
            }
        }
        Label {
            visible: hop.searchText !== ""
            text: hop.songs.count + (hop.songs.count === 1 ? " song" : " songs")
            color: Theme.muted; font { family: Theme.font; pixelSize: 14 }
        }
        Label {
            visible: hop.searchText === ""
            text: "search looks at titles, artists, albums and genres in your own library."
            color: Theme.dim; wrapMode: Text.WordWrap; Layout.fillWidth: true
            font { family: Theme.font; pixelSize: 15 }
        }
        Item { visible: hop.searchText === ""; Layout.fillHeight: true }
        ListView {
            id: results
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true
            visible: hop.searchText !== ""
            model: hop.songs
            boundsBehavior: Flickable.StopAtBounds
            delegate: SongRow {
                required property int index
                required property var model
                width: results.width
                wide: false
                trackId: model.trackId; title: model.title; artist: model.artist; album: model.album
                trackNo: 0; duration: model.duration
                path: model.path; hearted: model.hearted
                onPlay: hop.playSongs(index)
            }
        }
    }
}
