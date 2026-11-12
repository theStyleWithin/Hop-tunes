// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Kick, an app inside Hop-tunes: browse the live streams on kick.com and watch one, with a follow list and a block list that
// stay on this computer. It uses the internet only while it is open, only after the person allowed it, and nothing plays until a
// stream is picked. Everything it reaches goes through the gateway (net/AppNet): see the sites listed on the permission card.
FocusScope {
    id: root
    objectName: "kickApp"
    readonly property var kick: hop.kick
    property bool allowed: hop.appNetAllowed("kick")
    property string page: "browse"               // browse | watch | downloads | play | playlists | playlist | account | categories
    property bool disconnected: false
    // Android: switching to another app pauses the phone's video player and the stream's address (it carries a time-limited token) goes stale,
    // which showed "Demuxing failed" on coming back. So the stream is let go while the app is away and asked for again (with a fresh address) on return.
    property bool inBackground: false
    property bool videoFullscreen: false          // the picture fills the whole window (Esc / the back button / its own button leave it)
    // the small pop-out window (picture in picture, Android): allowed while a stream or a download is playing
    readonly property bool canPopOut: (livePlayer.started && livePlayer.error === "") || (localPlayer.started && localPlayer.error === "")
    onCanPopOutChanged: hop.setPictureInPictureAllowed(canPopOut, 16, 9)
    function setBackground(away) {
        if (inBackground === away) return
        inBackground = away
        if (!away && page === "watch" && kick && (kick.channel.slug || "") !== "") kick.openChannel(kick.channel.slug)
    }
    Connections {
        target: hop
        function onInPictureInPictureChanged() {
            if (hop.inPictureInPicture) return
            const s = Qt.application.state                                        // the small window was closed or expanded
            root.setBackground(Qt.platform.os === "android" && (s === Qt.ApplicationSuspended || s === Qt.ApplicationHidden))
        }
    }
    Connections {
        target: Qt.application
        function onStateChanged() {
            if (Qt.platform.os !== "android") return          // on a computer a stream keeps playing when the window loses focus
            const s = Qt.application.state
            root.setBackground((s === Qt.ApplicationSuspended || s === Qt.ApplicationHidden) && !hop.inPictureInPicture)    // popped out: the stream keeps playing
        }
    }
    property bool collapsed: false               // the menu above the streams (search, filters, categories) is pulled up out of the way
    readonly property bool collapsible: !!kick && allowed && !disconnected && (page === "browse" || page === "watch")
    function toggleCollapse() { collapsed = !collapsed }
    property string watchTab: "live"             // live | past | clips (on the watch page)
    // phones: a tap on a stream asks first, so a fat finger while scrolling does not open one (settings can turn that off)
    property var pending: null
    function cardTapped(m) {
        if (phone && kick.askBeforeOpening) pending = m
        else openChannel(m.slug)
    }
    property string playId: ""                   // the downloaded video being played
    property string playlistId: ""               // the playlist being looked at
    property string playlistMessage: ""
    property string deleteArmed: ""              // a playlist waiting for the second press of "delete"
    property string dlMessage: ""
    property int dlHeight: 720                   // download quality: up to this many lines of height (0 = the best there is)
    // what is already downloaded or on its way, by id (changes as downloads move)
    readonly property var dlById: {
        const m = ({}); const l = hop.downloads ? hop.downloads.items : []
        for (let i = 0; i < l.length; i++) m[l[i].id] = l[i]
        return m
    }
    function fmtDur(s) {
        s = Math.round(s)
        if (s >= 3600) return Math.floor(s / 3600) + "h " + Math.floor((s % 3600) / 60) + "m"
        if (s >= 60) return Math.floor(s / 60) + "m " + (s % 60) + "s"
        return s + " s"
    }
    function fmtBytes(n) {
        if (n >= 1073741824) return (n / 1073741824).toFixed(1) + " GB"
        if (n >= 1048576) return Math.round(n / 1048576) + " MB"
        return Math.max(1, Math.round(n / 1024)) + " KB"
    }
    // the recording in progress, if any
    readonly property var activeRecording: {
        const l = hop.downloads ? hop.downloads.items : []
        for (let i = 0; i < l.length; i++) if (l[i].kind === "recording" && l[i].state === "recording") return l[i]
        return null
    }
    property string recordMessage: ""
    function record() {
        const ch = kick.channel
        if (!ch.playbackUrl) { root.recordMessage = "this stream has no address to record"; return }
        root.recordMessage = hop.downloads.startRecording({ id: "rec-" + ch.slug + "-" + Qt.formatDateTime(new Date(), "yyyyMMdd-hhmmss"), title: ch.title || "", channel: ch.slug,
                                                            url: ch.playbackUrl, maxHeight: root.dlHeight })
    }
    function dlState(it) {
        if (!it) return ""
        const pc = Math.round((it.progress || 0) * 100)
        switch (it.state) {
        case "recording": return "recording " + fmtDur(it.seconds) + " \u00b7 " + fmtBytes(it.bytes)
        case "done": return "ready \u00b7 " + fmtBytes(it.bytes) + (it.error ? " \u00b7 " + it.error : "")
        case "running": return "downloading " + pc + "% \u00b7 " + fmtBytes(it.bytes)
        case "queued": return "waiting its turn"
        case "paused": return "paused at " + pc + "%"
        default: return it.error || it.state
        }
    }
    function download(entry) {
        root.dlMessage = hop.downloads.enqueue({ id: entry.id, kind: entry.kind, title: entry.title, channel: kick.channel.slug || "", thumb: entry.thumb,
                                                 url: entry.url, maxHeight: root.dlHeight, estimate: entry.estimate, seconds: entry.seconds })
    }    readonly property bool phone: Theme.compact   // a phone held upright: one column, touch-sized, no hover
    property string lastBlocked: ""
    signal leave()

    readonly property var hosts: [["kick.com", "finds the channel and its live stream"], ["web.kick.com", "the list of live streams"],
                                  ["images.kick.com", "the stream pictures"], ["stream.kick.com", "the developer\u2019s live picture"], ["files.kick.com", "profile pictures and emotes"], ["clips.kick.com", "clips, only when you download one"],
                                  ["live-video.net", "the video itself"]]

    Connections { target: hop; function onAppNetChanged() { root.allowed = hop.appNetAllowed("kick") } }
    Component.onCompleted: if (kick && allowed) kick.open()
    Component.onDestruction: { hop.setPictureInPictureAllowed(false, 16, 9); if (kick) kick.close() }
    onPageChanged: if (page !== "watch" && page !== "play") videoFullscreen = false

    // one step back inside the app (the host asks first); false = nothing to step back from, leave the app
    function goBack() {
        if (videoFullscreen) { videoFullscreen = false; return true }
        if (pending !== null) { pending = null; return true }
        if (page === "account") { kick.cancelSignIn(); page = "browse"; return true }
        if (page === "categories") { kick.searchCategories(""); page = "browse"; return true }
        if (page === "play") { page = "downloads"; playId = ""; return true }
        if (page === "playlist") { page = "playlists"; playlistId = ""; return true }
        if (page === "playlists") { page = "browse"; return true }
        if (page === "downloads") { page = "browse"; return true }
        if (page === "watch") { page = "browse"; kick.closeChannel(); watchTab = "live"; return true }
        return false
    }
    function openChannel(slug) { kick.openChannel(slug); watchTab = "live"; dlMessage = ""; recordMessage = ""; page = "watch" }
    // the phone's bottom "search" button, while this app is open: find a streamer here instead of searching the music
    function openSearch() {
        if (!kick || !allowed || disconnected) return false
        if (page !== "browse") { page = "browse"; kick.closeChannel(); watchTab = "live" }
        collapsed = false                                                    // searching brings the menu back down
        searchBox.forceActiveFocus()
        return true
    }
    readonly property bool searchActive: !!kick && allowed && !disconnected && page === "browse" && searchBox.activeFocus
    // "add to playlist": a menu of the playlists that can hold this (channels, clips or past streams); a tick means it is already in
    property var plChoices: []
    property var plEntry: ({})
    property string plKey: ""
    function addMenu(kind, entry, key, at) {
        plChoices = kick.playlists.filter(function (p) { return p.kind === kind })
        plEntry = Object.assign({}, entry, { channel: entry.channel || kick.channel.slug || "" }); plKey = key
        plMenu.popup(at)
    }
    function playlistById(id) {
        const l = kick.playlists
        for (let i = 0; i < l.length; i++) if (l[i].id === id) return l[i]
        return null
    }
    Menu {
        id: plMenu
        objectName: "kickPlaylistMenu"
        Repeater {
            model: root.plChoices
            delegate: MenuItem {
                required property var modelData
                objectName: "kickPlMenu_" + modelData.id
                text: (kick.isInPlaylist(modelData.id, root.plKey) ? "\u2713 " : "") + modelData.name
                onTriggered: {
                    if (kick.isInPlaylist(modelData.id, root.plKey)) kick.removeFromPlaylist(modelData.id, root.plKey)
                    else root.playlistMessage = kick.addToPlaylist(modelData.id, root.plEntry) ? "" : "that could not be added (it may already be there, or the list is full)"
                }
            }
        }
        MenuItem { visible: root.plChoices.length === 0; enabled: false; text: "no playlists of this kind yet: make one under playlists" }
    }
    function showTab(t) {
        watchTab = t; dlMessage = ""
        if (t === "past") kick.loadPastStreams(); else if (t === "clips") kick.loadClips()
    }
    function pic(url) { return url && url !== "" ? "image://kick/" + encodeURIComponent(url) : "" }

    component Pic: Item {
        id: pc
        property string address: ""
        property color backing: Theme.panel
        property bool round: false
        property color maskColor: Theme.bg
        Rectangle { anchors.fill: parent; color: pc.backing }
        Image {
            anchors.fill: parent; source: root.pic(pc.address); cache: false; asynchronous: true
            fillMode: Image.PreserveAspectCrop; sourceSize.width: pc.width * 2; sourceSize.height: pc.height * 2
        }
        Canvas {   // a round picture: the corners are painted over
            anchors.fill: parent; visible: pc.round
            property color mc: pc.maskColor
            onMcChanged: requestPaint()
            onWidthChanged: requestPaint()
            onPaint: {
                const g = getContext("2d"); g.reset(); g.fillStyle = mc
                g.beginPath(); g.rect(0, 0, width, height); g.moveTo(width, height / 2)
                g.arc(width / 2, height / 2, Math.min(width, height) / 2, 0, 2 * Math.PI, true); g.fill()
            }
        }
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
    component Box: Rectangle {   // a tick box drawn here (the tick-box characters are missing from some phone fonts)
        property bool on: false
        implicitWidth: 16; implicitHeight: 16; radius: 3; color: on ? Theme.accent : "transparent"
        border { width: 1.5; color: on ? Theme.accent : Theme.muted }
    }
    component Pill: Label {   // a small tag
        font { family: Theme.font; pixelSize: Theme.fs(11); letterSpacing: 1; bold: true }
        leftPadding: 6; rightPadding: 6; topPadding: 2; bottomPadding: 2
    }

    // ------------------------------------------------------------------------------------------------------
    // not built / not allowed yet / disconnected
    // ------------------------------------------------------------------------------------------------------
    Label {
        visible: !hop.netAppsBuilt || !kick
        anchors.centerIn: parent; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(18) }
        text: "this build of hop-tunes has no apps that use the internet."
    }

    Rectangle {   // the permission card
        objectName: "kickPermission"
        visible: !!kick && !root.allowed
        anchors.centerIn: parent; width: Math.min(parent.width - 40, 640); height: perm.implicitHeight + 56
        color: Theme.panel; border { width: 1; color: Theme.line }
        ColumnLayout {
            id: perm
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 28 }
            spacing: 14
            RowLayout {
                spacing: 16
                Image { source: "qrc:/images/app-tile.png"; sourceSize.width: 160; Layout.preferredWidth: 70; Layout.preferredHeight: 70
                        Image { anchors.centerIn: parent; width: 38; height: 38; source: "qrc:/images/app-kick.png"; sourceSize.width: 76 } }
                ColumnLayout {
                    spacing: 2
                    Label { text: "kick wants to use the internet"; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(28); weight: Font.Light } }
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
                    Label { text: modelData[0]; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(16) } Layout.preferredWidth: 180 }
                    Label { text: modelData[1]; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(15) } }
                }
            }
            Label { text: "what is sent: what you ask for (a channel name, a search) and your internet address, like any visitor. no hop-tunes details, no cookies, nothing is kept after you close the app. you never need a kick account to watch, and hop-tunes never follows or subscribes for you on kick: your follow and block lists stay on this computer."
                    color: Theme.text2; wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.topMargin: 4
                    font { family: Theme.font; pixelSize: Theme.fs(15) } }
            RowLayout {
                Layout.topMargin: 10; spacing: 12
                PlainButton { objectName: "kickAllow"; text: hop.guest ? "allow for this session" : "allow for this profile"; primary: true
                              onClicked: { hop.setAppNetAllowed("kick", true); root.allowed = true; root.disconnected = false; kick.open() } }
                PlainButton { objectName: "kickNotNow"; text: "not now"; onClicked: root.leave() }
                Item { Layout.fillWidth: true }
            }
            Rectangle { Layout.fillWidth: true; Layout.topMargin: 6; height: 1; color: Theme.line }
            Label {   // the developer's channel: opens in the browser only when it is clicked (nothing is fetched by Hop-tunes)
                objectName: "kickDevLink"
                Layout.fillWidth: true; Layout.topMargin: 2; wrapMode: Text.WordWrap
                textFormat: Text.StyledText
                text: "made by gamehopper \u00b7 <a href=\"https://kick.com/gamehopper/about\" style=\"color:" + Theme.accent + "\">kick.com/gamehopper</a>"
                color: Theme.muted; linkColor: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(14) }
                onLinkActivated: function (link) { Qt.openUrlExternally(link) }
                HoverHandler { cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor }
            }
        }
    }

    Rectangle {   // after "disconnect"
        objectName: "kickDisconnected"
        visible: !!kick && root.allowed && root.disconnected
        anchors.centerIn: parent; width: Math.min(parent.width - 40, 480); height: dc.implicitHeight + 48; color: Theme.panel; border { width: 1; color: Theme.line }
        ColumnLayout {
            id: dc
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 24 } spacing: 12
            Label { text: "disconnected"; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(26); weight: Font.Light } }
            Label { text: "kick.com is not being contacted. nothing is loaded until you reconnect."; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true
                    font { family: Theme.font; pixelSize: Theme.fs(15) } }
            PlainButton { objectName: "kickReconnect"; text: "reconnect"; primary: true; onClicked: { root.disconnected = false; root.page = "browse"; kick.open() } }
        }
    }

    // ------------------------------------------------------------------------------------------------------
    // the working app
    // ------------------------------------------------------------------------------------------------------
    Item {
        id: live
        anchors { fill: parent; topMargin: 30 }               // (the connection bar is above, right under the app's own title bar)
        visible: !!kick && root.allowed && !root.disconnected

        // ---- browse: an open, wide menu of live streams (nothing plays until you pick one) ----
        ColumnLayout {
            id: browse
            objectName: "kickBrowse"
            anchors.fill: parent; visible: root.page === "browse"; spacing: 12
            Item {   // the curtain: search, filters and categories. The arrow at the top right of the app pulls it up (more room for streams) and back down.
                id: curtain
                objectName: "kickCurtain"
                Layout.fillWidth: true
                property real reveal: root.collapsed ? 0 : 1          // 1 = fully down, 0 = pulled up; the height follows the content while it moves
                Behavior on reveal { NumberAnimation { duration: Theme.reduceMotion ? 0 : 200; easing.type: Easing.InOutQuad } }
                Layout.preferredHeight: controls.implicitHeight * reveal
                clip: true
                opacity: reveal
                enabled: !root.collapsed
                visible: !root.collapsed || reveal > 0.01
                ColumnLayout {
                    id: controls
                    width: parent.width; spacing: 12
            GridLayout {
                columns: root.phone ? 1 : 2; rowSpacing: 8; columnSpacing: 10; Layout.fillWidth: true
                TextField {
                    id: searchBox
                    objectName: "kickSearch"
                    Layout.fillWidth: true; implicitHeight: root.phone ? 46 : 42; selectByMouse: true
                    placeholderText: "search channels, or paste a kick.com link"
                    font { family: Theme.font; pixelSize: Theme.fs(16) } color: Theme.text; placeholderTextColor: Theme.dim
                    background: Rectangle { color: Theme.panel; border { width: 1; color: Theme.border } }
                    onAccepted: {
                        searchTimer.stop()
                        if (/kick\.com\//i.test(text)) root.openChannel(text)     // a pasted link opens that channel
                        else kick.search(text)
                    }
                    // search as you type: the list follows the spelling (a moment after you pause), closest names first
                    onTextChanged: searchTimer.restart()
                    Timer {
                        id: searchTimer
                        objectName: "kickSearchTimer"
                        interval: 350
                        onTriggered: {
                            const t = searchBox.text.trim()
                            if (/kick\.com\//i.test(t)) return                   // a pasted link waits for enter
                            if (t.length === 1) return                           // one letter finds too much
                            if (t !== kick.query) kick.search(t)
                        }
                    }
                }
                Flow {   // sort, language, 18+, refresh: one row on a computer, wraps on a phone
                    id: filters
                    spacing: 10
                    Layout.fillWidth: root.phone
                    Layout.preferredWidth: root.phone ? -1 : childrenRect.width
                    Layout.preferredHeight: childrenRect.height
                AbstractButton {
                    objectName: "kickSort"; hoverEnabled: true; focusPolicy: Qt.NoFocus
                    onClicked: kick.sort = kick.sort === "viewers" ? "featured" : "viewers"
                    contentItem: Row { spacing: 6
                        Label { text: kick.sort === "viewers" ? "most viewers" : "featured"; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(14) } anchors.verticalCenter: parent.verticalCenter }
                        Icon { kind: "down"; ink: Theme.text; width: 12; height: 12; anchors.verticalCenter: parent.verticalCenter } }
                    background: Item {}
                }
                AbstractButton {   // language: a small menu in the same plain style as the sort label
                    id: lang
                    objectName: "kickLanguage"
                    readonly property var codes: ["", "en", "es", "pt", "fr", "de", "it", "tr", "ru", "ja", "ar"]
                    readonly property var names: ["all languages", "english", "espa\u00f1ol", "portugu\u00eas", "fran\u00e7ais", "deutsch", "italiano", "t\u00fcrk\u00e7e", "\u0440\u0443\u0441\u0441\u043a\u0438\u0439", "\u65e5\u672c\u8a9e", "\u0627\u0644\u0639\u0631\u0628\u064a\u0629"]
                    hoverEnabled: true; focusPolicy: Qt.NoFocus
                    enabled: !kick.englishOnly
                    onClicked: langMenu.popup()
                    contentItem: Row { spacing: 6
                        Label { text: kick.englishOnly ? "english only" : lang.names[Math.max(0, lang.codes.indexOf(kick.language))]; color: kick.englishOnly ? Theme.accent : Theme.text; font { family: Theme.font; pixelSize: Theme.fs(14) } anchors.verticalCenter: parent.verticalCenter }
                        Icon { visible: !kick.englishOnly; kind: "down"; ink: Theme.text; width: 12; height: 12; anchors.verticalCenter: parent.verticalCenter } }
                    background: Item {}
                    Menu {
                        id: langMenu
                        Repeater { model: lang.names
                                   delegate: MenuItem { required property int index; required property string modelData; text: modelData
                                                        onTriggered: kick.language = lang.codes[index] } }
                    }
                }
                AbstractButton {
                    objectName: "kickEnglishOnly"; hoverEnabled: true; focusPolicy: Qt.NoFocus
                    onClicked: kick.englishOnly = !kick.englishOnly
                    contentItem: Row { spacing: 8
                        Box { on: kick.englishOnly; anchors.verticalCenter: parent.verticalCenter }
                        Label { text: "english only"; color: kick.englishOnly ? Theme.accent : Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) } anchors.verticalCenter: parent.verticalCenter } }
                    background: Item {}
                }
                AbstractButton {
                    objectName: "kickMature"; hoverEnabled: true; focusPolicy: Qt.NoFocus
                    onClicked: kick.showMature = !kick.showMature
                    contentItem: Row { spacing: 8
                        Box { on: kick.showMature; anchors.verticalCenter: parent.verticalCenter }
                        Label { text: "show 18+"; color: kick.showMature ? Theme.accent : Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) } anchors.verticalCenter: parent.verticalCenter } }
                    background: Item {}
                }
                PlainButton { objectName: "kickRefresh"; text: "refresh"; implicitHeight: 36; onClicked: { if (kick.query === "") kick.refresh(); else kick.search(kick.query); kick.refreshFollowing() } }
                PlainButton { objectName: "kickAccountButton"; implicitHeight: 36; text: kick.signedIn ? "account (signed in)" : "account"; onClicked: root.page = "account" }
                PlainButton { objectName: "kickPlaylistsButton"; text: "playlists" + (kick.playlists.length > 0 ? " (" + kick.playlists.length + ")" : ""); implicitHeight: 36
                              onClicked: { root.playlistMessage = ""; root.page = "playlists" } }
                PlainButton { objectName: "kickDownloadsButton"; visible: !!hop.downloads; implicitHeight: 36
                              text: "downloads" + (hop.downloads && hop.downloads.count > 0 ? " (" + hop.downloads.count + ")" : "")
                              onClicked: root.page = "downloads" }
                }
            }
            RowLayout {   // following: kept on this computer only
                objectName: "kickFollowingRow"
                visible: kick.following.length > 0 || kick.blocked.length > 0
                Layout.fillWidth: true; spacing: 10
                Label { visible: kick.following.length > 0; text: "following"; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(13); letterSpacing: 1 } }
                Flow {
                    Layout.fillWidth: true; spacing: 8
                    Repeater {
                        model: kick.following
                        delegate: Rectangle {
                            required property var modelData
                            objectName: "kickFollow_" + modelData.slug
                            height: 34; width: fname.implicitWidth + 70; radius: 17; color: Theme.panel; border { width: 1; color: Theme.line }
                            RowLayout {
                                anchors { fill: parent; leftMargin: 5; rightMargin: 12 } spacing: 8
                                Pic { Layout.preferredWidth: 24; Layout.preferredHeight: 24; address: modelData.avatar || ""; round: true; maskColor: Theme.panel
                                      backing: Qt.hsla((modelData.slug.charCodeAt(0) % 36) / 36, 0.4, 0.4, 1) }
                                Label { id: fname; text: modelData.name; color: modelData.live ? Theme.text : Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) } }
                                Rectangle { width: 7; height: 7; radius: 3.5; color: modelData.live ? "#e5484d" : "transparent"; border { width: modelData.live ? 0 : 1; color: Theme.dim } }
                            }
                            TapHandler { onTapped: root.openChannel(modelData.slug) }
                        }
                    }
                }
                Label { text: kick.following.length > 0 ? "on this computer only" : ""; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(12) } }
                Label { visible: kick.blocked.length > 0; text: kick.blocked.length + " blocked · manage in settings"; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(12) } }
            }
            Flow {   // categories
                visible: kick.query === ""
                Layout.fillWidth: true; spacing: 8
                Chip { objectName: "kickCat_all"; text: "all"; on: kick.categoryId === 0; onClicked: kick.categoryId = 0 }
                Repeater { model: kick.chips
                           delegate: Chip { required property var modelData; objectName: "kickCat_" + modelData.id; text: modelData.name.toLowerCase(); on: kick.categoryId === modelData.id
                                            onClicked: kick.categoryId = modelData.id } }
                Chip { objectName: "kickEditCategories"; text: "edit"; onClicked: root.page = "categories" }
            }
                }
            }
            Label { visible: kick.query !== ""; text: "results for “" + kick.query + "”"; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(15) } }

            Flickable {
                id: gridScroll
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                contentHeight: grid.height + footer.height + 24
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                readonly property int cols: Math.max(1, Math.floor((width + 18) / (276 + 18)))
                readonly property real cardW: (width - (cols - 1) * 18 - 12) / cols
                Flow {
                    id: grid
                    objectName: "kickGrid"
                    width: gridScroll.width - 12; spacing: 18
                    Repeater {
                        model: kick.streams
                        delegate: Item {
                            id: card
                            required property var modelData
                            objectName: "kickCard_" + modelData.slug
                            width: gridScroll.cardW; height: gridScroll.cardW * 9 / 16 + 78
                            Rectangle { anchors.fill: parent; anchors.margins: -4; color: Theme.hover; visible: hit.containsMouse; radius: 4 }
                            Pic {   // a still picture, never live video
                                id: still
                                width: parent.width; height: width * 9 / 16
                                address: modelData.thumb; backing: Theme.artColors("kick" + modelData.slug)[1]
                                Pill { visible: modelData.viewers > 0 || modelData.live === true; anchors { left: parent.left; top: parent.top; margins: 8 } text: "LIVE"; color: "#fff"
                                       background: Rectangle { color: "#e5484d"; radius: 3 } }
                                Pill { objectName: "kickDeveloperTag"; visible: modelData.developer === true; anchors { left: parent.left; top: parent.top; leftMargin: 54; topMargin: 8 }
                                       text: "DEVELOPER"; color: Theme.ink; background: Rectangle { color: Theme.accent; radius: 3 } }
                                Pill { visible: modelData.viewersText !== ""; anchors { left: parent.left; bottom: parent.bottom; margins: 8 } text: modelData.viewersText + " watching"; color: "#fff"
                                       font.bold: false; font.letterSpacing: 0; background: Rectangle { color: Qt.rgba(0, 0, 0, 0.6); radius: 3 } }
                            }
                            MouseArea { id: hit; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.cardTapped(modelData) }
                            Rectangle {   // the card's menu
                                objectName: "kickCardMenu_" + modelData.slug
                                anchors { right: still.right; top: still.top; margins: 8 }
                                width: root.phone ? 40 : 28; height: root.phone ? 34 : 24; radius: 4; color: Qt.rgba(0, 0, 0, 0.6); visible: root.phone || hit.containsMouse || cardMenu.visible
                                Label { anchors.centerIn: parent; text: "⋯"; color: "#fff"; font.pixelSize: 16 }
                                MouseArea { anchors.fill: parent; onClicked: cardMenu.popup() }
                                Menu {
                                    id: cardMenu
                                    MenuItem { text: kick.isFollowing(modelData.slug) ? "unfollow" : "follow"
                                               onTriggered: kick.isFollowing(modelData.slug) ? kick.unfollow(modelData.slug) : kick.follow(modelData.slug) }
                                    MenuItem { text: "open"; onTriggered: root.openChannel(modelData.slug) }
                                    MenuItem { objectName: "kickBlockMenu_" + modelData.slug; text: "block this streamer"
                                               onTriggered: { root.lastBlocked = modelData.slug; kick.block(modelData.slug); undoTimer.restart() } }
                                }
                            }
                            RowLayout {
                                anchors { left: parent.left; right: parent.right; top: still.bottom; topMargin: 8 } spacing: 10
                                Pic { Layout.preferredWidth: 34; Layout.preferredHeight: 34; Layout.alignment: Qt.AlignTop; address: modelData.avatar; round: true
                                      backing: Qt.hsla((modelData.slug.charCodeAt(0) % 36) / 36, 0.4, 0.4, 1) }
                                ColumnLayout {
                                    spacing: 1; Layout.fillWidth: true
                                    Label { text: modelData.title !== "" ? modelData.title : modelData.name; color: Theme.text; elide: Text.ElideRight; Layout.fillWidth: true; font { family: Theme.font; pixelSize: Theme.fs(14) } }
                                    Label { text: modelData.name; color: Theme.text2; elide: Text.ElideRight; Layout.fillWidth: true; font { family: Theme.font; pixelSize: Theme.fs(13) } }
                                    Label { text: modelData.category; color: Theme.dim; elide: Text.ElideRight; Layout.fillWidth: true; font { family: Theme.font; pixelSize: Theme.fs(12) } }
                                }
                            }
                        }
                    }
                }
                Column {
                    id: footer
                    y: grid.height + 14; width: parent.width - 12; spacing: 10
                    Label { visible: kick.loading; text: "loading…"; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(15) } }
                    Label { objectName: "kickEmpty"; visible: !kick.loading && kick.error === "" && kick.streams.length === 0
                            width: parent.width; wrapMode: Text.WordWrap; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(15) }
                            text: kick.query !== "" ? "nothing found for that search." : "nothing live here right now (or everything is blocked or 18+: check the filters above)." }
                    Row {
                        spacing: 12; visible: kick.error !== ""
                        Label { objectName: "kickError"; text: kick.error; color: Theme.danger; width: Math.min(560, gridScroll.width - 140); wrapMode: Text.WordWrap; font { family: Theme.font; pixelSize: Theme.fs(15) } }
                        PlainButton { text: "try again"; onClicked: { kick.refresh() } }
                    }
                    PlainButton { objectName: "kickMore"; visible: kick.hasMore && !kick.loading && kick.query === ""; text: "show more"; onClicked: kick.loadMore() }
                }
            }
        }

        // ---- watch: the stream, with follow and block, and the chat ----
        Flickable {   // on a phone the watch page scrolls when the video, the chat and the menu do not all fit
            id: watchFlick
            objectName: "kickWatch"
            anchors.fill: parent; visible: root.page === "watch"; clip: true
            contentWidth: width; contentHeight: Math.max(height, watch.implicitHeight)
            interactive: root.phone && contentHeight > height + 1
            boundsBehavior: Flickable.StopAtBounds
        GridLayout {
            id: watch
            width: watchFlick.width; height: Math.max(watchFlick.height, root.phone ? watch.implicitHeight : 0)
            columns: root.phone ? 1 : 2; columnSpacing: 22; rowSpacing: 10
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: !root.phone; spacing: 12
                Item {   // the curtain over the channel's header, tabs and record row (pulled up by the arrow at the top right of the app)
                    id: watchCurtain
                    objectName: "kickWatchCurtain"
                    Layout.fillWidth: true
                    property real reveal: root.collapsed ? 0 : 1
                    Behavior on reveal { NumberAnimation { duration: Theme.reduceMotion ? 0 : 200; easing.type: Easing.InOutQuad } }
                    Layout.preferredHeight: watchControls.implicitHeight * reveal
                    clip: true
                    opacity: reveal
                    enabled: !root.collapsed
                    visible: !root.collapsed || reveal > 0.01
                    ColumnLayout {
                    id: watchControls
                    width: parent.width; spacing: 12
                GridLayout {
                    columns: root.phone ? 1 : 2; columnSpacing: 12; rowSpacing: 8; Layout.fillWidth: true
                RowLayout {
                    spacing: 12; Layout.fillWidth: true; Layout.minimumWidth: 0
                    PlainButton { objectName: "kickBackToStreams"; text: root.phone ? "‹" : "‹ streams"; implicitHeight: 36; Layout.preferredWidth: root.phone ? 44 : -1; Layout.minimumWidth: 0; onClicked: root.goBack() }
                    Pic { visible: !root.phone; Layout.preferredWidth: 44; Layout.preferredHeight: 44; address: kick.channel.avatar || ""; round: true
                          backing: Qt.hsla(((kick.channel.slug || "a").charCodeAt(0) % 36) / 36, 0.4, 0.4, 1) }
                    ColumnLayout {
                        spacing: 0; Layout.fillWidth: true; Layout.minimumWidth: 0
                        RowLayout { spacing: 10; Layout.fillWidth: true
                            Label { objectName: "kickChannelName"; text: kick.channel.name || ""; color: Theme.strong; elide: Text.ElideRight; Layout.fillWidth: true; Layout.minimumWidth: 0
                                    font { family: Theme.font; pixelSize: Theme.fs(22) } }
                            Pill { visible: kick.channel.live === true; text: "LIVE"; color: "#fff"; background: Rectangle { color: "#e5484d"; radius: 3 } }
                            Label { visible: kick.channel.live === true && !root.phone; text: (kick.channel.viewersText || "") + " watching"; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) } }
                        }
                        Label { text: kick.channel.title || ""; color: Theme.text2; elide: Text.ElideRight; Layout.fillWidth: true; Layout.minimumWidth: 0; font { family: Theme.font; pixelSize: Theme.fs(14) } }
                    }
                }
                RowLayout {
                    spacing: 8; Layout.fillWidth: root.phone
                    PlainButton { objectName: "kickFollowButton"; implicitHeight: 36; Layout.minimumWidth: 0; Layout.preferredWidth: root.phone ? 104 : -1
                                  text: kick.isFollowing(kick.channel.slug || "") ? "★ following" : "☆ follow"
                                  onClicked: kick.isFollowing(kick.channel.slug) ? kick.unfollow(kick.channel.slug) : kick.follow(kick.channel.slug) }
                    PlainButton { objectName: "kickAddToPlaylist"; text: root.phone ? "+ list" : "+ playlist"; implicitHeight: 36; Layout.minimumWidth: 0; Layout.preferredWidth: root.phone ? 84 : -1
                                  onClicked: root.addMenu("channels", ({ slug: kick.channel.slug }), kick.channel.slug || "", Qt.point(0, height)) }
                    PlainButton { objectName: "kickBlockButton"; text: "block"; danger: true; implicitHeight: 36; Layout.minimumWidth: 0; Layout.preferredWidth: root.phone ? 76 : -1
                                  onClicked: { const s = kick.channel.slug; root.lastBlocked = s; kick.block(s); undoTimer.restart(); root.goBack() } }
                }
                }
                RowLayout {   // what to look at: the live stream, finished streams (to download), clips
                    spacing: 8; Layout.fillWidth: true
                    visible: !!hop.downloads && kick.channel.notFound !== true
                    Chip { objectName: "kickTab_live"; text: "live"; on: root.watchTab === "live"; onClicked: root.showTab("live") }
                    Chip { objectName: "kickTab_past"; text: "past streams"; on: root.watchTab === "past"; onClicked: root.showTab("past") }
                    Chip { objectName: "kickTab_clips"; text: "clips"; on: root.watchTab === "clips"; onClicked: root.showTab("clips") }
                    Item { Layout.fillWidth: true }
                }
                RowLayout {   // record this live stream (only when pressed; never by itself)
                    objectName: "kickRecordRow"
                    spacing: 10; Layout.fillWidth: true
                    visible: !!hop.downloads && root.watchTab === "live" && kick.channel.live === true
                    PlainButton { objectName: "kickRecordButton"; visible: !root.activeRecording; text: "\u25cf record"; implicitHeight: 36; Layout.minimumWidth: 0
                                  onClicked: root.record() }
                    Rectangle { visible: !!root.activeRecording; width: 10; height: 10; radius: 5; color: "#e5484d" }
                    Label { objectName: "kickRecordingBar"; visible: !!root.activeRecording; color: "#e5484d"; font { family: Theme.font; pixelSize: Theme.fs(14) }
                            text: root.activeRecording ? "recording \u00b7 " + root.fmtDur(root.activeRecording.seconds) + " \u00b7 " + root.fmtBytes(root.activeRecording.bytes) : "" }
                    PlainButton { objectName: "kickStopRecord"; visible: !!root.activeRecording; text: "stop"; implicitHeight: 36; Layout.minimumWidth: 0
                                  onClicked: hop.downloads.stopRecording(root.activeRecording.id) }
                    Label { Layout.fillWidth: true; Layout.minimumWidth: 0; elide: Text.ElideRight; color: !root.activeRecording && root.recordMessage !== "" ? Theme.danger : Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(12) }
                            text: root.activeRecording ? "kept in your downloads when you stop" : (root.recordMessage !== "" ? root.recordMessage : "keeps this stream from now on, in your offline downloads") }
                }
                    }
                }
                ColumnLayout {   // finished streams and clips: pick one to download for offline viewing
                    id: library
                    objectName: "kickLibrary"
                    visible: root.watchTab !== "live"
                    Layout.fillWidth: true; Layout.fillHeight: !root.phone
                    Layout.preferredHeight: root.phone ? 420 : -1
                    spacing: 8
                    RowLayout {
                        spacing: 8
                        Label { text: "quality"; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(13); letterSpacing: 1 } }
                        Chip { objectName: "kickQuality_480"; text: "480p"; on: root.dlHeight === 480; onClicked: root.dlHeight = 480 }
                        Chip { objectName: "kickQuality_720"; text: "720p"; on: root.dlHeight === 720; onClicked: root.dlHeight = 720 }
                        Chip { objectName: "kickQuality_best"; text: "best"; on: root.dlHeight === 0; onClicked: root.dlHeight = 0 }
                        Label { Layout.fillWidth: true; elide: Text.ElideRight; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(12) }
                                text: hop.offline ? hop.offline.summary : "" }
                    }
                    Label { objectName: "kickLibraryState"; visible: kick.libraryState !== ""; Layout.fillWidth: true; wrapMode: Text.WordWrap
                            color: kick.libraryState === "loading" ? Theme.muted : Theme.danger; font { family: Theme.font; pixelSize: Theme.fs(15) }
                            text: kick.libraryState === "loading" ? "loading…" : kick.libraryState }
                    Label { objectName: "kickDownloadMessage"; visible: root.dlMessage !== ""; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.danger
                            text: root.dlMessage; font { family: Theme.font; pixelSize: Theme.fs(14) } }
                    ListView {
                        id: libList
                        objectName: "kickLibraryList"
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 10
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar {}
                        model: root.watchTab === "past" ? kick.pastStreams : (root.watchTab === "clips" ? kick.clips : [])
                        delegate: Item {
                            id: row
                            required property var modelData
                            readonly property var dl: root.dlById[modelData.id]
                            objectName: "kickLib_" + modelData.id
                            width: libList.width - 12; height: root.phone ? 150 : 84
                            Rectangle { anchors.fill: parent; color: Theme.panel; border { width: 1; color: Theme.line } }
                            GridLayout {
                                anchors { fill: parent; margins: 8 }
                                columns: root.phone ? 1 : 3; columnSpacing: 12; rowSpacing: 6
                                RowLayout {
                                    Layout.fillWidth: true; spacing: 12
                                    Pic { Layout.preferredWidth: 107; Layout.preferredHeight: 60; Layout.alignment: Qt.AlignTop; address: row.modelData.thumb
                                          backing: Theme.artColors("kick" + row.modelData.id)[1] }
                                    ColumnLayout {
                                        spacing: 2; Layout.fillWidth: true; Layout.minimumWidth: 0
                                        Label { text: row.modelData.title !== "" ? row.modelData.title : "(untitled)"; color: Theme.text; elide: Text.ElideRight; Layout.fillWidth: true; font { family: Theme.font; pixelSize: Theme.fs(14) } }
                                        Label { Layout.fillWidth: true; elide: Text.ElideRight; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(12) }
                                                text: root.fmtDur(row.modelData.seconds) + " \u00b7 " + String(row.modelData.date).substring(0, 10)
                                                      + (row.modelData.viewers > 0 ? " \u00b7 " + kick.viewersText(row.modelData.viewers) + (root.watchTab === "clips" ? " views" : " watched") : "") }
                                        Label { Layout.fillWidth: true; elide: Text.ElideRight; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(12) }
                                                text: row.dl ? root.dlState(row.dl) : "about " + root.fmtBytes(row.modelData.estimate * (root.dlHeight === 480 ? 0.45 : (root.dlHeight === 0 ? 1.9 : 1))) + " to download" }
                                    }
                                }
                                PlainButton {
                                    objectName: "kickDownload_" + row.modelData.id
                                    visible: !row.dl || row.dl.state === "failed"
                                    text: "download"; implicitHeight: 36
                                    onClicked: root.download(row.modelData)
                                }
                                PlainButton {
                                    objectName: "kickAddPl_" + row.modelData.id
                                    text: "+ list"; implicitHeight: 36
                                    onClicked: root.addMenu(row.modelData.kind === "clip" ? "clips" : "streams", row.modelData, row.modelData.id, Qt.point(0, height))
                                }
                                PlainButton {
                                    objectName: "kickPlayLocal_" + row.modelData.id
                                    visible: !!row.dl && row.dl.state === "done"
                                    text: "play"; primary: true; implicitHeight: 36
                                    onClicked: { root.playId = row.modelData.id; root.page = "play" }
                                }
                                Label { visible: !!row.dl && row.dl.state !== "done" && row.dl.state !== "failed"; text: "in your downloads"; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(12) } }
                            }
                        }
                    }
                }
                Item {
                    visible: root.watchTab === "live"
                    Layout.fillWidth: true; Layout.fillHeight: !root.phone
                    Layout.preferredHeight: root.phone ? width * 9 / 16 : -1
                    KickPlayer {
                        id: livePlayer
                        anchors.fill: parent
                        fullscreen: root.videoFullscreen
                        onFullscreenToggled: root.videoFullscreen = !root.videoFullscreen
                        url: root.page === "watch" && root.watchTab === "live" && !root.inBackground && kick.channel.live === true ? (kick.channel.playbackUrl || "") : ""
                        onRetryRequested: { if ((kick.channel.slug || "") !== "") kick.openChannel(kick.channel.slug) }
                    }
                    Rectangle {
                        anchors.fill: parent; color: Theme.panel; border { width: 1; color: Theme.line }
                        visible: root.page === "watch" && kick.channel.live !== true
                        Label {
                            objectName: "kickChannelState"
                            anchors.centerIn: parent; width: parent.width - 60; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                            color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(18) }
                            text: kick.channel.loading === true ? "loading…"
                                : kick.channel.notFound === true ? "there is no kick channel called “" + kick.channel.slug + "”."
                                : kick.channel.error ? kick.channel.error
                                : (kick.channel.name || "this channel") + " is offline right now."
                        }
                    }
                }
            }
            ColumnLayout {   // the chat: none (the start), or the pretend one; Kick's own chat is for people who sign in (later step)
                visible: root.watchTab === "live"
                Layout.preferredWidth: root.phone ? -1 : 360; Layout.fillWidth: root.phone; Layout.fillHeight: true; spacing: 8
                Layout.minimumHeight: root.phone ? 230 : 0                            // the chat is never squeezed to a sliver; the page scrolls instead
                RowLayout {
                    spacing: 8
                    Label { text: "chat"; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(13); letterSpacing: 1 } }
                    Chip { objectName: "kickChat_off"; text: "none"; on: kick.chatMode === "off"; onClicked: kick.chatMode = "off" }
                    Chip { objectName: "kickChat_pretend"; text: "pretend"; on: kick.chatMode === "pretend"; onClicked: kick.chatMode = "pretend" }
                }
                Label { visible: kick.chatMode === "off"; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) }
                        text: "no live chat. choose pretend for a made-up one (nobody in it is real)." }
                LiveChat {
                    id: kickChat
                    objectName: "kickPretendChat"
                    visible: kick.chatMode === "pretend"
                    Layout.fillWidth: true; Layout.fillHeight: true
                    look: hop.videoChatLook
                    seed: "kick|" + (kick.channel.slug || "")
                    info: ({ meta: { title: kick.channel.title || "", year: "", album: kick.channel.category || "" },
                             cast: kick.channel.name ? [{ name: kick.channel.name, role: "streamer" }] : [] })
                    property real clock: 900000 + ((kick.channel.slug || "a").charCodeAt(0) * 7919) % 1800000   // a stable "how far into the stream"
                    position: clock
                    duration: 0
                    Timer { interval: 250; repeat: true; running: kickChat.visible; onTriggered: kickChat.clock += 250 }
                }
                Label { visible: kick.chatMode === "pretend"; text: "pretend chat: nobody here is real"; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(12) } }
                Item { visible: kick.chatMode === "off"; Layout.fillHeight: true }
            }
        }
        }

        // ---- categories: pick the chips under the search box (swap Kick's popular ones for what you care about) ----
        ColumnLayout {
            objectName: "kickCategoriesPage"
            anchors.fill: parent; visible: root.page === "categories"; spacing: 10
            RowLayout {
                spacing: 12; Layout.fillWidth: true
                PlainButton { objectName: "kickCategoriesBack"; text: root.phone ? "\u2039" : "\u2039 streams"; implicitHeight: 36; Layout.preferredWidth: root.phone ? 44 : -1; Layout.minimumWidth: 0; onClicked: root.goBack() }
                Label { text: "your categories"; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(24); weight: Font.Light } }
                Item { Layout.fillWidth: true }
                PlainButton { objectName: "kickCatReset"; visible: kick.categoriesCustom; text: root.phone ? "reset" : "back to kick\u2019s popular ones"; implicitHeight: 36; Layout.minimumWidth: 0; onClicked: kick.resetCategories() }
            }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) }
                    text: kick.categoriesCustom ? "these are the category chips under the search box. take away the ones you do not care about, move the rest, and add your own below. kept in this profile."
                                                : "these are kick\u2019s popular categories. change anything (take one away, or add your own below) and the list becomes yours, kept in this profile." }
            TextField {
                id: catSearch
                objectName: "kickCatSearch"
                Layout.fillWidth: true; implicitHeight: root.phone ? 46 : 40; selectByMouse: true
                placeholderText: "find a category to add (a game, irl, music\u2026)"
                font { family: Theme.font; pixelSize: Theme.fs(15) } color: Theme.text; placeholderTextColor: Theme.dim
                background: Rectangle { color: Theme.panel; border { width: 1; color: Theme.border } }
                onTextChanged: catTimer.restart()
                Timer { id: catTimer; objectName: "kickCatTimer"; interval: 350; onTriggered: kick.searchCategories(catSearch.text) }
            }
            Label { visible: catSearch.text.trim().length >= 2 && !kick.categorySearching && kick.categoryResults.length === 0; Layout.fillWidth: true; color: Theme.muted
                    text: "no category found for that name."; font { family: Theme.font; pixelSize: Theme.fs(14) } }
            Flow {   // what the search found
                objectName: "kickCatResults"
                Layout.fillWidth: true; spacing: 8; visible: kick.categoryResults.length > 0
                Repeater {
                    model: kick.categoryResults
                    delegate: Chip {
                        required property var modelData
                        readonly property bool pinned: { for (let i = 0; i < kick.chips.length; i++) if (kick.chips[i].id === modelData.id && kick.categoriesCustom) return true; return false }
                        objectName: "kickCatAdd_" + modelData.id
                        text: (pinned ? "in your list: " : "+ ") + modelData.name.toLowerCase() + (modelData.mature ? " (18+)" : "")
                        enabled: !pinned
                        onClicked: kick.pinCategory(modelData.id, modelData.name)
                    }
                }
            }
            Label { text: "your list"; color: Theme.muted; Layout.topMargin: 6; font { family: Theme.font; pixelSize: Theme.fs(13); letterSpacing: 1 } }
            Label { objectName: "kickCatEmpty"; visible: kick.chips.length === 0; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) }
                    text: kick.categoriesCustom ? "nothing in your list: only \u201call\u201d shows. add some above, or go back to kick\u2019s popular ones." : "kick\u2019s popular categories have not loaded yet." }
            ListView {
                id: catList
                objectName: "kickCatList"
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 6
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                model: kick.chips
                delegate: Item {
                    id: crow
                    required property var modelData
                    required property int index
                    objectName: "kickCatRow_" + modelData.id
                    width: catList.width - 12; height: 46
                    Rectangle { anchors.fill: parent; color: Theme.panel; border { width: 1; color: Theme.line } }
                    RowLayout {
                        anchors { fill: parent; leftMargin: 12; rightMargin: 8 } spacing: 8
                        Label { text: crow.modelData.name.toLowerCase(); color: Theme.text; elide: Text.ElideRight; Layout.fillWidth: true; Layout.minimumWidth: 0; font { family: Theme.font; pixelSize: Theme.fs(15) } }
                        PlainButton { objectName: "kickCatUp_" + crow.modelData.id; text: "up"; enabled: crow.index > 0; implicitHeight: 34; Layout.preferredWidth: 60; Layout.minimumWidth: 0
                                      onClicked: kick.movePinnedCategory(crow.modelData.id, -1) }
                        PlainButton { objectName: "kickCatDown_" + crow.modelData.id; text: "down"; enabled: crow.index < kick.chips.length - 1; implicitHeight: 34; Layout.preferredWidth: 72; Layout.minimumWidth: 0
                                      onClicked: kick.movePinnedCategory(crow.modelData.id, 1) }
                        PlainButton { objectName: "kickCatRemove_" + crow.modelData.id; text: "remove"; danger: true; implicitHeight: 34; Layout.preferredWidth: 84; Layout.minimumWidth: 0
                                      onClicked: kick.unpinCategory(crow.modelData.id) }
                    }
                }
            }
        }

        // ---- account: optional. Watching never needs it; it only lets you import who you follow on Kick ----
        ColumnLayout {
            objectName: "kickAccountPage"
            anchors.fill: parent; visible: root.page === "account"; spacing: 10
            RowLayout {
                spacing: 12; Layout.fillWidth: true
                PlainButton { objectName: "kickAccountBack"; text: root.phone ? "\u2039" : "\u2039 streams"; implicitHeight: 36; Layout.preferredWidth: root.phone ? 44 : -1; Layout.minimumWidth: 0; onClicked: root.goBack() }
                Label { text: "kick account"; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(24); weight: Font.Light } }
                Item { Layout.fillWidth: true }
            }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(14) }
                    text: "signing in is optional: you never need a kick account to watch, search, download or record. it only lets you import the channels you follow on kick into the follow list on this device. nothing is ever followed or changed on kick." }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(12) }
                    text: "how it works: hop-tunes shows a 6-digit code and an address on kick.com; you open it in your own browser (signed in to kick) and approve it there. hop-tunes never sees your password. kick then gives hop-tunes a session token, which is kept only in this profile on this device (a guest keeps it only until the session ends) and is sent only to kick.com. sign out forgets it. this uses kick's unofficial TV sign-in, so it may stop working if kick changes it." }
            // signed out
            ColumnLayout {
                visible: !kick.signedIn; Layout.fillWidth: true; spacing: 8
                PlainButton { objectName: "kickSignIn"; visible: kick.signInState === ""; text: "sign in with a code"; primary: true; implicitHeight: 40; onClicked: kick.startSignIn() }
                ColumnLayout {
                    visible: kick.signInState === "waiting"; Layout.fillWidth: true; spacing: 6
                    Label { text: "your code"; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(12); letterSpacing: 1 } }
                    Label { objectName: "kickSignInCode"; text: kick.signInCode; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(40); weight: Font.Light; letterSpacing: 6 } }
                    Label { text: "open this address in your browser, signed in to kick, and approve:"; color: Theme.text2; Layout.fillWidth: true; wrapMode: Text.WordWrap; font { family: Theme.font; pixelSize: Theme.fs(13) } }
                    TextEdit { objectName: "kickSignInUrl"; text: kick.signInUrl; readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere; Layout.fillWidth: true
                               color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(13) } }
                    RowLayout {
                        spacing: 10
                        PlainButton { objectName: "kickOpenSignIn"; text: "open it"; implicitHeight: 38; onClicked: Qt.openUrlExternally(kick.signInUrl) }
                        PlainButton { objectName: "kickCancelSignIn"; text: "cancel"; implicitHeight: 38; onClicked: kick.cancelSignIn() }
                    }
                }
                Label { objectName: "kickSignInMessage"; visible: kick.signInMessage !== ""; Layout.fillWidth: true; wrapMode: Text.WordWrap
                        color: kick.signInState === "error" ? Theme.danger : Theme.muted; text: kick.signInMessage; font { family: Theme.font; pixelSize: Theme.fs(14) } }
                PlainButton { objectName: "kickSignInAgain"; visible: kick.signInState === "error"; text: "try again"; implicitHeight: 38; onClicked: { kick.cancelSignIn(); kick.startSignIn() } }
            }
            // signed in
            ColumnLayout {
                visible: kick.signedIn; Layout.fillWidth: true; spacing: 8
                Label { objectName: "kickAccountName"; text: "signed in as " + kick.accountName; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(18) } }
                RowLayout {
                    spacing: 10
                    PlainButton { objectName: "kickImportFollows"; text: kick.importing ? "reading…" : "import my follows"; primary: true; implicitHeight: 40; enabled: !kick.importing; onClicked: kick.importFollows() }
                    PlainButton { objectName: "kickSignOut"; text: "sign out"; danger: true; implicitHeight: 40; onClicked: kick.signOut() }
                }
                Label { objectName: "kickImportMessage"; visible: kick.importMessage !== ""; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.text2; text: kick.importMessage
                        font { family: Theme.font; pixelSize: Theme.fs(14) } }
                Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(12) }
                        text: "imported channels go into your follow list on this device and stay there even after you sign out. a streamer you blocked is not imported." }
            }
            Item { Layout.fillHeight: true }
        }

        // ---- playlists: favourite channels (and clips / past streams), kept on this device only ----
        ColumnLayout {
            objectName: "kickPlaylistsPage"
            anchors.fill: parent; visible: root.page === "playlists"; spacing: 10
            RowLayout {
                spacing: 12; Layout.fillWidth: true
                PlainButton { objectName: "kickPlaylistsBack"; text: root.phone ? "\u2039" : "\u2039 streams"; implicitHeight: 36; Layout.preferredWidth: root.phone ? 44 : -1; Layout.minimumWidth: 0; onClicked: root.goBack() }
                Label { text: "playlists"; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(24); weight: Font.Light } }
                Item { Layout.fillWidth: true }
            }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) }
                    text: "your own lists of favourite channels (or clips, or past streams). they stay on this device, in this profile, and nothing about them is sent anywhere." }
            GridLayout {
                columns: root.phone ? 1 : 3; columnSpacing: 10; rowSpacing: 8; Layout.fillWidth: true
                TextField {
                    id: newPlName
                    objectName: "kickNewPlaylistName"
                    Layout.fillWidth: true; implicitHeight: 40; selectByMouse: true; maximumLength: 60
                    placeholderText: "name of a new playlist"
                    font { family: Theme.font; pixelSize: Theme.fs(15) } color: Theme.text; placeholderTextColor: Theme.dim
                    background: Rectangle { color: Theme.panel; border { width: 1; color: Theme.border } }
                    onAccepted: createPl.clicked()
                }
                RowLayout {
                    spacing: 6
                    property string kind: "channels"
                    id: plKind
                    Chip { objectName: "kickNewPlaylistKind_channels"; text: "channels"; on: plKind.kind === "channels"; onClicked: plKind.kind = "channels" }
                    Chip { objectName: "kickNewPlaylistKind_clips"; text: "clips"; on: plKind.kind === "clips"; onClicked: plKind.kind = "clips" }
                    Chip { objectName: "kickNewPlaylistKind_streams"; text: "past streams"; on: plKind.kind === "streams"; onClicked: plKind.kind = "streams" }
                }
                PlainButton {
                    id: createPl
                    objectName: "kickCreatePlaylist"; text: "create"; primary: true; implicitHeight: 40
                    onClicked: {
                        const id = kick.createPlaylist(newPlName.text, plKind.kind)
                        root.playlistMessage = id === "" ? "give it a name (and there is room for 50 playlists)" : ""
                        if (id !== "") newPlName.text = ""
                    }
                }
            }
            Label { objectName: "kickPlaylistMessage"; visible: root.playlistMessage !== ""; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.danger; text: root.playlistMessage
                    font { family: Theme.font; pixelSize: Theme.fs(14) } }
            Label { objectName: "kickPlaylistsEmpty"; visible: kick.playlists.length === 0; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(15) }
                    text: "no playlists yet. make one above, then use + playlist on a channel (or + list on a clip or past stream) to fill it." }
            ListView {
                id: plList
                objectName: "kickPlaylistsList"
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 8
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                model: kick.playlists
                delegate: Item {
                    id: prow
                    required property var modelData
                    objectName: "kickPlaylistRow_" + modelData.id
                    width: plList.width - 12; height: root.phone ? 110 : 64
                    Rectangle { anchors.fill: parent; color: Theme.panel; border { width: 1; color: Theme.line } }
                    GridLayout {
                        anchors { fill: parent; margins: 8 }
                        columns: root.phone ? 1 : 2; columnSpacing: 12; rowSpacing: 6
                        ColumnLayout {
                            spacing: 0; Layout.fillWidth: true; Layout.minimumWidth: 0
                            Label { text: prow.modelData.name; color: Theme.text; elide: Text.ElideRight; Layout.fillWidth: true; font { family: Theme.font; pixelSize: Theme.fs(16) } }
                            Label { text: (prow.modelData.kind === "channels" ? "channels" : (prow.modelData.kind === "clips" ? "clips" : "past streams")) + " \u00b7 " + prow.modelData.count
                                          + (prow.modelData.count === 1 ? " item" : " items")
                                    color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(12) } }
                        }
                        RowLayout {
                            spacing: 8
                            PlainButton { objectName: "kickPlaylistOpen_" + prow.modelData.id; text: "open"; primary: true; implicitHeight: 34
                                          onClicked: { root.playlistId = prow.modelData.id; root.deleteArmed = ""; root.page = "playlist"; if (prow.modelData.kind === "channels") kick.refreshPlaylist(prow.modelData.id) } }
                            PlainButton { objectName: "kickPlaylistDelete_" + prow.modelData.id; danger: true; implicitHeight: 34
                                          text: root.deleteArmed === prow.modelData.id ? "really delete?" : "delete"
                                          onClicked: { if (root.deleteArmed === prow.modelData.id) { const id = prow.modelData.id; root.deleteArmed = ""; kick.deletePlaylist(id) } else root.deleteArmed = prow.modelData.id } }
                        }
                    }
                }
            }
        }

        // ---- one playlist ----
        ColumnLayout {
            id: plPage
            objectName: "kickPlaylistPage"
            readonly property var pl: root.page === "playlist" ? root.playlistById(root.playlistId) : null
            anchors.fill: parent; visible: root.page === "playlist"; spacing: 10
            RowLayout {
                spacing: 12; Layout.fillWidth: true
                PlainButton { objectName: "kickPlaylistBack"; text: root.phone ? "\u2039" : "\u2039 playlists"; implicitHeight: 36; Layout.preferredWidth: root.phone ? 44 : -1; Layout.minimumWidth: 0; onClicked: root.goBack() }
                Label { Layout.fillWidth: true; elide: Text.ElideRight; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(22); weight: Font.Light }
                        text: plPage.pl ? plPage.pl.name : "" }
                PlainButton { objectName: "kickPlRefresh"; visible: !!plPage.pl && plPage.pl.kind === "channels" && plPage.pl.count > 0; text: root.phone ? "who\u2019s live" : "who\u2019s live now"; implicitHeight: 36
                              Layout.minimumWidth: 0; onClicked: kick.refreshPlaylist(root.playlistId) }
            }
            Label { objectName: "kickPlEmpty"; visible: !!plPage.pl && plPage.pl.count === 0; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(15) }
                    text: plPage.pl && plPage.pl.kind === "channels" ? "empty. open a channel and use + playlist to add it here."
                                                                      : "empty. use + list on a clip or past stream (on a channel's page) to add it here." }
            ListView {
                id: plItems
                objectName: "kickPlaylistItems"
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 8
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                model: plPage.pl ? plPage.pl.items : []
                delegate: Item {
                    id: it
                    required property var modelData
                    readonly property bool isChannel: !!plPage.pl && plPage.pl.kind === "channels"
                    readonly property string key: isChannel ? modelData.slug : modelData.id
                    readonly property var dl: isChannel ? null : root.dlById[modelData.id]
                    objectName: "kickPlItem_" + key
                    width: plItems.width - 12; height: root.phone ? (isChannel ? 84 : 150) : 72
                    Rectangle { anchors.fill: parent; color: Theme.panel; border { width: 1; color: Theme.line } }
                    TapHandler { enabled: it.isChannel; onTapped: root.openChannel(it.key) }
                    GridLayout {
                        anchors { fill: parent; margins: 8 }
                        columns: root.phone && !it.isChannel ? 1 : 2; columnSpacing: 12; rowSpacing: 6
                        RowLayout {
                            Layout.fillWidth: true; spacing: 12
                            Pic { visible: it.isChannel; Layout.preferredWidth: 44; Layout.preferredHeight: 44; address: it.isChannel ? (it.modelData.avatar || "") : ""; round: true; maskColor: Theme.panel
                                  backing: Qt.hsla((it.key.charCodeAt(0) % 36) / 36, 0.4, 0.4, 1) }
                            Pic { visible: !it.isChannel; Layout.preferredWidth: 96; Layout.preferredHeight: 54; address: it.isChannel ? "" : (it.modelData.thumb || ""); backing: Theme.artColors("kick" + it.key)[1] }
                            ColumnLayout {
                                spacing: 1; Layout.fillWidth: true; Layout.minimumWidth: 0
                                Label { text: it.isChannel ? (it.modelData.name || it.key) : (it.modelData.title || "(untitled)"); color: Theme.text; elide: Text.ElideRight; Layout.fillWidth: true; font { family: Theme.font; pixelSize: Theme.fs(15) } }
                                Label { Layout.fillWidth: true; elide: Text.ElideRight; font { family: Theme.font; pixelSize: Theme.fs(12) }
                                        color: it.isChannel && it.modelData.live ? "#e5484d" : Theme.muted
                                        text: it.isChannel ? (it.modelData.checked ? (it.modelData.live ? "\u25cf live \u00b7 " + kick.viewersText(it.modelData.viewers) + " watching" : "offline") : "press who\u2019s live now to check")
                                                           : it.modelData.channel + " \u00b7 " + root.fmtDur(it.modelData.seconds) + (it.dl ? " \u00b7 " + root.dlState(it.dl) : "") }
                            }
                        }
                        RowLayout {
                            spacing: 8
                            PlainButton { objectName: "kickPlDownload_" + it.key; visible: !it.isChannel && (!it.dl || it.dl.state === "failed"); text: "download"; implicitHeight: 34
                                          onClicked: { root.dlMessage = hop.downloads ? hop.downloads.enqueue({ id: it.modelData.id, kind: it.modelData.kind, title: it.modelData.title, channel: it.modelData.channel,
                                                                                                           thumb: it.modelData.thumb, url: it.modelData.url, maxHeight: root.dlHeight, estimate: it.modelData.estimate, seconds: it.modelData.seconds }) : "" } }
                            PlainButton { objectName: "kickPlPlay_" + it.key; visible: !it.isChannel && !!it.dl && it.dl.state === "done"; text: "play"; primary: true; implicitHeight: 34
                                          onClicked: { root.playId = it.modelData.id; root.page = "play" } }
                            PlainButton { objectName: "kickPlRemove_" + it.key; text: "remove"; danger: true; implicitHeight: 34
                                          onClicked: kick.removeFromPlaylist(root.playlistId, it.key) }
                        }
                    }
                }
            }
            Label { objectName: "kickPlDownloadMessage"; visible: root.dlMessage !== "" && !!plPage.pl && plPage.pl.kind !== "channels"; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.danger; text: root.dlMessage
                    font { family: Theme.font; pixelSize: Theme.fs(14) } }
        }

        // ---- downloads: what is kept on this device for offline viewing ----
        ColumnLayout {
            id: dlPage
            objectName: "kickDownloads"
            anchors.fill: parent; visible: root.page === "downloads"; spacing: 10
            RowLayout {
                spacing: 12; Layout.fillWidth: true
                PlainButton { objectName: "kickDownloadsBack"; text: root.phone ? "‹" : "‹ streams"; implicitHeight: 36; Layout.preferredWidth: root.phone ? 44 : -1; Layout.minimumWidth: 0; onClicked: root.goBack() }
                Label { text: "downloads"; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(24); weight: Font.Light } }
                Item { Layout.fillWidth: true }
            }
            Label { objectName: "kickDownloadsSummary"; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(14) }
                    text: (hop.offline ? hop.offline.summary + " " : "") + "kept inside hop-tunes on this device (change the limit or the drive in settings)." }
            Label { objectName: "kickDownloadsEmpty"; visible: !hop.downloads || hop.downloads.count === 0; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.muted
                    font { family: Theme.font; pixelSize: Theme.fs(15) }
                    text: "nothing downloaded yet. open a channel, choose past streams or clips and press download. downloads only run while the kick app is open." }
            ListView {
                id: dlList
                objectName: "kickDownloadsList"
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 10
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                model: hop.downloads ? hop.downloads.items : []
                delegate: Item {
                    id: drow
                    required property var modelData
                    objectName: "kickDl_" + modelData.id
                    width: dlList.width - 12; height: root.phone ? 168 : 96
                    Rectangle { anchors.fill: parent; color: Theme.panel; border { width: 1; color: Theme.line } }
                    GridLayout {
                        anchors { fill: parent; margins: 8 }
                        columns: root.phone ? 1 : 2; columnSpacing: 12; rowSpacing: 6
                        RowLayout {
                            Layout.fillWidth: true; spacing: 12
                            Rectangle {
                                Layout.preferredWidth: 107; Layout.preferredHeight: 60; Layout.alignment: Qt.AlignTop; color: Theme.artColors("kick" + drow.modelData.id)[1]
                                Image { anchors.fill: parent; fillMode: Image.PreserveAspectCrop; asynchronous: true; cache: false
                                        source: drow.modelData.thumb !== "" ? "file://" + drow.modelData.thumb : "" }
                            }
                            ColumnLayout {
                                spacing: 2; Layout.fillWidth: true; Layout.minimumWidth: 0
                                Label { text: drow.modelData.title !== "" ? drow.modelData.title : "(untitled)"; color: Theme.text; elide: Text.ElideRight; Layout.fillWidth: true; font { family: Theme.font; pixelSize: Theme.fs(14) } }
                                Label { text: drow.modelData.channel + " \u00b7 " + (drow.modelData.kind === "clip" ? "clip" : (drow.modelData.kind === "recording" ? "recording" : "past stream")) + " \u00b7 " + root.fmtDur(drow.modelData.seconds)
                                               + (drow.modelData.quality !== "" ? " \u00b7 " + drow.modelData.quality : "")
                                        color: Theme.muted; elide: Text.ElideRight; Layout.fillWidth: true; font { family: Theme.font; pixelSize: Theme.fs(12) } }
                                Label { objectName: "kickDlState_" + drow.modelData.id; text: root.dlState(drow.modelData); elide: Text.ElideRight; Layout.fillWidth: true
                                        color: drow.modelData.state === "failed" || drow.modelData.state === "limit" ? Theme.danger : Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(12) } }
                                Rectangle { visible: drow.modelData.state !== "done" && drow.modelData.kind !== "recording"; Layout.fillWidth: true; height: 4; color: Theme.line
                                            Rectangle { width: parent.width * (drow.modelData.progress || 0); height: parent.height; color: Theme.accent } }
                            }
                        }
                        RowLayout {
                            spacing: 8
                            PlainButton { objectName: "kickDlPlay_" + drow.modelData.id; visible: drow.modelData.doneParts > 0; text: "play"; primary: true; implicitHeight: 34
                                          enabled: drow.modelData.state === "done"; onClicked: { root.playId = drow.modelData.id; root.page = "play" } }
                            PlainButton { objectName: "kickDlPause_" + drow.modelData.id; visible: drow.modelData.state === "running" || drow.modelData.state === "queued"; text: "pause"; implicitHeight: 34
                                          onClicked: hop.downloads.pause(drow.modelData.id) }
                            PlainButton { objectName: "kickDlStop_" + drow.modelData.id; visible: drow.modelData.state === "recording"; text: "stop"; implicitHeight: 34
                                          onClicked: hop.downloads.stopRecording(drow.modelData.id) }
                            PlainButton { objectName: "kickDlResume_" + drow.modelData.id; visible: drow.modelData.kind !== "recording" && ["paused", "waiting", "limit", "failed"].indexOf(drow.modelData.state) >= 0; text: "resume"; implicitHeight: 34
                                          onClicked: hop.downloads.resume(drow.modelData.id) }
                            PlainButton { objectName: "kickDlDelete_" + drow.modelData.id; text: "delete"; danger: true; implicitHeight: 34
                                          onClicked: hop.downloads.remove(drow.modelData.id) }
                        }
                    }
                }
            }
        }

        // ---- playing a download from disk: nothing is fetched ----
        ColumnLayout {
            objectName: "kickLocalPlay"
            anchors.fill: parent; visible: root.page === "play"; spacing: 10
            RowLayout {
                spacing: 12; Layout.fillWidth: true
                PlainButton { objectName: "kickLocalBack"; text: root.phone ? "‹" : "‹ downloads"; implicitHeight: 36; Layout.preferredWidth: root.phone ? 44 : -1; Layout.minimumWidth: 0; onClicked: root.goBack() }
                Label { Layout.fillWidth: true; elide: Text.ElideRight; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(18) }
                        text: root.dlById[root.playId] ? root.dlById[root.playId].title : "" }
            }
            KickPlayer {
                id: localPlayer
                objectName: "kickLocalPlayer"
                Layout.fillWidth: true; Layout.fillHeight: true
                fullscreen: root.videoFullscreen
                onFullscreenToggled: root.videoFullscreen = !root.videoFullscreen
                file: root.page === "play" && hop.downloads ? hop.downloads.videoPath(root.playId) : ""
            }
        }

        Rectangle {   // "watch this stream?" (phones)
            objectName: "kickConfirm"
            visible: root.pending !== null && root.page === "browse"
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 6 }
            height: confirmRow.implicitHeight + 24; radius: 12; color: Theme.panel; border { width: 1; color: Theme.border }
            RowLayout {
                id: confirmRow
                anchors { fill: parent; margins: 12 } spacing: 12
                Pic { Layout.preferredWidth: 44; Layout.preferredHeight: 44; address: root.pending ? (root.pending.avatar || "") : ""; round: true; maskColor: Theme.panel
                      backing: Qt.hsla((((root.pending && root.pending.slug) ? root.pending.slug : "a").charCodeAt(0) % 36) / 36, 0.4, 0.4, 1) }
                ColumnLayout {
                    spacing: 0; Layout.fillWidth: true; Layout.minimumWidth: 0
                    Label { text: root.pending ? "watch " + (root.pending.name || root.pending.slug) + "?" : ""; color: Theme.strong; elide: Text.ElideRight; Layout.fillWidth: true; font { family: Theme.font; pixelSize: Theme.fs(17) } }
                    Label { text: root.pending ? (root.pending.title || "") : ""; color: Theme.muted; elide: Text.ElideRight; Layout.fillWidth: true; font { family: Theme.font; pixelSize: Theme.fs(13) } }
                }
                PlainButton { objectName: "kickConfirmNo"; text: "no"; implicitHeight: 40; Layout.preferredWidth: 64; Layout.minimumWidth: 0; onClicked: root.pending = null }
                PlainButton { objectName: "kickConfirmYes"; text: "watch"; primary: true; implicitHeight: 40; Layout.preferredWidth: 96; Layout.minimumWidth: 0
                              onClicked: { const s = root.pending ? root.pending.slug : ""; root.pending = null; if (s !== "") root.openChannel(s) } }
            }
        }
        Rectangle {   // "blocked <name>: undo"
            objectName: "kickUndo"
            visible: undoTimer.running
            anchors { left: parent.left; bottom: parent.bottom; margins: 10 }
            width: undoRow.implicitWidth + 24; height: 44; color: Theme.panel; border { width: 1; color: Theme.border }
            RowLayout { id: undoRow; anchors.centerIn: parent; spacing: 14
                Label { text: "blocked " + root.lastBlocked; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(14) } }
                PlainButton { objectName: "kickUndoButton"; text: "undo"; implicitHeight: 30; onClicked: { kick.unblock(root.lastBlocked); undoTimer.stop() } } }
        }
        Timer { id: undoTimer; interval: 8000 }
    }

    // the honest bar: what is connected, and a way to cut it
    RowLayout {
        objectName: "kickStatusBar"
        anchors { left: parent.left; right: parent.right; top: parent.top } height: 26
        visible: !!kick && root.allowed
        spacing: 10
        Rectangle { width: 8; height: 8; radius: 4; color: root.disconnected ? Theme.dim : "#53fc18" }
        Label { objectName: "kickContacted"; Layout.fillWidth: true; elide: Text.ElideRight; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(12) }
                text: root.disconnected ? "not connected" : "connected to " + (kick && kick.contacted.length > 0 ? kick.contacted.join(" · ") : "kick.com when you ask for something") }
        AbstractButton {
            objectName: "kickDisconnect"; visible: !root.disconnected; focusPolicy: Qt.NoFocus
            onClicked: { kick.close(); root.disconnected = true }
            contentItem: Label { text: "disconnect"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(12); underline: true } }
            background: Item {}
        }
    }
}
