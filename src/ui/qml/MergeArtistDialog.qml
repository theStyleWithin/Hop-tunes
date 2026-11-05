// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Fixing artist names the scan got wrong. Three steps in one small modal:
//   "pick"    choose the artist to merge into (scroll or type to filter),
//   "confirm" "A into B": the artist on the receiving end takes over; swap flips it,
//   "undo"    split back anything merged into this artist.
Popup {
    id: d
    objectName: "mergeArtistDialog"
    property string step: "pick"
    property string from: ""
    property string into: ""
    property string message: ""
    property var names: []
    property var undoList: []
    signal done(string text, var undo)

    function pick(artist) {
        from = artist; into = ""; message = ""; step = "pick"; filter.text = ""
        names = hop.artistNames().filter(n => n !== artist)
        open(); filter.forceActiveFocus()
    }
    function confirm(a, b) { from = a; into = b; message = ""; step = "confirm"; open() }
    function undo(artist) { from = artist; undoList = hop.mergedInto(artist); message = ""; step = "undo"; open() }
    function run() {
        const was = from, to = into
        message = hop.mergeArtist(was, to)
        if (message === "") { close(); done("merged “" + was + "” into “" + to + "”", { from: was, ref: "" }) }
    }

    modal: true; focus: true
    anchors.centerIn: Overlay.overlay
    padding: 24
    width: Math.min(460, Overlay.overlay ? Overlay.overlay.width - 32 : 460)
    background: Rectangle { color: Theme.panel; border { width: 1; color: Theme.line } }
    Overlay.modal: Rectangle { color: Theme.scrim }

    contentItem: ColumnLayout {
        spacing: 14
        Label {
            text: d.step === "pick" ? "merge “" + d.from + "” into…"
                : d.step === "confirm" ? "merge these two?" : "split back"
            color: Theme.strong; wrapMode: Text.WordWrap; Layout.fillWidth: true
            font { family: Theme.font; pixelSize: Theme.fs(26); weight: Font.Light }
        }

        // ---- pick ----
        TextField {
            id: filter
            objectName: "mergeFilter"
            visible: d.step === "pick"
            placeholderText: "type to find an artist"
            Layout.fillWidth: true
            color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(17) }
            background: Rectangle { implicitHeight: 44; color: Theme.bg; border { width: 1; color: filter.activeFocus ? Theme.accent : Theme.line } }
        }
        ListView {
            id: list
            objectName: "mergeList"
            visible: d.step === "pick"
            Layout.fillWidth: true; Layout.preferredHeight: Math.min(320, Theme.compact ? 260 : 320)
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            model: d.names.filter(n => filter.text === "" || n.toLowerCase().indexOf(filter.text.toLowerCase()) >= 0)
            delegate: ItemDelegate {
                required property string modelData
                width: list.width; height: 42
                onClicked: { d.into = modelData; d.step = "confirm" }
                background: Rectangle { color: parent.hovered ? Theme.hover : "transparent" }
                contentItem: Label { text: modelData; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
                                     color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(17) } }
            }
        }

        // ---- confirm ----
        ColumnLayout {
            visible: d.step === "confirm"
            spacing: 10; Layout.fillWidth: true
            Label {
                objectName: "mergeSummary"
                text: "“" + d.from + "” goes into “" + d.into + "”"
                color: Theme.text; wrapMode: Text.WordWrap; Layout.fillWidth: true
                font { family: Theme.font; pixelSize: Theme.fs(20) }
            }
            Label {
                text: "“" + d.into + "” takes over: its name and cover are kept, and albums with the same title become one. your music files are not changed, and you can split this back any time."
                color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true
                font { family: Theme.font; pixelSize: Theme.fs(14) }
            }
            PlainButton { objectName: "mergeSwap"; text: "⇅ swap who takes over"; onClicked: { const t = d.from; d.from = d.into; d.into = t } }
        }

        // ---- undo ----
        ColumnLayout {
            visible: d.step === "undo"
            spacing: 8; Layout.fillWidth: true
            Label { visible: d.undoList.length === 0; text: "nothing has been merged into this artist."; color: Theme.muted
                    font { family: Theme.font; pixelSize: Theme.fs(15) } }
            Repeater {
                model: d.undoList
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    Label {
                        text: modelData.album === "" ? modelData.from : modelData.album + " (from " + modelData.from + ")"
                        color: Theme.text; elide: Text.ElideRight; Layout.fillWidth: true
                        font { family: Theme.font; pixelSize: Theme.fs(16) }
                    }
                    PlainButton {
                        objectName: "splitBack"
                        text: "split back"
                        onClicked: { const r = hop.unmerge(modelData.from, modelData.ref); d.message = r; d.undoList = hop.mergedInto(d.from) }
                    }
                }
            }
        }

        Label { visible: d.message !== ""; text: d.message; color: Theme.danger; wrapMode: Text.WordWrap; Layout.fillWidth: true
                font { family: Theme.font; pixelSize: Theme.fs(15) } }
        RowLayout {
            Layout.alignment: Qt.AlignRight; spacing: 12
            PlainButton { visible: d.step === "confirm"; text: "back"; onClicked: { d.step = "pick"; d.message = "" } }
            PlainButton { text: d.step === "undo" ? "close" : "cancel"; onClicked: d.close() }
            PlainButton { objectName: "mergeGo"; visible: d.step === "confirm"; text: "merge"; primary: true; onClicked: d.run() }
        }
    }
}
