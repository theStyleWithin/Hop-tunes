// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A wide meter along the top of a video that is kept on this device (a download or a recording): drag it left to rewind or right to skip
// ahead, or tap -10 / +10. It knows nothing about the video engine: the player gives it the position and length (milliseconds) and moves
// the picture when it asks (seekRequested). A live stream does not get one: it is just watched.
Item {
    id: root
    objectName: "kickSeekMeter"
    property real position: 0                // ms
    property real duration: 0                // ms; 0 = the engine cannot say, so there is nothing to move around in
    property int step: 10000                 // what -10 / +10 move by
    readonly property bool seekable: duration > 0
    signal seekRequested(real ms)

    function clampMs(ms) { return Math.max(0, Math.min(duration, ms)) }
    function skip(by) { if (seekable) seekRequested(clampMs(position + by)) }
    function fmt(ms) {
        const s = Math.max(0, Math.round(ms / 1000)), h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60), r = s % 60
        const two = n => (n < 10 ? "0" : "") + n
        return h > 0 ? h + ":" + two(m) + ":" + two(r) : m + ":" + two(r)
    }
    readonly property string text: seekable ? fmt(slider.pressed ? slider.value : position) + " / " + fmt(duration) : ""

    implicitHeight: 66
    Rectangle {   // a soft shade so the meter reads on any picture
        anchors.fill: parent; radius: 6
        gradient: Gradient { GradientStop { position: 0; color: Qt.rgba(0, 0, 0, 0.6) } GradientStop { position: 1; color: Qt.rgba(0, 0, 0, 0.0) } }
    }
    ColumnLayout {
        anchors { fill: parent; leftMargin: 4; rightMargin: 4; topMargin: 4 } spacing: 0
        Label { objectName: "kickSeekText"; Layout.fillWidth: true; Layout.preferredHeight: 20; elide: Text.ElideRight; text: root.text
                color: Qt.rgba(1, 1, 1, 0.85); font { family: Theme.font; pixelSize: Theme.fs(12) } }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 6
            AbstractButton {
                objectName: "kickSeekBack"
                implicitWidth: 44; implicitHeight: 36; enabled: root.seekable; opacity: enabled ? 1 : 0.4; focusPolicy: Qt.NoFocus
                onClicked: root.skip(-root.step)
                Accessible.name: "back 10 seconds"
                background: Rectangle { radius: 18; color: Qt.rgba(0, 0, 0, 0.55); border { width: 1; color: Qt.rgba(1, 1, 1, 0.5) } }
                contentItem: Label { text: "−10"; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font { family: Theme.font; pixelSize: Theme.fs(13) } }
            }
            Slider {
                id: slider
                objectName: "kickSeek"
                Layout.fillWidth: true; Layout.fillHeight: true; focusPolicy: Qt.NoFocus
                enabled: root.seekable
                from: 0; to: root.seekable ? root.duration : 1
                Binding { target: slider; property: "value"; value: root.position; when: !slider.pressed }
                onMoved: root.seekRequested(value)
                background: Rectangle {
                    x: slider.leftPadding; y: slider.topPadding + slider.availableHeight / 2 - height / 2
                    width: slider.availableWidth; height: 8; radius: 4; color: Qt.rgba(1, 1, 1, 0.28)
                    Rectangle { width: slider.visualPosition * parent.width; height: parent.height; radius: 4; color: Theme.accent }
                }
                handle: Rectangle {
                    x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width); y: slider.topPadding + slider.availableHeight / 2 - height / 2
                    width: slider.pressed ? 30 : 26; height: width; radius: width / 2; color: "white"; opacity: slider.enabled ? 1 : 0.4
                }
            }
            AbstractButton {
                objectName: "kickSeekForward"
                implicitWidth: 44; implicitHeight: 36; enabled: root.seekable; opacity: enabled ? 1 : 0.4; focusPolicy: Qt.NoFocus
                onClicked: root.skip(root.step)
                Accessible.name: "forward 10 seconds"
                background: Rectangle { radius: 18; color: Qt.rgba(0, 0, 0, 0.55); border { width: 1; color: Qt.rgba(1, 1, 1, 0.5) } }
                contentItem: Label { text: "+10"; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font { family: Theme.font; pixelSize: Theme.fs(13) } }
            }
        }
    }
}
