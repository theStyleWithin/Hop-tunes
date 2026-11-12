// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The apps tab: small apps that are part of Hop-tunes and run inside its window, each one a glass tile.
// To add an app: one entry in `apps` below and its screen in AppHost.qml.
Item {
    id: root
    objectName: "appsView"
    signal openApp(string appId)

    // id, the name under the tile, a line for the tooltip, an icon (a url, or "" for the plain glass)
    readonly property var apps: [
        { id: "sample", name: "sample app", blurb: "a sample, to show how apps open inside Hop-tunes", icon: "" }
    ]

    // one glass tile
    component AppTile: AbstractButton {
        id: tile
        required property var app
        objectName: "appTile_" + app.id
        readonly property real side: 132
        width: side; height: side + 34
        hoverEnabled: true
        focusPolicy: Qt.StrongFocus
        Accessible.name: app.name
        ToolTip.visible: hovered && app.blurb !== ""
        ToolTip.delay: 600
        ToolTip.text: app.blurb
        onClicked: root.openApp(app.id)
        Keys.onReturnPressed: clicked()
        Keys.onEnterPressed: clicked()
        Keys.onSpacePressed: clicked()
        contentItem: Item {
            Item {
                id: glass
                width: tile.side; height: tile.side
                scale: tile.pressed ? 0.96 : (tile.hovered || tile.activeFocus ? 1.05 : 1.0)
                Behavior on scale { NumberAnimation { duration: Theme.reduceMotion ? 0 : 140; easing.type: Easing.OutQuad } }
                Image {   // the glass bubble
                    anchors.fill: parent
                    source: "qrc:/images/app-tile.png"
                    sourceSize.width: tile.side * 2; sourceSize.height: tile.side * 2
                    smooth: true; mipmap: true
                }
                Image {   // the app's own icon, centred on the glass
                    anchors.centerIn: parent
                    width: tile.side * 0.56; height: width
                    visible: tile.app.icon !== ""
                    source: tile.app.icon
                    sourceSize.width: width * 2; sourceSize.height: height * 2
                    fillMode: Image.PreserveAspectFit
                    smooth: true; mipmap: true
                }
                Rectangle {   // keyboard focus ring
                    anchors.fill: parent; anchors.margins: -4; radius: tile.side * 0.2
                    color: "transparent"; border { width: 2; color: Theme.accent }
                    visible: tile.activeFocus
                }
            }
            Label {
                anchors { top: glass.bottom; topMargin: 10; horizontalCenter: parent.horizontalCenter }
                width: tile.side
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                text: tile.app.name
                color: tile.hovered || tile.activeFocus ? Theme.text : Theme.text2
                font { family: Theme.font; pixelSize: Theme.fs(16) }
            }
        }
        background: Item {}
    }

    Flickable {
        anchors.fill: parent
        contentHeight: grid.height + 24
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        Flow {
            id: grid
            width: parent.width
            spacing: 34
            topPadding: 8
            Repeater { model: root.apps; delegate: AppTile { required property var modelData; app: modelData } }
        }
    }
}
