// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A wide meter along the top of a video: drag it left to rewind or right to skip ahead, or tap -10 / +10. It knows nothing about the video
// engine: the player gives it the position and length (milliseconds) and moves the picture when it asks (seekRequested).
// A live stream only keeps a short stretch behind "now" (Kick's live playlist holds about half a minute), so there the meter spans that
// stretch and the "live" pill jumps back to the front. A video that cannot be moved around in shows why instead of a dead bar.
Item {
    id: root
    objectName: "kickSeekMeter"
    property real position: 0                // ms
    property real duration: 0                // ms; 0 = the engine cannot say, so there is nothing to move around in
    property bool live: false
    property int step: 10000                 // what -10 / +10 move by
    readonly property bool seekable: duration > 0
    readonly property real behind: live && seekable ? Math.max(0, duration - position) : 0
    readonly property bool atLive: behind < 3000
    signal seekRequested(real ms)

    function clampMs(ms) { return Math.max(0, Math.min(duration, ms)) }
    function skip(by) { if (seekable) seekRequested(clampMs(position + by)) }
    function goLive() { if (live && seekable && !atLive) seekRequested(Math.max(0, duration - 1500)) }
    function fmt(ms) {
        const s = Math.max(0, Math.round(ms / 1000)), h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60), r = s % 60
        const two = n => (n < 10 ? "0" : "") + n
        return h > 0 ? h + ":" + two(m) + ":" + two(r) : m + ":" + two(r)
    }
    readonly property string text: !seekable ? (live ? "live: this stream cannot be rewound here" : "")
                                   : live ? (atLive ? "live" : "-" + fmt(behind) + " behind live")
                                          : fmt(slider.pressed ? slider.value : position) + " / " + fmt(duration)

    implicitHeight: 66
    Rectangle {   // a soft shade so the meter reads on any picture
        anchors.fill: parent; radius: 6
        gradient: Gradient { GradientStop { position: 0; color: Qt.rgba(0, 0, 0, 0.6) } GradientStop { position: 1; color: Qt.rgba(0, 0, 0, 0.0) } }
    }
    ColumnLayout {
        anchors { fill: parent; leftMargin: 4; rightMargin: 4; topMargin: 4 } spacing: 0
        RowLayout {
            Layout.fillWidth: true; Layout.preferredHeight: 20; spacing: 8
            Label {
                objectName: "kickLivePill"
                visible: root.live
                text: "live"; color: "white"; font { family: Theme.font; pixelSize: Theme.fs(12) }
                leftPadding: 7; rightPadding: 7; topPadding: 1; bottomPadding: 1
                background: Rectangle { radius: 3; color: root.atLive || !root.seekable ? "#e5484d" : Qt.rgba(0.9, 0.28, 0.3, 0.45) }
                MouseArea { anchors.fill: parent; anchors.margins: -8; enabled: root.live && root.seekable && !root.atLive; onClicked: root.goLive() }
            }
            Label { objectName: "kickSeekText"; Layout.fillWidth: true; elide: Text.ElideRight; text: root.text
                    color: Qt.rgba(1, 1, 1, 0.85); font { family: Theme.font; pixelSize: Theme.fs(12) } }
        }
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
                    Rectangle { width: slider.visualPosition * parent.width; height: parent.height; radius: 4; color: root.live ? "#e5484d" : Theme.accent }
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
