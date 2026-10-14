// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// settings › library: the folders Hop-tunes watches. (Profiles, playback options: later milestones.)
Item {
    id: root
    signal addFolderRequested()

    ColumnLayout {
        anchors { fill: parent; leftMargin: 48; rightMargin: 48; topMargin: 28 }
        spacing: 18

        Label { text: "music folders"; color: Theme.accent; font { family: Theme.font; pixelSize: 24 } }
        Label {
            text: "hop-tunes reads these folders and never changes or uploads your files."
            color: Theme.muted; font { family: Theme.font; pixelSize: 15 }
        }

        Repeater {
            model: hop.folders
            delegate: Rectangle {
                required property string modelData
                Layout.fillWidth: true; Layout.maximumWidth: 760
                implicitHeight: 56
                color: Theme.panel
                RowLayout {
                    anchors { fill: parent; leftMargin: 16; rightMargin: 8 }
                    Label { text: modelData; color: Theme.text; font { family: Theme.font; pixelSize: 17 } elide: Text.ElideMiddle; Layout.fillWidth: true }
                    Button {
                        text: "remove"
                        onClicked: hop.removeFolder(modelData)
                        Accessible.name: "remove " + modelData
                        contentItem: Label { text: parent.text; color: Theme.text2; font { family: Theme.font; pixelSize: 15 } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { implicitWidth: 96; implicitHeight: 44; color: parent.hovered ? Theme.hover : "transparent" }
                    }
                }
            }
        }

        RowLayout {
            spacing: 14
            Button {
                text: "+ add folder"
                onClicked: root.addFolderRequested()
                contentItem: Label { text: parent.text; color: Theme.text; font { family: Theme.font; pixelSize: 16 } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { implicitWidth: 150; implicitHeight: 44; color: "transparent"; border { width: 2; color: "#5b5b63" } }
            }
            Button {
                text: hop.scanning ? "scanning…" : "rescan now"
                enabled: !hop.scanning && hop.folders.length > 0
                onClicked: hop.rescan()
                contentItem: Label { text: parent.text; color: parent.enabled ? Theme.text : Theme.dim; font { family: Theme.font; pixelSize: 16 } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { implicitWidth: 150; implicitHeight: 44; color: "transparent"; border { width: 2; color: "#5b5b63" } }
            }
        }

        Item { Layout.fillHeight: true }
        Label { text: "hop-tunes " + hop.version + " · offline · GPL-3.0"; color: Theme.faint; font { family: Theme.font; pixelSize: 13 } bottomPadding: 18 }
    }
}
