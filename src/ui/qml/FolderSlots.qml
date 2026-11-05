// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A shelf of folder slots (videos, audiobooks, podcasts): one tile per folder with its name, how many items it
// holds and a cover when there is one. Click opens it; "rename" gives the slot a name of your own (the folder is
// never renamed). Pictures have the same shelf built into SectionView.
GridView {
    id: shelf
    property var slots: []
    property string prefix: "slot"          // objectName prefix: <prefix>_<n>, <prefix>Edit_<n>, <prefix>Rename_<n>
    property real aspect: 9 / 16            // cover height / width
    signal opened(string dir)
    signal renamed(string dir, string name)

    clip: true
    model: slots
    readonly property int columns: Theme.compact ? 2 : Math.max(2, Math.floor(width / 260))
    cellWidth: width / columns; cellHeight: cellWidth * aspect + 64
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar {}

    delegate: AbstractButton {
        id: slot
        required property int index
        required property var modelData
        objectName: shelf.prefix + "_" + index
        width: shelf.cellWidth - 16; height: shelf.cellHeight - 8
        hoverEnabled: true
        onClicked: shelf.opened(slot.modelData.dir)
        Accessible.name: slot.modelData.name
        background: Item {}
        contentItem: ColumnLayout {
            spacing: 6
            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: width * shelf.aspect
                color: Theme.panel; clip: true
                Image {
                    anchors.fill: parent
                    visible: slot.modelData.cover !== ""
                    source: slot.modelData.cover
                    asynchronous: true; cache: false
                    fillMode: Image.PreserveAspectCrop
                    sourceSize.width: 520
                    opacity: slot.hovered ? 0.85 : 1.0
                }
                Label {   // no cover: the folder's first letter on the panel
                    anchors.centerIn: parent
                    visible: slot.modelData.cover === ""
                    text: slot.modelData.name.length > 0 ? slot.modelData.name.charAt(0).toUpperCase() : "?"
                    color: slot.hovered ? Theme.accent : Theme.dim
                    font { family: Theme.font; pixelSize: Theme.fs(54); weight: Font.Light }
                }
                Rectangle {
                    anchors { right: parent.right; bottom: parent.bottom; margins: 8 }
                    width: countLabel.implicitWidth + 16; height: 24; radius: 12
                    color: Theme.fade(0.6)
                    Label { id: countLabel; anchors.centerIn: parent; text: slot.modelData.count
                            color: "white"; font { family: Theme.font; pixelSize: Theme.fs(13) } }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Label { visible: !edit.visible; text: slot.modelData.name; Layout.fillWidth: true; elide: Text.ElideRight
                        color: slot.hovered ? Theme.accent : Theme.text; font { family: Theme.font; pixelSize: Theme.fs(16) } }
                TextField {
                    id: edit
                    objectName: shelf.prefix + "Edit_" + slot.index
                    visible: false
                    Layout.fillWidth: true
                    maximumLength: 60
                    selectByMouse: true
                    color: Theme.text
                    font { family: Theme.font; pixelSize: Theme.fs(15) }
                    onAccepted: { shelf.renamed(slot.modelData.dir, text); visible = false }
                    Keys.onEscapePressed: visible = false
                    onActiveFocusChanged: if (!activeFocus) visible = false
                    background: Rectangle { color: Theme.panel; border { width: 2; color: Theme.accent } }
                }
                AbstractButton {
                    id: rename
                    objectName: shelf.prefix + "Rename_" + slot.index
                    visible: (slot.hovered || rename.hovered || Theme.compact) && !edit.visible && !hop.guest
                    hoverEnabled: true
                    onClicked: { edit.text = slot.modelData.name; edit.visible = true; edit.forceActiveFocus(); edit.selectAll() }
                    Accessible.name: "rename folder"
                    contentItem: Label { text: "rename"; color: rename.hovered ? Theme.accent : Theme.muted
                                         font { family: Theme.font; pixelSize: Theme.fs(12); underline: rename.hovered } }
                    background: Item {}
                }
            }
        }
    }
}
