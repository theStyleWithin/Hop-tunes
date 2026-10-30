// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// collection › music: artists (three panes, design/screens/4), albums grid, songs list.
Item {
    id: root
    signal addFolderRequested()
    signal addVideoFolderRequested()
    signal addVideoCategoryFolderRequested(string category)
    signal addSectionFolderRequested(string kind)
    property string mode: ["albums", "songs", "playlists"].indexOf(String(initialView)) >= 0 ? String(initialView) : "artists"
    property string collectionTab: "music"   // music | videos | pictures | audiobooks | ebooks | podcasts (spec §4.1)
    function showArtists() { mode = "artists"; collectionTab = "music" }

    TrackMenu {
        id: menu
        property var playFn: null
        onPlayRequested: if (playFn) playFn()
        onNewPlaylistRequested: function (paths) { prompt.pendingPaths = paths; prompt.ask("new playlist", "", "create") }
    }
    NamePrompt {
        id: prompt
        property var pendingPaths: []
        onAccepted: function (name) { var k = hop.createPlaylist(name, pendingPaths); pendingPaths = []; if (k) hop.selectPlaylist(k) }
    }
    // Per-profile album cover override (feature: "image" button under an album's art).
    // hop.selectAlbum() is called right before open() so selectedAlbumRef() names the right album.
    FileDialog {
        id: albumArtDialog
        title: "choose album cover"
        nameFilters: ["pictures (*.png *.jpg *.jpeg)"]
        onAccepted: hop.setAlbumArt(hop.selectedAlbumRef(), selectedFile)
    }
    function songMenu(paths, path, hearted, playFn, src, x, y) {
        menu.paths = paths; menu.heartPath = path; menu.hearted = hearted
        menu.pinKind = ""; menu.playlistRow = -1; menu.playFn = playFn
        menu.popup(src, x, y)
    }
    function albumMenu(paths, ref, label, playFn, src, x, y) {
        menu.paths = paths; menu.heartPath = ""; menu.pinKind = "album"; menu.pinRef = ref; menu.pinLabel = label
        menu.playlistRow = -1; menu.playFn = playFn
        menu.popup(src, x, y)
    }

    // phones: artists › albums › songs is one pane at a time (0 artists, 1 albums, 2 songs)
    property int phoneStep: 0
    AbstractButton {   // phones: the "+" button adds a folder to the videos category on show
        objectName: "videoAddFab"
        visible: root.phone && root.collectionTab === "videos" && !hop.guest && hop.videoDetailRow < 0
        anchors { right: parent.right; bottom: parent.bottom; rightMargin: 20; bottomMargin: 16 }
        width: 56; height: 56; z: 5
        onClicked: root.addVideoCategoryFolderRequested(hop.videoCategory)
        Accessible.name: "add a folder"
        background: Rectangle { radius: 16; color: Theme.accent }
        contentItem: Label { text: "+"; color: Theme.ink; font { family: Theme.font; pixelSize: 34 }
                             horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
    }
    readonly property bool phone: Theme.compact
    onPhoneChanged: if (phone && mode === "artists") { mode = "albums"; phoneStep = 0 }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // phones: the collection tabs as scrolling pills
        Label {   // phones: the big light title, as in the mockups
            visible: root.phone
            text: "collection"
            color: Theme.strong
            font { family: Theme.font; pixelSize: 46; weight: Font.Light }
            Layout.leftMargin: Theme.pad; Layout.topMargin: 12
        }
        PillRow {
            visible: root.phone
            underline: true
            Layout.fillWidth: true; Layout.leftMargin: Theme.pad; Layout.rightMargin: Theme.pad; Layout.topMargin: 4
            namePrefix: "pillTab_"
            items: [["music", "music"], ["videos", "videos"]].concat(hop.sectionKinds.map(function (k) { return [k, k] }))
            current: root.collectionTab
            onPicked: function (v) { root.collectionTab = v }
        }

        Label {
            visible: root.phone && hop.scanning && root.collectionTab === "music"
            text: hop.scanTotal > 0 ? "scanning " + hop.scanDone + " / " + hop.scanTotal : "scanning…"
            color: Theme.muted; font { family: Theme.font; pixelSize: 14 }
            Layout.leftMargin: Theme.pad
        }

        // music · videos
        RowLayout {
            visible: !root.phone
            Layout.leftMargin: Theme.pad; Layout.rightMargin: Theme.pad; Layout.topMargin: root.phone ? 6 : 14
            spacing: 28
            AbstractButton {
                id: musicTab
                visible: !root.phone
                objectName: "subTab_music"
                hoverEnabled: true
                onClicked: root.collectionTab = "music"
                contentItem: Label { text: "music"; color: root.collectionTab === "music" ? Theme.accent : (musicTab.hovered ? Theme.text2 : Theme.dim)
                                      font { family: Theme.font; pixelSize: Theme.fs(22) } }
                background: Item {}
            }
            AbstractButton {
                id: videosTab
                visible: !root.phone
                objectName: "subTab_videos"
                hoverEnabled: true
                onClicked: root.collectionTab = "videos"
                contentItem: Label { text: "videos"; color: root.collectionTab === "videos" ? Theme.accent : (videosTab.hovered ? Theme.text2 : Theme.dim)
                                      font { family: Theme.font; pixelSize: Theme.fs(22) } }
                background: Item {}
            }
            Repeater {
                model: hop.sectionKinds
                delegate: AbstractButton {
                    id: secTab
                    visible: !root.phone
                    required property string modelData
                    objectName: "subTab_" + modelData
                    hoverEnabled: true
                    onClicked: root.collectionTab = modelData
                    contentItem: Label { text: secTab.modelData; color: root.collectionTab === secTab.modelData ? Theme.accent : (secTab.hovered ? Theme.text2 : Theme.dim)
                                          font { family: Theme.font; pixelSize: Theme.fs(22) } }
                    background: Item {}
                }
            }
            Item { visible: !root.phone; Layout.preferredWidth: 20 }
            // instant search (spec §4.2)
            TextField {
                id: search
                objectName: "searchField"
                visible: hop.librarySize > 0 && root.collectionTab === "music"
                implicitWidth: 260
                Layout.fillWidth: root.phone
                placeholderText: "search"
                placeholderTextColor: Theme.dim
                color: Theme.text
                font { family: Theme.font; pixelSize: Theme.fs(16) }
                leftPadding: 34
                text: hop.searchText
                onTextEdited: searchDelay.restart()
                Keys.onEscapePressed: { text = ""; hop.searchText = "" }
                Accessible.name: "search your music"
                Timer { id: searchDelay; interval: 120; onTriggered: hop.searchText = search.text }
                // Typing breaks the text binding; keep the box in step when search is
                // cleared elsewhere (profile switch, "show in collection").
                Connections { target: hop; function onSearchChanged() { if (search.text !== hop.searchText) search.text = hop.searchText } }
                background: Rectangle {
                    implicitHeight: 38; color: Theme.panel
                    border { width: search.activeFocus ? 2 : 1; color: search.activeFocus ? Theme.accent : Theme.line }
                    Label { x: 11; anchors.verticalCenter: parent.verticalCenter; text: "⌕"; color: Theme.dim; font.pixelSize: Theme.fs(18) }
                }
                AbstractButton {
                    visible: search.text !== ""
                    anchors { right: parent.right; rightMargin: 6; verticalCenter: parent.verticalCenter }
                    width: 26; height: 26
                    onClicked: { search.text = ""; hop.searchText = "" }
                    Accessible.name: "clear search"
                    contentItem: Label { text: "×"; color: Theme.muted; font.pixelSize: Theme.fs(20); horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Item { }
                }
            }
            Label {
                visible: hop.scanning && root.collectionTab === "music"
                text: hop.scanTotal > 0 ? "scanning " + hop.scanDone + " / " + hop.scanTotal : "scanning…"
                color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) }
            }
            Label {
                visible: !hop.scanning && hop.librarySize > 0 && root.collectionTab === "music"
                text: hop.searchText !== "" ? hop.songs.count + " of " + hop.librarySize : hop.songs.count + (hop.songs.count === 1 ? " song" : " songs")
                color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) }
            }
            Item { visible: !root.phone; Layout.fillWidth: true }
            // centred on this row but adding nothing to its height, so the tabs stay exactly where they were
            Item {
                visible: !root.phone
                Layout.preferredWidth: tabChip.implicitWidth; Layout.preferredHeight: 0; Layout.alignment: Qt.AlignVCenter
                ProfileChip { id: tabChip; anchors { right: parent.right; verticalCenter: parent.verticalCenter } }
            }
            Shortcut { sequences: ["Ctrl+F", "/"]; enabled: hop.profileReady && search.visible; onActivated: { root.mode = root.mode === "playlists" ? "songs" : root.mode; search.forceActiveFocus(); search.selectAll() } }
        }

        // ARTISTS · ALBUMS · SONGS · PLAYLISTS (genres: later)
        PillRow {
            visible: root.phone && root.collectionTab === "music"
            Layout.fillWidth: true; Layout.leftMargin: Theme.pad; Layout.rightMargin: Theme.pad
            namePrefix: "pillMode_"
            items: [["albums", "albums"], ["artists", "artists"], ["songs", "songs"], ["playlists", "playlists"]]
            current: root.mode
            onPicked: function (v) { root.mode = v; root.phoneStep = 0 }
        }
        RowLayout {
            visible: !root.phone && root.collectionTab === "music"
            Layout.leftMargin: Theme.pad; Layout.topMargin: 10
            spacing: 24
            Repeater {
                model: [["ARTISTS", "artists"], ["ALBUMS", "albums"], ["SONGS", "songs"], ["GENRES", ""], ["PLAYLISTS", "playlists"]]
                delegate: AbstractButton {
                    id: tab
                    required property var modelData
                    objectName: "tab_" + modelData[1]
                    readonly property bool on: root.mode === modelData[1]
                    enabled: modelData[1] !== ""
                    onClicked: root.mode = modelData[1]
                    hoverEnabled: true
                    Accessible.role: Accessible.PageTab
                    Accessible.name: modelData[0].toLowerCase()
                    background: Item {}
                    contentItem: Label {
                        text: tab.modelData[0]
                        color: tab.on ? Theme.strong : (!tab.enabled ? Theme.faint : (tab.hovered ? Theme.text2 : Theme.tabIdle))
                        font { family: Theme.font; pixelSize: Theme.fs(15); letterSpacing: 1 }
                        bottomPadding: 4
                        Rectangle { visible: tab.on; anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 2; color: Theme.accent }
                    }
                }
            }
        }

        // empty state
        ColumnLayout {
            visible: root.collectionTab === "music" && hop.librarySize === 0 && !hop.scanning
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.leftMargin: Theme.pad; Layout.topMargin: 40
            spacing: 18
            Label {
                text: hop.folders.length === 0 ? "add your music" : "no songs found yet"
                color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(56); weight: Font.Light }
            }
            Label {
                text: hop.folders.length === 0
                      ? "pick a folder and hop-tunes will find your songs. nothing is uploaded anywhere."
                      : "hop-tunes looked in " + hop.folders.join(", ") + " and found no audio files."
                color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(18) }
                wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.maximumWidth: 640
            }
            Button {
                text: "choose a music folder…"
                onClicked: root.addFolderRequested()
                contentItem: Label { text: parent.text; color: Theme.ink; font { family: Theme.font; pixelSize: Theme.fs(18); weight: Font.Medium } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { implicitHeight: 52; implicitWidth: 260; color: parent.hovered ? Qt.lighter(Theme.accent, 1.08) : Theme.accent }
            }
            Item { Layout.fillHeight: true }
        }

        // no search hits
        ColumnLayout {
            visible: root.collectionTab === "music" && hop.librarySize > 0 && hop.songs.count === 0 && root.mode !== "playlists"
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.leftMargin: Theme.pad; Layout.topMargin: 40
            spacing: 12
            Label { text: "nothing matches \u201c" + hop.searchText + "\u201d"; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(40); weight: Font.Light } }
            Label { text: "search looks at titles, artists, albums and genres."; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(16) } }
            Item { Layout.fillHeight: true }
        }

        VideosView {
            onAddFolderRequested: function (c) { root.addVideoCategoryFolderRequested(c) }
            visible: root.collectionTab === "videos"
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.leftMargin: Theme.pad; Layout.rightMargin: Theme.pad; Layout.topMargin: 18
        }

        Repeater {
            model: hop.sectionKinds
            delegate: SectionView {
                id: secView
                required property string modelData
                kind: modelData
                model: hop[modelData]
                visible: root.collectionTab === modelData
                onAddFolderRequested: function (k) { root.addSectionFolderRequested(k) }
                onItemActivated: function (row) {
                    if (secView.kind === "pictures") pictureViewer.openAt(row)
                    else if (secView.kind === "ebooks") hop.openBook(row)
                    else hop.playSectionItem(secView.kind, row)
                }
                Layout.fillWidth: true; Layout.fillHeight: true
                Layout.leftMargin: Theme.pad; Layout.rightMargin: Theme.pad; Layout.topMargin: 18
            }
        }
        PictureViewer { id: pictureViewer; model: hop.pictureAlbumItems }
        EbookReader { id: ebookReader }

        StackLayout {
            visible: root.collectionTab === "music" && hop.librarySize > 0 && (hop.songs.count > 0 || root.mode === "playlists")
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.leftMargin: Theme.pad; Layout.rightMargin: Theme.pad; Layout.topMargin: 18
            currentIndex: ["artists", "albums", "songs", "playlists"].indexOf(root.mode)

            // ---- artists › albums › songs ----
            RowLayout {
                spacing: root.phone ? 0 : 32
                ListView {
                    id: artistList
                    visible: !root.phone || root.phoneStep === 0
                    Layout.preferredWidth: 240; Layout.fillWidth: root.phone; Layout.fillHeight: true
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
                        width: artistList.width; height: root.phone ? 52 : 40
                        leftPadding: 10
                        onClicked: { hop.selectArtist(index); root.phoneStep = 1 }
                        Accessible.name: name
                        MouseArea {
                            anchors.fill: parent; acceptedButtons: Qt.RightButton
                            onClicked: function (mouse) { hop.selectArtist(ad.index); artistMenu.artist = ad.name; artistMenu.popup(ad, mouse.x, mouse.y) }
                        }
                        background: Rectangle { color: ad.sel || ad.hovered ? Theme.hover : "transparent" }
                        contentItem: Label { text: ad.name; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
                                             color: ad.sel ? Theme.accent : Theme.text; font { family: Theme.font; pixelSize: Theme.fs(18) } }
                    }
                }

                ColumnLayout {
                    visible: !root.phone || root.phoneStep === 1
                    Layout.preferredWidth: 300; Layout.fillWidth: root.phone; Layout.fillHeight: true
                    spacing: 6
                    AbstractButton {   // phones: back to the artists
                        visible: root.phone
                        objectName: "backToArtists"
                        implicitHeight: 44
                        onClicked: root.phoneStep = 0
                        Accessible.name: "back to artists"
                        background: Item {}
                        contentItem: Label { text: "‹  artists"; color: Theme.accent; verticalAlignment: Text.AlignVCenter; font { family: Theme.font; pixelSize: Theme.fs(18) } }
                    }
                    Label { text: hop.selectedArtistName; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(30); weight: Font.Light }
                            wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight; Layout.fillWidth: true }
                    Label {
                        text: hop.artistAlbums.count + (hop.artistAlbums.count === 1 ? " album" : " albums")
                        color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) }
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
                            onClicked: { hop.selectAlbum(index); root.phoneStep = 2 }
                            onDoubleClicked: { hop.selectAlbum(index); hop.playAlbum(0) }
                            Accessible.name: title
                            MouseArea {
                                anchors.fill: parent; acceptedButtons: Qt.RightButton
                                onClicked: function (mouse) { hop.selectAlbum(alb.index); root.albumMenu(hop.selectedAlbumPaths(), hop.selectedAlbumRef(), alb.title, function () { hop.playAlbum(0) }, alb, mouse.x, mouse.y) }
                            }
                            background: Item {}
                            contentItem: ColumnLayout {
                                spacing: 4
                                AlbumArt { key: alb.title; source: alb.art; Layout.fillWidth: true; Layout.preferredHeight: width }
                                AbstractButton {
                                    id: imageBtn
                                    objectName: "albumImageBtn_" + alb.index
                                    visible: !hop.guest && !Theme.compact
                                    hoverEnabled: true
                                    Layout.fillWidth: true; Layout.preferredHeight: 18
                                    Accessible.name: "change album cover"
                                    onClicked: { hop.selectAlbum(alb.index); albumArtDialog.open() }
                                    contentItem: Label {
                                        text: "image"; horizontalAlignment: Text.AlignHCenter
                                        color: imageBtn.hovered ? Theme.accent : Theme.muted
                                        font { family: Theme.font; pixelSize: Theme.fs(12); underline: imageBtn.hovered }
                                    }
                                    background: Item {}
                                }
                                TextField {
                                    id: titleEdit
                                    objectName: "albumTitleEdit_" + alb.index
                                    text: alb.title
                                    Layout.fillWidth: true
                                    readOnly: hop.guest
                                    selectByMouse: true
                                    color: alb.sel ? Theme.accent : Theme.text
                                    font { family: Theme.font; pixelSize: Theme.fs(15) }
                                    leftPadding: 0; rightPadding: 0; topPadding: 2; bottomPadding: 2
                                    background: Rectangle {
                                        implicitHeight: 22; color: "transparent"
                                        border { width: titleEdit.activeFocus ? 1 : 0; color: Theme.accent }
                                    }
                                    Accessible.name: "album title"
                                    onAccepted: {
                                        const t = text.trim()
                                        if (t !== "" && t !== alb.title) { hop.selectAlbum(alb.index); hop.setAlbumTitle(hop.selectedAlbumRef(), t) }
                                        else text = alb.title
                                        focus = false
                                    }
                                }
                                Label { text: alb.year > 0 ? alb.year : ""; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(13) } }
                            }
                        }
                    }
                }

                ColumnLayout {
                    visible: !root.phone || root.phoneStep === 2
                    Layout.fillWidth: true; Layout.fillHeight: true
                    spacing: 0
                    AbstractButton {   // phones: back to this artist's albums
                        visible: root.phone
                        objectName: "backToAlbums"
                        implicitHeight: 44
                        onClicked: root.phoneStep = 1
                        Accessible.name: "back to albums"
                        background: Item {}
                        contentItem: Label { text: "‹  albums"; color: Theme.accent; verticalAlignment: Text.AlignVCenter; font { family: Theme.font; pixelSize: Theme.fs(18) } }
                    }
                    RowLayout {
                        Layout.fillWidth: true; Layout.bottomMargin: 10
                        Label { text: hop.selectedAlbumTitle; Layout.fillWidth: true; elide: Text.ElideRight
                                color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(22) } }
                        Button {
                            text: "play album"
                            enabled: hop.albumSongs.count > 0
                            onClicked: hop.playAlbum(0)
                            contentItem: Label { text: parent.text; color: Theme.ink; font { family: Theme.font; pixelSize: Theme.fs(15); weight: Font.Medium }
                                                 horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { implicitWidth: 120; implicitHeight: 40; color: parent.hovered ? Qt.lighter(Theme.accent, 1.08) : Theme.accent }
                        }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.line }
                    ListView {
                        id: albumSongList
                        objectName: "albumSongList"
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
                            draggable: true
                            trackId: model.trackId; title: model.title; artist: model.artist; album: model.album
                            trackNo: model.trackNo; duration: model.duration
                            path: model.path; hearted: model.hearted
                            onPlay: hop.playAlbum(index)
                            onMenu: function (src, x, y) { root.songMenu([model.path], model.path, model.hearted, function () { hop.playAlbum(index) }, src, x, y) }
                            onMoveRequested: function (steps) { hop.moveAlbumSong(index, Math.max(0, Math.min(hop.albumSongs.count - 1, index + steps))) }
                        }
                    }
                }
            }

            // ---- albums grid ----
            GridView {
                id: allAlbums
                clip: true
                model: hop.allAlbums
                readonly property int columns: root.phone ? 2 : Math.max(3, Math.floor(width / 200))
                cellWidth: width / columns; cellHeight: cellWidth + (Theme.compact ? 56 : 52)
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                delegate: AbstractButton {
                    id: tile
                    required property int index
                    required property string title
                    required property string artist
                    required property string art
                    width: allAlbums.cellWidth - (Theme.compact ? 14 : 18); height: allAlbums.cellHeight
                    onClicked: { hop.openAlbum(index); root.mode = "artists"; root.phoneStep = 2 }
                    Accessible.name: title + ", " + artist
                    MouseArea {
                        anchors.fill: parent; acceptedButtons: Qt.RightButton
                        onClicked: function (mouse) { root.albumMenu(hop.albumPaths(tile.index), hop.albumRef(tile.index), tile.title, function () { hop.playTile("album", hop.albumRef(tile.index)) }, tile, mouse.x, mouse.y) }
                    }
                    background: Item {}
                    contentItem: ColumnLayout {
                        spacing: 4
                        AlbumArt { key: tile.title; source: tile.art; Layout.fillWidth: true; Layout.preferredHeight: width
                                   opacity: tile.hovered ? 0.85 : 1 }
                        Label { text: tile.title; Layout.fillWidth: true; Layout.topMargin: Theme.compact ? 4 : 0; elide: Text.ElideRight; color: Theme.text; font { family: Theme.font; pixelSize: Theme.compact ? 17 : Theme.fs(15) } }
                        Label { text: tile.artist; Layout.fillWidth: true; elide: Text.ElideRight; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.compact ? 14 : Theme.fs(13) } }
                        Item { Layout.fillHeight: true }
                    }
                }
            }

            // ---- all songs ----
            ListView {
                id: list
                objectName: "songList"
                clip: true
                model: hop.songs
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { }
                keyNavigationEnabled: true
                focus: true

                header: Rectangle {
                    visible: !Theme.compact
                    width: list.width; height: Theme.compact ? 0 : 34; color: "transparent"
                    RowLayout {
                        anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                        spacing: 16
                        Label { text: "#"; Layout.preferredWidth: 28; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) } }
                        Label { text: "title"; Layout.fillWidth: true; Layout.preferredWidth: 4; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) } }
                        Label { text: "artist"; Layout.fillWidth: true; Layout.preferredWidth: 3; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) } }
                        Label { text: "album"; Layout.fillWidth: true; Layout.preferredWidth: 3; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) } }
                        Item { Layout.preferredWidth: 27 }
                        Label { text: "length"; Layout.preferredWidth: 56; horizontalAlignment: Text.AlignRight; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) } }
                    }
                    Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 1; color: Theme.line }
                }

                delegate: SongRow {
                    required property int index
                    required property var model
                    width: list.width
                    draggable: true
                    trackId: model.trackId; title: model.title; artist: model.artist; album: model.album
                    trackNo: model.trackNo; duration: model.duration
                    path: model.path; hearted: model.hearted
                    onPlay: hop.playSongs(index)
                    onMenu: function (src, x, y) { root.songMenu([model.path], model.path, model.hearted, function () { hop.playSongs(index) }, src, x, y) }
                    onMoveRequested: function (steps) { hop.moveSong(index, Math.max(0, Math.min(hop.songs.count - 1, index + steps))) }
                }
            }

            // ---- playlists ----
            PlaylistsPane {
                onSongMenu: function (paths, path, hearted, row, count, playFn, src, x, y) {
                    menu.paths = paths; menu.heartPath = path; menu.hearted = hearted; menu.pinKind = ""
                    menu.playlistRow = row; menu.playlistCount = count; menu.playFn = playFn
                    menu.popup(src, x, y)
                }
            }
        }
    }

    Menu {
        id: artistMenu
        property string artist: ""
        property bool pinned: false
        onAboutToShow: pinned = hop.isPinned("artist", artist)
        background: Rectangle { implicitWidth: 240; color: Theme.panel; border { width: 1; color: Theme.line } }
        MenuItem {
            text: "play all by " + artistMenu.artist
            onTriggered: hop.playTile("artist", artistMenu.artist)
            contentItem: Label { text: parent.text; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(15) } leftPadding: 6 }
            background: Rectangle { implicitHeight: 36; color: parent.highlighted ? Theme.hover : "transparent" }
        }
        MenuItem {
            text: "smart DJ from " + artistMenu.artist
            onTriggered: hop.startDj(artistMenu.artist)
            contentItem: Label { text: parent.text; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(15) } leftPadding: 6 }
            background: Rectangle { implicitHeight: 36; color: parent.highlighted ? Theme.hover : "transparent" }
        }
        MenuItem {
            text: artistMenu.pinned ? "unpin from quickplay" : "pin to quickplay"
            onTriggered: artistMenu.pinned ? hop.unpinTile("artist", artistMenu.artist) : hop.pinTile("artist", artistMenu.artist, artistMenu.artist)
            contentItem: Label { text: parent.text; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(15) } leftPadding: 6 }
            background: Rectangle { implicitHeight: 36; color: parent.highlighted ? Theme.hover : "transparent" }
        }
    }
}
