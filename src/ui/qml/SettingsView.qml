// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// settings: this profile (name, colour, picture, PIN, remove) and the folders it watches.
Item {
    id: root
    signal addFolderRequested()
    signal addVideoFolderRequested()
    signal addVideoCategoryFolderRequested(string category)
    signal addSectionFolderRequested(string kind)

    property string profileMsg: ""
    property var invite: ({})          // the invite code just made (sharing)
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

        // ---- video settings ----------------------------------------------------------
        Label { text: "video settings"; color: hop.accent; font { family: Theme.font; pixelSize: 24 } Layout.topMargin: 16 }
        RowLayout {
            spacing: 16
            Label { text: "viewing style"; color: Theme.text; font { family: Theme.font; pixelSize: 17 } Layout.preferredWidth: 140 }
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
            color: Theme.muted; font { family: Theme.font; pixelSize: 14 }
            wrapMode: Text.WordWrap; Layout.maximumWidth: 640
        }
        RowLayout {
            spacing: 16
            Layout.topMargin: 8
            Label { text: "livestream chat"; color: Theme.text; font { family: Theme.font; pixelSize: 17 } Layout.preferredWidth: 140 }
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
            Label { text: "chat look"; color: Theme.text; font { family: Theme.font; pixelSize: 17 } Layout.preferredWidth: 140 }
            Choice {
                objectName: "videoChatLookChoice"
                options: [["clean", "clean"], ["lively", "lively"]]
                value: hop.videoChatLook
                onPicked: function (v) { hop.videoChatLook = v }
            }
        }
        Label {
            text: "a pretend live chat beside the video, as if you were watching a stream. nothing is streamed and nobody in it is real: the chatters are generated on your computer, react to the details you filled in on the movie's page, and answer when you type. clean is plain and tidy; lively adds colours, badges and emote chips."
            color: Theme.muted; font { family: Theme.font; pixelSize: 14 }
            wrapMode: Text.WordWrap; Layout.maximumWidth: 640
        }
        RowLayout {
            spacing: 16
            Layout.topMargin: 8
            Label { text: "comments"; color: Theme.text; font { family: Theme.font; pixelSize: 17 } Layout.preferredWidth: 140 }
            Choice {
                objectName: "videoCommentsChoice"
                options: [["off", "off"], ["mine", "only mine"], ["populated", "populated"]]
                value: hop.videoComments
                onPicked: function (v) { hop.videoComments = v }
            }
        }
        Label {
            text: "a comments section under the video (window and theater only). off hides it completely. only mine is a box where you can write comments, as many as you like, and nobody ever answers. populated adds made-up commenters and replies, who also answer what you write after a little while. nobody is real; only your own comments are saved."
            color: Theme.muted; font { family: Theme.font; pixelSize: 14 }
            wrapMode: Text.WordWrap; Layout.maximumWidth: 640
        }
        Label {
            text: "while watching: t switches to theater, f to fullscreen, c shows or hides the chat, esc steps back."
            color: Theme.dim; font { family: Theme.font; pixelSize: 13 }
        }

        // ---- music folders ---------------------------------------------------------
        // The music scanner's list. Videos use their own list below once one is added; until
        // then they scan these same folders, so existing libraries keep working.

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

        // ---- video folders ---------------------------------------------------------
        // The movies in the videos tab. Until one is added here it uses the media folders above.
        Label { text: "movies folders"; color: hop.accent; font { family: Theme.font; pixelSize: 24 } Layout.topMargin: 16 }
        Label {
            visible: hop.videoFolders.length === 0
            text: "no movies folders yet."
            color: Theme.muted; font { family: Theme.font; pixelSize: 15 }
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
                    Label { text: modelData; color: Theme.text; font { family: Theme.font; pixelSize: 17 } elide: Text.ElideMiddle; Layout.fillWidth: true }
                    Button {
                        text: "remove"
                        onClicked: hop.removeVideoFolder(modelData)
                        Accessible.name: "remove video folder " + modelData
                        contentItem: Label { text: parent.text; color: Theme.text2; font { family: Theme.font; pixelSize: 15 } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { implicitWidth: 96; implicitHeight: 44; color: parent.hovered ? Theme.hover : "transparent" }
                    }
                }
            }
        }
        Button {
            text: "+ add video folder"
            onClicked: root.addVideoFolderRequested()
            contentItem: Label { text: parent.text; color: Theme.text; font { family: Theme.font; pixelSize: 16 } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
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
                Label { text: vc.label + " folders"; color: hop.accent; font { family: Theme.font; pixelSize: 24 } Layout.topMargin: 16 }
                Label {
                    visible: vc.paths.length === 0
                    text: "no " + vc.label + " folders yet."
                    color: Theme.muted; font { family: Theme.font; pixelSize: 15 }
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
                            Label { text: modelData; color: Theme.text; font { family: Theme.font; pixelSize: 17 } elide: Text.ElideMiddle; Layout.fillWidth: true }
                            Button {
                                text: "remove"
                                onClicked: hop.removeVideoCategoryFolder(vc.modelData, modelData)
                                Accessible.name: "remove " + vc.label + " folder " + modelData
                                contentItem: Label { text: parent.text; color: Theme.text2; font { family: Theme.font; pixelSize: 15 } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                background: Rectangle { implicitWidth: 96; implicitHeight: 44; color: parent.hovered ? Theme.hover : "transparent" }
                            }
                        }
                    }
                }
                Button {
                    objectName: "settingsAddVideoFolder_" + vc.modelData
                    text: "+ add " + vc.label + " folder"
                    onClicked: root.addVideoCategoryFolderRequested(vc.modelData)
                    contentItem: Label { text: parent.text; color: Theme.text; font { family: Theme.font; pixelSize: 16 } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
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
                Label { text: sec.modelData + " folders"; color: hop.accent; font { family: Theme.font; pixelSize: 24 } Layout.topMargin: 16 }
                Label {
                    visible: sec.paths.length === 0
                    text: "no " + sec.modelData + " folders yet."
                    color: Theme.muted; font { family: Theme.font; pixelSize: 15 }
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
                            Label { text: modelData; color: Theme.text; font { family: Theme.font; pixelSize: 17 } elide: Text.ElideMiddle; Layout.fillWidth: true }
                            Button {
                                text: "remove"
                                onClicked: hop.removeSectionFolder(sec.modelData, modelData)
                                Accessible.name: "remove " + sec.modelData + " folder " + modelData
                                contentItem: Label { text: parent.text; color: Theme.text2; font { family: Theme.font; pixelSize: 15 } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                background: Rectangle { implicitWidth: 96; implicitHeight: 44; color: parent.hovered ? Theme.hover : "transparent" }
                            }
                        }
                    }
                }
                Button {
                    objectName: "settingsAddFolder_" + sec.modelData
                    text: "+ add " + sec.modelData + " folder"
                    onClicked: root.addSectionFolderRequested(sec.modelData)
                    contentItem: Label { text: parent.text; color: Theme.text; font { family: Theme.font; pixelSize: 16 } horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
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
                visible: root.confirmRemove
                text: "remove \u201c" + (hop.profile.name || "") + "\u201d? its settings and history go. your music files are not touched."
                color: Theme.text2; font { family: Theme.font; pixelSize: 15 }
            }
            PlainButton { visible: root.confirmRemove; text: "cancel"; onClicked: root.confirmRemove = false }
            PlainButton { visible: root.confirmRemove; text: "remove"; danger: true; onClicked: { root.confirmRemove = false; root.profileMsg = hop.removeProfile() } }
        }

        // ---- sharing (the host's side) -----------------------------------------------
        Connections { target: hop; function onSharingChanged() { if (!hop.sharing) root.invite = ({}) } }
        RowLayout {
            visible: !hop.guest
            Layout.topMargin: 28
            spacing: 12
            Label { text: "sharing"; color: hop.accent; font { family: Theme.font; pixelSize: 24 } }
            Label {
                objectName: "betaTag"
                text: "party watch beta"; color: Theme.ink
                font { family: Theme.font; pixelSize: 12; weight: Font.Medium }
                leftPadding: 8; rightPadding: 8; topPadding: 2; bottomPadding: 2
                background: Rectangle { color: hop.accent }
            }
        }
        Label {
            visible: !hop.guest
            text: "let people you trust watch and listen to what you approve, from their own Hop-tunes. nothing is shared until you approve it, guests can only stream (never download), and sharing is off every time you start Hop-tunes."
            color: Theme.muted; font { family: Theme.font; pixelSize: 14 }
            wrapMode: Text.WordWrap; Layout.maximumWidth: 640
        }
        Label {
            visible: !hop.guest && !hop.sharingAvailable
            text: "sharing is not available on this computer: it needs the system's encryption library (OpenSSL), which was not found."
            color: Theme.danger; font { family: Theme.font; pixelSize: 14 }
            wrapMode: Text.WordWrap; Layout.maximumWidth: 640
        }
        RowLayout {
            visible: !hop.guest && hop.sharingAvailable
            spacing: 16
            Label { text: "sharing"; color: Theme.text; font { family: Theme.font; pixelSize: 17 } Layout.preferredWidth: 140 }
            Choice {
                objectName: "sharingChoice"
                options: [["stopped", "off"], ["sharing", "on"]]
                value: hop.sharing ? "sharing" : "stopped"
                onPicked: function (v) { hop.sharing = (v === "sharing") }
            }
        }
        ColumnLayout {
            visible: !hop.guest && hop.sharing
            objectName: "sharingPanel"
            Layout.fillWidth: true
            spacing: 10
            Label {
                objectName: "sharingWhere"
                text: "you are sharing. on this network your address is " + (hop.sharingAddresses.length > 0 ? hop.sharingAddresses.join(" or ") : "(not connected to a network)") + ", port " + hop.sharingPort + "."
                color: Theme.text2; font { family: Theme.font; pixelSize: 15 }
                wrapMode: Text.WordWrap; Layout.maximumWidth: 640
            }

            Label { text: "invite someone"; color: Theme.text; font { family: Theme.font; pixelSize: 17 } Layout.topMargin: 6  }
            RowLayout {
                spacing: 16
                Label { text: "valid for"; color: Theme.muted; font { family: Theme.font; pixelSize: 15 } Layout.preferredWidth: 140  }
                Choice {
                    id: inviteHours
                    objectName: "inviteHoursChoice"
                    options: [[0.1667, "10 minutes"], [1, "1 hour"], [24, "1 day"]]
                    value: 1
                    onPicked: function (v) { value = v }
                }
            }
            PlainButton {
                objectName: "makeInvite"
                text: "make an invite code"
                onClicked: root.invite = hop.makeShareInvite(inviteHours.value)
            }
            ColumnLayout {
                visible: !!root.invite.code
                spacing: 6
                Label {
                    objectName: "inviteCode"
                    text: root.invite.code || ""
                    color: hop.accent; font { family: Theme.font; pixelSize: 30; weight: Font.Medium }
                }
                RowLayout {
                    spacing: 10
                    TextField {
                        id: linkField
                        objectName: "inviteLink"
                        text: root.invite.link || ""
                        readOnly: true; selectByMouse: true
                        Layout.preferredWidth: 440
                        color: Theme.text; font { family: Theme.font; pixelSize: 15 }
                        background: Rectangle { color: Theme.panel; border { width: 1; color: Theme.line } }
                    }
                    PlainButton { text: "copy"; onClicked: { linkField.selectAll(); linkField.copy(); linkField.deselect() } }
                }
                Label {
                    text: "one use only, and it stops working after " + (root.invite.hours < 1 ? "10 minutes" : root.invite.hours === 1 ? "1 hour" : "1 day") + ". send it to them yourself; they paste it into their Hop-tunes."
                    color: Theme.muted; font { family: Theme.font; pixelSize: 13 }
                    wrapMode: Text.WordWrap; Layout.maximumWidth: 640
                }
            }

            Label { text: "guests"; color: Theme.text; font { family: Theme.font; pixelSize: 17 } Layout.topMargin: 10  }
            Label {
                visible: hop.shareGuests.length === 0
                text: "no guests yet. a guest appears here once they have used an invite."
                color: Theme.muted; font { family: Theme.font; pixelSize: 15 }
            }
            Repeater {
                model: hop.shareGuests.length   // a count, so the rows are not rebuilt (and a click not lost) when a guest connects
                delegate: Rectangle {
                    id: guestRow
                    required property int index
                    readonly property var modelData: hop.shareGuests[index] || ({ id: 0, name: "", connected: false })
                    objectName: "shareGuest_" + index
                    Layout.fillWidth: true; Layout.maximumWidth: 760
                    implicitHeight: 56
                    color: Theme.panel
                    RowLayout {
                        anchors { fill: parent; leftMargin: 16; rightMargin: 8 }
                        spacing: 12
                        Rectangle {
                            objectName: "shareGuestDot_" + guestRow.index
                            property bool connected: guestRow.modelData.connected
                            width: 12; height: 12; radius: 6
                            color: connected ? hop.accent : "transparent"
                            border { width: 2; color: connected ? hop.accent : Theme.dim }
                            Accessible.name: connected ? "connected" : "not connected"
                        }
                        Label { text: guestRow.modelData.name; color: Theme.text; font { family: Theme.font; pixelSize: 17 } elide: Text.ElideRight; Layout.fillWidth: true  }
                        PlainButton { objectName: "removeGuest_" + guestRow.index; text: "remove"; danger: true; onClicked: hop.removeShareGuest(guestRow.modelData.id) }
                    }
                }
            }
        }

        // ---- joining someone else's library (the guest's side) ------------------------
        ColumnLayout {
            objectName: "joinPanel"
            visible: !hop.guest && hop.sharingAvailable
            Layout.fillWidth: true
            spacing: 8
            Label { text: "join someone's library"; color: Theme.text; font { family: Theme.font; pixelSize: 17 } Layout.topMargin: 14 }
            Label {
                text: "paste the invite link they sent you (it looks like hop://192.168.1.20:47800/ABCD-EFGH-JKMN-PQRS). what they share with you then appears in the \"shared\" tab of the collection."
                color: Theme.muted; font { family: Theme.font; pixelSize: 14 }
                wrapMode: Text.WordWrap; Layout.maximumWidth: 640
            }
            RowLayout {
                spacing: 10
                TextField {
                    id: joinField
                    objectName: "joinLink"
                    Layout.preferredWidth: 440
                    placeholderText: "hop://…"; placeholderTextColor: Theme.dim
                    selectByMouse: true
                    color: Theme.text; font { family: Theme.font; pixelSize: 15 }
                    onAccepted: if (!hop.shareJoining) hop.joinShare(text)
                    background: Rectangle { color: Theme.panel; border { width: joinField.activeFocus ? 2 : 1; color: joinField.activeFocus ? hop.accent : Theme.line } }
                }
                PlainButton { objectName: "joinButton"; text: hop.shareJoining ? "joining…" : "join"; enabled: !hop.shareJoining; onClicked: hop.joinShare(joinField.text) }
            }
            Label {
                objectName: "joinMessage"
                visible: hop.shareJoinMessage !== ""
                text: hop.shareJoinMessage
                color: Theme.text2; font { family: Theme.font; pixelSize: 14 }
                wrapMode: Text.WordWrap; Layout.maximumWidth: 640
            }
        }

        // ---- about -------------------------------------------------------------------
        Label { text: "about"; color: hop.accent; font { family: Theme.font; pixelSize: 24 } Layout.topMargin: 28 }
        Label {
            objectName: "aboutVersion"
            text: "hop-tunes " + hop.version + " · GPL-3.0-or-later · works offline, no accounts, no tracking"
            color: Theme.text2; font { family: Theme.font; pixelSize: 15 }
        }
        Label { text: "keyboard"; color: Theme.text; font { family: Theme.font; pixelSize: 17 } Layout.topMargin: 6 }
        GridLayout {
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
                        Label { text: modelData[0]; color: Theme.text; font { family: Theme.font; pixelSize: 14; weight: Font.Medium } Layout.preferredWidth: 190 }
                        Label { text: modelData[1]; color: Theme.muted; font { family: Theme.font; pixelSize: 14 } }
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
            color: Theme.dim; font { family: Theme.font; pixelSize: 13 }
        }
        Item { height: 18; width: 1 }
    }
    }
}
