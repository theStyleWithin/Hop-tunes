// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// settings: this profile (name, colour, picture, PIN, remove) and the folders it watches.
Item {
    id: root
    signal back()
    signal addFolderRequested()
    signal addVideoFolderRequested()
    signal addVideoCategoryFolderRequested(string category)
    signal addSectionFolderRequested(string kind)

    property string profileMsg: ""
    property bool confirmRemove: false
    // eject / export: "" idle, "export" or "eject" while a saved file is being shown
    property string keepMode: ""
    property var keepResult: null
    property bool haveFile: false

    // A settings row: label and control side by side; on phones it is a rounded card with the control under its label.
    component SRow: Rectangle {
        id: sr
        property real spacing: 16
        default property alias content: sg.data
        color: Theme.compact ? Theme.panel : "transparent"
        radius: 12
        Layout.fillWidth: Theme.compact
        implicitWidth: sg.implicitWidth + (Theme.compact ? 28 : 0)
        implicitHeight: sg.implicitHeight + (Theme.compact ? 28 : 0)
        GridLayout {
            id: sg
            anchors { fill: parent; margins: Theme.compact ? 14 : 0 }
            columns: Theme.compact ? 1 : 99
            columnSpacing: sr.spacing
            rowSpacing: 8
        }
    }

    // Segmented choice: one of a few options, the chosen one filled with the accent.
    component Choice: Row {
        id: ch
        property var options: []     // [[value, label], …]
        property var value
        signal picked(var v)
        spacing: 0
        Layout.fillWidth: Theme.compact
        Layout.minimumWidth: 0
        Layout.preferredWidth: Theme.compact ? 100 : -1
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
                    font { family: Theme.font; pixelSize: Theme.fs(15) }
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
                width: Theme.compact ? Math.floor(ch.width / Math.max(1, ch.options.length)) : implicitWidth
                background: Rectangle {
                    implicitWidth: Math.max(90, ob.contentItem.implicitWidth + 28); implicitHeight: Theme.compact ? 44 : 40
                    radius: Theme.compact ? 22 : 0
                    color: ob.on ? Theme.accent : (ob.hovered ? Theme.hover : "transparent")
                    border { width: 1; color: ob.on ? Theme.accent : (Theme.compact ? Theme.borderSoft : Theme.border) }
                }
            }
            }
        }
    }

    FileDialog {
        id: keepDialog
        title: root.keepMode === "eject" ? "save your profile before it goes" : "save a copy of your profile"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        currentFile: "file://" + hop.suggestedExportName()
        nameFilters: ["hop-tunes profile (*.json)"]
        onAccepted: { root.haveFile = false; root.keepResult = hop.exportProfileTo(selectedFile) }
        onRejected: if (!root.keepResult || !root.keepResult.ok) root.keepMode = ""
    }

    FileDialog {
        id: avatarDialog
        title: "choose a picture"
        nameFilters: ["pictures (*.png *.jpg *.jpeg *.webp *.bmp *.gif)"]
        onAccepted: root.profileMsg = hop.setProfileAvatar(selectedFile)
    }

    Flickable {
        objectName: "settingsFlick"
        id: settingsFlick
        anchors.fill: parent
        contentHeight: col.implicitHeight + 40
        contentWidth: width
        flickableDirection: Flickable.VerticalFlick
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { }

    ColumnLayout {
        id: col
        onWidthChanged: Qt.callLater(col.forceLayout)
        Connections { target: Theme; function onCompactChanged() { Qt.callLater(col.forceLayout) } }
        width: settingsFlick.width - 2 * Theme.pad
        x: Theme.pad; y: 28
        spacing: 18

        RowLayout {   // phones: settings is a page under profile
            visible: Theme.compact
            AbstractButton {
                objectName: "settingsBack"
                implicitHeight: 44
                onClicked: root.back()
                Accessible.name: "back to profile"
                background: Item {}
                contentItem: Label { text: "‹  profile"; color: Theme.accent; verticalAlignment: Text.AlignVCenter; font { family: Theme.font; pixelSize: 18 } }
            }
        }
        Label { visible: Theme.compact; text: "settings"; color: Theme.strong; font { family: Theme.font; pixelSize: 46; weight: Font.Light } }
        SRow {   // phones and tablets: how you move around (the bottom bar or the side drawer)
            visible: Window.window !== null && Window.window.mobileNav === true
            Label { text: "navigation"; color: Theme.text; font { family: Theme.font; pixelSize: 20 } }
            Label { text: "choose how you move around"; color: Theme.muted; font { family: Theme.font; pixelSize: 14 } Layout.topMargin: -6 }
            Choice {
                objectName: "navStyleChoice"
                options: [["bar", "bottom bar"], ["drawer", "side drawer"]]
                value: hop.navStyle
                onPicked: function (v) { hop.navStyle = v }
            }
        }
        SRow {   // phones: how videos open (the rotate button in the player turns one video either way)
            visible: Theme.compact
            Label { text: "video player"; color: Theme.text; font { family: Theme.font; pixelSize: 20 } }
            Label { text: "orientation: how videos open"; color: Theme.muted; font { family: Theme.font; pixelSize: 14 } Layout.topMargin: -6 }
            Choice {
                objectName: "videoOrientationChoice"
                options: [["portrait", "portrait"], ["auto", "auto"], ["landscape", "landscape"]]
                value: hop.videoOrientation
                onPicked: function (v) { hop.videoOrientation = v }
            }
        }
        SRow {
            visible: Window.window !== null && Window.window.mobileNav === true
            Label { text: "on tablets"; color: Theme.text; font { family: Theme.font; pixelSize: 20 } }
            Label { text: "screens wider than 600 dp show a side rail or a permanent drawer, whichever you pick above"
                    color: Theme.muted; font { family: Theme.font; pixelSize: 14 }
                    wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.topMargin: -6 }
        }

        // ---- profile ------------------------------------------------------------
        Label { text: hop.guest ? "guest mode" : "profile"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(24) } }
        Label {
            wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.minimumWidth: 0
            visible: hop.guest
            text: "guest mode keeps no history and saves nothing. switch profile to create your own."
            color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(15) }
        }
        GridLayout {   // phones: the picture sits above the name
            visible: !hop.guest
            Layout.fillWidth: true
            columns: Theme.compact ? 1 : 2
            columnSpacing: 24; rowSpacing: 12
            AbstractButton {
                id: avatarBtn
                hoverEnabled: true
                onClicked: avatarDialog.open()
                Accessible.name: "change profile picture"
                contentItem: ProfileAvatar { profile: hop.profile; size: Theme.compact ? 80 : 112; border { width: avatarBtn.hovered ? 3 : 0; color: Theme.strong } }
                background: Item { }
                ToolTip.visible: hovered; ToolTip.delay: 500; ToolTip.text: "change picture"
            }
            ColumnLayout {
                spacing: 12
                Layout.fillWidth: true
                RowLayout {
                    spacing: 12
                    Layout.fillWidth: true
                    TextField {
                        id: nameEdit
                        Layout.fillWidth: true; Layout.maximumWidth: 300; Layout.minimumWidth: 0
                        text: hop.profile.name || ""
                        maximumLength: 40
                        color: Theme.text
                        font { family: Theme.font; pixelSize: Theme.fs(20) }
                        implicitWidth: Theme.compact ? 120 : 300
                        Accessible.name: "profile name"
                        background: Rectangle { implicitHeight: 46; color: Theme.panel; border { width: nameEdit.activeFocus ? 2 : 1; color: nameEdit.activeFocus ? Theme.accent : Theme.line } }
                        onAccepted: root.profileMsg = hop.renameProfile(text)
                    }
                    PlainButton { text: "rename"; enabled: nameEdit.text.trim() !== "" && nameEdit.text !== hop.profile.name; onClicked: root.profileMsg = hop.renameProfile(nameEdit.text) }
                }
                Flow {
                    spacing: 10
                    Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 100
                    Repeater {
                        model: hop.accents
                        delegate: AbstractButton {
                            required property string modelData
                            width: 32; height: 32
                            onClicked: root.profileMsg = hop.setProfileAccent(modelData)
                            Accessible.name: "colour " + modelData
                            background: Rectangle { color: modelData; border { width: Theme.accent === modelData ? 3 : 0; color: Theme.strong } }
                        }
                    }
                    Item { width: 14; height: 1 }
                    PlainButton { visible: !!hop.profile.avatar; text: "remove picture"; onClicked: root.profileMsg = hop.setProfileAvatar("") }
                }
            }
        }

        // PIN
        Item {
            visible: !hop.guest
            Layout.fillWidth: true; Layout.minimumWidth: 0
            implicitWidth: 100; implicitHeight: pinCol.implicitHeight
        ColumnLayout {
            id: pinCol
            width: parent.width
            spacing: 10
            Label {
            wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.minimumWidth: 0
                text: hop.profile.hasPin ? "this profile is locked with a PIN." : "no PIN. anyone using this computer account can open this profile."
                color: Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(16) }
            }
            GridLayout {
                columns: Theme.compact ? 1 : 4
                columnSpacing: 12; rowSpacing: 10
                Layout.fillWidth: true
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
            wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.minimumWidth: 0
                text: "a PIN is a privacy curtain, not a lock: anyone who can use this computer account can still read hop-tunes files."
                color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) }
            }
        }
        }
        Label { visible: root.profileMsg !== ""; text: root.profileMsg; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(15) } }

        // ---- keep your profile: export a copy, or eject it from this device -------------
        ColumnLayout {
            visible: !hop.guest
            Layout.fillWidth: true; Layout.minimumWidth: 0
            Layout.topMargin: 24
            spacing: 10
            Label { text: "take your profile with you"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(24) } }
            Label {
                text: "a profile file holds your name, colour, picture, settings, playlists, hearts, history, pins and resume points. not your music, not your PIN. import it on any device, in any version, from the profile picker."
                color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.minimumWidth: 0
                font { family: Theme.font; pixelSize: Theme.fs(14) }
            }
            Flow {
                Layout.fillWidth: true; spacing: 12
                visible: root.keepMode === ""
                PlainButton { objectName: "exportProfile"; text: "save a copy…"; onClicked: { root.keepMode = "export"; root.keepResult = null; keepDialog.open() } }
                PlainButton { objectName: "ejectProfile"; text: "eject profile…"; danger: true; onClicked: { root.keepMode = "eject"; root.keepResult = null; root.haveFile = false; keepDialog.open() } }
            }
            Label {
                objectName: "keepFailed"
                visible: root.keepMode !== "" && !!root.keepResult && !root.keepResult.ok
                text: root.keepResult ? root.keepResult.error : ""
                color: Theme.danger; wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.minimumWidth: 0
                font { family: Theme.font; pixelSize: Theme.fs(15) }
            }
            Label {
                objectName: "keepSaved"
                visible: root.keepMode !== "" && !!root.keepResult && root.keepResult.ok
                text: root.keepResult && root.keepResult.ok
                      ? "saved and checked: " + root.keepResult.where + " (" + Math.max(1, Math.round(root.keepResult.bytes / 1024)) + " KB). keep it somewhere safe."
                      : ""
                color: Theme.text; wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.minimumWidth: 0
                font { family: Theme.font; pixelSize: Theme.fs(15) }
            }
            Label {
                visible: root.keepMode === "eject" && !!root.keepResult && root.keepResult.ok
                text: "ejecting removes \u201c" + (hop.profile.name || "") + "\u201d from this device: its settings, playlists and history. your music files are not touched."
                color: Theme.text2; wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.minimumWidth: 0
                font { family: Theme.font; pixelSize: Theme.fs(15) }
            }
            PlainButton {
                objectName: "haveTheFile"
                visible: root.keepMode === "eject" && !!root.keepResult && root.keepResult.ok
                text: (root.haveFile ? "\u2611 " : "\u2610 ") + "I have the file and it is safe"
                onClicked: root.haveFile = !root.haveFile
            }
            Flow {
                Layout.fillWidth: true; spacing: 12
                visible: root.keepMode !== ""
                PlainButton {
                    objectName: "confirmEject"
                    visible: root.keepMode === "eject" && !!root.keepResult && root.keepResult.ok
                    enabled: root.haveFile
                    text: "eject"; danger: true
                    onClicked: { var m = hop.ejectProfile(root.haveFile); root.keepMode = ""; root.keepResult = null; root.profileMsg = m }
                }
                PlainButton { visible: root.keepMode === "eject" && (!root.keepResult || !root.keepResult.ok); text: "try again"; onClicked: keepDialog.open() }
                PlainButton { text: root.keepMode === "eject" ? "cancel, keep my profile" : "done"; onClicked: { root.keepMode = ""; root.keepResult = null; root.haveFile = false } }
            }
        }


        // ---- playback (spec §5) ------------------------------------------------------
        Label { text: "playback"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(24) } Layout.topMargin: 16 }
        SRow {
            spacing: 16
            Label { text: "crossfade"; color: Theme.text; font { family: Theme.font; pixelSize: Theme.compact ? 20 : 17 } Layout.preferredWidth: Theme.compact ? -1 : 140 }
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
                    Rectangle { width: xfade.visualPosition * parent.width; height: parent.height; color: Theme.accent }
                }
                handle: Rectangle {
                    x: xfade.leftPadding + xfade.visualPosition * (xfade.availableWidth - width)
                    y: xfade.topPadding + xfade.availableHeight / 2 - height / 2
                    width: 16; height: 16; radius: 8; color: Theme.text
                }
            }
            Label {
            wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.minimumWidth: 0
                text: hop.player.crossfadeMs === 0 ? "off (gapless)" : Math.round(hop.player.crossfadeMs / 1000) + " s"
                color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(15) } Layout.preferredWidth: 110
            }
        }
        Label {
            wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.minimumWidth: 0
            text: "songs from one album always play gapless, without a fade."
            color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) }
        }
        SRow {
            spacing: 16
            Label { text: "even volume"; color: Theme.text; font { family: Theme.font; pixelSize: Theme.compact ? 20 : 17 } Layout.preferredWidth: Theme.compact ? -1 : 140 }
            Choice {
                enabled: hop.player.replayGainAvailable
                options: [[0, "off"], [1, "per song"], [2, "per album"]]
                value: hop.player.replayGainMode
                onPicked: function (v) { hop.player.replayGainMode = v }
            }
            Label {
            wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.minimumWidth: 0
                visible: hop.player.replayGainMode !== 0
                text: "pre-amp " + (hop.player.replayGainPreamp > 0 ? "+" : "") + hop.player.replayGainPreamp.toFixed(0) + " dB"
                color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(15) }
            }
            PlainButton { visible: hop.player.replayGainMode !== 0; text: "−"; implicitWidth: 44; onClicked: hop.player.replayGainPreamp = hop.player.replayGainPreamp - 1 }
            PlainButton { visible: hop.player.replayGainMode !== 0; text: "+"; implicitWidth: 44; onClicked: hop.player.replayGainPreamp = hop.player.replayGainPreamp + 1 }
        }
        Label {
            text: hop.player.replayGainAvailable
                  ? "uses the ReplayGain tags in your files so songs play at a similar loudness. \u201cper album\u201d keeps an album's own quiet and loud parts. takes effect from the next song."
                  : "even volume needs GStreamer's replaygain plugin (gst-plugins-good)."
            color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) }
            wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: 760
        }

        // ---- appearance (spec §4.5) ---------------------------------------------------
        Label { text: "appearance"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(24) } Layout.topMargin: 16 }
        SRow {
            spacing: 16
            Label { text: "theme"; color: Theme.text; font { family: Theme.font; pixelSize: Theme.compact ? 20 : 17 } Layout.preferredWidth: Theme.compact ? -1 : 140 }
            Choice {
                objectName: "themeChoice"
                options: hop.systemThemeAvailable ? [["dark", "dark"], ["light", "light"], ["system", "match system"]] : [["dark", "dark"], ["light", "light"]]
                value: hop.theme
                onPicked: function (v) { hop.theme = v }
            }
        }
        SRow {   // the sound tap behind the visualizer and the games can be switched off: music never depends on it
            objectName: "visualizerGamesRow"
            visible: hop.player.analysisAvailable
            spacing: 16
            Label { text: "visualizer & games"; color: Theme.text; font { family: Theme.font; pixelSize: Theme.compact ? 20 : 17 } Layout.preferredWidth: Theme.compact ? -1 : 140 }
            Label { visible: Theme.compact || hop.analysisSafeMode; Layout.fillWidth: true; wrapMode: Text.WordWrap; Layout.topMargin: Theme.compact ? -6 : 0
                    text: hop.analysisSafeMode ? "switched off: the app stopped unexpectedly while it was on. turn it back on if you want to try again."
                                               : "listens to the song for the visualizer and the games. if you ever have a playback problem, switch it off."
                    color: hop.analysisSafeMode ? Theme.danger : Theme.muted; font { family: Theme.font; pixelSize: 14 } }
            Choice {
                objectName: "visualizerGamesChoice"
                options: [["vgOn", "on"], ["vgOff", "off"]]
                value: hop.gameSettings.visualizerGames && !hop.analysisSafeMode ? "vgOn" : "vgOff"
                onPicked: function (v) { hop.setGameSetting("visualizerGames", v === "vgOn") }
            }
        }
        SRow {   // a game controller: starting a song opens Now playing
            objectName: "autoNowPlayingRow"
            spacing: 16
            Label { text: "controller"; color: Theme.text; font { family: Theme.font; pixelSize: Theme.compact ? 20 : 17 } Layout.preferredWidth: Theme.compact ? -1 : 140 }
            Label { visible: Theme.compact; text: "when a game controller is connected, starting a song opens Now playing"; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true
                    font { family: Theme.font; pixelSize: 14 } Layout.topMargin: -6 }
            Choice {
                objectName: "autoNowPlayingChoice"
                options: [["controllerOpen", "open Now playing"], ["controllerStay", "stay where I am"]]
                value: hop.gameSettings.autoNowPlaying ? "controllerOpen" : "controllerStay"
                onPicked: function (v) { hop.setGameSetting("autoNowPlaying", v === "controllerOpen") }
            }
        }
        SRow {
            spacing: 16
            Label { text: "motion"; color: Theme.text; font { family: Theme.font; pixelSize: Theme.compact ? 20 : 17 } Layout.preferredWidth: Theme.compact ? -1 : 140 }
            Choice {
                options: [[false, "animate"], [true, "reduce motion"]]
                value: hop.reduceMotion
                onPicked: function (v) { hop.reduceMotion = v }
            }
        }
        Label {
            visible: hop.guest
            text: "guest mode does not keep these settings."
            color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) }
        }

        // ---- video settings ----------------------------------------------------------
        Label { text: "video settings"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(24) } Layout.topMargin: 16 }
        SRow {
            spacing: 16
            Label { text: "viewing style"; color: Theme.text; font { family: Theme.font; pixelSize: Theme.compact ? 20 : 17 } Layout.preferredWidth: Theme.compact ? -1 : 140 }
            Choice {
                objectName: "videoStyleChoice"
                options: [["window", "window"], ["theater", "theater"], ["fullscreen", "fullscreen"]]
                value: hop.videoViewStyle
                onPicked: function (v) { hop.videoViewStyle = v }
            }
        }
        Label {
            text: hop.videoViewStyle === "window" ? "the video sits beside a list of your other videos, with the movie's details underneath."
                : hop.videoViewStyle === "theater" ? "a wide video across the window, with the details and your other videos underneath."
                : "the video fills the whole screen."
            color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) }
            wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: 640
        }
        SRow {
            spacing: 16
            Layout.topMargin: 8
            Label { text: "livestream chat"; color: Theme.text; font { family: Theme.font; pixelSize: Theme.compact ? 20 : 17 } Layout.preferredWidth: Theme.compact ? -1 : 140 }
            Choice {
                objectName: "videoChatChoice"
                options: [[false, "off"], [true, "on"]]
                value: hop.videoChat
                onPicked: function (v) { hop.videoChat = v }
            }
        }
        RowLayout {
            visible: hop.videoChat
            spacing: 16
            Label { text: "chat look"; color: Theme.text; font { family: Theme.font; pixelSize: Theme.compact ? 20 : 17 } Layout.preferredWidth: Theme.compact ? -1 : 140 }
            Choice {
                objectName: "videoChatLookChoice"
                options: [["clean", "clean"], ["lively", "lively"]]
                value: hop.videoChatLook
                onPicked: function (v) { hop.videoChatLook = v }
            }
        }
        Label {
            text: "a pretend live chat beside the video, as if you were watching a stream. nothing is streamed and nobody in it is real: the chatters are generated on your computer, react to the details you filled in on the movie's page, and answer when you type. clean is plain and tidy; lively adds colours, badges and emote chips."
            color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) }
            wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: 640
        }
        SRow {
            spacing: 16
            Layout.topMargin: 8
            Label { text: "comments"; color: Theme.text; font { family: Theme.font; pixelSize: Theme.compact ? 20 : 17 } Layout.preferredWidth: Theme.compact ? -1 : 140 }
            Choice {
                objectName: "videoCommentsChoice"
                options: [["off", "off"], ["mine", "only mine"], ["populated", "populated"]]
                value: hop.videoComments
                onPicked: function (v) { hop.videoComments = v }
            }
        }
        Label {
            text: "a comments section under the video (window and theater only). off hides it completely. only mine is a box where you can write comments, as many as you like, and nobody ever answers. populated adds made-up commenters and replies, who also answer what you write after a little while. nobody is real; only your own comments are saved."
            color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) }
            wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: 640
        }
        Label {
            wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.minimumWidth: 0
            text: "while watching: t switches to theater, f to fullscreen, c shows or hides the chat, esc steps back."
            color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) }
        }

        // ---- apps settings -----------------------------------------------------------
        Label { text: "apps settings"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(24) } Layout.topMargin: 16 }
        SRow {
            objectName: "appsRow"
            spacing: 16
            Label { text: "apps"; color: Theme.text; font { family: Theme.font; pixelSize: Theme.compact ? 20 : 17 } Layout.preferredWidth: Theme.compact ? -1 : 140 }
            Choice {
                objectName: "appsEnabledChoice"
                options: [["appsOff", "off"], ["appsOn", "show the apps tab"]]
                value: hop.appsEnabled ? "appsOn" : "appsOff"
                onPicked: function (v) { hop.appsEnabled = v === "appsOn" }
            }
        }
        Label {
            wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: 640
            text: "apps are small programs that are part of hop-tunes and open inside its window. they are off until you switch them on here, so a fresh hop-tunes shows every other tab and stays exactly as quiet as before. an app that needs the internet says so and asks you first."
            color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) }
        }

        SRow {   // apps that use the internet: each one asks, and can be switched off here
            objectName: "kickNetRow"
            visible: hop.netAppsBuilt && hop.appsEnabled
            spacing: 16
            Label { text: "kick"; color: Theme.text; font { family: Theme.font; pixelSize: Theme.compact ? 20 : 17 } Layout.preferredWidth: Theme.compact ? -1 : 140 }
            Choice {
                objectName: "kickNetChoice"
                options: [["kickNetOff", "not allowed to use the internet"], ["kickNetOn", "allowed"]]
                value: hop.netAllowedApps.indexOf("kick") >= 0 ? "kickNetOn" : "kickNetOff"
                onPicked: function (v) { hop.setAppNetAllowed("kick", v === "kickNetOn") }
            }
        }
        Label {
            visible: hop.netAppsBuilt && hop.appsEnabled
            wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.maximumWidth: 640
            text: "kick contacts kick.com, web.kick.com, images.kick.com, stream.kick.com, files.kick.com and live-video.net, only while the kick app is open. your follow and block lists stay on this computer (in this profile) and are never sent anywhere."
            color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) }
        }
        ColumnLayout {   // the kick lists
            objectName: "kickListsBox"
            visible: hop.netAppsBuilt && hop.appsEnabled && hop.kick && (hop.kick.blocked.length > 0 || hop.kick.following.length > 0)
            Layout.fillWidth: true; Layout.maximumWidth: 640; spacing: 6
            Label { visible: hop.kick && hop.kick.following.length > 0; text: "kick: following " + (hop.kick ? hop.kick.following.length : 0) + " channels"; color: Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(15) } }
            Label { visible: hop.kick && hop.kick.blocked.length > 0; text: "kick: blocked streamers"; color: Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(15) } Layout.topMargin: 6 }
            Repeater {
                model: hop.kick ? hop.kick.blocked : []
                delegate: RowLayout {
                    required property string modelData
                    Layout.fillWidth: true
                    Label { text: modelData; color: Theme.text; Layout.fillWidth: true; elide: Text.ElideRight; font { family: Theme.font; pixelSize: Theme.fs(15) } }
                    Button { objectName: "kickUnblock_" + modelData; text: "unblock"; onClicked: hop.kick.unblock(modelData) }
                }
            }
            RowLayout {
                spacing: 10
                Button { objectName: "kickClearBlocked"; visible: hop.kick && hop.kick.blocked.length > 0; text: "unblock everyone"; onClicked: hop.kick.clearBlocked() }
            }
        }

        ColumnLayout {   // offline kick: the room set aside for downloads and recordings (same idea as a posting limit: you choose it, any time)
            id: offlineBox
            objectName: "offlineBox"
            visible: hop.netAppsBuilt && hop.appsEnabled && hop.offline
            Layout.fillWidth: true; Layout.maximumWidth: 640; spacing: 8
            property string moveMsg: ""
            Label { text: "kick offline storage"; color: Theme.text; Layout.topMargin: 8; font { family: Theme.font; pixelSize: Theme.compact ? 20 : 17 } }
            Label {
                objectName: "offlineSummary"
                Layout.fillWidth: true; wrapMode: Text.WordWrap; color: hop.offline && !hop.offline.available ? Theme.danger : Theme.text2
                text: hop.offline ? hop.offline.summary : ""
                font { family: Theme.font; pixelSize: Theme.fs(14) }
            }
            Rectangle {   // the meter
                objectName: "offlineMeter"
                Layout.fillWidth: true; height: 8; color: Theme.line; radius: 4
                Rectangle { width: Math.max(hop.offline && hop.offline.usedBytes > 0 ? 4 : 0, parent.width * (hop.offline ? hop.offline.fraction : 0)); height: parent.height; radius: 4
                            color: hop.offline && hop.offline.nearLimit ? Theme.danger : Theme.accent }
            }
            Label { text: "set aside up to:"; color: Theme.muted; Layout.topMargin: 6; font { family: Theme.font; pixelSize: Theme.fs(14) } }
            RowLayout {
                spacing: 10; Layout.fillWidth: true
                TextField {
                    id: limitEdit
                    objectName: "offlineLimitEdit"
                    Layout.preferredWidth: 110; implicitHeight: 42; selectByMouse: true
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    validator: DoubleValidator { bottom: 0; decimals: 2; notation: DoubleValidator.StandardNotation }
                    property bool useGB: hop.offline ? hop.offline.limitMB >= 1024 && hop.offline.limitMB % 1024 === 0 : true
                    text: hop.offline ? String(useGB ? hop.offline.limitMB / 1024 : hop.offline.limitMB) : ""
                    color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(16) }
                    Accessible.name: "offline storage limit"
                    background: Rectangle { color: Theme.panel; border { width: limitEdit.activeFocus ? 2 : 1; color: limitEdit.activeFocus ? Theme.accent : Theme.line } }
                    onAccepted: offlineBox.applyLimit()
                }
                Choice {
                    id: unitChoice
                    objectName: "offlineUnit"
                    options: [["offlineMB", "MB"], ["offlineGB", "GB"]]
                    value: limitEdit.useGB ? "offlineGB" : "offlineMB"
                    onPicked: function (v) { limitEdit.useGB = (v === "offlineGB") }
                }
                PlainButton { objectName: "offlineSetLimit"; text: "set limit"; implicitHeight: 42; onClicked: offlineBox.applyLimit() }
            }
            function applyLimit() {
                const n = Number(limitEdit.text.replace(",", "."))
                if (!(n > 0) || !hop.offline.setLimitMB(n * (limitEdit.useGB ? 1024 : 1))) offlineBox.moveMsg = "the smallest limit is 50 MB."
                else offlineBox.moveMsg = ""
            }
            Label {
                Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) }
                text: "downloads and recordings in the kick app are kept in their own folder inside hop-tunes (not as loose video files). nothing is ever deleted to make room: at the limit, or when the drive is nearly full, new downloads pause and you decide. you can change the limit any time."
            }
            Label { text: "where it is kept:"; color: Theme.muted; Layout.topMargin: 6; font { family: Theme.font; pixelSize: Theme.fs(14) } }
            Label {
                objectName: "offlineWhere"
                Layout.fillWidth: true; wrapMode: Text.WrapAnywhere; color: Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(14) }
                text: hop.offline ? (hop.offline.location === "" ? "hop-tunes' own folder (" + hop.offline.folder + ")" : hop.offline.folder) : ""
            }
            RowLayout {
                spacing: 10
                PlainButton { objectName: "offlineChoose"; text: "choose a drive or folder"; implicitHeight: 42; onClicked: offlineFolder.open() }
                PlainButton { objectName: "offlineDefault"; visible: hop.offline && hop.offline.location !== ""; text: "back to the default"; implicitHeight: 42
                              onClicked: offlineBox.moveMsg = hop.offline.moveToLocation("") }
            }
            Label { objectName: "offlineMessage"; visible: offlineBox.moveMsg !== ""; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.danger; text: offlineBox.moveMsg
                    font { family: Theme.font; pixelSize: Theme.fs(14) } }
            FolderPick {
                id: offlineFolder
                title: "choose where offline kick is kept"
                onAccepted: offlineBox.moveMsg = hop.offlineMoveTo(selectedFolder)
            }
            Component.onCompleted: if (hop.offline) hop.offline.refresh()
        }

        // ---- music folders ---------------------------------------------------------
        // The music scanner's list. Videos use their own list below once one is added; until
        // then they scan these same folders, so existing libraries keep working.

        Label { text: "music folders"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(24) } Layout.topMargin: 16 }
        Label {
            wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.minimumWidth: 0
            text: "hop-tunes reads these folders and never changes or uploads your files."
            color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(15) }
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
                    Label { text: modelData; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(17) } elide: Text.ElideMiddle; Layout.fillWidth: true }
                    Label { objectName: "folderNotConnected"; visible: hop.missingFolders.indexOf(modelData) >= 0; text: "not connected"; color: Theme.danger
                            font { family: Theme.font; pixelSize: Theme.fs(13) } }
                    Button {
                        text: "remove"
                        onClicked: hop.removeFolder(modelData)
                        Accessible.name: "remove " + modelData
                        contentItem: Label { text: parent.text; color: Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(15) } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { implicitWidth: 96; implicitHeight: 44; color: parent.hovered ? Theme.hover : "transparent" }
                    }
                }
            }
        }

        Flow {
            spacing: 14
            Layout.fillWidth: true
            Button {
                text: "+ add folder"
                onClicked: root.addFolderRequested()
                contentItem: Label { text: parent.text; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(16) } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { implicitWidth: 150; implicitHeight: 44; color: "transparent"; border { width: 2; color: Theme.border } }
            }
            Button {
                text: hop.scanning ? "scanning…" : "rescan now"
                enabled: !hop.scanning && hop.folders.length > 0
                onClicked: hop.rescan()
                contentItem: Label { text: parent.text; color: parent.enabled ? Theme.text : Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(16) } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { implicitWidth: 150; implicitHeight: 44; color: "transparent"; border { width: 2; color: Theme.border } }
            }
        }

        // ---- video folders ---------------------------------------------------------
        // The movies in the videos tab. Until one is added here it uses the media folders above.
        Label { text: "movies folders"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(24) } Layout.topMargin: 16 }
        Label {
            visible: hop.videoFolders.length === 0
            text: "no movies folders yet."
            color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(15) }
        }
        Repeater {
            model: hop.videoFolders
            delegate: Rectangle {
                required property string modelData
                Layout.fillWidth: true; Layout.maximumWidth: 760
                implicitHeight: 56
                color: Theme.panel
                RowLayout {
                    anchors { fill: parent; leftMargin: 16; rightMargin: 8 }
                    Label { text: modelData; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(17) } elide: Text.ElideMiddle; Layout.fillWidth: true }
                    Button {
                        text: "remove"
                        onClicked: hop.removeVideoFolder(modelData)
                        Accessible.name: "remove video folder " + modelData
                        contentItem: Label { text: parent.text; color: Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(15) } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { implicitWidth: 96; implicitHeight: 44; color: parent.hovered ? Theme.hover : "transparent" }
                    }
                }
            }
        }
        Button {
            text: "+ add video folder"
            onClicked: root.addVideoFolderRequested()
            contentItem: Label { text: parent.text; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(16) } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            background: Rectangle { implicitWidth: 190; implicitHeight: 44; color: "transparent"; border { width: 2; color: Theme.border } }
        }

        // ---- shows, cartoons, anime, other videos: each has its own folders (the folders above are movies) ----
        Repeater {
            model: ["shows", "cartoons", "anime", "other"]
            delegate: ColumnLayout {
                id: vc
                required property string modelData
                readonly property string label: modelData === "other" ? "other video" : modelData
                readonly property var paths: hop.videoCategoryFolders[modelData] || []
                Layout.fillWidth: true
                spacing: 8
                Label { text: vc.label + " folders"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(24) } Layout.topMargin: 16 }
                Label {
            wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.minimumWidth: 0
                    visible: vc.paths.length === 0
                    text: "no " + vc.label + " folders yet."
                    color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(15) }
                }
                Repeater {
                    model: vc.paths
                    delegate: Rectangle {
                        required property string modelData
                        Layout.fillWidth: true; Layout.maximumWidth: 760
                        implicitHeight: 56
                        color: Theme.panel
                        RowLayout {
                            anchors { fill: parent; leftMargin: 16; rightMargin: 8 }
                            Label { text: modelData; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(17) } elide: Text.ElideMiddle; Layout.fillWidth: true }
                            Button {
                                text: "remove"
                                onClicked: hop.removeVideoCategoryFolder(vc.modelData, modelData)
                                Accessible.name: "remove " + vc.label + " folder " + modelData
                                contentItem: Label { text: parent.text; color: Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(15) } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                background: Rectangle { implicitWidth: 96; implicitHeight: 44; color: parent.hovered ? Theme.hover : "transparent" }
                            }
                        }
                    }
                }
                Button {
                    objectName: "settingsAddVideoFolder_" + vc.modelData
                    text: "+ add " + vc.label + " folder"
                    onClicked: root.addVideoCategoryFolderRequested(vc.modelData)
                    contentItem: Label { text: parent.text; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(16) } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { implicitWidth: 230; implicitHeight: 44; color: "transparent"; border { width: 2; color: Theme.border } }
                }
            }
        }

        // ---- pictures, audiobooks, ebooks, podcasts: each section has its own folders ----
        Repeater {
            model: hop.sectionKinds
            delegate: ColumnLayout {
                id: sec
                required property string modelData
                readonly property var paths: hop.sectionFolders[modelData] || []
                Layout.fillWidth: true
                spacing: 8
                Label { text: sec.modelData + " folders"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(24) } Layout.topMargin: 16 }
                Label {
            wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.minimumWidth: 0
                    visible: sec.paths.length === 0
                    text: "no " + sec.modelData + " folders yet."
                    color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(15) }
                }
                Repeater {
                    model: sec.paths
                    delegate: Rectangle {
                        required property string modelData
                        Layout.fillWidth: true; Layout.maximumWidth: 760
                        implicitHeight: 56
                        color: Theme.panel
                        RowLayout {
                            anchors { fill: parent; leftMargin: 16; rightMargin: 8 }
                            Label { text: modelData; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(17) } elide: Text.ElideMiddle; Layout.fillWidth: true }
                            Button {
                                text: "remove"
                                onClicked: hop.removeSectionFolder(sec.modelData, modelData)
                                Accessible.name: "remove " + sec.modelData + " folder " + modelData
                                contentItem: Label { text: parent.text; color: Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(15) } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                background: Rectangle { implicitWidth: 96; implicitHeight: 44; color: parent.hovered ? Theme.hover : "transparent" }
                            }
                        }
                    }
                }
                Button {
                    objectName: "settingsAddFolder_" + sec.modelData
                    text: "+ add " + sec.modelData + " folder"
                    onClicked: root.addSectionFolderRequested(sec.modelData)
                    contentItem: Label { text: parent.text; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(16) } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { implicitWidth: 230; implicitHeight: 44; color: "transparent"; border { width: 2; color: Theme.border } }
                }
            }
        }

        // ---- remove profile ------------------------------------------------------
        RowLayout {
            visible: !hop.guest
            Layout.topMargin: 24
            spacing: 14
            PlainButton { visible: !root.confirmRemove; text: "remove this profile…"; danger: true; onClicked: root.confirmRemove = true }
            Label {
            wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.minimumWidth: 0
                visible: root.confirmRemove
                text: "remove \u201c" + (hop.profile.name || "") + "\u201d? its settings and history go. your music files are not touched."
                color: Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(15) }
            }
            PlainButton { visible: root.confirmRemove; text: "cancel"; onClicked: root.confirmRemove = false }
            PlainButton { visible: root.confirmRemove; text: "remove"; danger: true; onClicked: { root.confirmRemove = false; root.profileMsg = hop.removeProfile() } }
        }

        // ---- diagnostics (phones): what the app noted, to copy and send when something goes wrong ----
        ColumnLayout {
            visible: Theme.compact
            Layout.fillWidth: true; Layout.minimumWidth: 0
            spacing: 10
            property string shown: ""
            Label { text: "diagnostics"; color: Theme.accent; font { family: Theme.font; pixelSize: 24 }
                    Layout.topMargin: 10 }
            Label {
                text: "a small log of what the app did, kept on this phone only. if something goes wrong, restart the app, open this page, tap copy and paste it to the developer."
                color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.minimumWidth: 0
                font { family: Theme.font; pixelSize: 14 }
            }
            Label {
                objectName: "diagController"
                text: "game controller: " + (hop.gamepad.connected ? hop.gamepad.name : "none found")
                      + (hop.gamepad.recentEvents.length > 0 ? "\n" + hop.gamepad.recentEvents.slice(-8).join("\n") : "\n(press a button on it and it shows up here)")
                color: Theme.text2; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true; Layout.minimumWidth: 0
                font { family: "monospace"; pixelSize: 12 }
            }
            Flow {
                Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 100
                spacing: 10
                PlainButton { objectName: "diagShow"; text: "show this run"; onClicked: parent.parent.shown = hop.diagLog().split("\n").slice(-60).join("\n") }
                PlainButton { objectName: "diagShowPrev"; text: "show last run"; onClicked: parent.parent.shown = hop.diagPreviousLog().split("\n").slice(-60).join("\n") }
                PlainButton { objectName: "diagCopy"; text: "copy this run"; onClicked: hop.copyText(hop.diagLog()) }
                PlainButton { objectName: "diagCopyPrev"; text: "copy last run"; onClicked: hop.copyText(hop.diagPreviousLog()) }
            }
            Label {
                visible: parent.shown !== ""
                text: parent.shown
                color: Theme.text2; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true; Layout.minimumWidth: 0
                font { family: "monospace"; pixelSize: 11 }
            }
        }

        // ---- about -------------------------------------------------------------------
        Label { text: "about"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(24) } Layout.topMargin: 28 }
        Label {
            wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.minimumWidth: 0
            objectName: "aboutVersion"
            text: "hop-tunes " + hop.version + " · GPL-3.0-or-later · works offline, no accounts, no tracking"
            color: Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(15) }
        }
        Label { visible: !Theme.compact; text: "keyboard"; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(17) } Layout.topMargin: 6 }
        GridLayout {
            visible: !Theme.compact   // a phone has no keyboard shortcuts
            columns: 2; columnSpacing: 28; rowSpacing: 6
            Repeater {
                model: [["space", "play / pause"], ["ctrl + → / ctrl + ←", "next / previous song"],
                        ["ctrl + F  or  /", "search your music"], ["esc", "clear the search"],
                        ["ctrl + 1 / 2 / 3", "collection / now playing / quickplay"], ["ctrl + ,", "settings"],
                        ["media keys", "play, pause, next, previous (also from desktop widgets)"]]
                delegate: Component {
                    RowLayout {
                        required property var modelData
                        Layout.columnSpan: 2
                        spacing: 28
                        Label { text: modelData[0]; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(14); weight: Font.Medium } Layout.preferredWidth: 190 }
                        Label { text: modelData[1]; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) } }
                    }
                }
            }
        }
        Label {
            Layout.topMargin: 10
            Layout.fillWidth: true; Layout.maximumWidth: 760
            wrapMode: Text.WordWrap
            text: "built with Qt, GStreamer, TagLib and SQLite, and set in Jost (SIL Open Font License). "
                + "see THIRD_PARTY_NOTICES.md for the full list. hop-tunes is inspired by how the Zune software "
                + "organised music; it is an independent project, not affiliated with or endorsed by Microsoft. "
                + "Zune is a trademark of Microsoft Corporation."
            color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) }
        }
        Label {
            objectName: "tipNote"
            Layout.topMargin: 6
            Layout.fillWidth: true; Layout.maximumWidth: 760; Layout.minimumWidth: 0
            wrapMode: Text.WordWrap
            text: "If you enjoy hoptunes, a tip of any amount is appreciated: Cash App $Normand64. "
                + "You can also support my Locals at locals.com/gamehopper. There is no shame in freeloading."
            color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) }
        }
        Item { height: 18; width: 1 }
    }
    }
}
