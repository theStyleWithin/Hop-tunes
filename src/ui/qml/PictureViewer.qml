// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Full-window picture viewer: arrow keys or buttons to move, space to start or stop a
// slideshow, escape to close. Reads straight from the pictures model.
Popup {
    id: root
    property var model
    property int current: 0
    property bool slideshow: false

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: parent ? parent.width : 0
    height: parent ? parent.height : 0
    modal: true
    padding: 0
    closePolicy: Popup.CloseOnEscape
    background: Rectangle { color: "black" }

    function openAt(row) { current = row; slideshow = false; open() }
    function step(d) {
        if (!model || model.count === 0) return
        current = (current + d + model.count) % model.count
    }
    onClosed: slideshow = false
    onOpened: scope.forceActiveFocus()

    // Bound to `current` and to the model's row count, so they update as either changes.
    readonly property string currentUrl: model && model.count > 0 ? model.fileUrlAt(current) : ""
    readonly property string currentTitle: model && model.count > 0 ? model.titleAt(current) : ""

    Timer { interval: 4000; running: root.slideshow && root.visible; repeat: true; onTriggered: root.step(1) }

    contentItem: FocusScope {
        id: scope
        focus: true
        Keys.onPressed: function (e) {
            if (e.key === Qt.Key_Right) { root.step(1); e.accepted = true }
            else if (e.key === Qt.Key_Left) { root.step(-1); e.accepted = true }
            else if (e.key === Qt.Key_Space) { root.slideshow = !root.slideshow; e.accepted = true }
        }

        Image {
            objectName: "pictureViewerImage"
            anchors.fill: parent
            source: root.currentUrl
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            cache: false
            autoTransform: true
        }

        // a soft scrim so the title and counter stay readable over bright pictures
        Rectangle {
            anchors { left: parent.left; right: parent.right; top: parent.top }
            height: 90
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#b0000000" }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }

        RowLayout {
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 20 }
            Label { text: root.currentTitle; color: "white"; elide: Text.ElideRight; Layout.fillWidth: true
                    font { family: Theme.font; pixelSize: 18 } }
            Label { text: root.model ? (root.current + 1) + " / " + root.model.count : ""; color: "white"
                    font { family: Theme.font; pixelSize: 14 } }
            AbstractButton {
                objectName: "pictureViewerClose"
                implicitWidth: 44; implicitHeight: 44
                onClicked: root.close()
                Accessible.name: "close"
                background: Rectangle { radius: 22; color: Theme.fade(0.5); border { width: 2; color: Theme.border } }
                contentItem: Label { text: "×"; color: "white"; font.pixelSize: 26; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
        }

        AbstractButton {
            objectName: "pictureViewerPrev"
            anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 20 }
            implicitWidth: 52; implicitHeight: 52
            onClicked: root.step(-1)
            Accessible.name: "previous picture"
            background: Rectangle { radius: 26; color: Theme.fade(0.5) }
            contentItem: Label { text: "‹"; color: "white"; font.pixelSize: 32; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; bottomPadding: 4 }
        }
        AbstractButton {
            objectName: "pictureViewerNext"
            anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: 20 }
            implicitWidth: 52; implicitHeight: 52
            onClicked: root.step(1)
            Accessible.name: "next picture"
            background: Rectangle { radius: 26; color: Theme.fade(0.5) }
            contentItem: Label { text: "›"; color: "white"; font.pixelSize: 32; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; bottomPadding: 4 }
        }
        AbstractButton {
            objectName: "pictureViewerSlideshow"
            anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 24 }
            implicitWidth: 150; implicitHeight: 40
            onClicked: root.slideshow = !root.slideshow
            Accessible.name: "slideshow"
            background: Rectangle { radius: 6; color: Theme.fade(0.5) }
            contentItem: Label { text: root.slideshow ? "pause slideshow" : "start slideshow"; color: "white"
                                 horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                 font { family: Theme.font; pixelSize: 14 } }
        }
    }
}
