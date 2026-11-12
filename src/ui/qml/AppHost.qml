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
    readonly property var names: ({ "kick": "kick" })
    visible: active
    signal leave()

    function open(id) { appId = id; forceActiveFocus() }
    // Esc and the back button: the app steps back inside itself first (item.goBack() returns true when it did), then it closes
    function close() {
        if (!active) return
        if (appLoader.item && appLoader.item.goBack && appLoader.item.goBack()) return
        appId = ""; leave()
    }

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
        id: appLoader
        anchors { left: parent.left; right: parent.right; top: bar.bottom; bottom: parent.bottom; margins: Theme.pad; topMargin: 18 }
        active: root.active
        sourceComponent: root.appId === "kick" ? kickApp : null
    }
    Component { id: kickApp; KickApp { onLeave: { root.appId = ""; root.leave() } } }
}
