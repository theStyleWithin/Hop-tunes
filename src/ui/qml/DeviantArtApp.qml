// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// DeviantArt, an app inside Hop-tunes: browse artwork from DeviantArt's public feeds (no account, nothing to register), look at it full size,
// and keep your own favourites, blocked artists, blocked words and the 18+ switch on this computer. Only after the person allowed it, only
// while it is open, and every address goes through the gateway (net/AppNet). Hop-tunes is not made or endorsed by DeviantArt; the pictures
// belong to their artists: they are shown, never saved.
FocusScope {
    id: root
    objectName: "deviantArtApp"
    readonly property var da: hop.deviantart
    property bool allowed: hop.appNetAllowed("deviantart")
    property string page: "browse"               // browse | work | lists
    property int current: -1                     // the work being looked at (an index into da.works)
    property bool slideshow: false
    readonly property bool phone: Theme.compact
    readonly property var work: da && current >= 0 && current < da.works.length ? da.works[current] : null
    signal leave()
    focus: true

    readonly property var hosts: [["backend.deviantart.com", "finds artwork (the public feeds)"],
                                  ["wixmp.com", "the artwork pictures (addresses like images-wixmp-….wixmp.com)"]]
    function pic(url) { return url && url !== "" ? "image://deviantart/" + encodeURIComponent(url) : "" }
    function openWork(i) { current = i; page = "work"; slideshow = false; forceActiveFocus() }
    function step(d) { if (!da || da.works.length === 0) return; current = (current + d + da.works.length) % da.works.length }
    function runSearch(text) { page = "browse"; current = -1; da.search(text) }

    Connections { target: hop; function onAppNetChanged() { root.allowed = hop.appNetAllowed("deviantart") } }
    Connections {
        target: root.da
        function onWorksChanged() { if (root.current >= root.da.works.length) root.current = root.da.works.length - 1; if (root.current < 0 && root.page === "work") root.page = "browse" }
    }
    Component.onCompleted: { if (da && allowed) da.open(); Qt.callLater(function () { root.forceActiveFocus() }) }
    Component.onDestruction: { if (da) da.close() }

    // one step back inside the app (the host asks first); false = nothing to step back from, leave the app
    function goBack() {
        if (page === "work") { page = "browse"; slideshow = false; return true }
        if (page === "lists") { page = "browse"; return true }
        return false
    }
    Keys.onPressed: function (e) {
        if (page !== "work") return
        if (e.key === Qt.Key_Right) { step(1); e.accepted = true }
        else if (e.key === Qt.Key_Left) { step(-1); e.accepted = true }
        else if (e.key === Qt.Key_Space) { slideshow = !slideshow; e.accepted = true }
    }
    Timer { interval: 5000; repeat: true; running: root.slideshow && root.page === "work"; onTriggered: root.step(1) }

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
        visible: !root.da
        anchors { left: parent.left; right: parent.right; top: parent.top }
        wrapMode: Text.WordWrap; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(16) }
        text: "deviantart needs the internet and was left out of this version."
    }
    Flickable {
        id: permission
        objectName: "daPermission"
        visible: !!root.da && !root.allowed
        anchors.fill: parent
        contentHeight: permCol.implicitHeight; clip: true; boundsBehavior: Flickable.StopAtBounds
        ColumnLayout {
            id: permCol
            width: Math.min(parent.width, 640); spacing: 12
            RowLayout {
                spacing: 16
                Image { source: "qrc:/images/app-tile.png"; sourceSize.width: 160; Layout.preferredWidth: 70; Layout.preferredHeight: 70
                        Image { anchors.centerIn: parent; width: 38; height: 38; source: "qrc:/images/app-deviantart.png"; sourceSize.width: 76 } }
                ColumnLayout {
                    spacing: 2
                    Label { text: "deviantart wants to use the internet"; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(26); weight: Font.Light } }
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
            Label { text: "what is sent: what you ask for (a search word, an artist's name) and your internet address, like any visitor. no hop-tunes details, no cookies, nothing is kept after you close the app, and no picture is ever saved. you never need a deviantart account, and hop-tunes never signs in, follows or posts for you: your favourites and blocked lists stay on this computer. hop-tunes is not made or endorsed by deviantart, the pictures belong to their artists, and the public feeds can change or stop at any time."
                    color: Theme.text2; wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.topMargin: 4
                    font { family: Theme.font; pixelSize: Theme.fs(15) } }
            RowLayout {
                Layout.topMargin: 10; spacing: 12
                PlainButton { objectName: "daAllow"; text: hop.guest ? "allow for this session" : "allow for this profile"; primary: true
                              onClicked: { hop.setAppNetAllowed("deviantart", true); root.allowed = true; root.da.open() } }
                PlainButton { objectName: "daNotNow"; text: "not now"; onClicked: root.leave() }
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
        visible: !!root.da && root.allowed

        // ---- browse ----
        ColumnLayout {
            objectName: "daBrowse"
            anchors.fill: parent; visible: root.page === "browse"; spacing: 10
            Flow {
                Layout.fillWidth: true; spacing: 10
                TextField {
                    id: searchField
                    objectName: "daSearch"
                    width: root.phone ? parent.width : 320; height: 36
                    placeholderText: "search artwork, or @artist"
                    selectByMouse: true
                    font { family: Theme.font; pixelSize: Theme.fs(15) } color: Theme.text; placeholderTextColor: Theme.dim
                    background: Rectangle { color: Theme.panel; border { width: 1; color: Theme.border } }
                    onAccepted: root.runSearch(text)
                }
                PlainButton { objectName: "daSearchGo"; text: "search"; implicitHeight: 36; onClicked: root.runSearch(searchField.text) }
                PlainButton { objectName: "daPopular"; text: "popular"; implicitHeight: 36; onClicked: { searchField.text = ""; root.runSearch("") } }
                Chip { objectName: "daMature"; text: root.da && root.da.showMature ? "18+ shown" : "18+ hidden"; on: root.da && root.da.showMature
                       onClicked: root.da.showMature = !root.da.showMature }
                PlainButton { objectName: "daListsButton"; text: "my lists"; implicitHeight: 36; onClicked: root.page = "lists" }
            }
            Label {
                objectName: "daMessage"
                visible: !!root.da && (root.da.message !== "" || root.da.loading)
                Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) }
                text: root.da && root.da.loading ? "loading…" : (root.da ? root.da.message : "")
            }
            Label {
                objectName: "daHidden"
                visible: !!root.da && root.da.hiddenCount > 0
                Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) }
                text: root.da ? root.da.hiddenCount + " hidden by your filters (18+, blocked artists or words)" : ""
            }
            GridView {
                id: grid
                objectName: "daGrid"
                Layout.fillWidth: true; Layout.fillHeight: true
                clip: true; boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                readonly property int columns: Math.max(1, Math.floor(width / (root.phone ? 160 : 220)))
                cellWidth: width / columns; cellHeight: cellWidth * 0.8 + 52
                model: root.da ? root.da.works : []
                delegate: Item {
                    id: card
                    required property var modelData
                    required property int index
                    objectName: "daCard_" + index
                    width: grid.cellWidth; height: grid.cellHeight
                    AbstractButton {
                        anchors { fill: parent; margins: 6 }
                        hoverEnabled: true; focusPolicy: Qt.NoFocus
                        onClicked: root.openWork(card.index)
                        Accessible.name: card.modelData.title + " by " + card.modelData.artist
                        background: Rectangle { color: parent.hovered ? Theme.hover : "transparent"; radius: 6 }
                        contentItem: Column {
                            spacing: 6
                            Rectangle {
                                width: parent.width; height: card.width * 0.8 - 12; color: Theme.panel; radius: 6; clip: true
                                Image {
                                    objectName: "daThumb_" + card.index
                                    anchors.fill: parent
                                    source: root.pic(card.modelData.thumb)
                                    fillMode: Image.PreserveAspectCrop; asynchronous: true; cache: false
                                    sourceSize.width: 400
                                }
                                Label { visible: card.modelData.mature; text: "18+"; color: "white"; leftPadding: 6; rightPadding: 6; topPadding: 2; bottomPadding: 2
                                        anchors { top: parent.top; right: parent.right; margins: 6 }
                                        background: Rectangle { radius: 4; color: "#cc000000" }
                                        font { family: Theme.font; pixelSize: Theme.fs(11); bold: true } }
                            }
                            Label { width: parent.width; text: card.modelData.title; elide: Text.ElideRight; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(14) } }
                            Label { width: parent.width; text: card.modelData.artist; elide: Text.ElideRight; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(12) } }
                        }
                    }
                }
                footer: Item {
                    width: grid.width; height: root.da && root.da.canLoadMore ? 56 : 0
                    PlainButton { objectName: "daMore"; visible: !!root.da && root.da.canLoadMore; anchors.centerIn: parent; text: "more"; implicitHeight: 38; onClicked: root.da.loadMore() }
                }
            }
        }

        // ---- one work, full size ----
        ColumnLayout {
            objectName: "daWork"
            anchors.fill: parent; visible: root.page === "work" && root.work !== null; spacing: 8
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                PlainButton { objectName: "daBack"; text: root.phone ? "‹" : "‹ artwork"; implicitHeight: 36; Layout.preferredWidth: root.phone ? 44 : -1; Layout.minimumWidth: 0; onClicked: root.goBack() }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 0
                    Label { objectName: "daWorkTitle"; Layout.fillWidth: true; elide: Text.ElideRight; text: root.work ? root.work.title : ""; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(20); weight: Font.Light } }
                    Label { objectName: "daWorkArtist"; Layout.fillWidth: true; elide: Text.ElideRight; text: root.work ? "by " + root.work.artist : ""; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(14) }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { searchField.text = "@" + root.work.artist; root.runSearch("@" + root.work.artist) } } }
                }
                PlainButton { objectName: "daPrev"; text: "‹"; implicitHeight: 36; Layout.preferredWidth: 44; Layout.minimumWidth: 0; onClicked: root.step(-1) }
                PlainButton { objectName: "daNext"; text: "›"; implicitHeight: 36; Layout.preferredWidth: 44; Layout.minimumWidth: 0; onClicked: root.step(1) }
            }
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true; color: "black"; radius: 8; clip: true
                Image {
                    id: big
                    objectName: "daWorkImage"
                    anchors.fill: parent
                    source: root.work ? root.pic(root.work.image) : ""
                    fillMode: Image.PreserveAspectFit; asynchronous: true; cache: false
                }
                Label { anchors.centerIn: parent; visible: big.status === Image.Loading; text: "loading…"; color: Qt.rgba(1, 1, 1, 0.7); font { family: Theme.font; pixelSize: Theme.fs(16) } }
                Label { anchors.centerIn: parent; visible: big.status === Image.Error; text: "this picture could not be loaded"; color: Qt.rgba(1, 1, 1, 0.7); font { family: Theme.font; pixelSize: Theme.fs(16) } }
            }
            Flow {
                Layout.fillWidth: true; spacing: 8
                Chip { objectName: "daFavourite"; text: root.work && root.da.isFavouriteWork(root.work.id) ? "★ favourite" : "☆ favourite"; on: !!root.work && root.da.isFavouriteWork(root.work.id)
                       onClicked: root.da.toggleFavouriteWork(root.work) }
                Chip { objectName: "daFavouriteArtist"; text: root.work && root.da.isFavouriteArtist(root.work.artist) ? "★ artist" : "☆ artist"; on: !!root.work && root.da.isFavouriteArtist(root.work.artist)
                       onClicked: root.da.toggleFavouriteArtist(root.work.artist) }
                Chip { objectName: "daBlockArtist"; text: "block this artist"; onClicked: { const a = root.work.artist; root.page = "browse"; root.da.blockArtist(a) } }
                Chip { objectName: "daSlideshow"; text: root.slideshow ? "stop slideshow" : "slideshow"; on: root.slideshow; onClicked: root.slideshow = !root.slideshow }
                Chip { objectName: "daOpenSite"; text: "open on deviantart.com"; onClicked: Qt.openUrlExternally(root.work.page) }
            }
            Label {
                objectName: "daWorkInfo"
                Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(13) }
                text: root.work ? (root.work.copyright + (root.work.license !== "" ? " · licence: " + root.work.license : " · all rights reserved unless the artist says otherwise") + (root.work.description !== "" ? "\n" + root.work.description : "")) : ""
            }
        }

        // ---- my lists: blocked artists and words, favourites ----
        Flickable {
            objectName: "daLists"
            anchors.fill: parent; visible: root.page === "lists"
            contentHeight: listsCol.implicitHeight + 20; clip: true; boundsBehavior: Flickable.StopAtBounds
            ColumnLayout {
                id: listsCol
                width: parent.width; spacing: 10
                RowLayout {
                    spacing: 12; Layout.fillWidth: true
                    PlainButton { objectName: "daListsBack"; text: root.phone ? "‹" : "‹ artwork"; implicitHeight: 36; Layout.preferredWidth: root.phone ? 44 : -1; Layout.minimumWidth: 0; onClicked: root.goBack() }
                    Label { text: "my lists"; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(24); weight: Font.Light } }
                }
                Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) }
                        text: "these lists stay in your hop-tunes profile on this computer: nothing is sent to deviantart and a guest keeps none." }

                Label { text: "blocked words"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(18) } Layout.topMargin: 6 }
                Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) }
                        text: "a work is hidden when one of these words is in its title or description, and searching one is refused. the feed carries no tag list, so this is a word filter, not a perfect tag filter." }
                RowLayout {
                    Layout.fillWidth: true; spacing: 8
                    TextField { id: wordField; objectName: "daBlockWordField"; Layout.fillWidth: true; Layout.maximumWidth: 320; placeholderText: "a word to hide"; selectByMouse: true
                                font { family: Theme.font; pixelSize: Theme.fs(15) } color: Theme.text; placeholderTextColor: Theme.dim
                                background: Rectangle { color: Theme.panel; border { width: 1; color: Theme.border } }
                                onAccepted: { if (root.da.blockTag(text)) text = "" } }
                    PlainButton { objectName: "daBlockWordAdd"; text: "block"; implicitHeight: 36; onClicked: { if (root.da.blockTag(wordField.text)) wordField.clear() } }
                }
                Flow {
                    Layout.fillWidth: true; spacing: 8
                    Repeater { model: root.da ? root.da.blockedTags : []
                               delegate: Chip { required property string modelData; objectName: "daBlockedWord_" + modelData; text: modelData + "  ×"; on: true; onClicked: root.da.unblockTag(modelData) } }
                }

                Label { text: "blocked artists"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(18) } Layout.topMargin: 10 }
                Label { visible: !!root.da && root.da.blockedArtists.length === 0; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) } text: "none: block an artist from a work's page" }
                Flow {
                    Layout.fillWidth: true; spacing: 8
                    Repeater { model: root.da ? root.da.blockedArtists : []
                               delegate: Chip { required property string modelData; objectName: "daBlockedArtist_" + modelData; text: modelData + "  ×"; on: true; onClicked: root.da.unblockArtist(modelData) } }
                }

                Label { text: "favourite artists"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(18) } Layout.topMargin: 10 }
                Label { visible: !!root.da && root.da.favouriteArtists.length === 0; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) } text: "none yet: star an artist on a work's page" }
                Flow {
                    Layout.fillWidth: true; spacing: 8
                    Repeater { model: root.da ? root.da.favouriteArtists : []
                               delegate: Chip { required property string modelData; objectName: "daFavArtist_" + modelData; text: modelData; onClicked: { searchField.text = "@" + modelData; root.runSearch("@" + modelData) } } }
                }

                Label { text: "favourite works"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(18) } Layout.topMargin: 10 }
                Label { visible: !!root.da && root.da.favouriteWorks.length === 0; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) } text: "none yet: star a work. only its title and link are kept, never the picture" }
                Repeater {
                    model: root.da ? root.da.favouriteWorks : []
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true; spacing: 10
                        Label { Layout.fillWidth: true; elide: Text.ElideRight; text: modelData.title + (modelData.artist !== "" ? "  ·  " + modelData.artist : ""); color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(15) } }
                        PlainButton { text: "open on deviantart.com"; implicitHeight: 32; onClicked: Qt.openUrlExternally(modelData.page) }
                        PlainButton { objectName: "daFavWorkRemove_" + modelData.id; text: "remove"; implicitHeight: 32; onClicked: root.da.toggleFavouriteWork(modelData) }
                    }
                }
            }
        }
    }
}
