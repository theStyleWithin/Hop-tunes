// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Navigation for phones and tablets: a bottom bar or a side drawer (settings › navigation). On a wide
// screen the bar becomes a side rail and the drawer stays open. The pivots (views) are the same as
// the desktop's top row.
Item {
    id: root
    property var views: []
    property string view: ""
    property bool tablet: false          // 600 dp and wider
    property bool bar: hop.navStyle !== "drawer"
    objectName: "mobileNav"
    readonly property bool drawerOpen: drawer.opened
    function closeDrawer() { drawer.close() }
    signal picked(string v)
    readonly property var glyphs: ({ "collection": "♪", "now playing": "◎", "search": "⌕", "profile": "☺", "quickplay": "★", "settings": "⚙" })

    // ---- bottom bar (phones) -----------------------------------------------------------
    Rectangle {
        visible: !root.tablet && root.bar
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 72
        color: Theme.bgDeep
        Rectangle { width: parent.width; height: 1; color: Theme.line }
        RowLayout {
            anchors.fill: parent
            spacing: 0
            Repeater {
                model: root.views
                delegate: AbstractButton {
                    id: tab
                    objectName: "navTab_" + modelData
                    required property string modelData
                    readonly property bool on: root.view === modelData
                    Layout.fillWidth: true; Layout.fillHeight: true
                    Accessible.name: modelData
                    onClicked: root.picked(modelData)
                    background: Item { }
                    contentItem: Column {
                        spacing: 3
                        anchors.centerIn: parent
                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: 56; height: 30; radius: 15
                            color: tab.on ? Theme.hover : "transparent"
                            Label { anchors.centerIn: parent; text: root.glyphs[tab.modelData] || "•"; color: tab.on ? Theme.strong : Theme.dim; font.pixelSize: Theme.fs(18) }
                        }
                        Label { anchors.horizontalCenter: parent.horizontalCenter; text: tab.modelData; color: tab.on ? Theme.strong : Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) } }
                    }
                }
            }
        }
    }

    // ---- ☰ header + modal drawer (phones) -------------------------------------------------
    Rectangle {
        visible: !root.tablet && !root.bar
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 56
        color: "transparent"
        AbstractButton {
            objectName: "navMenuButton"
            width: 56; height: 56
            Accessible.name: "menu"
            onClicked: drawer.open()
            background: Item { }
            contentItem: Label { text: "☰"; color: Theme.strong; font.pixelSize: Theme.fs(26); horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        }
        Label { anchors { left: parent.left; leftMargin: 64; verticalCenter: parent.verticalCenter } text: root.view; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(26); weight: Font.Light } }
    }
    Drawer {
        id: drawer
        objectName: "navDrawer"
        parent: Overlay.overlay
        width: Math.min(300, root.width * 0.8)
        height: root.Window.height
        edge: Qt.LeftEdge
        background: Rectangle { color: Theme.bgDeep }
        ColumnLayout {
            anchors { fill: parent; margins: 12 }
            spacing: 4
            Label { text: "hop-tunes"; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(30); weight: Font.Light }
                   Layout.margins: 12 }
            Repeater {
                model: root.views
                delegate: NavItem { Layout.fillWidth: true; onClicked: { drawer.close(); root.picked(modelData) } }
            }
            Item { Layout.fillHeight: true }
        }
    }

    // ---- tablets: a side rail (bar) or a permanent drawer ---------------------------------
    Rectangle {
        visible: root.tablet
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
        width: root.bar ? 84 : 240
        color: Theme.bgDeep
        Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Theme.line }
        ColumnLayout {
            anchors { fill: parent; margins: root.bar ? 6 : 12; topMargin: 18 }
            spacing: root.bar ? 14 : 4
            Label { visible: !root.bar; text: "hop-tunes"; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(28); weight: Font.Light }
                   Layout.margins: 12 }
            Repeater {
                model: root.views
                delegate: NavItem { Layout.fillWidth: true; rail: root.bar; onClicked: root.picked(modelData) }
            }
            Item { Layout.fillHeight: true }
        }
    }

    component NavItem: AbstractButton {
        id: it
        required property string modelData
        property bool rail: false
        readonly property bool on: root.view === modelData
        implicitHeight: rail ? 56 : 48
        Accessible.name: modelData
        background: Rectangle { radius: rail ? 15 : 24; color: it.on ? Theme.hover : "transparent"; visible: !it.rail || it.on; height: it.rail ? 30 : parent.height; y: it.rail ? 4 : 0 }
        contentItem: Item {
            Row {
                visible: !it.rail
                anchors { verticalCenter: parent.verticalCenter; left: parent.left; leftMargin: 14 }
                spacing: 14
                Label { text: root.glyphs[it.modelData] || "•"; color: it.on ? Theme.strong : Theme.muted; font.pixelSize: Theme.fs(18) }
                Label { text: it.modelData; color: it.on ? Theme.strong : Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(18) } }
            }
            Column {
                visible: it.rail
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 2
                Label { anchors.horizontalCenter: parent.horizontalCenter; text: root.glyphs[it.modelData] || "•"; color: it.on ? Theme.strong : Theme.dim; font.pixelSize: Theme.fs(18) }
                Label { anchors.horizontalCenter: parent.horizontalCenter; text: it.modelData; color: it.on ? Theme.strong : Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(11) } }
            }
        }
    }
}
