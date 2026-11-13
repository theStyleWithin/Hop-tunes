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
    // an app may say what is "playing" (nowPlayingTitle / nowPlayingSub on its root item): the mini player shows it when no song is playing
    readonly property string nowPlayingTitle: active && !!appLoader.item && appLoader.item.nowPlayingTitle !== undefined ? String(appLoader.item.nowPlayingTitle) : ""
    readonly property string nowPlayingSub: active && !!appLoader.item && appLoader.item.nowPlayingSub !== undefined ? String(appLoader.item.nowPlayingSub) : ""
    readonly property bool theatre: active && !!appLoader.item && appLoader.item.theatre === true     // the app wants the whole window (a phone turned sideways, watching)
    readonly property var names: ({ "kick": "kick" })
    visible: active
    signal leave()

    // an app can take over the phone's bottom "search" button while it is open (app.openSearch() returns true when it did)
    readonly property bool canSearch: active && !!appLoader.item && appLoader.item.openSearch !== undefined
    readonly property bool searching: canSearch && !!appLoader.item.searchActive
    function search() { return canSearch && appLoader.item.openSearch() === true }
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
        visible: !root.theatre
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
        // an app can offer a "curtain": the arrow pulls the app's own menu up out of the way (more room for what is under it) and back down
        AbstractButton {
            objectName: "appCollapse"
            visible: root.active && !!appLoader.item && appLoader.item.collapsible === true
            implicitWidth: 44; implicitHeight: 44
            hoverEnabled: true; focusPolicy: Qt.NoFocus
            onClicked: appLoader.item.toggleCollapse()
            Accessible.name: appLoader.item && appLoader.item.collapsed ? "show the menu" : "hide the menu"
            background: Rectangle { radius: 22; color: "transparent"; border { width: 2; color: parent.hovered ? Theme.text : Theme.border } }
            contentItem: Item {
                Icon { anchors.centerIn: parent; kind: "down"; ink: Theme.text; width: 20; height: 20
                       rotation: appLoader.item && appLoader.item.collapsed ? 0 : 180      // pointing up pulls it up; pointing down lets it drop back
                       Behavior on rotation { NumberAnimation { duration: Theme.reduceMotion ? 0 : 180 } } }
            }
        }
    }

    // ---- the apps ----
    Loader {
        id: appLoader
        anchors { left: parent.left; right: parent.right; top: root.theatre ? parent.top : bar.bottom; bottom: parent.bottom; margins: root.theatre ? 0 : Theme.pad; topMargin: root.theatre ? 0 : 18 }
        active: root.active
        sourceComponent: root.appId === "kick" ? kickApp : null
    }
    Component { id: kickApp; KickApp { onLeave: { root.appId = ""; root.leave() } } }
}
