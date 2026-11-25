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

    property string mode: hop.profileCount === 0 ? "create" : "pick"
    property var pinProfile: ({})
    property string error: ""
    property var ejectTarget: ({})    // the profile being ejected from the picker
    property string ejectStep: "pin"  // pin (secured profiles only) | save | confirm
    property var ejectResult: null    // exportProfileById() result of the saved file
    property bool ejectHaveFile: false
    property string ejectPinText: ""
    property var importInfo: null     // inspectProfileFile() result of the chosen file
    property url importFile

    function show(m) { error = ""; mode = m }
    function choose(p) {
        if (p.hasPin) { pinProfile = p; pinEntry.text = ""; show("pin"); pinEntry.forceActiveFocus() }
        else error = hop.openProfile(p.id, "")
    }
    function startEject(p) {
        ejectTarget = p; ejectResult = null; ejectHaveFile = false; ejectPinText = ""; error = ""
        ejectStep = p.hasPin ? "pin" : "save"
        show("eject")
        if (p.hasPin) ejectPinEntry.text = ""
    }
    function ejectFileName() {
        return "hop-tunes-" + String(ejectTarget.name || "profile").replace(/[^A-Za-z0-9._-]+/g, "-") + "-" + Qt.formatDate(new Date(), "yyyy-MM-dd") + ".hoptunes.json"
    }
    Connections {
        target: hop
        function onProfileChanged() { if (!hop.profileReady) gate.show(hop.profileCount === 0 ? "create" : "pick") }
    }

    FileDialog {
        id: ejectDialog
        title: "save the profile before it goes"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        currentFile: "file://" + gate.ejectFileName()
        nameFilters: ["hop-tunes profile (*.json)"]
        onAccepted: {
            gate.ejectHaveFile = false
            gate.ejectResult = hop.exportProfileById(gate.ejectTarget.id, gate.ejectPinText, selectedFile)
            if (gate.ejectResult.ok) { gate.error = ""; gate.ejectStep = "confirm" }
            else { gate.error = gate.ejectResult.error; if (gate.ejectTarget.hasPin) gate.ejectStep = "pin" }
        }
    }

    FileDialog {
        id: importDialog
        title: "choose a hop-tunes profile file"
        nameFilters: ["hop-tunes profile (*.json)", "all files (*)"]
        onAccepted: {
            gate.importFile = selectedFile
            gate.importInfo = hop.inspectProfileFile(selectedFile)
            gate.error = gate.importInfo.ok ? "" : gate.importInfo.error
            if (gate.importInfo.ok) { importName.text = gate.importInfo.suggestedName; importPin.text = ""; gate.show("import") }
        }
    }

    // ---- picker --------------------------------------------------------------------
    ColumnLayout {
        visible: gate.mode === "pick"
        anchors { fill: parent; leftMargin: Theme.compact ? 20 : 72; rightMargin: Theme.compact ? 20 : 72; topMargin: Theme.compact ? 32 : 64; bottomMargin: Theme.compact ? 24 : 48 }
        spacing: 12
        Label { text: "who's listening?"; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(52); weight: Font.Light }
                wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label { text: "profiles live on this computer only. no accounts, no sign-in."; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(16) }
                wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Flow {
            id: tiles
            readonly property real tileSize: Theme.compact ? Math.floor((width - 16) / 2) : 176
            Layout.fillWidth: true; Layout.topMargin: Theme.compact ? 16 : 36
            spacing: Theme.compact ? 16 : 28
            Repeater {
                model: hop.profiles
                delegate: AbstractButton {
                    id: tile
                    objectName: "tile_" + modelData.name
                    required property var modelData
                    hoverEnabled: true
                    width: tiles.tileSize; height: tiles.tileSize + 60
                    onClicked: gate.choose(modelData)
                    Accessible.name: "open profile " + modelData.name
                    contentItem: Column {
                        spacing: 12
                        ProfileAvatar {
                            profile: tile.modelData; size: tiles.tileSize
                            border { width: tile.hovered || tile.activeFocus ? 3 : 0; color: Theme.strong }
                        }
                        Row {
                            spacing: 8
                            Label { text: tile.modelData.name; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(20) } elide: Text.ElideRight; width: Math.min(implicitWidth, tiles.tileSize - 30) }
                            Label { visible: tile.modelData.hasPin; text: "PIN"; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(12); letterSpacing: 1 } anchors.verticalCenter: parent.verticalCenter }
                        }
                    }
                    background: Item { }
                    // a right click (or a long press on a touch screen) opens the menu; a left click still just opens the profile
                    MouseArea { anchors.fill: parent; acceptedButtons: Qt.RightButton; onClicked: function (m) { tileMenu.popup(m.x, m.y) } }
                    TapHandler { acceptedDevices: PointerDevice.TouchScreen; onLongPressed: tileMenu.popup(tile.width / 2, tile.height / 2) }
                    Menu {
                        id: tileMenu
                        background: Rectangle { implicitWidth: 260; color: Theme.panel; border { width: 1; color: Theme.line } }
                        MenuItem {
                            objectName: "tileOpen_" + tile.modelData.name
                            text: "open"
                            onTriggered: gate.choose(tile.modelData)
                            contentItem: Label { text: parent.text; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(15) } leftPadding: 6 }
                            background: Rectangle { implicitHeight: 36; color: parent.highlighted ? Theme.hover : "transparent" }
                        }
                        MenuItem {
                            objectName: "tileHide_" + tile.modelData.name
                            enabled: !tile.modelData.hasPin
                            text: tile.modelData.hasPin ? "hide (not for profiles with a PIN)" : "hide"
                            onTriggered: gate.error = hop.hideProfile(tile.modelData.id)
                            contentItem: Label { text: parent.text; color: parent.enabled ? Theme.text : Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(15) } leftPadding: 6 }
                            background: Rectangle { implicitHeight: 36; color: parent.highlighted ? Theme.hover : "transparent" }
                        }
                        MenuItem {
                            objectName: "tileEject_" + tile.modelData.name
                            text: "eject"
                            onTriggered: gate.startEject(tile.modelData)
                            contentItem: Label { text: parent.text; color: Theme.danger; font { family: Theme.font; pixelSize: Theme.fs(15) } leftPadding: 6 }
                            background: Rectangle { implicitHeight: 36; color: parent.highlighted ? Theme.hover : "transparent" }
                        }
                    }
                }
            }
            AbstractButton {
                id: addTile
                objectName: "addProfile"
                hoverEnabled: true
                width: tiles.tileSize; height: tiles.tileSize + 60
                onClicked: { nameField.text = ""; gate.show("create"); nameField.forceActiveFocus() }
                Accessible.name: "add profile"
                contentItem: Column {
                    spacing: 12
                    Rectangle {
                        width: tiles.tileSize; height: tiles.tileSize; color: "transparent"
                        border { width: 2; color: addTile.hovered ? Theme.text2 : Theme.borderSoft }
                        Label { anchors.centerIn: parent; text: "+"; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(64); weight: Font.Light } }
                    }
                    Label { text: "add profile"; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(20) } }
                }
                background: Item { }
            }
        }
        Label {
            objectName: "hiddenHint"
            visible: hop.hiddenProfiles.length > 0
            text: hop.hiddenProfiles.length + (hop.hiddenProfiles.length === 1 ? " profile is hidden" : " profiles are hidden") + ": open a profile or listen as guest, then settings > hidden profiles shows them again."
            color: Theme.dim; wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.topMargin: 6
            font { family: Theme.font; pixelSize: Theme.fs(14) }
        }
        Item { Layout.fillHeight: true }
        Flow {
            spacing: 16
            Layout.fillWidth: true
            PlainButton { objectName: "guestButton"; text: "listen as guest"; onClicked: hop.openGuest() }
            PlainButton { objectName: "importProfile"; text: "import a profile…"; onClicked: importDialog.open() }
            Label { text: "guest mode keeps no history and saves nothing."; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(14) } }
        }
        Label { visible: gate.error !== ""; text: gate.error; color: Theme.danger; font { family: Theme.font; pixelSize: Theme.fs(15) } }
    }

    // ---- eject from the picker: save the profile to a file, check it, then remove it from this computer ------------------
    ColumnLayout {
        objectName: "ejectFlow"
        visible: gate.mode === "eject"
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 560)
        spacing: 16
        ProfileAvatar { profile: gate.ejectTarget; size: 120; Layout.alignment: Qt.AlignHCenter }
        Label { text: "eject \u201c" + (gate.ejectTarget.name || "") + "\u201d"; color: Theme.strong; Layout.alignment: Qt.AlignHCenter
                font { family: Theme.font; pixelSize: Theme.fs(30); weight: Font.Light } }
        Label {
            text: "ejecting saves this profile to a file, checks the file, and only then removes the profile from this computer: its settings, playlists and history. your music files are not touched. import the file on any device to get it back."
            color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
            font { family: Theme.font; pixelSize: Theme.fs(15) }
        }
        PinField {
            id: ejectPinEntry
            objectName: "ejectPin"
            visible: gate.ejectStep === "pin"
            hint: "enter this profile's PIN"
            Layout.alignment: Qt.AlignHCenter
            onAccepted: ejectPinNext.clicked()
        }
        Label { objectName: "ejectError"; text: gate.error; visible: text !== "" && gate.mode === "eject"; color: Theme.danger; wrapMode: Text.WordWrap; Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter; font { family: Theme.font; pixelSize: Theme.fs(15) } }
        Label {
            objectName: "ejectSaved"
            visible: gate.ejectStep === "confirm" && !!gate.ejectResult && gate.ejectResult.ok
            text: gate.ejectResult && gate.ejectResult.ok ? "saved and checked: " + gate.ejectResult.where + " (" + Math.max(1, Math.round(gate.ejectResult.bytes / 1024)) + " KB). keep it somewhere safe." : ""
            color: Theme.text; wrapMode: Text.WordWrap; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
            font { family: Theme.font; pixelSize: Theme.fs(15) }
        }
        PlainButton {
            objectName: "ejectHaveFile"
            visible: gate.ejectStep === "confirm"
            Layout.alignment: Qt.AlignHCenter
            text: (gate.ejectHaveFile ? "\u2611 " : "\u2610 ") + "I have the file and it is safe"
            onClicked: gate.ejectHaveFile = !gate.ejectHaveFile
        }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 12
            PlainButton { objectName: "ejectCancel"; text: "cancel, keep the profile"; onClicked: { gate.ejectResult = null; gate.show("pick") } }
            PlainButton {
                id: ejectPinNext
                objectName: "ejectPinNext"
                visible: gate.ejectStep === "pin"
                text: "next"; primary: true
                enabled: ejectPinEntry.text.length >= 4
                onClicked: { gate.ejectPinText = ejectPinEntry.text; ejectPinEntry.text = ""; gate.error = ""; gate.ejectStep = "save" }
            }
            PlainButton {
                objectName: "ejectChooseFile"
                visible: gate.ejectStep === "save"
                text: "choose where to save the file…"; primary: true
                onClicked: ejectDialog.open()
            }
            PlainButton {
                objectName: "ejectConfirm"
                visible: gate.ejectStep === "confirm"
                enabled: gate.ejectHaveFile
                text: "eject"; danger: true
                onClicked: {
                    const m = hop.ejectProfileById(gate.ejectTarget.id, gate.ejectPinText, gate.ejectHaveFile)
                    gate.ejectPinText = ""; gate.ejectResult = null; gate.ejectHaveFile = false
                    gate.error = m
                    if (m === "") gate.show(hop.profileCount === 0 ? "create" : "pick")
                    else if (gate.ejectTarget.hasPin) gate.ejectStep = "pin"
                }
            }
        }
    }

    // ---- PIN curtain -----------------------------------------------------------------
    ColumnLayout {
        visible: gate.mode === "pin"
        anchors.centerIn: parent
        spacing: 18
        ProfileAvatar { profile: gate.pinProfile; size: 140; Layout.alignment: Qt.AlignHCenter }
        Label { text: gate.pinProfile.name || ""; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(34); weight: Font.Light } Layout.alignment: Qt.AlignHCenter }
        PinField {
            id: pinEntry
            objectName: "pinEntry"
            hint: "enter PIN"
            Layout.alignment: Qt.AlignHCenter
            onAccepted: unlock.clicked()
        }
        Label { objectName: "pinError"; text: gate.error; visible: text !== ""; color: Theme.danger; font { family: Theme.font; pixelSize: Theme.fs(15) } Layout.alignment: Qt.AlignHCenter }
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
    FolderPick {
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
            text: hop.profileCount === 0 ? "welcome to hop-tunes" : "add a profile"
            color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(52); weight: Font.Light }
            wrapMode: Text.WordWrap; Layout.fillWidth: true
        }
        Label {
            text: "a profile keeps your folders, settings and (soon) playlists separate. it stays on this computer."
            color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(16) }
            wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.maximumWidth: 720
        }

        GridLayout {   // phones: the avatar sits above the name
            Layout.topMargin: Theme.compact ? 12 : 24
            Layout.fillWidth: true
            columns: Theme.compact ? 1 : 2
            columnSpacing: 28; rowSpacing: 14
            ProfileAvatar {
                size: Theme.compact ? 96 : 132
                profile: ({ initial: nameField.text.trim().charAt(0).toUpperCase() || "?", accent: createForm.accent })
            }
            ColumnLayout {
                spacing: 14
                Layout.fillWidth: true
                TextField {
                    id: nameField
                    objectName: "profileName"
                    placeholderText: "your name"
                    placeholderTextColor: Theme.dim
                    maximumLength: 40
                    color: Theme.text
                    font { family: Theme.font; pixelSize: Theme.fs(24) }
                    implicitWidth: 360
                    Layout.fillWidth: true; Layout.maximumWidth: 480
                    focus: gate.mode === "create"
                    Accessible.name: "profile name"
                    background: Rectangle { implicitHeight: 52; color: Theme.panel; border { width: nameField.activeFocus ? 2 : 1; color: nameField.activeFocus ? createForm.accent : Theme.line } }
                }
                Flow {
                    spacing: 10
                    Layout.fillWidth: true
                    Repeater {
                        model: hop.accents
                        delegate: AbstractButton {
                            required property string modelData
                            width: Theme.compact ? 44 : 36; height: width
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

        // Phones: the profile is made first and the music folder is chosen afterwards, in the collection
        // (the profile still starts with the phone's own Music folder, so nothing is lost).
        ColumnLayout {
            objectName: "createFolderBlock"
            visible: !Theme.compact
            Layout.fillWidth: true
            spacing: 14
            Label { text: "music folder"; color: createForm.accent; font { family: Theme.font; pixelSize: Theme.fs(20) } Layout.topMargin: 18 }
            RowLayout {
                spacing: 14
                Layout.fillWidth: true
                Label {
                    text: createForm.folder.toString() ? decodeURIComponent(createForm.folder.toString().replace(/^file:\/\//, "")) : "no folder yet"
                    color: createForm.folder.toString() ? Theme.text : Theme.dim
                    font { family: Theme.font; pixelSize: Theme.fs(17) }
                    elide: Text.ElideMiddle; Layout.maximumWidth: 520; Layout.fillWidth: true
                }
                PlainButton { text: "choose…"; onClicked: createFolderDialog.open() }
            }
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
                Label { anchors.centerIn: parent; visible: usePin.checked; text: "✓"; color: Theme.ink; font { pixelSize: Theme.fs(16); weight: Font.Bold } }
            }
            contentItem: Label { text: usePin.text; leftPadding: usePin.indicator.width + 12; color: Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(16) } verticalAlignment: Text.AlignVCenter }
        }
        GridLayout {
            visible: usePin.checked
            columns: Theme.compact ? 1 : 2
            columnSpacing: 12; rowSpacing: 10
            Layout.fillWidth: true
            PinField { id: pin1; objectName: "pin1"; hint: "PIN (4–8 digits)" }
            PinField { id: pin2; objectName: "pin2"; hint: "repeat PIN" }
        }
        Label {
            visible: usePin.checked
            text: "a PIN is a privacy curtain, not a lock: anyone who can use this computer account can still read hop-tunes files."
            color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(14) }
            wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.maximumWidth: 720
        }

        Label { visible: gate.error !== ""; text: gate.error; color: Theme.danger; font { family: Theme.font; pixelSize: Theme.fs(15) } }
        Item { Layout.fillHeight: true }
        Flow {
            spacing: 12
            Layout.fillWidth: true
            PlainButton { visible: hop.profileCount > 0; text: "cancel"; onClicked: gate.show("pick") }
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
            PlainButton { visible: hop.profileCount === 0; text: "just listen as guest"; onClicked: hop.openGuest() }
            PlainButton { objectName: "importProfileFirst"; visible: hop.profileCount === 0; text: "import a profile…"; onClicked: importDialog.open() }
        }
    }

    // ---- import ----------------------------------------------------------------------
    ColumnLayout {
        id: importForm
        objectName: "importForm"
        visible: gate.mode === "import"
        anchors { fill: parent; leftMargin: Theme.compact ? 20 : 72; rightMargin: Theme.compact ? 20 : 72; topMargin: Theme.compact ? 32 : 64; bottomMargin: 48 }
        spacing: 12
        Label { text: "import a profile"; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(40); weight: Font.Light }
                wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label {
            visible: !!gate.importInfo && gate.importInfo.ok
            text: gate.importInfo && gate.importInfo.ok
                  ? "found \u201c" + gate.importInfo.name + "\u201d: " + gate.importInfo.playlists + " playlists, "
                    + gate.importInfo.songsPlayed + " songs with play counts, " + gate.importInfo.pins + " pins."
                  : ""
            color: Theme.text; wrapMode: Text.WordWrap; Layout.fillWidth: true; font { family: Theme.font; pixelSize: Theme.fs(17) }
        }
        Label {
            objectName: "importMissing"
            visible: !!gate.importInfo && gate.importInfo.ok && gate.importInfo.missing.length > 0
            text: gate.importInfo && gate.importInfo.ok
                  ? "these folders are not on this device, so add them again from your collection afterwards (your playlists find their songs by path):\n" + gate.importInfo.missing.join("\n")
                  : ""
            color: Theme.muted; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true; font { family: Theme.font; pixelSize: Theme.fs(14) }
        }
        TextField {
            id: importName
            objectName: "importName"
            placeholderText: "profile name"
            maximumLength: 40
            Layout.fillWidth: true; Layout.maximumWidth: 420
            color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(18) }
            background: Rectangle { color: "transparent"; border { width: 2; color: Theme.border } implicitHeight: 44 }
        }
        PinField { id: importPin; objectName: "importPin"; hint: "PIN (optional, 4\u20138 digits)" }
        Label { visible: gate.error !== ""; text: gate.error; color: Theme.danger; wrapMode: Text.WordWrap; Layout.fillWidth: true; font { family: Theme.font; pixelSize: Theme.fs(15) } }
        Item { Layout.fillHeight: true }
        Flow {
            spacing: 12; Layout.fillWidth: true
            PlainButton { text: "cancel"; onClicked: gate.show(hop.profileCount === 0 ? "create" : "pick") }
            PlainButton {
                objectName: "importButton"
                text: "import"; primary: true
                enabled: importName.text.trim().length > 0
                onClicked: {
                    gate.error = hop.importProfile(gate.importFile, importName.text, importPin.text)
                    if (gate.error === "") importPin.text = ""
                }
            }
        }
    }
}
