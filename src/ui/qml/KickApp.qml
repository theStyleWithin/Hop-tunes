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
    property string page: "browse"               // browse | watch
    property bool disconnected: false
    property string lastBlocked: ""
    signal leave()

    readonly property var hosts: [["kick.com", "finds the channel and its live stream"], ["web.kick.com", "the list of live streams"],
                                  ["images.kick.com", "the stream pictures"], ["files.kick.com", "profile pictures and emotes"],
                                  ["live-video.net", "the video itself"]]

    Connections { target: hop; function onAppNetChanged() { root.allowed = hop.appNetAllowed("kick") } }
    Component.onCompleted: if (kick && allowed) kick.open()
    Component.onDestruction: if (kick) kick.close()

    // one step back inside the app (the host asks first); false = nothing to step back from, leave the app
    function goBack() {
        if (page === "watch") { page = "browse"; kick.closeChannel(); return true }
        return false
    }
    function openChannel(slug) { kick.openChannel(slug); page = "watch" }
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
        anchors { fill: parent; bottomMargin: 30 }
        visible: !!kick && root.allowed && !root.disconnected

        // ---- browse: an open, wide menu of live streams (nothing plays until you pick one) ----
        ColumnLayout {
            id: browse
            objectName: "kickBrowse"
            anchors.fill: parent; visible: root.page === "browse"; spacing: 12
            RowLayout {
                spacing: 10; Layout.fillWidth: true
                TextField {
                    id: searchBox
                    objectName: "kickSearch"
                    Layout.fillWidth: true; implicitHeight: 42; selectByMouse: true
                    placeholderText: "search channels, or paste a kick.com link"
                    font { family: Theme.font; pixelSize: Theme.fs(16) } color: Theme.text; placeholderTextColor: Theme.dim
                    background: Rectangle { color: Theme.panel; border { width: 1; color: Theme.border } }
                    onAccepted: {
                        if (/kick\.com\//i.test(text)) root.openChannel(text)     // a pasted link opens that channel
                        else kick.search(text)
                    }
                }
                AbstractButton {
                    objectName: "kickSort"; hoverEnabled: true; focusPolicy: Qt.NoFocus
                    onClicked: kick.sort = kick.sort === "viewers" ? "featured" : "viewers"
                    contentItem: Label { text: (kick.sort === "viewers" ? "most viewers" : "featured") + " ▾"; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(14) } }
                    background: Item {}
                }
                AbstractButton {   // language: a small menu in the same plain style as the sort label
                    id: lang
                    objectName: "kickLanguage"
                    readonly property var codes: ["", "en", "es", "pt", "fr", "de", "it", "tr", "ru", "ja", "ar"]
                    readonly property var names: ["all languages", "english", "espa\u00f1ol", "portugu\u00eas", "fran\u00e7ais", "deutsch", "italiano", "t\u00fcrk\u00e7e", "\u0440\u0443\u0441\u0441\u043a\u0438\u0439", "\u65e5\u672c\u8a9e", "\u0627\u0644\u0639\u0631\u0628\u064a\u0629"]
                    hoverEnabled: true; focusPolicy: Qt.NoFocus
                    onClicked: langMenu.popup()
                    contentItem: Label { text: lang.names[Math.max(0, lang.codes.indexOf(kick.language))] + " \u25be"; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(14) } }
                    background: Item {}
                    Menu {
                        id: langMenu
                        Repeater { model: lang.names
                                   delegate: MenuItem { required property int index; required property string modelData; text: modelData
                                                        onTriggered: kick.language = lang.codes[index] } }
                    }
                }
                AbstractButton {
                    objectName: "kickMature"; hoverEnabled: true; focusPolicy: Qt.NoFocus
                    onClicked: kick.showMature = !kick.showMature
                    contentItem: Label { text: (kick.showMature ? "☑ " : "☐ ") + "show 18+"; color: kick.showMature ? Theme.accent : Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) } }
                    background: Item {}
                }
                PlainButton { objectName: "kickRefresh"; text: "refresh"; implicitHeight: 36; onClicked: { if (kick.query === "") kick.refresh(); else kick.search(kick.query); kick.refreshFollowing() } }
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
                Repeater { model: kick.categories
                           delegate: Chip { required property var modelData; objectName: "kickCat_" + modelData.id; text: modelData.name.toLowerCase(); on: kick.categoryId === modelData.id
                                            onClicked: kick.categoryId = modelData.id } }
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
                                Pill { visible: modelData.viewersText !== ""; anchors { left: parent.left; bottom: parent.bottom; margins: 8 } text: modelData.viewersText + " watching"; color: "#fff"
                                       font.bold: false; font.letterSpacing: 0; background: Rectangle { color: Qt.rgba(0, 0, 0, 0.6); radius: 3 } }
                            }
                            MouseArea { id: hit; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.openChannel(modelData.slug) }
                            Rectangle {   // the card's menu
                                objectName: "kickCardMenu_" + modelData.slug
                                anchors { right: still.right; top: still.top; margins: 8 }
                                width: 28; height: 24; radius: 4; color: Qt.rgba(0, 0, 0, 0.6); visible: hit.containsMouse || cardMenu.visible
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
        RowLayout {
            id: watch
            objectName: "kickWatch"
            anchors.fill: parent; visible: root.page === "watch"; spacing: 22
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 12
                RowLayout {
                    spacing: 12; Layout.fillWidth: true
                    PlainButton { objectName: "kickBackToStreams"; text: "‹ streams"; implicitHeight: 36; onClicked: root.goBack() }
                    Pic { Layout.preferredWidth: 44; Layout.preferredHeight: 44; address: kick.channel.avatar || ""; round: true
                          backing: Qt.hsla(((kick.channel.slug || "a").charCodeAt(0) % 36) / 36, 0.4, 0.4, 1) }
                    ColumnLayout {
                        spacing: 0; Layout.fillWidth: true
                        RowLayout { spacing: 10
                            Label { objectName: "kickChannelName"; text: kick.channel.name || ""; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(22) } }
                            Pill { visible: kick.channel.live === true; text: "LIVE"; color: "#fff"; background: Rectangle { color: "#e5484d"; radius: 3 } }
                            Label { visible: kick.channel.live === true; text: (kick.channel.viewersText || "") + " watching"; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) } }
                        }
                        Label { text: kick.channel.title || ""; color: Theme.text2; elide: Text.ElideRight; Layout.fillWidth: true; font { family: Theme.font; pixelSize: Theme.fs(14) } }
                    }
                    PlainButton { objectName: "kickFollowButton"; implicitHeight: 36
                                  text: kick.isFollowing(kick.channel.slug || "") ? "★ following" : "☆ follow"
                                  onClicked: kick.isFollowing(kick.channel.slug) ? kick.unfollow(kick.channel.slug) : kick.follow(kick.channel.slug) }
                    PlainButton { objectName: "kickBlockButton"; text: "block"; danger: true; implicitHeight: 36
                                  onClicked: { const s = kick.channel.slug; root.lastBlocked = s; kick.block(s); undoTimer.restart(); root.goBack() } }
                }
                Item {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    KickPlayer {
                        anchors.fill: parent
                        url: root.page === "watch" && kick.channel.live === true ? (kick.channel.playbackUrl || "") : ""
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
            Rectangle {   // the chat (real chat and the pretend chat come in the next steps)
                Layout.preferredWidth: 360; Layout.fillHeight: true; color: Theme.panel; border { width: 1; color: Theme.line }
                Label { anchors.centerIn: parent; width: parent.width - 40; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                        text: "chat is coming in the next step"; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(14) } }
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
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 26
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
