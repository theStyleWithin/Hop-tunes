// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// Phones: a row of pills that scrolls sideways when it is wider than the screen (tabs and sub-tabs).
Flickable {
    id: root
    property var items: []            // [[label, value], …]
    property var current
    property string namePrefix: "pill_"
    property bool underline: false    // text tabs with an underline under the chosen one, instead of pills
    signal picked(var value)
    implicitHeight: 46
    contentWidth: row.implicitWidth
    contentHeight: height
    flickableDirection: Flickable.HorizontalFlick
    boundsBehavior: Flickable.StopAtBounds
    clip: true
    Row {
        id: row
        spacing: root.underline ? 20 : 8
        height: root.height
        Repeater {
            model: root.items
            delegate: AbstractButton {
                id: pill
                required property var modelData
                readonly property bool on: root.current === modelData[1]
                objectName: root.namePrefix + modelData[1]
                anchors.verticalCenter: parent.verticalCenter
                implicitHeight: root.underline ? 40 : 32
                implicitWidth: lbl.implicitWidth + (root.underline ? 4 : 26)
                Accessible.name: modelData[0]
                Accessible.role: Accessible.PageTab
                onClicked: root.picked(modelData[1])
                background: Rectangle {
                    radius: height / 2
                    visible: !root.underline
                    color: pill.on ? Theme.accent : "transparent"
                    border { width: pill.on ? 0 : 1; color: Theme.borderSoft }
                }
                contentItem: Label {
                    id: lbl
                    text: pill.modelData[0]
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    color: root.underline ? (pill.on ? Theme.strong : Theme.dim) : (pill.on ? Theme.ink : Theme.text2)
                    font { family: Theme.font; pixelSize: root.underline ? 22 : 15; weight: root.underline && !pill.on ? Font.Light : Font.Normal }
                    Rectangle { visible: root.underline && pill.on; anchors { left: parent.left; right: parent.right; bottom: parent.bottom; bottomMargin: -2 } height: 3; color: Theme.accent }
                }
            }
        }
    }
}
