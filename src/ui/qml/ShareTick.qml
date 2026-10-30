// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// A tick box on a tile or row while "share" is on in the collection: ticked means the chosen guest may see these files
// (a half tick: some of them). Clicking it approves them, or takes them back.
Rectangle {
    id: tick
    property var paths: []             // every file this tile or row stands for
    readonly property int state: { hop.shareRevision; hop.shareTarget; return hop.shareMode ? hop.shareState(paths) : 0 }
    objectName: "shareTick"
    visible: hop.shareMode
    z: 20
    width: 24; height: 24
    color: state === 0 ? Qt.rgba(0, 0, 0, 0.55) : hop.accent
    border { width: 2; color: state === 0 ? "white" : hop.accent }
    Accessible.role: Accessible.CheckBox
    Accessible.name: "share with " + hop.shareTargetName
    Accessible.checked: state === 2
    Label {
        anchors.centerIn: parent
        text: tick.state === 2 ? "✓" : (tick.state === 1 ? "–" : "")
        color: Theme.ink; font { family: Theme.font; pixelSize: 16 }
    }
    MouseArea { anchors.fill: parent; onClicked: hop.toggleShare(tick.paths) }
}
