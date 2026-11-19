// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Internet Archive, an app inside Hop-tunes: find music, films and books on archive.org, play and watch them here, and read the books.
// No account and nothing to register. Only after the person allowed it, only while it is open, and every address goes through the
// gateway (net/AppNet). The archive's own curator picks (normand64, "the media preserver") are shown first. Hop-tunes is not made or
// endorsed by the Internet Archive; what is on it belongs to the people who made it and each item says what its licence is.
FocusScope {
    id: root
    objectName: "archiveApp"
    readonly property var ia: hop.archive
    property bool allowed: hop.appNetAllowed("archive")
    property string page: "browse"               // browse | item | read | favourites
    property string playUrl: ""                  // the music track or film being played ("" = nothing)
    property string playTitle: ""
    property int trackIndex: -1
    property int chapter: 0
    property real readSize: 18
    readonly property bool phone: Theme.compact
    readonly property var detail: ia && ia.detail ? ia.detail : ({})
    readonly property bool hasDetail: !!detail && detail.id !== undefined
    signal leave()
    focus: true

    readonly property var hosts: [["archive.org", "finds music, films and books, and holds their covers"],
                                  ["*.archive.org", "the servers the files come from (addresses like ia800000.us.archive.org)"]]
    function pic(url) { return url && url !== "" ? "image://archive/" + encodeURIComponent(url) : "" }
    function kindName(k) { return k === "audio" ? "music" : k === "movies" ? "film" : k === "texts" ? "book" : k }
    function stopPlaying() { playUrl = ""; playTitle = ""; trackIndex = -1 }
    function play(url, title, index) {
        if (hop.player.playing) hop.player.pause()                 // the library's own music steps aside
        playTitle = title; trackIndex = index; playUrl = url
    }
    function openItem(id) { stopPlaying(); page = "item"; ia.openItem(id) }
    function runSearch(text) { stopPlaying(); page = "browse"; ia.closeItem(); ia.search(text) }
    function openBook() { chapter = 0; page = "read"; ia.openBook() }

    Connections { target: hop; function onAppNetChanged() { root.allowed = hop.appNetAllowed("archive") } }
    Component.onCompleted: { if (ia && allowed) ia.open(); Qt.callLater(function () { root.forceActiveFocus() }) }
    Component.onDestruction: { if (ia) ia.close() }

    // one step back inside the app (the host asks first); false = nothing to step back from, leave the app
    function goBack() {
        if (player.fullscreen) { player.fullscreen = false; return true }
        if (page === "browse" && ia.curatorView) { ia.search(""); return true }
        if (page === "read") { ia.closeBook(); page = "item"; return true }
        if (page === "item") { stopPlaying(); ia.closeItem(); page = "browse"; return true }
        if (page === "favourites") { page = "browse"; return true }
        return false
    }

    component Chip: AbstractButton {
        id: ch
        property bool on: false
        hoverEnabled: true; focusPolicy: Qt.NoFocus
        implicitHeight: 30; implicitWidth: lbl.implicitWidth + 24
        background: Rectangle { radius: 15; color: ch.on ? Theme.accent : "transparent"; border { width: ch.on ? 0 : 1; color: ch.hovered ? Theme.text2 : Theme.border } }
        contentItem: Label { id: lbl; text: ch.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                             color: ch.on ? Theme.ink : Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(14) } }
    }

    // ------------------------------------------------------------------------------------------------------
    // not built / not allowed yet
    // ------------------------------------------------------------------------------------------------------
    Label {
        visible: !root.ia
        anchors { left: parent.left; right: parent.right; top: parent.top }
        wrapMode: Text.WordWrap; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(16) }
        text: "the internet archive needs the internet and was left out of this version."
    }
    Flickable {
        objectName: "iaPermission"
        visible: !!root.ia && !root.allowed
        anchors.fill: parent
        contentHeight: permCol.implicitHeight; clip: true; boundsBehavior: Flickable.StopAtBounds
        ColumnLayout {
            id: permCol
            width: Math.min(parent.width, 640); spacing: 12
            RowLayout {
                spacing: 16
                Image { source: "qrc:/images/app-tile.png"; sourceSize.width: 160; Layout.preferredWidth: 70; Layout.preferredHeight: 70
                        Image { anchors.centerIn: parent; width: 38; height: 38; source: "qrc:/images/app-archive.png"; sourceSize.width: 76 } }
                ColumnLayout {
                    spacing: 2
                    Label { text: "the internet archive wants to use the internet"; wrapMode: Text.WordWrap; Layout.fillWidth: true; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(26); weight: Font.Light } }
                    Label { text: "an app inside hop-tunes · off until you allow it"; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) } }
                }
            }
            Label { text: "it will only contact these sites, and only while this app is open:"; color: Theme.text2; wrapMode: Text.WordWrap; Layout.fillWidth: true
                    font { family: Theme.font; pixelSize: Theme.fs(16) } }
            Repeater {
                model: root.hosts
                delegate: RowLayout {
                    required property var modelData
                    spacing: 12
                    Rectangle { width: 8; height: 8; radius: 4; color: Theme.accent; Layout.alignment: Qt.AlignVCenter }
                    Label { text: modelData[0]; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(16) } Layout.preferredWidth: 200 }
                    Label { text: modelData[1]; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true; font { family: Theme.font; pixelSize: Theme.fs(15) } }
                }
            }
            Label { text: "what is sent: what you ask for (a search word, an item you open) and your internet address, like any visitor. no hop-tunes details, no cookies, nothing is kept after you close the app, and nothing is downloaded to your disk: music and films are streamed, books are read from memory. you never need an archive.org account, and hop-tunes never signs in or uploads. your favourites stay on this computer. hop-tunes is not made or endorsed by the internet archive; what is there belongs to the people who made it, and each item says what its licence is."
                    color: Theme.text2; wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.topMargin: 4
                    font { family: Theme.font; pixelSize: Theme.fs(15) } }
            RowLayout {
                Layout.topMargin: 10; spacing: 12
                PlainButton { objectName: "iaAllow"; text: hop.guest ? "allow for this session" : "allow for this profile"; primary: true
                              onClicked: { hop.setAppNetAllowed("archive", true); root.allowed = true; root.ia.open() } }
                PlainButton { objectName: "iaNotNow"; text: "not now"; onClicked: root.leave() }
                Item { Layout.fillWidth: true }
            }
        }
    }

    // ------------------------------------------------------------------------------------------------------
    // the working app
    // ------------------------------------------------------------------------------------------------------
    Item {
        id: live
        anchors.fill: parent
        visible: !!root.ia && root.allowed

        // ---- browse ----
        ColumnLayout {
            objectName: "iaBrowse"
            anchors.fill: parent; visible: root.page === "browse"; spacing: 10
            Flow {
                Layout.fillWidth: true; spacing: 10
                TextField {
                    id: searchField
                    objectName: "iaSearch"
                    width: root.phone ? parent.width : 320; height: 36
                    placeholderText: "search music, films and books"
                    selectByMouse: true
                    font { family: Theme.font; pixelSize: Theme.fs(15) } color: Theme.text; placeholderTextColor: Theme.dim
                    background: Rectangle { color: Theme.panel; border { width: 1; color: Theme.border } }
                    onAccepted: root.runSearch(text)
                }
                PlainButton { objectName: "iaSearchGo"; text: "search"; implicitHeight: 36; onClicked: root.runSearch(searchField.text) }
                PlainButton { objectName: "iaFavouritesButton"; text: "favourites"; implicitHeight: 36; onClicked: root.page = "favourites" }
            }
            Flow {
                Layout.fillWidth: true; spacing: 8
                Repeater {
                    model: [["all", "everything"], ["audio", "music"], ["movies", "films"], ["texts", "books"]]
                    delegate: Chip {
                        required property var modelData
                        objectName: "iaKind_" + modelData[0]
                        text: modelData[1]; on: !!root.ia && root.ia.kind === modelData[0]
                        onClicked: root.ia.kind = modelData[0]
                    }
                }
            }
            // the curator first: his public favourites are listed before everyone else's. The banner is a button: it opens his own page, with all of his picks
            AbstractButton {
                id: curatorBanner
                objectName: "iaCurator"
                visible: !!root.ia && (root.ia.featuredCount > 0 || root.ia.curatorView)
                Layout.fillWidth: true; implicitHeight: curRow.implicitHeight + 20; padding: 10
                hoverEnabled: true; focusPolicy: Qt.NoFocus
                onClicked: { if (root.ia.curatorView) root.ia.search(""); else root.ia.showCurator() }
                Accessible.name: root.ia ? root.ia.curatorName + ", " + root.ia.curatorTagline + ": " + (root.ia.curatorView ? "back to everything" : "open his page") : ""
                background: Rectangle { radius: 8; color: curatorBanner.hovered ? Theme.hover : Theme.panel; border { width: 1; color: Theme.accent } }
                contentItem: RowLayout {
                    id: curRow
                    spacing: 12
                    Label { text: root.ia && root.ia.curatorView ? "‹" : "★"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(26) } }
                    Label { Layout.fillWidth: true; elide: Text.ElideRight; text: root.ia ? root.ia.curatorName + " · " + root.ia.curatorTagline : ""; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(17) } }
                    Label { text: root.ia && root.ia.curatorView ? "back" : "see all his picks ›"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(14) } }
                }
            }
            PlainButton { objectName: "iaCuratorOpen"; visible: !!root.ia && root.ia.curatorView; text: "his page on archive.org"; implicitHeight: 32; onClicked: Qt.openUrlExternally(root.ia.curatorUrl) }
            Label {
                objectName: "iaMessage"
                visible: !!root.ia && (root.ia.message !== "" || root.ia.loading)
                Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) }
                text: root.ia && root.ia.loading ? "loading…" : (root.ia ? root.ia.message : "")
            }
            GridView {
                id: grid
                objectName: "iaGrid"
                Layout.fillWidth: true; Layout.fillHeight: true
                clip: true; boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                readonly property int columns: Math.max(1, Math.floor(width / (root.phone ? 150 : 200)))
                cellWidth: width / columns; cellHeight: cellWidth + 62
                model: root.ia ? root.ia.items : []
                delegate: Item {
                    id: card
                    required property var modelData
                    required property int index
                    objectName: "iaCard_" + index
                    width: grid.cellWidth; height: grid.cellHeight
                    AbstractButton {
                        anchors { fill: parent; margins: 6 }
                        hoverEnabled: true; focusPolicy: Qt.NoFocus
                        onClicked: root.openItem(card.modelData.id)
                        Accessible.name: card.modelData.title + (card.modelData.creator !== "" ? " by " + card.modelData.creator : "")
                        background: Rectangle { color: parent.hovered ? Theme.hover : "transparent"; radius: 6 }
                        contentItem: Column {
                            spacing: 6
                            Rectangle {
                                width: parent.width; height: width; color: Theme.panel; radius: 6; clip: true
                                Image {
                                    objectName: "iaThumb_" + card.index
                                    anchors.fill: parent
                                    source: root.pic(card.modelData.thumb)
                                    fillMode: Image.PreserveAspectCrop; asynchronous: true; cache: false
                                    sourceSize.width: 360
                                }
                                Label { text: root.kindName(card.modelData.kind); color: "white"; leftPadding: 6; rightPadding: 6; topPadding: 2; bottomPadding: 2
                                        anchors { bottom: parent.bottom; left: parent.left; margins: 6 }
                                        background: Rectangle { radius: 4; color: "#cc000000" }
                                        font { family: Theme.font; pixelSize: Theme.fs(11); bold: true } }
                                Label { objectName: "iaFeatured_" + card.index; visible: card.modelData.featured === true; text: "★ " + (root.ia ? root.ia.curatorName : "")
                                        color: "white"; leftPadding: 6; rightPadding: 6; topPadding: 2; bottomPadding: 2
                                        anchors { top: parent.top; left: parent.left; margins: 6 }
                                        background: Rectangle { radius: 4; color: Theme.accent }
                                        font { family: Theme.font; pixelSize: Theme.fs(11); bold: true } }
                            }
                            Label { width: parent.width; text: card.modelData.title; elide: Text.ElideRight; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(14) } }
                            Label { width: parent.width; text: card.modelData.creator; elide: Text.ElideRight; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(12) } }
                        }
                    }
                }
                footer: Item {
                    width: grid.width; height: root.ia && root.ia.canLoadMore ? 56 : 0
                    PlainButton { objectName: "iaMore"; visible: !!root.ia && root.ia.canLoadMore; anchors.centerIn: parent; text: "more"; implicitHeight: 38; onClicked: root.ia.loadMore() }
                }
            }
        }

        // ---- one item: play it, watch it or read it ----
        Flickable {
            objectName: "iaItem"
            anchors.fill: parent; visible: root.page === "item"
            contentHeight: itemCol.implicitHeight + 20; clip: true; boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            ColumnLayout {
                id: itemCol
                width: parent.width; spacing: 10
                RowLayout {
                    Layout.fillWidth: true; spacing: 8
                    PlainButton { objectName: "iaBack"; text: root.phone ? "‹" : "‹ archive"; implicitHeight: 36; Layout.preferredWidth: root.phone ? 44 : -1; Layout.minimumWidth: 0; onClicked: root.goBack() }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 0
                        Label { objectName: "iaItemTitle"; Layout.fillWidth: true; elide: Text.ElideRight; text: root.hasDetail ? root.detail.title : (root.ia && root.ia.detailLoading ? "loading…" : ""); color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(20); weight: Font.Light } }
                        Label { objectName: "iaItemCreator"; Layout.fillWidth: true; elide: Text.ElideRight; text: root.hasDetail ? (root.detail.creator !== "" ? "by " + root.detail.creator : "") + (root.detail.year !== "" ? "  ·  " + root.detail.year : "") : ""; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(14) } }
                    }
                }

                // the picture / the sound: music and films play here
                Item {
                    id: stage
                    objectName: "iaStage"
                    visible: root.hasDetail && root.detail.kind !== "texts"
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.hasDetail && root.detail.kind === "movies" ? Math.min(width * 9 / 16, 420) : 150
                    Rectangle { anchors.fill: parent; color: "black"; radius: 8 }
                    Image {
                        visible: root.hasDetail && root.detail.kind === "audio"
                        anchors { left: parent.left; top: parent.top; bottom: parent.bottom; margins: 8 }
                        width: height; fillMode: Image.PreserveAspectCrop; asynchronous: true; cache: false; sourceSize.width: 320
                        source: root.hasDetail ? root.pic(root.detail.thumb) : ""
                    }
                    Label {
                        visible: root.playUrl === "" && root.hasDetail
                        anchors.centerIn: parent; width: parent.width - 40; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                        text: root.hasDetail && root.detail.kind === "movies" ? "pick a video below to watch it" : ""
                        color: Qt.rgba(1, 1, 1, 0.7); font { family: Theme.font; pixelSize: Theme.fs(15) }
                    }
                    KickPlayer {
                        id: player
                        objectName: "iaPlayer"
                        anchors { fill: parent; leftMargin: root.hasDetail && root.detail.kind === "audio" ? 150 : 0 }
                        appId: "archive"; vod: true
                        url: root.playUrl
                        onFullscreenToggled: fullscreen = !fullscreen
                    }
                }
                Label {
                    objectName: "iaNowPlaying"
                    visible: root.playUrl !== ""
                    Layout.fillWidth: true; elide: Text.ElideRight; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(15) }
                    text: "playing: " + root.playTitle
                }

                Flow {
                    Layout.fillWidth: true; spacing: 8
                    Chip { objectName: "iaFavourite"; visible: root.hasDetail
                           text: root.hasDetail && root.ia.isFavourite(root.detail.id) ? "★ favourite" : "☆ favourite"; on: root.hasDetail && root.ia.isFavourite(root.detail.id)
                           onClicked: root.ia.toggleFavourite({ id: root.detail.id, title: root.detail.title, creator: root.detail.creator, kind: root.detail.kind }) }
                    Chip { objectName: "iaRead"; visible: root.hasDetail && root.detail.kind === "texts" && (root.detail.epubUrl !== "" || root.detail.textUrl !== "")
                           text: "read it here"; on: true; onClicked: root.openBook() }
                    Chip { objectName: "iaOpenSite"; visible: root.hasDetail; text: "open on archive.org"; onClicked: Qt.openUrlExternally(root.detail.page) }
                    Chip { objectName: "iaStop"; visible: root.playUrl !== ""; text: "stop"; onClicked: root.stopPlaying() }
                }
                Label {
                    objectName: "iaNoBook"
                    visible: root.hasDetail && root.detail.kind === "texts" && root.detail.epubUrl === "" && root.detail.textUrl === ""
                    Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) }
                    text: "this book has no text version that can be read here" + (root.hasDetail && root.detail.pdfUrl !== "" ? " (only a scanned pdf): open it on archive.org to read it." : ".")
                }

                // music: the tracks, in order
                Repeater {
                    model: root.hasDetail && root.detail.kind === "audio" ? root.detail.tracks : []
                    delegate: AbstractButton {
                        id: trk
                        required property var modelData
                        required property int index
                        objectName: "iaTrack_" + index
                        Layout.fillWidth: true; implicitHeight: 40; hoverEnabled: true; focusPolicy: Qt.NoFocus
                        onClicked: root.play(trk.modelData.url, trk.modelData.title, trk.index)
                        background: Rectangle { radius: 6; color: root.trackIndex === trk.index ? Theme.hover : (trk.hovered ? Theme.hover : "transparent") }
                        contentItem: RowLayout {
                            spacing: 10
                            Label { text: root.trackIndex === trk.index ? "▶" : String(trk.index + 1); color: root.trackIndex === trk.index ? Theme.accent : Theme.dim; Layout.preferredWidth: 26; horizontalAlignment: Text.AlignRight; font { family: Theme.font; pixelSize: Theme.fs(14) } }
                            Label { Layout.fillWidth: true; elide: Text.ElideRight; text: trk.modelData.title; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(15) } }
                            Label { text: trk.modelData.length; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) } }
                        }
                    }
                }
                // films: the video files
                Repeater {
                    model: root.hasDetail && root.detail.kind === "movies" ? root.detail.videos : []
                    delegate: AbstractButton {
                        id: vid
                        required property var modelData
                        required property int index
                        objectName: "iaVideo_" + index
                        Layout.fillWidth: true; implicitHeight: 40; hoverEnabled: true; focusPolicy: Qt.NoFocus
                        onClicked: root.play(vid.modelData.url, vid.modelData.title, vid.index)
                        background: Rectangle { radius: 6; color: root.trackIndex === vid.index ? Theme.hover : (vid.hovered ? Theme.hover : "transparent") }
                        contentItem: RowLayout {
                            spacing: 10
                            Label { text: "▶"; color: root.trackIndex === vid.index ? Theme.accent : Theme.dim; Layout.preferredWidth: 26; horizontalAlignment: Text.AlignRight; font { family: Theme.font; pixelSize: Theme.fs(14) } }
                            Label { Layout.fillWidth: true; elide: Text.ElideRight; text: vid.modelData.title; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(15) } }
                        }
                    }
                }
                Label {
                    objectName: "iaNoMedia"
                    visible: root.hasDetail && ((root.detail.kind === "audio" && root.detail.tracks.length === 0) || (root.detail.kind === "movies" && root.detail.videos.length === 0))
                    Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) }
                    text: "nothing in this item can be played here: open it on archive.org."
                }
                Label {
                    objectName: "iaItemInfo"
                    visible: root.hasDetail
                    Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(13) }
                    text: root.hasDetail ? ((root.detail.license !== "" ? "licence: " + root.detail.license : "all rights stay with their owners unless the item says otherwise")
                                            + (root.detail.rights !== "" ? "\n" + root.detail.rights : "") + (root.detail.description !== "" ? "\n\n" + root.detail.description : "")) : ""
                }
            }
        }

        // ---- the reader ----
        ColumnLayout {
            objectName: "iaReader"
            anchors.fill: parent; visible: root.page === "read"; spacing: 8
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                PlainButton { objectName: "iaReadBack"; text: root.phone ? "‹" : "‹ book"; implicitHeight: 36; Layout.preferredWidth: root.phone ? 44 : -1; Layout.minimumWidth: 0; onClicked: root.goBack() }
                Label { objectName: "iaBookTitle"; Layout.fillWidth: true; elide: Text.ElideRight; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(18); weight: Font.Light }
                        text: root.ia && root.ia.bookReady ? root.ia.bookTitle : (root.ia && root.ia.bookLoading ? "loading the book…" : "") }
                PlainButton { objectName: "iaSmaller"; text: "A−"; implicitHeight: 36; Layout.preferredWidth: 44; Layout.minimumWidth: 0; onClicked: root.readSize = Math.max(12, root.readSize - 2) }
                PlainButton { objectName: "iaBigger"; text: "A+"; implicitHeight: 36; Layout.preferredWidth: 44; Layout.minimumWidth: 0; onClicked: root.readSize = Math.min(34, root.readSize + 2) }
            }
            Label {
                objectName: "iaBookError"
                visible: !!root.ia && root.ia.bookError !== ""
                Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(15) }
                text: root.ia ? root.ia.bookError : ""
            }
            Flickable {
                id: pageFlick
                objectName: "iaPageFlick"
                visible: !!root.ia && root.ia.bookReady
                Layout.fillWidth: true; Layout.fillHeight: true
                contentHeight: pageText.implicitHeight + 24; clip: true; boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                Text {
                    id: pageText
                    objectName: "iaPageText"
                    width: Math.min(parent.width - 16, 760); x: Math.max(0, (parent.width - width) / 2)
                    wrapMode: Text.WordWrap; textFormat: Text.RichText
                    color: Theme.text; linkColor: Theme.accent
                    font { family: Theme.font; pixelSize: root.readSize }
                    text: root.ia && root.ia.bookReady ? root.ia.chapterHtml(root.chapter) : ""
                    onTextChanged: pageFlick.contentY = 0
                }
            }
            RowLayout {
                visible: !!root.ia && root.ia.bookReady
                Layout.fillWidth: true; spacing: 8
                PlainButton { objectName: "iaPrevPage"; text: "‹"; implicitHeight: 36; Layout.preferredWidth: 52; Layout.minimumWidth: 0; enabled: root.chapter > 0; onClicked: root.chapter = Math.max(0, root.chapter - 1) }
                Label { objectName: "iaPageNumber"; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) }
                        text: root.ia && root.ia.bookReady ? root.ia.chapterTitle(root.chapter) + "  ·  " + (root.chapter + 1) + " of " + root.ia.bookChapters : "" }
                PlainButton { objectName: "iaNextPage"; text: "›"; implicitHeight: 36; Layout.preferredWidth: 52; Layout.minimumWidth: 0; enabled: !!root.ia && root.chapter < root.ia.bookChapters - 1; onClicked: root.chapter = Math.min(root.ia.bookChapters - 1, root.chapter + 1) }
            }
        }

        // ---- favourites ----
        Flickable {
            objectName: "iaFavourites"
            anchors.fill: parent; visible: root.page === "favourites"
            contentHeight: favCol.implicitHeight + 20; clip: true; boundsBehavior: Flickable.StopAtBounds
            ColumnLayout {
                id: favCol
                width: parent.width; spacing: 10
                RowLayout {
                    spacing: 12; Layout.fillWidth: true
                    PlainButton { objectName: "iaFavBack"; text: root.phone ? "‹" : "‹ archive"; implicitHeight: 36; Layout.preferredWidth: root.phone ? 44 : -1; Layout.minimumWidth: 0; onClicked: root.goBack() }
                    Label { text: "favourites"; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(24); weight: Font.Light } }
                }
                Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) }
                        text: "these stay in your hop-tunes profile on this computer: nothing is sent to archive.org and a guest keeps none. only the title and the address are kept, never the music, film or book." }
                Label { visible: !!root.ia && root.ia.favourites.length === 0; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(14) } text: "none yet: star an item on its page" }
                Repeater {
                    model: root.ia ? root.ia.favourites : []
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true; spacing: 10
                        Label { Layout.fillWidth: true; elide: Text.ElideRight; text: modelData.title + (modelData.creator !== "" ? "  ·  " + modelData.creator : "") + "  ·  " + root.kindName(modelData.kind); color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(15) } }
                        PlainButton { objectName: "iaFavOpen_" + modelData.id; text: "open"; implicitHeight: 32; onClicked: { root.page = "item"; root.openItem(modelData.id) } }
                        PlainButton { objectName: "iaFavRemove_" + modelData.id; text: "remove"; implicitHeight: 32; onClicked: root.ia.toggleFavourite(modelData) }
                    }
                }
            }
        }
    }
}
