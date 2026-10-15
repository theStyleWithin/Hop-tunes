// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// collection › music: artists (three panes, design/screens/4), albums grid, songs list.
Item {
    id: root
    signal addFolderRequested()
    property string mode: "artists"

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

        // ARTISTS · ALBUMS · SONGS (genres and playlists arrive in later milestones)
        RowLayout {
            Layout.leftMargin: 48; Layout.topMargin: 10
            spacing: 24
            Repeater {
                model: [["ARTISTS", "artists"], ["ALBUMS", "albums"], ["SONGS", "songs"], ["GENRES", ""], ["PLAYLISTS", ""]]
                delegate: AbstractButton {
                    id: tab
                    required property var modelData
                    readonly property bool on: root.mode === modelData[1]
                    enabled: modelData[1] !== ""
                    onClicked: root.mode = modelData[1]
                    hoverEnabled: true
                    Accessible.role: Accessible.PageTab
                    Accessible.name: modelData[0].toLowerCase()
                    background: Item {}
                    contentItem: Label {
                        text: tab.modelData[0]
                        color: tab.on ? "#ffffff" : (!tab.enabled ? Theme.faint : (tab.hovered ? Theme.text2 : "#8a8a93"))
                        font { family: Theme.font; pixelSize: 15; letterSpacing: 1 }
                        bottomPadding: 4
                        Rectangle { visible: tab.on; anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 2; color: Theme.accent }
                    }
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

        StackLayout {
            visible: hop.songs.count > 0
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.leftMargin: 48; Layout.rightMargin: 48; Layout.topMargin: 18
            currentIndex: ["artists", "albums", "songs"].indexOf(root.mode)

            // ---- artists › albums › songs ----
            RowLayout {
                spacing: 32
                ListView {
                    id: artistList
                    Layout.preferredWidth: 240; Layout.fillHeight: true
                    clip: true
                    model: hop.artists
                    currentIndex: hop.selectedArtist
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}
                    keyNavigationEnabled: true
                    onCurrentIndexChanged: if (currentIndex !== hop.selectedArtist && currentIndex >= 0) hop.selectArtist(currentIndex)
                    delegate: ItemDelegate {
                        id: ad
                        required property int index
                        required property string name
                        readonly property bool sel: index === hop.selectedArtist
                        width: artistList.width; height: 40
                        leftPadding: 10
                        onClicked: hop.selectArtist(index)
                        Accessible.name: name
                        background: Rectangle { color: ad.sel || ad.hovered ? Theme.hover : "transparent" }
                        contentItem: Label { text: ad.name; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
                                             color: ad.sel ? Theme.accent : Theme.text; font { family: Theme.font; pixelSize: 18 } }
                    }
                }

                ColumnLayout {
                    Layout.preferredWidth: 300; Layout.fillHeight: true
                    spacing: 6
                    Label { text: hop.selectedArtistName; color: Theme.text; font { family: Theme.font; pixelSize: 30; weight: Font.Light }
                            wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight; Layout.fillWidth: true }
                    Label {
                        text: hop.artistAlbums.count + (hop.artistAlbums.count === 1 ? " album" : " albums")
                        color: Theme.muted; font { family: Theme.font; pixelSize: 14 }
                    }
                    GridView {
                        id: albumGrid
                        Layout.fillWidth: true; Layout.fillHeight: true; Layout.topMargin: 10
                        clip: true
                        cellWidth: width / 2; cellHeight: cellWidth + 46
                        model: hop.artistAlbums
                        boundsBehavior: Flickable.StopAtBounds
                        delegate: AbstractButton {
                            id: alb
                            required property int index
                            required property string title
                            required property string art
                            required property int year
                            readonly property bool sel: index === hop.selectedAlbum
                            width: albumGrid.cellWidth - 14; height: albumGrid.cellHeight
                            onClicked: hop.selectAlbum(index)
                            onDoubleClicked: { hop.selectAlbum(index); hop.playAlbum(0) }
                            Accessible.name: title
                            background: Item {}
                            contentItem: ColumnLayout {
                                spacing: 4
                                AlbumArt { key: alb.title; source: alb.art; Layout.fillWidth: true; Layout.preferredHeight: width }
                                Label { text: alb.title; Layout.fillWidth: true; elide: Text.ElideRight
                                        color: alb.sel ? Theme.accent : Theme.text; font { family: Theme.font; pixelSize: 15 } }
                                Label { text: alb.year > 0 ? alb.year : ""; color: Theme.muted; font { family: Theme.font; pixelSize: 13 } }
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    spacing: 0
                    RowLayout {
                        Layout.fillWidth: true; Layout.bottomMargin: 10
                        Label { text: hop.selectedAlbumTitle; Layout.fillWidth: true; elide: Text.ElideRight
                                color: Theme.text; font { family: Theme.font; pixelSize: 22 } }
                        Button {
                            text: "play album"
                            enabled: hop.albumSongs.count > 0
                            onClicked: hop.playAlbum(0)
                            contentItem: Label { text: parent.text; color: Theme.ink; font { family: Theme.font; pixelSize: 15; weight: Font.Medium }
                                                 horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { implicitWidth: 120; implicitHeight: 40; color: parent.hovered ? Qt.lighter(Theme.accent, 1.08) : Theme.accent }
                        }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.line }
                    ListView {
                        id: albumSongList
                        Layout.fillWidth: true; Layout.fillHeight: true
                        clip: true
                        model: hop.albumSongs
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar {}
                        delegate: SongRow {
                            required property int index
                            required property var model
                            width: albumSongList.width
                            wide: false
                            trackId: model.trackId; title: model.title; artist: model.artist; album: model.album
                            trackNo: model.trackNo; duration: model.duration
                            onPlay: hop.playAlbum(index)
                        }
                    }
                }
            }

            // ---- albums grid ----
            GridView {
                id: allAlbums
                clip: true
                model: hop.allAlbums
                readonly property int columns: Math.max(3, Math.floor(width / 200))
                cellWidth: width / columns; cellHeight: cellWidth + 52
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                delegate: AbstractButton {
                    id: tile
                    required property int index
                    required property string title
                    required property string artist
                    required property string art
                    width: allAlbums.cellWidth - 18; height: allAlbums.cellHeight
                    onClicked: { hop.openAlbum(index); root.mode = "artists" }
                    Accessible.name: title + ", " + artist
                    background: Item {}
                    contentItem: ColumnLayout {
                        spacing: 4
                        AlbumArt { key: tile.title; source: tile.art; Layout.fillWidth: true; Layout.preferredHeight: width
                                   opacity: tile.hovered ? 0.85 : 1 }
                        Label { text: tile.title; Layout.fillWidth: true; elide: Text.ElideRight; color: Theme.text; font { family: Theme.font; pixelSize: 15 } }
                        Label { text: tile.artist; Layout.fillWidth: true; elide: Text.ElideRight; color: Theme.muted; font { family: Theme.font; pixelSize: 13 } }
                    }
                }
            }

            // ---- all songs ----
            ListView {
                id: list
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

                delegate: SongRow {
                    required property int index
                    required property var model
                    width: list.width
                    trackId: model.trackId; title: model.title; artist: model.artist; album: model.album
                    trackNo: model.trackNo; duration: model.duration
                    onPlay: hop.playSongs(index)
                }
            }
        }
    }
}
