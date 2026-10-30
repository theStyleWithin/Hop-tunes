// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// The multi-line empty box (the description) on a movie's details page. Same rules as DetailField.
TextArea {
    id: root
    property string key: ""
    property int pixel: 15

    function reload() { if (!activeFocus) text = (hop.videoDetail.meta && hop.videoDetail.meta[key]) || "" }
    Component.onCompleted: reload()
    Connections { target: hop; function onVideoDetailChanged() { root.reload() } }
    onActiveFocusChanged: if (!activeFocus) { hop.setVideoField(key, text); reload() }

    wrapMode: TextEdit.Wrap
    selectByMouse: true
    color: Theme.text
    placeholderTextColor: Theme.dim
    font { family: Theme.font; pixelSize: root.pixel }
    leftPadding: 10; rightPadding: 10; topPadding: 8; bottomPadding: 8
    background: Rectangle {
        implicitHeight: 96
        color: root.activeFocus ? Theme.panel : "transparent"
        border { width: 1; color: root.activeFocus ? Theme.accent : Theme.borderSoft }
        radius: 3
    }
}
