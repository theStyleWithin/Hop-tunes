// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// settings: this profile (name, colour, picture, PIN, remove) and the folders it watches.
Item {
    id: root
    signal addFolderRequested()

    property string profileMsg: ""
    property bool confirmRemove: false

    // Segmented choice: one of a few options, the chosen one filled with the accent.
    component Choice: Row {
        id: ch
        property var options: []     // [[value, label], …]
        property var value
        signal picked(var v)
        spacing: 0
        Repeater {
            model: ch.options
            delegate: Component {   // explicit: Qt 6.4 does not wrap delegates inside inline components
            AbstractButton {
                id: ob
                required property var modelData
                readonly property bool on: ch.value === modelData[0]
                objectName: "choice_" + modelData[0]
                hoverEnabled: true
                onClicked: ch.picked(modelData[0])
                Accessible.name: modelData[1]
                Accessible.checked: on
                contentItem: Label {
                    text: ob.modelData[1]; color: ob.on ? Theme.ink : Theme.text
                    font { family: Theme.font; pixelSize: 15 }
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    implicitWidth: Math.max(90, ob.contentItem.implicitWidth + 28); implicitHeight: 40
                    color: ob.on ? hop.accent : (ob.hovered ? Theme.hover : "transparent")
                    border { width: 1; color: ob.on ? hop.accent : Theme.border }
                }
            }
            }
        }
    }

    FileDialog {
        id: avatarDialog
        title: "choose a picture"
        nameFilters: ["pictures (*.png *.jpg *.jpeg *.webp *.bmp *.gif)"]
        onAccepted: root.profileMsg = hop.setProfileAvatar(selectedFile)
    }

    Flickable {
        anchors.fill: parent
        contentHeight: col.implicitHeight + 40
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { }

    ColumnLayout {
        id: col
        width: parent.width - 96
        x: 48; y: 28
        spacing: 18

        // ---- profile ------------------------------------------------------------
        Label { text: hop.guest ? "guest mode" : "profile"; color: hop.accent; font { family: Theme.font; pixelSize: 24 } }
        Label {
            visible: hop.guest
            text: "guest mode keeps no history and saves nothing. switch profile to create your own."
            color: Theme.muted; font { family: Theme.font; pixelSize: 15 }
        }
        RowLayout {
            visible: !hop.guest
            spacing: 24
            AbstractButton {
                id: avatarBtn
                hoverEnabled: true
                onClicked: avatarDialog.open()
                Accessible.name: "change profile picture"
                contentItem: ProfileAvatar { profile: hop.profile; size: 112; border { width: avatarBtn.hovered ? 3 : 0; color: Theme.strong } }
                background: Item { }
                ToolTip.visible: hovered; ToolTip.delay: 500; ToolTip.text: "change picture"
            }
            ColumnLayout {
                spacing: 12
                RowLayout {
                    spacing: 12
                    TextField {
                        id: nameEdit
                        text: hop.profile.name || ""
                        maximumLength: 40
                        color: Theme.text
                        font { family: Theme.font; pixelSize: 20 }
                        implicitWidth: 300
                        Accessible.name: "profile name"
                        background: Rectangle { implicitHeight: 46; color: Theme.panel; border { width: nameEdit.activeFocus ? 2 : 1; color: nameEdit.activeFocus ? hop.accent : Theme.line } }
                        onAccepted: root.profileMsg = hop.renameProfile(text)
                    }
                    PlainButton { text: "rename"; enabled: nameEdit.text.trim() !== "" && nameEdit.text !== hop.profile.name; onClicked: root.profileMsg = hop.renameProfile(nameEdit.text) }
                }
                Row {
                    spacing: 10
                    Repeater {
                        model: hop.accents
                        delegate: AbstractButton {
                            required property string modelData
                            width: 32; height: 32
                            onClicked: root.profileMsg = hop.setProfileAccent(modelData)
                            Accessible.name: "colour " + modelData
                            background: Rectangle { color: modelData; border { width: hop.accent === modelData ? 3 : 0; color: Theme.strong } }
                        }
                    }
                    Item { width: 14; height: 1 }
                    PlainButton { visible: !!hop.profile.avatar; text: "remove picture"; onClicked: root.profileMsg = hop.setProfileAvatar("") }
                }
            }
        }

        // PIN
        ColumnLayout {
            visible: !hop.guest
            spacing: 10
            Label {
                text: hop.profile.hasPin ? "this profile is locked with a PIN." : "no PIN. anyone using this computer account can open this profile."
                color: Theme.text2; font { family: Theme.font; pixelSize: 16 }
            }
            RowLayout {
                spacing: 12
                PinField { id: curPin; visible: !!hop.profile.hasPin; hint: "current PIN" }
                PinField { id: newPin; hint: hop.profile.hasPin ? "new PIN" : "new PIN (4–8 digits)" }
                PinField { id: newPin2; hint: "repeat PIN" }
                PlainButton {
                    text: hop.profile.hasPin ? "change PIN" : "set PIN"
                    enabled: newPin.text.length >= 4 && (!hop.profile.hasPin || curPin.text.length >= 4)
                    onClicked: {
                        if (newPin.text !== newPin2.text) { root.profileMsg = "the two PINs don't match"; return }
                        root.profileMsg = hop.setProfilePin(curPin.text, newPin.text)
                        if (root.profileMsg === "") { root.profileMsg = "PIN saved"; curPin.text = newPin.text = newPin2.text = "" }
                    }
                }
                PlainButton {
                    visible: !!hop.profile.hasPin
                    text: "remove PIN"
                    enabled: curPin.text.length >= 4
                    onClicked: {
                        root.profileMsg = hop.setProfilePin(curPin.text, "")
                        if (root.profileMsg === "") { root.profileMsg = "PIN removed"; curPin.text = "" }
                    }
                }
            }
            Label {
                text: "a PIN is a privacy curtain, not a lock: anyone who can use this computer account can still read hop-tunes files."
                color: Theme.dim; font { family: Theme.font; pixelSize: 13 }
            }
        }
        Label { visible: root.profileMsg !== ""; text: root.profileMsg; color: Theme.muted; font { family: Theme.font; pixelSize: 15 } }

        // ---- playback (spec §5) ------------------------------------------------------
        Label { text: "playback"; color: hop.accent; font { family: Theme.font; pixelSize: 24 } Layout.topMargin: 16 }
        RowLayout {
            spacing: 16
            Label { text: "crossfade"; color: Theme.text; font { family: Theme.font; pixelSize: 17 } Layout.preferredWidth: 140 }
            Slider {
                id: xfade
                objectName: "crossfadeSlider"
                from: 0; to: 12; stepSize: 1; snapMode: Slider.SnapAlways
                implicitWidth: 260
                value: hop.player.crossfadeMs / 1000
                onMoved: hop.player.crossfadeMs = Math.round(value) * 1000
                Accessible.name: "crossfade seconds"
                background: Rectangle {
                    x: xfade.leftPadding; y: xfade.topPadding + xfade.availableHeight / 2 - height / 2
                    width: xfade.availableWidth; height: 4; color: Theme.track
                    Rectangle { width: xfade.visualPosition * parent.width; height: parent.height; color: hop.accent }
                }
                handle: Rectangle {
                    x: xfade.leftPadding + xfade.visualPosition * (xfade.availableWidth - width)
                    y: xfade.topPadding + xfade.availableHeight / 2 - height / 2
                    width: 16; height: 16; radius: 8; color: Theme.text
                }
            }
            Label {
                text: hop.player.crossfadeMs === 0 ? "off (gapless)" : Math.round(hop.player.crossfadeMs / 1000) + " s"
                color: Theme.muted; font { family: Theme.font; pixelSize: 15 } Layout.preferredWidth: 110
            }
        }
        Label {
            text: "songs from one album always play gapless, without a fade."
            color: Theme.dim; font { family: Theme.font; pixelSize: 13 }
        }
        RowLayout {
            spacing: 16
            Label { text: "even volume"; color: Theme.text; font { family: Theme.font; pixelSize: 17 } Layout.preferredWidth: 140 }
            Choice {
                enabled: hop.player.replayGainAvailable
                options: [[0, "off"], [1, "per song"], [2, "per album"]]
                value: hop.player.replayGainMode
                onPicked: function (v) { hop.player.replayGainMode = v }
            }
            Label {
                visible: hop.player.replayGainMode !== 0
                text: "pre-amp " + (hop.player.replayGainPreamp > 0 ? "+" : "") + hop.player.replayGainPreamp.toFixed(0) + " dB"
                color: Theme.muted; font { family: Theme.font; pixelSize: 15 }
            }
            PlainButton { visible: hop.player.replayGainMode !== 0; text: "−"; implicitWidth: 44; onClicked: hop.player.replayGainPreamp = hop.player.replayGainPreamp - 1 }
            PlainButton { visible: hop.player.replayGainMode !== 0; text: "+"; implicitWidth: 44; onClicked: hop.player.replayGainPreamp = hop.player.replayGainPreamp + 1 }
        }
        Label {
            text: hop.player.replayGainAvailable
                  ? "uses the ReplayGain tags in your files so songs play at a similar loudness. \u201cper album\u201d keeps an album's own quiet and loud parts. takes effect from the next song."
                  : "even volume needs GStreamer's replaygain plugin (gst-plugins-good)."
            color: Theme.dim; font { family: Theme.font; pixelSize: 13 }
            wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.maximumWidth: 760
        }

        // ---- appearance (spec §4.5) ---------------------------------------------------
        Label { text: "appearance"; color: hop.accent; font { family: Theme.font; pixelSize: 24 } Layout.topMargin: 16 }
        RowLayout {
            spacing: 16
            Label { text: "theme"; color: Theme.text; font { family: Theme.font; pixelSize: 17 } Layout.preferredWidth: 140 }
            Choice {
                objectName: "themeChoice"
                options: hop.systemThemeAvailable ? [["dark", "dark"], ["light", "light"], ["system", "match system"]] : [["dark", "dark"], ["light", "light"]]
                value: hop.theme
                onPicked: function (v) { hop.theme = v }
            }
        }
        RowLayout {
            spacing: 16
            Label { text: "motion"; color: Theme.text; font { family: Theme.font; pixelSize: 17 } Layout.preferredWidth: 140 }
            Choice {
                options: [[false, "animate"], [true, "reduce motion"]]
                value: hop.reduceMotion
                onPicked: function (v) { hop.reduceMotion = v }
            }
        }
        Label {
            visible: hop.guest
            text: "guest mode does not keep these settings."
            color: Theme.dim; font { family: Theme.font; pixelSize: 13 }
        }

        // ---- music folders --------------------------------------------------------

        Label { text: "music folders"; color: hop.accent; font { family: Theme.font; pixelSize: 24 } Layout.topMargin: 16 }
        Label {
            text: "hop-tunes reads these folders and never changes or uploads your files."
            color: Theme.muted; font { family: Theme.font; pixelSize: 15 }
        }

        Repeater {
            model: hop.folders
            delegate: Rectangle {
                required property string modelData
                Layout.fillWidth: true; Layout.maximumWidth: 760
                implicitHeight: 56
                color: Theme.panel
                RowLayout {
                    anchors { fill: parent; leftMargin: 16; rightMargin: 8 }
                    Label { text: modelData; color: Theme.text; font { family: Theme.font; pixelSize: 17 } elide: Text.ElideMiddle; Layout.fillWidth: true }
                    Button {
                        text: "remove"
                        onClicked: hop.removeFolder(modelData)
                        Accessible.name: "remove " + modelData
                        contentItem: Label { text: parent.text; color: Theme.text2; font { family: Theme.font; pixelSize: 15 } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { implicitWidth: 96; implicitHeight: 44; color: parent.hovered ? Theme.hover : "transparent" }
                    }
                }
            }
        }

        RowLayout {
            spacing: 14
            Button {
                text: "+ add folder"
                onClicked: root.addFolderRequested()
                contentItem: Label { text: parent.text; color: Theme.text; font { family: Theme.font; pixelSize: 16 } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { implicitWidth: 150; implicitHeight: 44; color: "transparent"; border { width: 2; color: Theme.border } }
            }
            Button {
                text: hop.scanning ? "scanning…" : "rescan now"
                enabled: !hop.scanning && hop.folders.length > 0
                onClicked: hop.rescan()
                contentItem: Label { text: parent.text; color: parent.enabled ? Theme.text : Theme.dim; font { family: Theme.font; pixelSize: 16 } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { implicitWidth: 150; implicitHeight: 44; color: "transparent"; border { width: 2; color: Theme.border } }
            }
        }

        // ---- remove profile ------------------------------------------------------
        RowLayout {
            visible: !hop.guest
            Layout.topMargin: 24
            spacing: 14
            PlainButton { visible: !root.confirmRemove; text: "remove this profile…"; danger: true; onClicked: root.confirmRemove = true }
            Label {
                visible: root.confirmRemove
                text: "remove \u201c" + (hop.profile.name || "") + "\u201d? its settings and history go. your music files are not touched."
                color: Theme.text2; font { family: Theme.font; pixelSize: 15 }
            }
            PlainButton { visible: root.confirmRemove; text: "cancel"; onClicked: root.confirmRemove = false }
            PlainButton { visible: root.confirmRemove; text: "remove"; danger: true; onClicked: { root.confirmRemove = false; root.profileMsg = hop.removeProfile() } }
        }

        Label { text: "hop-tunes " + hop.version + " · offline · GPL-3.0"; color: Theme.faint; font { family: Theme.font; pixelSize: 13 } Layout.topMargin: 24 }
    }
    }
}
