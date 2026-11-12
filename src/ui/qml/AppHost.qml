// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Runs one of the apps inside the Hop-tunes window: full area, with a back button, over the collection.
// The music keeps playing and the mini player stays. Esc and the back button leave the app.
FocusScope {
    id: root
    objectName: "appHost"
    property string appId: ""
    readonly property bool active: appId !== ""
    readonly property var names: ({ "sample": "sample app" })
    visible: active
    signal leave()

    function open(id) { appId = id; forceActiveFocus() }
    function close() { if (!active) return; appId = ""; leave() }

    Rectangle { anchors.fill: parent; color: Theme.bg }
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; hoverEnabled: true }   // nothing underneath hears the mouse

    Keys.onEscapePressed: root.close()

    RowLayout {
        id: bar
        anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: Theme.pad; rightMargin: Theme.pad; topMargin: 18 }
        spacing: 14
        AbstractButton {
            objectName: "appBack"
            implicitWidth: 44; implicitHeight: 44
            hoverEnabled: true; focusPolicy: Qt.NoFocus
            onClicked: root.close()
            Accessible.name: "back"
            background: Rectangle { radius: 22; color: "transparent"; border { width: 2; color: parent.hovered ? Theme.text : Theme.border } }
            contentItem: Label { text: "‹"; color: Theme.text; font.pixelSize: Theme.fs(26); horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; bottomPadding: 4 }
        }
        Label { text: root.names[root.appId] || root.appId; color: Theme.strong
                font { family: Theme.font; pixelSize: Theme.fs(26); weight: Font.Light } }
        Item { Layout.fillWidth: true }
    }

    // ---- the apps ----
    Loader {
        anchors { left: parent.left; right: parent.right; top: bar.bottom; bottom: parent.bottom; margins: Theme.pad; topMargin: 18 }
        active: root.active
        sourceComponent: root.appId === "sample" ? sampleApp : null
    }
    Component {
        id: sampleApp
        Item {
            objectName: "sampleApp"
            ColumnLayout {
                anchors.centerIn: parent; spacing: 14; width: Math.min(parent.width, 520)
                Label { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; color: Theme.strong; wrapMode: Text.WordWrap
                        text: "this is a sample app"; font { family: Theme.font; pixelSize: Theme.fs(34); weight: Font.Light } }
                Label { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; color: Theme.muted; wrapMode: Text.WordWrap
                        font { family: Theme.font; pixelSize: Theme.fs(17) }
                        text: "Apps are part of Hop-tunes and open right here, inside its window. The music keeps playing"
                              + (hop.player.hasTrack ? ": " + hop.player.title : "") + ". Press Esc or the back button to leave." }
            }
        }
    }
}
