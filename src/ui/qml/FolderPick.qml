// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Dialogs

// A folder chooser: the Qt dialog on desktop, the system's own chooser on Android (the Qt dialog there
// dims the whole screen and cannot be used). Use it like a FolderDialog: title, open(), onAccepted(selectedFolder).
Item {
    id: root
    property string title: "choose a folder"
    property url currentFolder
    property url selectedFolder
    property bool system: Qt.platform.os === "android"   // tests set it to try the system path on a desktop
    signal accepted()
    readonly property string token: "pick" + root.objectName + Math.floor(Math.random() * 1e9)
    visible: false

    function open() {
        if (root.system) {
            if (hop.pickAndroidFolder(root.token)) return
        }
        dialog.open()
    }

    FolderDialog {
        id: dialog
        title: root.title
        currentFolder: root.currentFolder
        onAccepted: { root.selectedFolder = dialog.selectedFolder; root.accepted() }
    }
    Connections {
        target: hop
        function onAndroidFolderPicked(token, folder) {
            if (token !== root.token || folder.toString() === "") return
            root.selectedFolder = folder
            root.accepted()
        }
    }
}
