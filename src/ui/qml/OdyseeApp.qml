// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Odysee, an app inside Hop-tunes: browse, search and watch the videos on odysee.com, and keep your own follows, favourites, playlists, blocked
// channels and words and the 18+ switch on this computer. No account. Only after the person allowed it, only while it is open, and every
// address goes through the gateway (net/AppNet). Odysee's search is unofficial and can change or stop at any time; Hop-tunes is not made or
// endorsed by Odysee, and the videos belong to their creators: they are streamed, never saved.
FocusScope {
    id: root
    objectName: "odyseeApp"
    readonly property var od: hop.odysee
    property bool allowed: hop.appNetAllowed("odysee")
    property string page: "browse"               // browse | video | lists
    property var current: null                   // the video being watched (a video map, from a list or from your lists)
    property string playUrl: ""
    readonly property bool phone: Theme.compact
    signal leave()
    focus: true

    readonly property var hosts: [["api.na-backend.odysee.com", "finds videos (Odysee's public search)"],
                                  ["thumbs.odycdn.com", "the cover pictures"],
                                  ["*.odycdn.com", "the video files (addresses like player.odycdn.com)"]]
    function pic(url) { return url && url !== "" ? "image://odysee/" + encodeURIComponent(url) : "" }
    function dur(s) { if (!s || s <= 0) return ""; const h = Math.floor(s / 3600), m = Math.floor(s % 3600 / 60), x = s % 60; return (h > 0 ? h + ":" + (m < 10 ? "0" : "") : "") + m + ":" + (x < 10 ? "0" : "") + x }
    function openVideo(v) { current = v; page = "video"; playUrl = v.url && od.streamAllowed(v.url) ? v.url : ""; if (playUrl !== "" && hop.player.playing) hop.player.pause(); forceActiveFocus() }
    function runSearch(text) { playUrl = ""; page = "browse"; current = null; od.search(text) }

    Connections { target: hop; function onAppNetChanged() { root.allowed = hop.appNetAllowed("odysee") } }
    Component.onCompleted: { if (od && allowed) od.open(); Qt.callLater(function () { root.forceActiveFocus() }) }
    Component.onDestruction: { if (od) od.close() }

    function goBack() {
        if (player.fullscreen) { player.fullscreen = false; return true }
        if (page === "video") { playUrl = ""; page = "browse"; return true }
        if (page === "lists") { page = "browse"; return true }
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

    // ---- not built / not allowed yet ----
    Label {
        visible: !root.od
        anchors { left: parent.left; right: parent.right; top: parent.top }
        wrapMode: Text.WordWrap; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(16) }
        text: "odysee needs the internet and was left out of this version."
    }
    Flickable {
        objectName: "odPermission"
        visible: !!root.od && !root.allowed
        anchors.fill: parent
        contentHeight: permCol.implicitHeight; clip: true; boundsBehavior: Flickable.StopAtBounds
        ColumnLayout {
            id: permCol
            width: Math.min(parent.width, 640); spacing: 12
            RowLayout {
                spacing: 16
                Image { source: "qrc:/images/app-tile.png"; sourceSize.width: 160; Layout.preferredWidth: 70; Layout.preferredHeight: 70
                        Image { anchors.centerIn: parent; width: 38; height: 38; source: "qrc:/images/app-odysee.png"; sourceSize.width: 76 } }
                ColumnLayout {
                    spacing: 2
                    Label { text: "odysee wants to use the internet"; wrapMode: Text.WordWrap; Layout.fillWidth: true; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(26); weight: Font.Light } }
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
                    Label { text: modelData[0]; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(16) } Layout.preferredWidth: 220 }
                    Label { text: modelData[1]; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true; font { family: Theme.font; pixelSize: Theme.fs(15) } }
                }
            }
            Label { text: "what is sent: what you search for or open, and your internet address, like any visitor. the search is one small read-only request in a form odysee's search needs (hop-tunes sends nothing else this way). no hop-tunes details, no cookies, nothing is kept after you close the app, and no video is ever saved. you never need an odysee account, and hop-tunes never signs in, follows or posts for you: your follows, favourites, playlists and blocked lists stay on this computer. odysee's public search is unofficial and can change or stop at any time, and some videos may not play outside odysee's own site. hop-tunes is not made or endorsed by odysee, and the videos belong to their creators."
                    color: Theme.text2; wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.topMargin: 4
                    font { family: Theme.font; pixelSize: Theme.fs(15) } }
            RowLayout {
                Layout.topMargin: 10; spacing: 12
                PlainButton { objectName: "odAllow"; text: hop.guest ? "allow for this session" : "allow for this profile"; primary: true
                              onClicked: { hop.setAppNetAllowed("odysee", true); root.allowed = true; root.od.open() } }
                PlainButton { objectName: "odNotNow"; text: "not now"; onClicked: root.leave() }
                Item { Layout.fillWidth: true }
            }
        }
    }

    // ---- the working app ----
    Item {
        id: live
        anchors.fill: parent
        visible: !!root.od && root.allowed

        // browse
        ColumnLayout {
            objectName: "odBrowse"
            anchors.fill: parent; visible: root.page === "browse"; spacing: 10
            Flow {
                Layout.fillWidth: true; spacing: 10
                TextField {
                    id: searchField
                    objectName: "odSearch"
                    width: root.phone ? parent.width : 320; height: 36
                    placeholderText: "search videos, or @channel"
                    selectByMouse: true
                    font { family: Theme.font; pixelSize: Theme.fs(15) } color: Theme.text; placeholderTextColor: Theme.dim
                    background: Rectangle { color: Theme.panel; border { width: 1; color: Theme.border } }
                    onAccepted: root.runSearch(text)
                }
                PlainButton { objectName: "odSearchGo"; text: "search"; implicitHeight: 36; onClicked: root.runSearch(searchField.text) }
                PlainButton { objectName: "odTrending"; text: "trending"; implicitHeight: 36; onClicked: { searchField.text = ""; root.runSearch("") } }
                PlainButton { objectName: "odFollowing"; text: "following"; implicitHeight: 36; onClicked: { searchField.text = ""; root.playUrl = ""; root.page = "browse"; root.od.showFollowing() } }
                Chip { objectName: "odMature"; text: root.od && root.od.showMature ? "18+ shown" : "18+ hidden"; on: root.od && root.od.showMature
                       onClicked: root.od.showMature = !root.od.showMature }
                PlainButton { objectName: "odListsButton"; text: "my lists"; implicitHeight: 36; onClicked: root.page = "lists" }
            }
            Label {
                objectName: "odMessage"
                visible: !!root.od && (root.od.message !== "" || root.od.loading)
                Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) }
                text: root.od && root.od.loading ? "loading…" : (root.od ? root.od.message : "")
            }
            Label {
                objectName: "odHidden"
                visible: !!root.od && root.od.hiddenCount > 0
                Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) }
                text: root.od ? root.od.hiddenCount + " hidden by your filters (18+, blocked channels or words)" : ""
            }
            GridView {
                id: grid
                objectName: "odGrid"
                Layout.fillWidth: true; Layout.fillHeight: true
                clip: true; boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                readonly property int columns: Math.max(1, Math.floor(width / (root.phone ? 160 : 240)))
                cellWidth: width / columns; cellHeight: cellWidth * 0.5625 + 62
                model: root.od ? root.od.videos : []
                delegate: Item {
                    id: card
                    required property var modelData
                    required property int index
                    objectName: "odCard_" + index
                    width: grid.cellWidth; height: grid.cellHeight
                    AbstractButton {
                        anchors { fill: parent; margins: 6 }
                        hoverEnabled: true; focusPolicy: Qt.NoFocus
                        onClicked: root.openVideo(card.modelData)
                        Accessible.name: card.modelData.title + " by " + card.modelData.channel
                        background: Rectangle { color: parent.hovered ? Theme.hover : "transparent"; radius: 6 }
                        contentItem: Column {
                            spacing: 6
                            Rectangle {
                                width: parent.width; height: width * 0.5625; color: Theme.panel; radius: 6; clip: true
                                Image {
                                    objectName: "odThumb_" + card.index
                                    anchors.fill: parent
                                    source: root.pic(card.modelData.thumb)
                                    fillMode: Image.PreserveAspectCrop; asynchronous: true; cache: false
                                    sourceSize.width: 480
                                }
                                Label { visible: card.modelData.duration > 0; text: root.dur(card.modelData.duration); color: "white"; leftPadding: 5; rightPadding: 5; topPadding: 1; bottomPadding: 1
                                        anchors { bottom: parent.bottom; right: parent.right; margins: 6 }
                                        background: Rectangle { radius: 4; color: "#cc000000" }
                                        font { family: Theme.font; pixelSize: Theme.fs(11); bold: true } }
                                Label { visible: card.modelData.mature; text: "18+"; color: "white"; leftPadding: 6; rightPadding: 6; topPadding: 2; bottomPadding: 2
                                        anchors { top: parent.top; right: parent.right; margins: 6 }
                                        background: Rectangle { radius: 4; color: "#cc000000" }
                                        font { family: Theme.font; pixelSize: Theme.fs(11); bold: true } }
                            }
                            Label { width: parent.width; text: card.modelData.title; elide: Text.ElideRight; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(14) } }
                            Label { width: parent.width; text: card.modelData.channel; elide: Text.ElideRight; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(12) } }
                        }
                    }
                }
                footer: Item {
                    width: grid.width; height: root.od && root.od.canLoadMore ? 56 : 0
                    PlainButton { objectName: "odMore"; visible: !!root.od && root.od.canLoadMore; anchors.centerIn: parent; text: "more"; implicitHeight: 38; onClicked: root.od.loadMore() }
                }
            }
        }

        // one video
        Flickable {
            objectName: "odVideo"
            anchors.fill: parent; visible: root.page === "video" && root.current !== null
            contentHeight: vidCol.implicitHeight + 20; clip: true; boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            ColumnLayout {
                id: vidCol
                width: parent.width; spacing: 10
                RowLayout {
                    Layout.fillWidth: true; spacing: 8
                    PlainButton { objectName: "odBack"; text: root.phone ? "‹" : "‹ videos"; implicitHeight: 36; Layout.preferredWidth: root.phone ? 44 : -1; Layout.minimumWidth: 0; onClicked: root.goBack() }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 0
                        Label { objectName: "odVideoTitle"; Layout.fillWidth: true; elide: Text.ElideRight; text: root.current ? root.current.title : ""; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(20); weight: Font.Light } }
                        Label { objectName: "odVideoChannel"; Layout.fillWidth: true; elide: Text.ElideRight; text: root.current ? root.current.channel : ""; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(14) }
                                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { searchField.text = root.current.channel; root.runSearch(root.current.channel) } } }
                    }
                }
                Item {
                    id: stage
                    objectName: "odStage"
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(width * 9 / 16, 460)
                    Rectangle { anchors.fill: parent; color: "black"; radius: 8 }
                    KickPlayer {
                        id: player
                        objectName: "odPlayer"
                        anchors.fill: parent
                        appId: "odysee"; vod: true
                        url: root.playUrl
                        onFullscreenToggled: fullscreen = !fullscreen
                    }
                    Label {
                        objectName: "odNoFile"
                        visible: root.playUrl === ""
                        anchors.centerIn: parent; width: parent.width - 40; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                        text: "this video has no file that can be played here: open it on odysee.com"
                        color: Qt.rgba(1, 1, 1, 0.75); font { family: Theme.font; pixelSize: Theme.fs(15) }
                    }
                }
                Label {
                    objectName: "odPlayHelp"
                    visible: root.playUrl !== "" && player.error !== ""
                    Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) }
                    text: "odysee did not give this video to hop-tunes (some videos only play on odysee's own site). you can open it there with the button below."
                }
                Flow {
                    Layout.fillWidth: true; spacing: 8
                    Chip { objectName: "odFollow"; text: root.current && root.od.isFollowing(root.current.channel) ? "✓ following" : "follow channel"; on: !!root.current && root.od.isFollowing(root.current.channel)
                           visible: !!root.current && root.current.channel !== ""
                           onClicked: root.od.toggleFollow(root.current.channel, root.current.channelId) }
                    Chip { objectName: "odFavourite"; text: root.current && root.od.isFavourite(root.current.id) ? "★ favourite" : "☆ favourite"; on: !!root.current && root.od.isFavourite(root.current.id)
                           onClicked: root.od.toggleFavourite(root.current) }
                    Chip { objectName: "odBlockChannel"; text: "block this channel"; visible: !!root.current && root.current.channel !== ""
                           onClicked: { const c = root.current.channel; root.playUrl = ""; root.page = "browse"; root.od.blockChannel(c) } }
                    Chip { objectName: "odOpenSite"; text: "open on odysee.com"; onClicked: Qt.openUrlExternally(root.current.page) }
                }
                Flow {
                    visible: !!root.od && root.od.playlists.length > 0
                    Layout.fillWidth: true; spacing: 8
                    Label { text: "playlists:"; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) } }
                    Repeater {
                        model: root.od ? root.od.playlists : []
                        delegate: Chip {
                            required property var modelData
                            objectName: "odPlaylistToggle_" + modelData.name
                            readonly property bool inIt: !!root.current && root.od.inPlaylist(modelData.name, root.current.id) && modelData.videos.length >= 0
                            text: (inIt ? "✓ " : "+ ") + modelData.name; on: inIt
                            onClicked: root.od.togglePlaylistVideo(modelData.name, root.current)
                        }
                    }
                }
            }
        }

        // my lists
        Flickable {
            objectName: "odLists"
            anchors.fill: parent; visible: root.page === "lists"
            contentHeight: listsCol.implicitHeight + 20; clip: true; boundsBehavior: Flickable.StopAtBounds
            ColumnLayout {
                id: listsCol
                width: parent.width; spacing: 10
                RowLayout {
                    spacing: 12; Layout.fillWidth: true
                    PlainButton { objectName: "odListsBack"; text: root.phone ? "‹" : "‹ videos"; implicitHeight: 36; Layout.preferredWidth: root.phone ? 44 : -1; Layout.minimumWidth: 0; onClicked: root.goBack() }
                    Label { text: "my lists"; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(24); weight: Font.Light } }
                }
                Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) }
                        text: "these stay in your hop-tunes profile on this computer: nothing is sent to odysee and a guest keeps none. only titles and addresses are kept, never a video." }

                Label { text: "following"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(18) } Layout.topMargin: 6 }
                Label { visible: !!root.od && root.od.follows.length === 0; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) } text: "none yet: follow a channel from a video's page" }
                Flow {
                    Layout.fillWidth: true; spacing: 8
                    Repeater { model: root.od ? root.od.follows : []
                               delegate: Chip { required property var modelData; objectName: "odFollowed_" + modelData.name; text: modelData.name; onClicked: { searchField.text = modelData.name; root.runSearch(modelData.name) } } }
                }

                Label { text: "playlists"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(18) } Layout.topMargin: 10 }
                RowLayout {
                    Layout.fillWidth: true; spacing: 8
                    TextField { id: plField; objectName: "odPlaylistField"; Layout.fillWidth: true; Layout.maximumWidth: 320; placeholderText: "a new playlist's name"; selectByMouse: true
                                font { family: Theme.font; pixelSize: Theme.fs(15) } color: Theme.text; placeholderTextColor: Theme.dim
                                background: Rectangle { color: Theme.panel; border { width: 1; color: Theme.border } }
                                onAccepted: { if (root.od.createPlaylist(text)) text = "" } }
                    PlainButton { objectName: "odPlaylistAdd"; text: "create"; implicitHeight: 36; onClicked: { if (root.od.createPlaylist(plField.text)) plField.clear() } }
                }
                Repeater {
                    model: root.od ? root.od.playlists : []
                    delegate: ColumnLayout {
                        id: pl
                        required property var modelData
                        Layout.fillWidth: true; spacing: 4
                        RowLayout {
                            Layout.fillWidth: true
                            Label { Layout.fillWidth: true; elide: Text.ElideRight; text: pl.modelData.name + "  ·  " + pl.modelData.videos.length + " videos"; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(16) } }
                            PlainButton { objectName: "odPlaylistDelete_" + pl.modelData.name; text: "delete"; implicitHeight: 30; onClicked: root.od.deletePlaylist(pl.modelData.name) }
                        }
                        Repeater {
                            model: pl.modelData.videos
                            delegate: RowLayout {
                                required property var modelData
                                Layout.fillWidth: true; Layout.leftMargin: 16; spacing: 10
                                Label { Layout.fillWidth: true; elide: Text.ElideRight; text: modelData.title + (modelData.channel !== "" ? "  ·  " + modelData.channel : ""); color: Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(14) } }
                                PlainButton { text: "watch"; implicitHeight: 30; onClicked: root.openVideo(modelData) }
                            }
                        }
                    }
                }

                Label { text: "favourites"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(18) } Layout.topMargin: 10 }
                Label { visible: !!root.od && root.od.favourites.length === 0; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) } text: "none yet: star a video" }
                Repeater {
                    model: root.od ? root.od.favourites : []
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true; spacing: 10
                        Label { Layout.fillWidth: true; elide: Text.ElideRight; text: modelData.title + (modelData.channel !== "" ? "  ·  " + modelData.channel : ""); color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(15) } }
                        PlainButton { objectName: "odFavWatch_" + modelData.id; text: "watch"; implicitHeight: 32; onClicked: root.openVideo(modelData) }
                        PlainButton { objectName: "odFavRemove_" + modelData.id; text: "remove"; implicitHeight: 32; onClicked: root.od.toggleFavourite(modelData) }
                    }
                }

                Label { text: "blocked words"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(18) } Layout.topMargin: 10 }
                RowLayout {
                    Layout.fillWidth: true; spacing: 8
                    TextField { id: wordField; objectName: "odBlockWordField"; Layout.fillWidth: true; Layout.maximumWidth: 320; placeholderText: "a word to hide"; selectByMouse: true
                                font { family: Theme.font; pixelSize: Theme.fs(15) } color: Theme.text; placeholderTextColor: Theme.dim
                                background: Rectangle { color: Theme.panel; border { width: 1; color: Theme.border } }
                                onAccepted: { if (root.od.blockWord(text)) text = "" } }
                    PlainButton { objectName: "odBlockWordAdd"; text: "block"; implicitHeight: 36; onClicked: { if (root.od.blockWord(wordField.text)) wordField.clear() } }
                }
                Flow {
                    Layout.fillWidth: true; spacing: 8
                    Repeater { model: root.od ? root.od.blockedWords : []
                               delegate: Chip { required property string modelData; objectName: "odBlockedWord_" + modelData; text: modelData + "  ×"; on: true; onClicked: root.od.unblockWord(modelData) } }
                }
                Label { text: "blocked channels"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(18) } Layout.topMargin: 10 }
                Label { visible: !!root.od && root.od.blockedChannels.length === 0; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) } text: "none: block a channel from a video's page" }
                Flow {
                    Layout.fillWidth: true; spacing: 8
                    Repeater { model: root.od ? root.od.blockedChannels : []
                               delegate: Chip { required property string modelData; objectName: "odBlockedChannel_" + modelData; text: modelData + "  ×"; on: true; onClicked: root.od.unblockChannel(modelData) } }
                }
            }
        }
    }
}
