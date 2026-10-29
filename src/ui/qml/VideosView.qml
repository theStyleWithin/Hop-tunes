// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// collection › videos (spec §5b): a grid of what VideoScanner has found under the
// profile's folders — the same folders music is scanned from.
Item {
    id: root

    signal addFolderRequested()

    // Per-profile thumbnail override, like the album "image" button in collection › music.
    // The row is set right before open().
    FileDialog {
        id: thumbDialog
        property int row: -1
        title: "choose video thumbnail"
        nameFilters: ["pictures (*.png *.jpg *.jpeg)"]
        onAccepted: hop.setVideoThumb(row, selectedFile)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Label {
                visible: hop.videoScanning
                text: hop.videoScanTotal > 0 ? "scanning videos " + hop.videoScanDone + " / " + hop.videoScanTotal : "scanning videos…"
                color: Theme.muted; font { family: Theme.font; pixelSize: 14 }
            }
            Item { Layout.fillWidth: true }
            Label {
                visible: !hop.videoScanning && hop.videos.count > 0
                text: hop.videos.count + (hop.videos.count === 1 ? " video" : " videos")
                color: Theme.muted; font { family: Theme.font; pixelSize: 14 }
            }
        }

        ColumnLayout {
            visible: hop.videos.count === 0 && !hop.videoScanning
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.topMargin: 40
            spacing: 18
            Label {
                text: hop.videoFolders.length === 0 ? "add a folder to find videos" : "no videos found yet"
                color: Theme.text; font { family: Theme.font; pixelSize: 40; weight: Font.Light }
            }
            Label {
                text: "mkv, mp4, webm, mov, avi, wmv and a few others — anywhere under your video folders."
                color: Theme.muted; font { family: Theme.font; pixelSize: 16 }
                wrapMode: Text.WordWrap; Layout.maximumWidth: 640
            }
            Button {
                objectName: "videosChooseFolder"
                text: "choose a video folder…"
                onClicked: root.addFolderRequested()
                contentItem: Label { text: parent.text; color: Theme.ink; font { family: Theme.font; pixelSize: 18; weight: Font.Medium } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { implicitHeight: 52; implicitWidth: 260; color: parent.hovered ? Qt.lighter(hop.accent, 1.08) : hop.accent }
            }
            Item { Layout.fillHeight: true }
        }

        GridView {
            id: grid
            visible: hop.videos.count > 0
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true
            model: hop.videos
            readonly property int columns: Math.max(2, Math.floor(width / 260))
            cellWidth: width / columns; cellHeight: cellWidth * 9 / 16 + 70
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            delegate: AbstractButton {
                id: tile
                required property int index
                required property string title
                required property string path
                required property string thumb
                required property string duration
                required property string resolutionBadge
                required property string codecBadge
                required property bool hdr
                required property bool customThumb
                objectName: "videoTile_" + index
                width: grid.cellWidth - 16; height: grid.cellHeight
                hoverEnabled: true
                onClicked: hop.playVideo(tile.index)
                Accessible.name: title
                background: Item {}
                contentItem: ColumnLayout {
                    spacing: 4
                    VideoThumb {
                        key: tile.title; source: tile.thumb
                        resolutionBadge: tile.resolutionBadge; codecBadge: tile.codecBadge; hdr: tile.hdr
                        hovered: tile.hovered
                        Layout.fillWidth: true; Layout.preferredHeight: width * 9 / 16
                        Label {
                            anchors { right: parent.right; bottom: parent.bottom; margins: 4 }
                            text: tile.duration
                            color: "white"; font { family: Theme.font; pixelSize: 11 }
                        }
                    }
                    Label { text: tile.title; Layout.fillWidth: true; elide: Text.ElideRight
                            color: Theme.text; font { family: Theme.font; pixelSize: 14 } }
                    RowLayout {
                        visible: !hop.guest
                        spacing: 12
                        AbstractButton {
                            id: imageBtn
                            objectName: "videoImageBtn_" + tile.index
                            hoverEnabled: true
                            implicitHeight: 18
                            Accessible.name: "change video thumbnail"
                            onClicked: { thumbDialog.row = tile.index; thumbDialog.open() }
                            contentItem: Label {
                                text: "image"
                                color: imageBtn.hovered ? hop.accent : Theme.muted
                                font { family: Theme.font; pixelSize: 12; underline: imageBtn.hovered }
                            }
                            background: Item {}
                        }
                        AbstractButton {
                            id: resetBtn
                            visible: tile.customThumb
                            objectName: "videoResetBtn_" + tile.index
                            hoverEnabled: true
                            implicitHeight: 18
                            Accessible.name: "use the detected thumbnail"
                            onClicked: hop.clearVideoThumb(tile.index)
                            contentItem: Label {
                                text: "reset"
                                color: resetBtn.hovered ? hop.accent : Theme.muted
                                font { family: Theme.font; pixelSize: 12; underline: resetBtn.hovered }
                            }
                            background: Item {}
                        }
                    }
                }
            }
        }
    }
}
