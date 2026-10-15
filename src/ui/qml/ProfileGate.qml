// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// Shown until a profile is open (spec §3.2): first-run "create your profile", the picker
// with big tiles, and the PIN curtain. Local only: no accounts, nothing leaves the machine.
Rectangle {
    id: gate
    objectName: "profileGate"
    color: Theme.bg

    property string mode: hop.profiles.length === 0 ? "create" : "pick"
    property var pinProfile: ({})
    property string error: ""

    function show(m) { error = ""; mode = m }
    function choose(p) {
        if (p.hasPin) { pinProfile = p; pinEntry.text = ""; show("pin"); pinEntry.forceActiveFocus() }
        else error = hop.openProfile(p.id, "")
    }
    Connections {
        target: hop
        function onProfileChanged() { if (!hop.profileReady) gate.show(hop.profiles.length === 0 ? "create" : "pick") }
    }

    // ---- picker --------------------------------------------------------------------
    ColumnLayout {
        visible: gate.mode === "pick"
        anchors { fill: parent; leftMargin: 72; rightMargin: 72; topMargin: 64; bottomMargin: 48 }
        spacing: 12
        Label { text: "who's listening?"; color: Theme.strong; font { family: Theme.font; pixelSize: 52; weight: Font.Light } }
        Label { text: "profiles live on this computer only. no accounts, no sign-in."; color: Theme.muted; font { family: Theme.font; pixelSize: 16 } }
        Flow {
            Layout.fillWidth: true; Layout.topMargin: 36
            spacing: 28
            Repeater {
                model: hop.profiles
                delegate: AbstractButton {
                    id: tile
                    objectName: "tile_" + modelData.name
                    required property var modelData
                    hoverEnabled: true
                    width: 176; height: 236
                    onClicked: gate.choose(modelData)
                    Accessible.name: "open profile " + modelData.name
                    contentItem: Column {
                        spacing: 12
                        ProfileAvatar {
                            profile: tile.modelData; size: 176
                            border { width: tile.hovered || tile.activeFocus ? 3 : 0; color: Theme.strong }
                        }
                        Row {
                            spacing: 8
                            Label { text: tile.modelData.name; color: Theme.text; font { family: Theme.font; pixelSize: 20 } elide: Text.ElideRight; width: Math.min(implicitWidth, 150) }
                            Label { visible: tile.modelData.hasPin; text: "PIN"; color: Theme.dim; font { family: Theme.font; pixelSize: 12; letterSpacing: 1 } anchors.verticalCenter: parent.verticalCenter }
                        }
                    }
                    background: Item { }
                }
            }
            AbstractButton {
                id: addTile
                objectName: "addProfile"
                hoverEnabled: true
                width: 176; height: 236
                onClicked: { nameField.text = ""; gate.show("create"); nameField.forceActiveFocus() }
                Accessible.name: "add profile"
                contentItem: Column {
                    spacing: 12
                    Rectangle {
                        width: 176; height: 176; color: "transparent"
                        border { width: 2; color: addTile.hovered ? Theme.text2 : Theme.borderSoft }
                        Label { anchors.centerIn: parent; text: "+"; color: Theme.muted; font { family: Theme.font; pixelSize: 64; weight: Font.Light } }
                    }
                    Label { text: "add profile"; color: Theme.muted; font { family: Theme.font; pixelSize: 20 } }
                }
                background: Item { }
            }
        }
        Item { Layout.fillHeight: true }
        RowLayout {
            spacing: 16
            PlainButton { objectName: "guestButton"; text: "listen as guest"; onClicked: hop.openGuest() }
            Label { text: "guest mode keeps no history and saves nothing."; color: Theme.dim; font { family: Theme.font; pixelSize: 14 } }
        }
        Label { visible: gate.error !== ""; text: gate.error; color: Theme.danger; font { family: Theme.font; pixelSize: 15 } }
    }

    // ---- PIN curtain -----------------------------------------------------------------
    ColumnLayout {
        visible: gate.mode === "pin"
        anchors.centerIn: parent
        spacing: 18
        ProfileAvatar { profile: gate.pinProfile; size: 140; Layout.alignment: Qt.AlignHCenter }
        Label { text: gate.pinProfile.name || ""; color: Theme.strong; font { family: Theme.font; pixelSize: 34; weight: Font.Light } Layout.alignment: Qt.AlignHCenter }
        PinField {
            id: pinEntry
            objectName: "pinEntry"
            hint: "enter PIN"
            Layout.alignment: Qt.AlignHCenter
            onAccepted: unlock.clicked()
        }
        Label { objectName: "pinError"; text: gate.error; visible: text !== ""; color: Theme.danger; font { family: Theme.font; pixelSize: 15 } Layout.alignment: Qt.AlignHCenter }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 12
            PlainButton { text: "back"; onClicked: gate.show("pick") }
            PlainButton {
                id: unlock
                objectName: "unlock"
                text: "open"; primary: true
                enabled: pinEntry.text.length >= 4
                onClicked: { gate.error = hop.openProfile(gate.pinProfile.id, pinEntry.text); pinEntry.text = "" }
            }
        }
    }

    // ---- create ----------------------------------------------------------------------
    FolderDialog {
        id: createFolderDialog
        title: "choose your music folder"
        onAccepted: createForm.folder = selectedFolder
    }
    ColumnLayout {
        id: createForm
        visible: gate.mode === "create"
        anchors { fill: parent; leftMargin: 72; rightMargin: 72; topMargin: 64; bottomMargin: 48 }
        spacing: 14
        property string accent: hop.accents[0]
        property url folder: hop.suggestedFolderUrl

        Label {
            text: hop.profiles.length === 0 ? "welcome to hop-tunes" : "add a profile"
            color: Theme.strong; font { family: Theme.font; pixelSize: 52; weight: Font.Light }
        }
        Label {
            text: "a profile keeps your folders, settings and (soon) playlists separate. it stays on this computer."
            color: Theme.muted; font { family: Theme.font; pixelSize: 16 }
            wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.maximumWidth: 720
        }

        RowLayout {
            Layout.topMargin: 24
            spacing: 28
            ProfileAvatar {
                size: 132
                profile: ({ initial: nameField.text.trim().charAt(0).toUpperCase() || "?", accent: createForm.accent })
            }
            ColumnLayout {
                spacing: 14
                TextField {
                    id: nameField
                    objectName: "profileName"
                    placeholderText: "your name"
                    placeholderTextColor: Theme.dim
                    maximumLength: 40
                    color: Theme.text
                    font { family: Theme.font; pixelSize: 24 }
                    implicitWidth: 360
                    focus: gate.mode === "create"
                    Accessible.name: "profile name"
                    background: Rectangle { implicitHeight: 52; color: Theme.panel; border { width: nameField.activeFocus ? 2 : 1; color: nameField.activeFocus ? createForm.accent : Theme.line } }
                }
                Row {
                    spacing: 10
                    Repeater {
                        model: hop.accents
                        delegate: AbstractButton {
                            required property string modelData
                            width: 36; height: 36
                            onClicked: createForm.accent = modelData
                            Accessible.name: "colour " + modelData
                            background: Rectangle {
                                color: modelData
                                border { width: createForm.accent === modelData ? 3 : 0; color: Theme.strong }
                            }
                        }
                    }
                }
            }
        }

        Label { text: "music folder"; color: createForm.accent; font { family: Theme.font; pixelSize: 20 } Layout.topMargin: 18 }
        RowLayout {
            spacing: 14
            Label {
                text: createForm.folder.toString() ? decodeURIComponent(createForm.folder.toString().replace(/^file:\/\//, "")) : "no folder yet"
                color: createForm.folder.toString() ? Theme.text : Theme.dim
                font { family: Theme.font; pixelSize: 17 }
                elide: Text.ElideMiddle; Layout.maximumWidth: 520
            }
            PlainButton { text: "choose…"; onClicked: createFolderDialog.open() }
        }

        CheckBox {
            id: usePin
            objectName: "usePin"
            Layout.topMargin: 12
            text: "lock this profile with a PIN"
            Accessible.name: text
            indicator: Rectangle {
                implicitWidth: 24; implicitHeight: 24
                x: usePin.leftPadding; y: (usePin.height - height) / 2
                color: usePin.checked ? createForm.accent : "transparent"
                border { width: 2; color: usePin.checked ? createForm.accent : Theme.border }
                Label { anchors.centerIn: parent; visible: usePin.checked; text: "✓"; color: Theme.ink; font { pixelSize: 16; weight: Font.Bold } }
            }
            contentItem: Label { text: usePin.text; leftPadding: usePin.indicator.width + 12; color: Theme.text2; font { family: Theme.font; pixelSize: 16 } verticalAlignment: Text.AlignVCenter }
        }
        RowLayout {
            visible: usePin.checked
            spacing: 12
            PinField { id: pin1; objectName: "pin1"; hint: "PIN (4–8 digits)" }
            PinField { id: pin2; objectName: "pin2"; hint: "repeat PIN" }
        }
        Label {
            visible: usePin.checked
            text: "a PIN is a privacy curtain, not a lock: anyone who can use this computer account can still read hop-tunes files."
            color: Theme.dim; font { family: Theme.font; pixelSize: 14 }
            wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.maximumWidth: 720
        }

        Label { visible: gate.error !== ""; text: gate.error; color: Theme.danger; font { family: Theme.font; pixelSize: 15 } }
        Item { Layout.fillHeight: true }
        RowLayout {
            spacing: 12
            PlainButton { visible: hop.profiles.length > 0; text: "cancel"; onClicked: gate.show("pick") }
            PlainButton {
                objectName: "createButton"
                text: "create profile"; primary: true
                enabled: nameField.text.trim().length > 0
                onClicked: {
                    if (usePin.checked && pin1.text !== pin2.text) { gate.error = "the two PINs don't match"; return }
                    if (usePin.checked && pin1.text.length < 4) { gate.error = "a PIN is 4 to 8 digits"; return }
                    gate.error = hop.createProfile(nameField.text, createForm.accent, usePin.checked ? pin1.text : "", createForm.folder)
                    if (gate.error === "") { pin1.text = ""; pin2.text = ""; usePin.checked = false }
                }
            }
            PlainButton { visible: hop.profiles.length === 0; text: "just listen as guest"; onClicked: hop.openGuest() }
        }
    }
}
