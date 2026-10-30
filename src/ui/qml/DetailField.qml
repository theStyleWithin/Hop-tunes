// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// One empty box on a movie's details page: the user types, it is saved when they leave the box (or
// press enter). `key` names the field in hop.videoDetail.meta. Nothing here is detected for you.
TextField {
    id: root
    property string key: ""
    property bool multiline: false
    property int pixel: 15
    property bool light: false          // big, borderless: used for the title

    function reload() { if (!activeFocus) text = (hop.videoDetail.meta && hop.videoDetail.meta[key]) || "" }
    Component.onCompleted: reload()
    Connections { target: hop; function onVideoDetailChanged() { root.reload() } }
    onEditingFinished: hop.setVideoField(key, text)
    onActiveFocusChanged: if (!activeFocus) { hop.setVideoField(key, text); reload() }

    selectByMouse: true
    color: Theme.text
    placeholderTextColor: Theme.dim
    font { family: Theme.font; pixelSize: root.pixel; weight: root.light ? Font.Light : Font.Normal }
    leftPadding: 10; rightPadding: 10; topPadding: 6; bottomPadding: 6
    background: Rectangle {
        implicitHeight: root.pixel + 18
        color: root.activeFocus ? Theme.panel : "transparent"
        border { width: 1; color: root.activeFocus ? Theme.accent : Theme.borderSoft }
        radius: 3
    }
}
