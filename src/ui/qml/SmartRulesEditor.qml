// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Smart playlist rule builder (spec §7): "genre is …", "added in the last N days",
// "play count less than …", "hearted". Shows how many songs match as you edit.
Popup {
    id: ed
    property string key: ""        // "" = new playlist
    property var rules: []
    property bool matchAll: true
    property string sortBy: "album"
    property int limit: 0
    property int rev: 0            // bumps on in-place value edits (keeps text fields focused)

    readonly property var fields: [
        { id: "genre", label: "genre", kind: "text" }, { id: "artist", label: "artist", kind: "text" },
        { id: "album", label: "album", kind: "text" }, { id: "title", label: "title", kind: "text" },
        { id: "year", label: "year", kind: "number" }, { id: "plays", label: "play count", kind: "number" },
        { id: "skips", label: "skip count", kind: "number" }, { id: "added_days", label: "added", kind: "days" },
        { id: "played_days", label: "last played", kind: "days" }, { id: "hearted", label: "hearted", kind: "bool" }
    ]
    readonly property var ops: ({
        text: [["contains", "contains"], ["is", "is"], ["is_not", "is not"], ["not_contains", "does not contain"]],
        number: [["eq", "is"], ["ne", "is not"], ["gt", "more than"], ["lt", "less than"]],
        days: [["within", "in the last (days)"], ["not_within", "not in the last (days)"]],
        bool: [["is", "is"]]
    })
    readonly property var sorts: [["album", "album order"], ["added", "newest first"], ["plays", "most played"],
                                  ["played", "recently played"], ["title", "title"], ["random", "random"]]

    function fieldKind(f) { for (var i = 0; i < fields.length; ++i) if (fields[i].id === f) return fields[i].kind; return "text" }
    function json() { return JSON.stringify({ match: matchAll ? "all" : "any", sort: sortBy, limit: limit, rules: rules }) }
    function edit(k, name, rulesJson) {
        key = k
        nameField.text = name || ""
        var o = {}
        try { o = rulesJson ? JSON.parse(rulesJson) : {} } catch (e) { o = {} }
        rules = o.rules && o.rules.length ? JSON.parse(JSON.stringify(o.rules)) : [{ field: "genre", op: "contains", value: "" }]
        matchAll = o.match !== "any"
        sortBy = o.sort || "album"
        limit = o.limit || 0
        open()
        nameField.forceActiveFocus()
    }
    function setRule(i, prop, v) {
        if (prop === "value") { rules[i].value = v; rev++; return }
        var r = rules.slice()
        r[i] = Object.assign({}, r[i])
        r[i][prop] = v
        if (prop === "field") { r[i].op = ops[fieldKind(v)][0][0]; r[i].value = fieldKind(v) === "bool" ? "true" : "" }
        rules = r
    }

    modal: true; focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(820, Overlay.overlay ? Overlay.overlay.width - 80 : 820)
    padding: 28
    background: Rectangle { color: Theme.panel; border { width: 1; color: Theme.line } }
    Overlay.modal: Rectangle { color: Theme.scrim }

    component Combo: ComboBox {
        id: cb
        property var pairs: []          // [[value, label], …]
        property string current: ""
        signal picked(string value)
        model: pairs.map(function (p) { return p[1] })
        currentIndex: Math.max(0, pairs.findIndex(function (p) { return p[0] === current }))
        onActivated: function (i) { cb.picked(pairs[i][0]) }
        implicitWidth: 180
        font { family: Theme.font; pixelSize: 15 }
        contentItem: Label { text: cb.displayText; leftPadding: 10; color: Theme.text; font: cb.font; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
        background: Rectangle { implicitHeight: 40; color: Theme.bg; border { width: 1; color: cb.activeFocus ? hop.accent : Theme.line } }
        popup.background: Rectangle { color: Theme.panel; border { width: 1; color: Theme.line } }
        delegate: Component {
            ItemDelegate {
                required property int index
                required property var modelData
                width: cb.width
                highlighted: cb.highlightedIndex === index
                contentItem: Label { text: modelData; color: Theme.text; font: cb.font }
                background: Rectangle { color: highlighted ? Theme.hover : "transparent" }
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: 16
        Label { text: ed.key ? "edit smart playlist" : "new smart playlist"; color: Theme.strong; font { family: Theme.font; pixelSize: 30; weight: Font.Light } }
        TextField {
            id: nameField
            objectName: "smartName"
            Layout.fillWidth: true
            placeholderText: "name"
            placeholderTextColor: Theme.dim
            maximumLength: 80
            color: Theme.text
            font { family: Theme.font; pixelSize: 20 }
            background: Rectangle { implicitHeight: 46; color: Theme.bg; border { width: nameField.activeFocus ? 2 : 1; color: nameField.activeFocus ? hop.accent : Theme.line } }
        }
        RowLayout {
            spacing: 10
            Label { text: "songs that match"; color: Theme.text2; font { family: Theme.font; pixelSize: 16 } }
            Combo { implicitWidth: 90; pairs: [["all", "all"], ["any", "any"]]; current: ed.matchAll ? "all" : "any"; onPicked: function (v) { ed.matchAll = v === "all" } }
            Label { text: "of these rules:"; color: Theme.text2; font { family: Theme.font; pixelSize: 16 } }
        }
        Repeater {
            model: ed.rules
            delegate: RowLayout {
                id: rr
                required property int index
                required property var modelData
                readonly property string kind: ed.fieldKind(modelData.field)
                spacing: 10
                Combo { pairs: ed.fields.map(function (f) { return [f.id, f.label] }); current: rr.modelData.field; onPicked: function (v) { ed.setRule(rr.index, "field", v) } }
                Combo { implicitWidth: 210; pairs: ed.ops[rr.kind]; current: rr.modelData.op; onPicked: function (v) { ed.setRule(rr.index, "op", v) } }
                Combo {
                    visible: rr.kind === "bool"
                    implicitWidth: 120
                    pairs: [["true", "yes"], ["false", "no"]]
                    current: rr.modelData.value || "true"
                    onPicked: function (v) { ed.setRule(rr.index, "value", v) }
                }
                TextField {
                    visible: rr.kind !== "bool"
                    Layout.fillWidth: true
                    text: rr.modelData.value
                    inputMethodHints: rr.kind === "text" ? Qt.ImhNone : Qt.ImhDigitsOnly
                    validator: rr.kind === "text" ? null : intOnly
                    color: Theme.text
                    font { family: Theme.font; pixelSize: 16 }
                    onTextEdited: ed.setRule(rr.index, "value", text)
                    background: Rectangle { implicitHeight: 40; color: Theme.bg; border { width: 1; color: parent.activeFocus ? hop.accent : Theme.line } }
                }
                PlainButton { text: "−"; implicitWidth: 44; enabled: ed.rules.length > 1; onClicked: { var r = ed.rules.slice(); r.splice(rr.index, 1); ed.rules = r } }
            }
        }
        IntValidator { id: intOnly; bottom: 0; top: 99999 }
        PlainButton { text: "+ add rule"; onClicked: ed.rules = ed.rules.concat([{ field: "genre", op: "contains", value: "" }]) }
        RowLayout {
            spacing: 10
            Label { text: "order"; color: Theme.text2; font { family: Theme.font; pixelSize: 16 } }
            Combo { pairs: ed.sorts; current: ed.sortBy; onPicked: function (v) { ed.sortBy = v } }
            Label { text: "limit to"; color: Theme.text2; font { family: Theme.font; pixelSize: 16 } Layout.leftMargin: 14 }
            TextField {
                implicitWidth: 80
                text: ed.limit > 0 ? String(ed.limit) : ""
                placeholderText: "all"
                placeholderTextColor: Theme.dim
                validator: intOnly
                color: Theme.text
                font { family: Theme.font; pixelSize: 16 }
                onTextEdited: ed.limit = parseInt(text) || 0
                background: Rectangle { implicitHeight: 40; color: Theme.bg; border { width: 1; color: parent.activeFocus ? hop.accent : Theme.line } }
            }
            Label { text: "songs"; color: Theme.text2; font { family: Theme.font; pixelSize: 16 } }
        }
        RowLayout {
            Layout.topMargin: 6
            spacing: 12
            Label {
                objectName: "smartPreview"
                Layout.fillWidth: true
                text: { ed.rev; var n = hop.smartPreviewCount(ed.json()); return n + (n === 1 ? " song matches" : " songs match") }
                color: hop.accent; font { family: Theme.font; pixelSize: 16 }
            }
            PlainButton { text: "cancel"; onClicked: ed.close() }
            PlainButton {
                objectName: "smartSave"
                text: ed.key ? "save" : "create"; primary: true
                enabled: nameField.text.trim().length > 0
                onClicked: {
                    if (ed.key) { hop.renamePlaylist(ed.key, nameField.text); hop.setSmartRules(ed.key, ed.json()) }
                    else { var k = hop.createSmartPlaylist(nameField.text, ed.json()); if (k) hop.selectPlaylist(k) }
                    ed.close()
                }
            }
        }
    }
}
