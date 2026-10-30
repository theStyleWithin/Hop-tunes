// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// quickplay (spec §4.4, design/screens/3): pinned, history, new, and smart DJ.
Item {
    id: root
    signal openAlbum()

    // A single horizontal row of tiles, reused for every group below a Strip's title.
    component TileRow: ListView {
        id: row
        property string rowLabel: ""   // set on a Strip with more than one group (e.g. "pinned"'s kinds)
        Layout.fillWidth: true
        Layout.preferredHeight: 196
        orientation: ListView.Horizontal
        spacing: 18
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
        delegate: Component {
            AbstractButton {
                id: t
                required property var modelData
                objectName: "tile_" + row.rowLabel + "_" + modelData.title
                width: 150; height: 196
                hoverEnabled: true
                onClicked: hop.playTile(modelData.kind, modelData.ref)
                Accessible.name: "play " + modelData.title
                ToolTip.visible: hovered; ToolTip.delay: 800; ToolTip.text: "click to play · right-click for more"
                MouseArea {
                    anchors.fill: parent; acceptedButtons: Qt.RightButton
                    onClicked: function (mouse) { tileMenu.tile = t.modelData; tileMenu.popup(t, mouse.x, mouse.y) }
                }
                contentItem: ColumnLayout {
                    spacing: 4
                    Item {
                        Layout.preferredWidth: 150; Layout.preferredHeight: 150
                        AlbumArt { anchors.fill: parent; key: t.modelData.title; source: t.modelData.art || ""; opacity: t.hovered ? 0.8 : 1 }
                        Rectangle {   // artists read as round, like the Zune artist tiles
                            visible: t.modelData.kind === "artist"
                            anchors { left: parent.left; bottom: parent.bottom; margins: 8 }
                            color: Theme.fade(0.8); width: kindLabel.implicitWidth + 12; height: 22
                            Label { id: kindLabel; anchors.centerIn: parent; text: "ARTIST"; color: Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(11); letterSpacing: 1 } }
                        }
                        Rectangle {
                            visible: t.modelData.kind === "playlist"
                            anchors { left: parent.left; bottom: parent.bottom; margins: 8 }
                            color: Theme.fade(0.8); width: plLabel.implicitWidth + 12; height: 22
                            Label { id: plLabel; anchors.centerIn: parent; text: "PLAYLIST"; color: Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(11); letterSpacing: 1 } }
                        }
                        Label {
                            visible: t.hovered
                            anchors.centerIn: parent
                            text: "▶"; color: "white"; font.pixelSize: Theme.fs(34)
                        }
                    }
                    Label { text: t.modelData.title; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(15) } elide: Text.ElideRight; Layout.fillWidth: true }
                    Label { text: t.modelData.subtitle; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(13) } elide: Text.ElideRight; Layout.fillWidth: true }
                }
                background: Item { }
            }
        }
    }

    // A titled section. "groups" (optional) splits its tiles into labelled sub-rows, e.g.
    // pinned's "artists" / "albums" / "playlists" — kept apart so they read as organised
    // rather than one jumbled strip; a group with no tiles doesn't render at all.
    component Strip: ColumnLayout {
        id: strip
        property string title
        property var tiles: []
        property var groups: null   // [{label, tiles}], overrides "tiles" when set
        property string empty: ""
        readonly property var effectiveGroups: strip.groups || [{label: strip.title, tiles: strip.tiles}]
        readonly property bool hasAny: strip.effectiveGroups.some(g => g.tiles.length > 0)
        spacing: 12
        Layout.fillWidth: true
        Label { text: strip.title; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(24) } }
        Label { visible: !strip.hasAny; text: strip.empty; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(15) } }
        Repeater {
            model: strip.effectiveGroups
            delegate: ColumnLayout {
                id: groupCol
                required property var modelData
                visible: modelData.tiles.length > 0
                Layout.fillWidth: true
                spacing: 6
                Label {
                    visible: strip.groups !== null
                    text: groupCol.modelData.label
                    color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(13); letterSpacing: 1 }
                }
                TileRow { rowLabel: strip.title + "_" + groupCol.modelData.label; model: groupCol.modelData.tiles }
            }
        }
    }

    Menu {
        id: tileMenu
        property var tile: ({})
        background: Rectangle { implicitWidth: 230; color: Theme.panel; border { width: 1; color: Theme.line } }
        component TItem: MenuItem {
            id: mi
            contentItem: Label { text: mi.text; leftPadding: 6; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(15) } }
            background: Rectangle { implicitHeight: 36; color: mi.highlighted ? Theme.hover : "transparent" }
        }
        TItem { text: "play"; onTriggered: hop.playTile(tileMenu.tile.kind, tileMenu.tile.ref) }
        TItem {
            text: "show in collection"; visible: tileMenu.tile.kind === "album"; height: visible ? implicitHeight : 0
            onTriggered: if (hop.showAlbum(tileMenu.tile.ref)) root.openAlbum()
        }
        TItem {
            text: "smart DJ from this artist"; visible: tileMenu.tile.kind !== "playlist"; height: visible ? implicitHeight : 0
            onTriggered: hop.startDj(tileMenu.tile.kind === "artist" ? tileMenu.tile.ref : tileMenu.tile.subtitle)
        }
        TItem {
            text: tileMenu.tile.pinned ? "unpin" : "pin to quickplay"
            onTriggered: tileMenu.tile.pinned ? hop.unpinTile(tileMenu.tile.kind, tileMenu.tile.ref)
                                              : hop.pinTile(tileMenu.tile.kind, tileMenu.tile.ref, tileMenu.tile.title)
        }
    }

    Flickable {
        anchors.fill: parent
        contentHeight: col.implicitHeight + 40
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { }

        ColumnLayout {
            id: col
            x: Theme.pad; y: Theme.compact ? 12 : 24
            width: parent.width - 2 * Theme.pad
            spacing: 30

            // ---- smart DJ ----
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: dj.implicitHeight + 40
                color: Theme.panel
                GridLayout {   // phones: the controls go under the text
                    id: dj
                    anchors { fill: parent; margins: 20 }
                    columns: Theme.compact ? 1 : 4
                    columnSpacing: 20; rowSpacing: 12
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Label { text: "smart dj"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(24) } }
                        Label {
                            text: hop.djSeed !== "" ? "playing an endless mix around " + hop.djSeed + "."
                                                    : "pick an artist and hop-tunes keeps playing music like it, from your own library. nothing goes online."
                            color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(15) }
                            wrapMode: Text.WordWrap; Layout.fillWidth: true
                        }
                    }
                    ComboBox {
                        id: djPick
                        objectName: "djArtist"
                        model: hop.djArtists
                        implicitWidth: 260
                        Layout.fillWidth: Theme.compact
                        enabled: hop.djArtists.length > 0
                        font { family: Theme.font; pixelSize: Theme.fs(16) }
                        contentItem: Label { text: djPick.displayText; leftPadding: 12; color: Theme.text; font: djPick.font; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                        background: Rectangle { implicitHeight: 44; color: Theme.bg; border { width: 1; color: djPick.activeFocus ? Theme.accent : Theme.line } }
                        popup.background: Rectangle { color: Theme.panel; border { width: 1; color: Theme.line } }
                        delegate: ItemDelegate {
                            required property int index
                            required property var modelData
                            width: djPick.width
                            highlighted: djPick.highlightedIndex === index
                            contentItem: Label { text: modelData; color: Theme.text; font: djPick.font }
                            background: Rectangle { color: highlighted ? Theme.hover : "transparent" }
                        }
                    }
                    Button {
                        objectName: "djStart"
                        text: "start mix"
                        enabled: hop.djArtists.length > 0
                        onClicked: hop.startDj(djPick.currentText)
                        contentItem: Label { text: parent.text; color: Theme.ink; font { family: Theme.font; pixelSize: Theme.fs(16); weight: Font.Medium } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { implicitWidth: 130; implicitHeight: 44; color: parent.enabled ? (parent.hovered ? Qt.lighter(Theme.accent, 1.08) : Theme.accent) : Theme.line }
                    }
                    PlainButton { text: "shuffle all"; enabled: hop.librarySize > 0; onClicked: hop.shuffleAll() }
                }
            }

            Strip {
                title: "pinned"
                empty: "right-click an album, artist or playlist and choose “pin to quickplay”."
                groups: [
                    { label: "artists", tiles: hop.quickplayPinned.filter(function (t) { return t.kind === "artist" }) },
                    { label: "albums", tiles: hop.quickplayPinned.filter(function (t) { return t.kind === "album" }) },
                    { label: "playlists", tiles: hop.quickplayPinned.filter(function (t) { return t.kind === "playlist" }) }
                ]
            }
            Strip { title: "history"; tiles: hop.quickplayHistory; empty: "albums you play show up here." }
            Strip { title: "new"; tiles: hop.quickplayNew; empty: "recently added albums show up here." }
        }
    }
}
