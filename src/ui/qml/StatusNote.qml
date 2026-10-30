// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// A private status box: whatever you want to see here (a lyric, a quote, how you feel). Nobody else
// ever sees it; it is saved in this profile and nowhere else.
Rectangle {
    id: root
    property alias text: edit.text
    implicitWidth: 460
    implicitHeight: 56
    color: Theme.panel
    border { width: edit.activeFocus ? 2 : 1; color: edit.activeFocus ? Theme.accent : Theme.line }

    Flickable {
        id: flick
        anchors { fill: parent; margins: 2 }
        contentWidth: width
        contentHeight: edit.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { }
        TextArea.flickable: TextArea {
            id: edit
            objectName: "statusNote"
            width: flick.width
            wrapMode: TextEdit.Wrap
            selectByMouse: true
            color: Theme.text
            placeholderText: "a lyric, a quote, how you feel… only you see this"
            placeholderTextColor: Theme.dim
            font { family: Theme.font; pixelSize: Theme.fs(15) }
            leftPadding: 10; rightPadding: 10; topPadding: 6; bottomPadding: 6
            background: Item { }
            Accessible.name: "status note (only you see it)"
            // saved a moment after you stop typing, and when you click away
            onTextChanged: { if (text.length > 300) text = text.slice(0, 300); if (activeFocus) save.restart() }
            onActiveFocusChanged: if (!activeFocus) { save.stop(); hop.statusNote = text }
            Keys.onEscapePressed: edit.focus = false
            text: hop.statusNote
            Timer { id: save; interval: 600; onTriggered: hop.statusNote = edit.text }
            // follows the profile if it changes underneath (switching profile, reset)
            Connections { target: hop; function onStatusNoteChanged() { if (!edit.activeFocus && edit.text !== hop.statusNote) edit.text = hop.statusNote } }
        }
    }
}
