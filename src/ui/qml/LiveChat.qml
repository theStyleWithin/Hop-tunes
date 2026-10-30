// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "ChatGen.js" as Gen

// A pretend live chat for a video. Nothing is streamed and nobody in it is real: the chatters are
// calculated from (this video, the second you are at) by ChatGen.js, so seeking just recalculates
// it, and they use the details you filled in on the movie's page. What you type appears in the chat
// and a chatter or two answers it. Nothing here is saved.
Item {
    id: root

    property string seed: ""           // identifies the video: the same video always gives the same chat
    property var info: ({})            // the movie's page (hop.videoPlayerInfo)
    property real position: 0          // milliseconds into the video
    property real duration: 0
    property string look: "clean"      // "clean" | "lively"
    property bool overlay: false       // drawn over a fullscreen picture: see-through, white text
    readonly property string me: (hop.profile && hop.profile.name) ? hop.profile.name.toLowerCase().replace(/[^a-z0-9_]/g, "").slice(0, 15) || "you" : "you"

    readonly property var ctx: Gen.context(info, duration)
    readonly property color fg: overlay ? "#f4f4f6" : Theme.text
    readonly property color dim: overlay ? "#bdbdc6" : Theme.muted
    readonly property bool lively: look === "lively"

    property real lastAt: -1
    property int nonce: 0
    property var pending: []           // answers waiting for their moment: [{due, name, color, badge, parts}]
    property real lastSent: 0
    property bool following: true        // stay on the newest message until the user scrolls up
    signal leaveRequested()            // esc in the chat box: hand the keys back to the player

    ListModel { id: msgs }

    // ---- rich text for one message ---------------------------------------------------------------
    readonly property var glyph: ({ smile: ":)", laugh: ":D", wow: ":O", heart: "<3", fire: "!!", clap: "+1", gg: "GG", cry: ";_;" })
    readonly property var emoteColor: ({ smile: "#ffd166", laugh: "#ffb347", wow: "#7bdff2", heart: "#ff7aa2", fire: "#ff8c42", clap: "#b8f2a0", gg: "#c3a6ff", cry: "#8ecae6" })

    function chip(text, bg) {
        return '<span style="background-color:' + bg + '; color:#101014;"><b>&nbsp;' + text + '&nbsp;</b></span>&nbsp;'
    }
    function emoteHtml(name) {
        const g = Gen.esc(glyph[name] || name), c = emoteColor[name] || "#cccccc"
        return lively ? '<span style="background-color:' + c + '; color:#101014;"><b>&nbsp;' + g + '&nbsp;</b></span>'
                      : '<font color="' + c + '"><b>' + g + '</b></font>'
    }
    function htmlOf(partsJson, kind, name, color, badge, mine) {
        const ps = JSON.parse(partsJson)
        let body = ""
        for (let i = 0; i < ps.length; i++) {
            const p = ps[i]
            if (p.t === "emote") body += emoteHtml(p.v)
            else if (p.t === "mention") body += '<font color="' + hop.accent + '"><b>' + Gen.esc(p.v) + '</b></font>'
            else body += Gen.esc(p.v)
        }
        if (kind === "notice") return '<i>' + body + '</i>'
        let head = ""
        if (lively) {
            if (mine) head += chip("YOU", "#f0c040")
            else if (badge === "mod") head += chip("MOD", "#3fb950")
            else if (badge === "vip") head += chip("VIP", "#b388ff")
            else if (badge === "member") head += chip("MEMBER", hop.accent)
        } else if (badge === "mod") head += '<font color="' + dim + '">(mod) </font>'
        head += '<font color="' + (mine ? hop.accent : color) + '"><b>' + Gen.esc(name) + '</b></font>' + (lively ? ': ' : '&nbsp;&nbsp;')
        return head + body
    }

    // ---- filling the list ---------------------------------------------------------------------------
    function append(m) {
        const stick = following || msgs.count === 0
        msgs.append({ mid: m.id || ("u" + Date.now()), kind: m.kind || "chat", name: m.name, color: m.color || "#cccccc",
                      badge: m.badge || "", mine: !!m.me, partsJson: JSON.stringify(m.parts) })
        while (msgs.count > 120) msgs.remove(0)
        if (stick) Qt.callLater(function () { view.positionViewAtEnd() })
    }
    // Everything said between lastAt and now, from the schedule for this video.
    function fillTo(posMs) {
        const from = Math.floor(Math.max(0, lastAt) / 1000), to = Math.floor(posMs / 1000)
        for (let s = from; s <= to; s++) {
            const list = Gen.messagesAt(seed, s, ctx)
            for (let i = 0; i < list.length; i++) if (list[i].at > lastAt && list[i].at <= posMs) append(list[i])
        }
        lastAt = posMs
    }
    function rebuild(posMs) {
        msgs.clear()
        lastAt = Math.max(0, posMs - 45000)
        fillTo(posMs)
    }
    function tick() {
        if (seed === "") return
        const p = position
        // A seek (backwards, or a long way forwards) starts the visible chat afresh from that moment.
        if (lastAt < 0 || p < lastAt - 600 || p - lastAt > 6000) rebuild(p)
        else if (p > lastAt) fillTo(p)
        // Answers whose moment has come.
        const now = Date.now()
        const keep = []
        for (let i = 0; i < pending.length; i++) {
            if (pending[i].due <= now) append({ name: pending[i].name, color: pending[i].color, badge: pending[i].badge, parts: pending[i].parts })
            else keep.push(pending[i])
        }
        pending = keep
    }
    onSeedChanged: { lastAt = -1; pending = []; msgs.clear() }

    function send(text) {
        const t = text.trim().slice(0, 160)
        const now = Date.now()
        if (t === "" || now - lastSent < 1200) return false   // slow mode, like a real stream
        lastSent = now
        append({ name: me, color: hop.accent, me: true, parts: Gen.parts(t) })
        const answers = Gen.replies(seed, me, t, nonce++, ctx)
        const list = pending.slice()
        for (let i = 0; i < answers.length; i++)
            list.push({ due: now + answers[i].delayMs, name: answers[i].name, color: answers[i].color, badge: answers[i].badge, parts: answers[i].parts })
        pending = list
        return true
    }

    Timer { interval: 250; repeat: true; running: root.visible; onTriggered: root.tick() }

    // ---- looks ------------------------------------------------------------------------------------------
    Rectangle {
        anchors.fill: parent
        color: root.overlay ? "#a0000000" : (root.lively ? Theme.bgDeep : Theme.panel)
        radius: root.overlay ? 8 : 0
        border { width: root.lively && !root.overlay ? 1 : 0; color: Theme.borderSoft }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 0
        spacing: 0

        // header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            color: root.lively ? (root.overlay ? "#40ffffff" : Theme.panel) : "transparent"
            RowLayout {
                anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                Rectangle { visible: root.lively; width: 8; height: 8; radius: 4; color: "#ff4d4d" }
                Label {
                    text: root.lively ? "stream chat" : "live chat"
                    color: root.fg
                    font { family: Theme.font; pixelSize: Theme.fs(15); weight: root.lively ? Font.Medium : Font.Normal }
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: Gen.viewerCount(root.seed, Math.floor(root.position / 1000)) + " watching"
                    color: root.dim; font { family: Theme.font; pixelSize: Theme.fs(13) }
                }
            }
            Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 1; color: root.overlay ? "#30ffffff" : Theme.borderSoft; visible: !root.lively }
        }

        ListView {
            id: view
            objectName: "liveChatList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: msgs
            spacing: root.lively ? 2 : 0
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            onMovementStarted: root.following = false
            onAtYEndChanged: if (atYEnd) root.following = true
            onHeightChanged: if (root.following) Qt.callLater(function () { view.positionViewAtEnd() })
            delegate: Item {
                id: row
                required property string mid
                required property string kind
                required property string name
                required property string color
                required property string badge
                required property bool mine
                required property string partsJson
                width: view.width
                height: line.implicitHeight + (root.lively ? 10 : 8)

                Rectangle {   // a highlighted message: a soft band
                    anchors.fill: parent
                    visible: row.kind === "highlight" || row.mine
                    color: root.lively ? Qt.rgba(1, 1, 1, 0.07) : Qt.rgba(1, 1, 1, 0.05)
                    Rectangle { visible: root.lively; width: 3; height: parent.height; color: row.mine ? "#f0c040" : hop.accent }
                }
                RowLayout {
                    anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 12; rightMargin: 12 }
                    spacing: 8
                    Rectangle {   // the clean look has little round avatars
                        visible: !root.lively && row.kind !== "notice"
                        Layout.alignment: Qt.AlignTop
                        Layout.preferredWidth: 22; Layout.preferredHeight: 22; radius: 11
                        color: row.mine ? hop.accent : row.color
                        opacity: 0.85
                        Label { anchors.centerIn: parent; text: row.name.charAt(0).toUpperCase(); color: "#101014"
                                font { family: Theme.font; pixelSize: Theme.fs(12); weight: Font.Bold } }
                    }
                    Text {
                        id: line
                        Layout.fillWidth: true
                        textFormat: Text.RichText
                        wrapMode: Text.Wrap
                        color: row.kind === "notice" ? root.dim : root.fg
                        font { family: Theme.font; pixelSize: Theme.fs(14) }
                        text: root.htmlOf(row.partsJson, row.kind, row.name, row.color, row.badge, row.mine)
                        horizontalAlignment: row.kind === "notice" && root.lively ? Text.AlignHCenter : Text.AlignLeft
                    }
                }
            }
        }

        Label {
            text: "pretend chat: nobody here is real"
            color: root.dim
            Layout.leftMargin: 12; Layout.topMargin: 4
            font { family: Theme.font; pixelSize: Theme.fs(11) }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 10
            spacing: 8
            TextField {
                id: input
                objectName: "liveChatInput"
                Layout.fillWidth: true
                placeholderText: "say something"
                maximumLength: 160
                color: root.fg; placeholderTextColor: root.dim
                selectByMouse: true
                font { family: Theme.font; pixelSize: Theme.fs(14) }
                leftPadding: 10; rightPadding: 10; topPadding: 6; bottomPadding: 6
                onAccepted: if (root.send(text)) text = ""
                Keys.onEscapePressed: root.leaveRequested()
                background: Rectangle {
                    color: root.overlay ? "#30ffffff" : (input.activeFocus ? Theme.bg : "transparent")
                    radius: root.lively ? 16 : 3
                    border { width: 1; color: input.activeFocus ? hop.accent : (root.overlay ? "#50ffffff" : Theme.borderSoft) }
                }
            }
            AbstractButton {
                id: sendBtn
                objectName: "liveChatSend"
                implicitWidth: 56; implicitHeight: 34
                onClicked: if (root.send(input.text)) input.text = ""
                Accessible.name: "send"
                background: Rectangle { radius: root.lively ? 17 : 3; color: sendBtn.hovered ? Qt.lighter(hop.accent, 1.1) : hop.accent }
                contentItem: Label { text: "chat"; color: Theme.ink; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                     font { family: Theme.font; pixelSize: Theme.fs(14); weight: Font.Medium } }
            }
        }
    }
}
