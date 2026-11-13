// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// The live stream that keeps playing after the Kick app is left (desktop). It is one picture that lives here, in the main window, not inside
// the app: while the channel page is open the picture sits exactly over the page's video slot (full controls); when you go back to the hub, to
// another part of Hop-tunes or out of the app, it shrinks to a small window you can drag anywhere. A small ✕ in its corner always closes it, and
// it also ends when a song or a video starts (hop.dock, src/apps/StreamDock.cpp, holds those rules).
Item {
    id: root
    objectName: "streamDock"
    property Item slot: null                 // where the open channel page wants the picture (null: it goes to the corner)
    property real bottomInset: 16            // room kept free at the bottom (the mini player)
    readonly property bool on: !!hop.dock && hop.dock.active
    visible: on
    z: 40

    readonly property bool followSlot: on && slot !== null && slot.visible && slot.width > 40 && slot.height > 40 && !hop.dock.fullscreen
    readonly property bool corner: on && !followSlot && !hop.dock.fullscreen
    readonly property real dockW: Math.max(260, Math.min(420, Math.round(width * 0.24)))
    readonly property real dockH: Math.round(dockW * 9 / 16)
    property point spot: Qt.point(-1, -1)    // where the person put it (-1: the corner), kept for the session
    function clampX(x) { return Math.max(8, Math.min(Math.max(8, width - dockW - 8), x)) }
    function clampY(y) { return Math.max(8, Math.min(Math.max(8, height - dockH - 8), y)) }
    readonly property real homeX: spot.x >= 0 ? clampX(spot.x) : clampX(width - dockW - 24)
    readonly property real homeY: spot.y >= 0 ? clampY(spot.y) : clampY(height - dockH - bottomInset - 8)

    // while the page wants the picture, the box sits exactly over the page's slot
    property real slotX: 0
    property real slotY: 0
    property real slotW: 0
    property real slotH: 0
    function syncSlot() {
        if (!slot) return
        const p = slot.mapToItem(root, 0, 0)
        slotX = p.x; slotY = p.y; slotW = slot.width; slotH = slot.height
    }
    Timer { interval: 16; repeat: true; running: root.followSlot; triggeredOnStart: true; onTriggered: root.syncSlot() }
    property bool settling: false            // only the move between the corner and the page is animated
    onFollowSlotChanged: { settling = true; settleTimer.restart() }
    Timer { id: settleTimer; interval: 260; onTriggered: root.settling = false }

    Item {
        id: box
        objectName: "streamDockBox"
        x: root.followSlot ? root.slotX : root.homeX
        y: root.followSlot ? root.slotY : root.homeY
        width: root.followSlot ? root.slotW : root.dockW
        height: root.followSlot ? root.slotH : root.dockH
        Behavior on x { enabled: root.settling && !drag.active; NumberAnimation { duration: Theme.reduceMotion ? 0 : 200; easing.type: Easing.InOutQuad } }
        Behavior on y { enabled: root.settling && !drag.active; NumberAnimation { duration: Theme.reduceMotion ? 0 : 200; easing.type: Easing.InOutQuad } }
        Behavior on width { enabled: root.settling; NumberAnimation { duration: Theme.reduceMotion ? 0 : 200; easing.type: Easing.InOutQuad } }
        Behavior on height { enabled: root.settling; NumberAnimation { duration: Theme.reduceMotion ? 0 : 200; easing.type: Easing.InOutQuad } }

        KickPlayer {
            id: player
            objectName: "dockPlayer"
            anchors.fill: parent
            url: hop.dock ? hop.dock.url : ""
            showControls: !root.corner
            fullscreen: !!hop.dock && hop.dock.fullscreen
            onFullscreenToggled: hop.dock.fullscreen = !hop.dock.fullscreen
            onRetryRequested: hop.dock.requestOpen()          // a fresh address comes from the channel page
        }

        // the small window's frame and buttons (only in the corner)
        Rectangle { anchors.fill: parent; visible: root.corner; color: "transparent"; radius: 6; border { width: 1; color: Theme.border } }
        Row {
            visible: root.corner
            anchors { left: parent.left; top: parent.top; margins: 6 } spacing: 6
            Label {
                objectName: "streamDockName"
                text: hop.dock ? hop.dock.name : ""
                color: "white"; elide: Text.ElideRight; width: Math.min(implicitWidth, box.width - 90)
                leftPadding: 6; rightPadding: 6; topPadding: 2; bottomPadding: 2
                font { family: Theme.font; pixelSize: Theme.fs(12) }
                background: Rectangle { radius: 3; color: Qt.rgba(0, 0, 0, 0.6) }
            }
            Label { text: "live"; color: "white"; leftPadding: 6; rightPadding: 6; topPadding: 2; bottomPadding: 2
                    font { family: Theme.font; pixelSize: Theme.fs(12) }
                    background: Rectangle { radius: 3; color: "#e5484d" } }
        }
        AbstractButton {     // ✕: close the stream and move on
            id: closeButton
            objectName: "streamDockStop"
            visible: root.corner
            anchors { right: parent.right; top: parent.top; margins: 6 }
            implicitWidth: 28; implicitHeight: 28; focusPolicy: Qt.NoFocus
            onClicked: hop.dock.stop()
            Accessible.name: "close the stream"
            background: Rectangle { radius: 14; color: Qt.rgba(0, 0, 0, 0.65); border { width: 1; color: Qt.rgba(1, 1, 1, 0.55) } }
            contentItem: Label { text: "✕"; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 13 }
        }
        AbstractButton {     // ⤢: back to the channel page
            objectName: "streamDockExpand"
            visible: root.corner
            anchors { right: parent.right; bottom: parent.bottom; margins: 6 }
            implicitWidth: 28; implicitHeight: 28; focusPolicy: Qt.NoFocus
            onClicked: hop.dock.requestOpen()
            Accessible.name: "back to the channel"
            background: Rectangle { radius: 14; color: Qt.rgba(0, 0, 0, 0.65); border { width: 1; color: Qt.rgba(1, 1, 1, 0.55) } }
            contentItem: Label { text: "⤢"; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 15 }
        }
        AbstractButton {     // mute / sound on
            objectName: "streamDockMute"
            visible: root.corner
            anchors { left: parent.left; bottom: parent.bottom; margins: 6 }
            implicitWidth: 28; implicitHeight: 28; focusPolicy: Qt.NoFocus
            property bool muted: false
            onClicked: { muted = !muted; if (player.surface) player.surface.volume = muted ? 0 : 100 }
            Accessible.name: muted ? "sound on" : "mute"
            background: Rectangle { radius: 14; color: Qt.rgba(0, 0, 0, 0.65); border { width: 1; color: Qt.rgba(1, 1, 1, 0.55) } }
            contentItem: Label { text: parent.muted ? "×" : "♪"; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 15 }
        }
        // drag the small window anywhere (the buttons above keep their own clicks)
        DragHandler {
            id: drag
            target: null
            enabled: root.corner
            property real startX: 0
            property real startY: 0
            onActiveChanged: if (active) { startX = box.x; startY = box.y }
            onTranslationChanged: if (active) root.spot = Qt.point(root.clampX(startX + translation.x), root.clampY(startY + translation.y))
        }
    }
}
