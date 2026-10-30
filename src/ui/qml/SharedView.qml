// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// collection > shared: the libraries other people shared with this profile (Party Watch, beta). The guest sees only what
// each host ticked for them; films and shows play straight from the host, and nothing is saved on this computer.
Item {
    id: root

    readonly property var host: {
        const list = hop.sharedHosts
        for (let i = 0; i < list.length; ++i) if (list[i].id === hop.sharedHost) return list[i]
        return null
    }
    property string addressMsg: ""

    onVisibleChanged: hop.setSharedWatching(visible)
    Component.onCompleted: if (hop.sharedHost === 0 && hop.sharedHosts.length > 0) hop.openSharedHost(hop.sharedHosts[0].id)

    RowLayout {
        anchors.fill: parent
        spacing: 32

        // the hosts
        ColumnLayout {
            Layout.preferredWidth: 260; Layout.maximumWidth: 260; Layout.fillWidth: false; Layout.fillHeight: true
            spacing: 6
            RowLayout {
                spacing: 8
                Label { text: "shared with me"; color: Theme.muted; font { family: Theme.font; pixelSize: 14 } }
                Label { text: "beta"; color: hop.accent; font { family: Theme.font; pixelSize: 12 } }
            }
            ListView {
                id: hostList
                objectName: "sharedHostList"
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.topMargin: 8
                clip: true
                model: hop.sharedHosts
                boundsBehavior: Flickable.StopAtBounds
                delegate: ItemDelegate {
                    id: hd
                    required property int index
                    required property var modelData
                    objectName: "sharedHost_" + index
                    readonly property bool sel: modelData.id === hop.sharedHost
                    width: hostList.width; height: 52
                    leftPadding: 10
                    onClicked: hop.openSharedHost(modelData.id)
                    background: Rectangle { color: hd.sel || hd.hovered ? Theme.hover : "transparent" }
                    contentItem: ColumnLayout {
                        spacing: 0
                        Label { text: hd.modelData.name; elide: Text.ElideRight; Layout.fillWidth: true
                                color: hd.sel ? hop.accent : Theme.text; font { family: Theme.font; pixelSize: 18 } }
                        Label { text: hd.modelData.address + ":" + hd.modelData.port; elide: Text.ElideRight; Layout.fillWidth: true
                                color: Theme.dim; font { family: Theme.font; pixelSize: 12 } }
                    }
                }
            }
        }

        // what the selected host shares
        ColumnLayout {
            Layout.fillWidth: true; Layout.fillHeight: true
            spacing: 10
            RowLayout {
                Layout.fillWidth: true
                spacing: 18
                Label { visible: root.host !== null; text: root.host ? root.host.name : ""; color: Theme.text
                        font { family: Theme.font; pixelSize: 30; weight: Font.Light } elide: Text.ElideRight }
                Item { Layout.fillWidth: true }
                AbstractButton {
                    id: refreshBtn
                    objectName: "sharedRefresh"
                    visible: root.host !== null
                    hoverEnabled: true
                    onClicked: hop.openSharedHost(hop.sharedHost)
                    contentItem: Label { text: "refresh"; color: refreshBtn.hovered ? hop.accent : Theme.muted; font { family: Theme.font; pixelSize: 14; underline: refreshBtn.hovered } }
                    background: Item {}
                }
                AbstractButton {
                    id: addrBtn
                    objectName: "sharedAddress"
                    visible: root.host !== null && !addrField.visible
                    hoverEnabled: true
                    onClicked: { addrField.text = root.host.address + ":" + root.host.port; addrField.visible = true; addrField.forceActiveFocus(); addrField.selectAll() }
                    contentItem: Label { text: "change address"; color: addrBtn.hovered ? hop.accent : Theme.muted; font { family: Theme.font; pixelSize: 14; underline: addrBtn.hovered } }
                    background: Item {}
                }
                TextField {
                    id: addrField
                    objectName: "sharedAddressField"
                    visible: false
                    implicitWidth: 220
                    selectByMouse: true
                    color: Theme.text; font { family: Theme.font; pixelSize: 15 }
                    placeholderText: "address:port"; placeholderTextColor: Theme.dim
                    onAccepted: { root.addressMsg = hop.setSharedHostAddress(hop.sharedHost, text); if (root.addressMsg === "") visible = false }
                    Keys.onEscapePressed: { visible = false; root.addressMsg = "" }
                    onActiveFocusChanged: if (!activeFocus) visible = false
                    background: Rectangle { color: Theme.panel; border { width: 2; color: hop.accent } }
                }
                AbstractButton {
                    id: leaveBtn
                    objectName: "sharedLeave"
                    visible: root.host !== null
                    hoverEnabled: true
                    onClicked: hop.leaveSharedHost(hop.sharedHost)
                    contentItem: Label { text: "leave"; color: leaveBtn.hovered ? Theme.danger : Theme.muted; font { family: Theme.font; pixelSize: 14; underline: leaveBtn.hovered } }
                    background: Item {}
                }
            }
            RowLayout {
                objectName: "partyBanner"
                visible: !!hop.sharedParty.active
                spacing: 14
                Label {
                    text: (hop.sharedParty.host || "the host") + (hop.sharedParty.item ? " is watching " + hop.sharedParty.title : " is watching something that is not shared with you")
                    color: Theme.text2; font { family: Theme.font; pixelSize: 15 } elide: Text.ElideRight; Layout.maximumWidth: 560
                }
                AbstractButton {
                    id: joinParty
                    objectName: "joinParty"
                    visible: !!hop.sharedParty.item && !hop.partyFollowing
                    hoverEnabled: true
                    onClicked: hop.joinParty()
                    contentItem: Label { text: "join the party"; color: joinParty.hovered ? Theme.ink : hop.accent; font { family: Theme.font; pixelSize: 15 }
                                         horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { implicitHeight: 30; implicitWidth: 130; color: joinParty.hovered ? hop.accent : "transparent"; border { width: 1; color: hop.accent } }
                }
            }
            Label { visible: root.addressMsg !== ""; text: root.addressMsg; color: Theme.danger; font { family: Theme.font; pixelSize: 14 } }
            Label {
                objectName: "sharedStatus"
                visible: hop.sharedStatus !== ""
                text: hop.sharedStatus
                color: Theme.muted; font { family: Theme.font; pixelSize: 16 }
                wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.maximumWidth: 640
            }
            ListView {
                id: itemList
                objectName: "sharedItemList"
                Layout.fillWidth: true; Layout.fillHeight: true
                clip: true
                model: hop.sharedItems
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                delegate: ItemDelegate {
                    id: row
                    required property int index
                    required property var modelData
                    objectName: "sharedItem_" + index
                    width: itemList.width - 12; height: 46
                    leftPadding: 8
                    onClicked: hop.playShared(modelData.id)
                    enabled: modelData.playable
                    Accessible.name: modelData.title
                    background: Rectangle {
                        color: row.hovered ? Theme.hover : "transparent"
                        Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 1; color: Theme.rowLine }
                    }
                    contentItem: RowLayout {
                        spacing: 16
                        Label { text: row.modelData.title; elide: Text.ElideRight; Layout.fillWidth: true
                                color: row.modelData.playable ? Theme.text : Theme.dim; font { family: Theme.font; pixelSize: 17 } }
                        Label { visible: row.modelData.kind === "audio"; text: (row.modelData.artist || "") + (row.modelData.album ? " · " + row.modelData.album : "")
                                elide: Text.ElideRight; Layout.maximumWidth: 300; color: Theme.muted; font { family: Theme.font; pixelSize: 13 } }
                        Label { visible: row.modelData.kind === "audio"; text: row.modelData.durationText || ""; color: Theme.dim; font { family: Theme.font; pixelSize: 13 }
                                Layout.preferredWidth: 44; horizontalAlignment: Text.AlignRight }
                        Label { text: row.modelData.kind === "audio" ? "music" : row.modelData.kind; color: Theme.dim; font { family: Theme.font; pixelSize: 13 } }
                        Label { text: row.modelData.sizeText; color: Theme.muted; font { family: Theme.font; pixelSize: 13 } Layout.preferredWidth: 70; horizontalAlignment: Text.AlignRight }
                    }
                }
            }
        }
    }
}
