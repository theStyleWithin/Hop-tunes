// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Small modal asking for a name (new playlist, rename).
Popup {
    id: p
    property string title: "new playlist"
    property string okText: "create"
    property alias text: field.text
    signal accepted(string name)

    function ask(t, initial, ok) { title = t; field.text = initial || ""; okText = ok || "create"; open(); field.selectAll(); field.forceActiveFocus() }

    modal: true; focus: true
    anchors.centerIn: Overlay.overlay
    padding: 28
    background: Rectangle { color: Theme.panel; border { width: 1; color: Theme.line } }
    Overlay.modal: Rectangle { color: Theme.scrim }

    contentItem: ColumnLayout {
        spacing: 18
        Label { text: p.title; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(30); weight: Font.Light } }
        TextField {
            id: field
            objectName: "namePromptField"
            implicitWidth: 380
            maximumLength: 80
            color: Theme.text
            font { family: Theme.font; pixelSize: Theme.fs(20) }
            Accessible.name: p.title
            background: Rectangle { implicitHeight: 48; color: Theme.bg; border { width: field.activeFocus ? 2 : 1; color: field.activeFocus ? hop.accent : Theme.line } }
            onAccepted: ok.clicked()
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 12
            PlainButton { text: "cancel"; onClicked: p.close() }
            PlainButton {
                id: ok
                objectName: "namePromptOk"
                text: p.okText; primary: true
                enabled: field.text.trim().length > 0
                onClicked: { p.accepted(field.text.trim()); p.close() }
            }
        }
    }
}
