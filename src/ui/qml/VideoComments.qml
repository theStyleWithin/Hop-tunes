// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "CommentGen.js" as Gen

// Pretend comments under a video. "mine" shows only what you write (nobody ever answers); "populated"
// adds made-up commenters, who also answer what you write after a little while. Nobody here is real:
// they are calculated from (this video, its page) by CommentGen.js and never stored. Only your own
// comments are saved (in your profile).
ColumnLayout {
    id: root

    property string mode: "mine"       // "mine" | "populated"
    property string seed: ""
    property var info: ({})
    property var mine: []              // [{id, text, at}] newest first (hop.myVideoComments)
    property real duration: 0
    signal leaveRequested()

    readonly property bool populated: mode === "populated"
    readonly property var ctx: Gen.context(info, duration)
    readonly property string me: (hop.profile && hop.profile.name) ? hop.profile.name : "you"
    property int shown: 8
    property string sort: "top"        // "top" | "new"
    property real now: Date.now()
    onSeedChanged: shown = 8

    readonly property var generated: {
        if (!populated) return []
        const list = Gen.comments(seed, 0, shown, ctx)
        if (sort === "new") list.sort(function (a, b) { return a.days - b.days })
        else list.sort(function (a, b) { return b.likes - a.likes })
        return list
    }

    function answersOf(m) {
        if (!populated) return []
        return Gen.answers(seed, m.id, m.text, ctx).filter(function (a) { return a.afterMs <= root.now - m.at })
    }
    function mineAgo(m) { return Gen.ago(Math.max(0, (root.now - m.at) / 86400000)) }

    Timer { interval: 5000; running: root.populated && root.visible && root.mine.length > 0; repeat: true; onTriggered: root.now = Date.now() }

    spacing: 10

    RowLayout {
        Layout.fillWidth: true
        spacing: 14
        Label {
            text: root.populated ? "comments" : "your comments"
            color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(18) }
        }
        Item { Layout.fillWidth: true }
        Repeater {
            model: root.populated ? [["top", "top"], ["new", "newest first"]] : []
            delegate: AbstractButton {
                id: sortBtn
                required property var modelData
                objectName: "commentSort_" + modelData[0]
                hoverEnabled: true
                onClicked: root.sort = modelData[0]
                contentItem: Label { text: sortBtn.modelData[1]; color: root.sort === sortBtn.modelData[0] ? Theme.accent : (sortBtn.hovered ? Theme.text : Theme.muted)
                                     font { family: Theme.font; pixelSize: Theme.fs(14) } }
                background: Item {}
            }
        }
    }

    TextField {
        id: input
        objectName: "commentInput"
        Layout.fillWidth: true
        placeholderText: root.populated ? "add a comment" : "write a comment (nobody will answer)"
        maximumLength: 500
        color: Theme.text; placeholderTextColor: Theme.muted
        selectByMouse: true
        font { family: Theme.font; pixelSize: Theme.fs(14) }
        leftPadding: 10; rightPadding: 10; topPadding: 8; bottomPadding: 8
        onAccepted: { if (text.trim() !== "") { hop.addVideoComment(text); text = "" } }
        Keys.onEscapePressed: root.leaveRequested()
        background: Rectangle { radius: 6; color: Theme.hover; border { width: input.activeFocus ? 2 : 0; color: Theme.accent } }
    }

    // what you wrote
    Repeater {
        model: root.mine
        delegate: ColumnLayout {
            id: mc
            required property var modelData
            readonly property var answers: root.answersOf(modelData)
            Layout.fillWidth: true
            spacing: 6
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Rectangle { Layout.alignment: Qt.AlignTop; width: 36; height: 36; radius: 18; color: Theme.accent
                            Label { anchors.centerIn: parent; text: root.me.charAt(0).toUpperCase(); color: "white"; font { family: Theme.font; pixelSize: Theme.fs(16) } } }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label { text: "@" + root.me + "   " + root.mineAgo(mc.modelData); color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(13) } }
                    Label { text: mc.modelData.text; color: Theme.text; Layout.fillWidth: true; wrapMode: Text.WrapAnywhere; font { family: Theme.font; pixelSize: Theme.fs(15) } }
                    AbstractButton {
                        id: delBtn
                        objectName: "commentDelete"
                        hoverEnabled: true
                        onClicked: hop.removeVideoComment(mc.modelData.id)
                        Accessible.name: "delete comment"
                        contentItem: Label { text: "delete"; color: delBtn.hovered ? Theme.danger : Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) } }
                        background: Item {}
                    }
                }
            }
            Repeater {
                model: mc.answers
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true; Layout.leftMargin: 48
                    spacing: 10
                    Rectangle { Layout.alignment: Qt.AlignTop; width: 26; height: 26; radius: 13; color: modelData.color }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 1
                        Label { text: "@" + modelData.name; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(12) } }
                        Label { text: modelData.text; color: Theme.text; Layout.fillWidth: true; wrapMode: Text.WordWrap; font { family: Theme.font; pixelSize: Theme.fs(14) } }
                    }
                }
            }
        }
    }

    // everybody else (made up)
    Repeater {
        model: root.generated
        delegate: ColumnLayout {
            id: gcm
            required property var modelData
            property bool open: false
            Layout.fillWidth: true
            spacing: 6
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Rectangle { Layout.alignment: Qt.AlignTop; width: 36; height: 36; radius: 18; color: gcm.modelData.color
                            Label { anchors.centerIn: parent; text: gcm.modelData.name.charAt(0).toUpperCase(); color: "#202024"; font { family: Theme.font; pixelSize: Theme.fs(16) } } }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label { text: "@" + gcm.modelData.name + "   " + gcm.modelData.ago; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(13) } }
                    Label { text: gcm.modelData.text; color: Theme.text; Layout.fillWidth: true; wrapMode: Text.WordWrap; font { family: Theme.font; pixelSize: Theme.fs(15) } }
                    Label { text: "♥ " + gcm.modelData.likes; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) } }
                    AbstractButton {
                        id: tog
                        visible: gcm.modelData.replies.length > 0
                        hoverEnabled: true
                        onClicked: gcm.open = !gcm.open
                        contentItem: Label { text: (gcm.open ? "hide " : "show ") + gcm.modelData.replies.length + (gcm.modelData.replies.length === 1 ? " reply" : " replies")
                                             color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(13); underline: tog.hovered } }
                        background: Item {}
                    }
                }
            }
            Repeater {
                model: gcm.open ? gcm.modelData.replies : []
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true; Layout.leftMargin: 48
                    spacing: 10
                    Rectangle { Layout.alignment: Qt.AlignTop; width: 26; height: 26; radius: 13; color: modelData.color }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 1
                        Label { text: "@" + modelData.name + "   " + modelData.ago; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(12) } }
                        Label { text: modelData.text; color: Theme.text; Layout.fillWidth: true; wrapMode: Text.WordWrap; font { family: Theme.font; pixelSize: Theme.fs(14) } }
                    }
                }
            }
        }
    }

    AbstractButton {
        id: more
        objectName: "commentMore"
        visible: root.populated
        hoverEnabled: true
        onClicked: root.shown += 10
        contentItem: Label { text: "show more comments"; color: more.hovered ? Theme.accent : Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14); underline: more.hovered } }
        background: Item {}
    }
    Label { visible: !root.populated; text: "pretend comments: nobody here is real"; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(12) } }
    Label { visible: root.populated; text: "pretend comments: nobody here is real, and they never end"; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(12) } }
}
