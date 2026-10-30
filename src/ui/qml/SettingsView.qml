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

    // A settings row: label and control side by side; on phones the control goes under its label.
    component SRow: GridLayout {
        property real spacing: 16
        columns: Theme.compact ? 1 : 99
        columnSpacing: spacing
        rowSpacing: 6
        Layout.fillWidth: Theme.compact
    }

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
            wrapMode: Text.WordWrap; Layout.fillWidth: true
                    text: ob.modelData[1]; color: ob.on ? Theme.ink : Theme.text
                    font { family: Theme.font; pixelSize: Theme.fs(15) }
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    implicitWidth: Math.max(90, ob.contentItem.implicitWidth + 28); implicitHeight: 40
                    color: ob.on ? Theme.accent : (ob.hovered ? Theme.hover : "transparent")
                    border { width: 1; color: ob.on ? Theme.accent : Theme.border }
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

        // ---- profile ------------------------------------------------------------
        Label { text: hop.guest ? "guest mode" : "profile"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(24) } }
        Label {
            wrapMode: Text.WordWrap; Layout.fillWidth: true
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
                    Layout.fillWidth: true
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
        ColumnLayout {
            visible: !hop.guest
            Layout.fillWidth: true
            spacing: 10
            Label {
            wrapMode: Text.WordWrap; Layout.fillWidth: true
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
            wrapMode: Text.WordWrap; Layout.fillWidth: true
                text: "a PIN is a privacy curtain, not a lock: anyone who can use this computer account can still read hop-tunes files."
                color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) }
            }
        }
        Label { visible: root.profileMsg !== ""; text: root.profileMsg; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(15) } }

        // ---- playback (spec §5) ------------------------------------------------------
        Label { text: "playback"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(24) } Layout.topMargin: 16 }
        SRow {
            spacing: 16
            Label { text: "crossfade"; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(17) } Layout.preferredWidth: Theme.compact ? -1 : 140 }
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
            wrapMode: Text.WordWrap; Layout.fillWidth: true
                text: hop.player.crossfadeMs === 0 ? "off (gapless)" : Math.round(hop.player.crossfadeMs / 1000) + " s"
                color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(15) } Layout.preferredWidth: 110
            }
        }
        Label {
            wrapMode: Text.WordWrap; Layout.fillWidth: true
            text: "songs from one album always play gapless, without a fade."
            color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) }
        }
        SRow {
            spacing: 16
            Label { text: "even volume"; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(17) } Layout.preferredWidth: Theme.compact ? -1 : 140 }
            Choice {
                enabled: hop.player.replayGainAvailable
                options: [[0, "off"], [1, "per song"], [2, "per album"]]
                value: hop.player.replayGainMode
                onPicked: function (v) { hop.player.replayGainMode = v }
            }
            Label {
            wrapMode: Text.WordWrap; Layout.fillWidth: true
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
            wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.maximumWidth: 760
        }

        // ---- appearance (spec §4.5) ---------------------------------------------------
        Label { text: "appearance"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(24) } Layout.topMargin: 16 }
        SRow {
            spacing: 16
            Label { text: "theme"; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(17) } Layout.preferredWidth: Theme.compact ? -1 : 140 }
            Choice {
                objectName: "themeChoice"
                options: hop.systemThemeAvailable ? [["dark", "dark"], ["light", "light"], ["system", "match system"]] : [["dark", "dark"], ["light", "light"]]
                value: hop.theme
                onPicked: function (v) { hop.theme = v }
            }
        }
        SRow {
            spacing: 16
            Label { text: "motion"; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(17) } Layout.preferredWidth: Theme.compact ? -1 : 140 }
            Choice {
                options: [[false, "animate"], [true, "reduce motion"]]
                value: hop.reduceMotion
                onPicked: function (v) { hop.reduceMotion = v }
            }
        }
        RowLayout {
            visible: Window.window !== null && Window.window.mobileNav === true
            spacing: 16
            Label { text: "navigation"; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(17) } Layout.preferredWidth: Theme.compact ? -1 : 140 }
            Choice {
                objectName: "navStyleChoice"
                options: [["bar", "bottom bar"], ["drawer", "side drawer"]]
                value: hop.navStyle
                onPicked: function (v) { hop.navStyle = v }
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
            Label { text: "viewing style"; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(17) } Layout.preferredWidth: Theme.compact ? -1 : 140 }
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
            wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.maximumWidth: 640
        }
        SRow {
            spacing: 16
            Layout.topMargin: 8
            Label { text: "livestream chat"; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(17) } Layout.preferredWidth: Theme.compact ? -1 : 140 }
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
            Label { text: "chat look"; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(17) } Layout.preferredWidth: Theme.compact ? -1 : 140 }
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
            wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.maximumWidth: 640
        }
        SRow {
            spacing: 16
            Layout.topMargin: 8
            Label { text: "comments"; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(17) } Layout.preferredWidth: Theme.compact ? -1 : 140 }
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
            wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.maximumWidth: 640
        }
        Label {
            wrapMode: Text.WordWrap; Layout.fillWidth: true
            text: "while watching: t switches to theater, f to fullscreen, c shows or hides the chat, esc steps back."
            color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) }
        }

        // ---- music folders ---------------------------------------------------------
        // The music scanner's list. Videos use their own list below once one is added; until
        // then they scan these same folders, so existing libraries keep working.

        Label { text: "music folders"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(24) } Layout.topMargin: 16 }
        Label {
            wrapMode: Text.WordWrap; Layout.fillWidth: true
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
            wrapMode: Text.WordWrap; Layout.fillWidth: true
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
            wrapMode: Text.WordWrap; Layout.fillWidth: true
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
            wrapMode: Text.WordWrap; Layout.fillWidth: true
                visible: root.confirmRemove
                text: "remove \u201c" + (hop.profile.name || "") + "\u201d? its settings and history go. your music files are not touched."
                color: Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(15) }
            }
            PlainButton { visible: root.confirmRemove; text: "cancel"; onClicked: root.confirmRemove = false }
            PlainButton { visible: root.confirmRemove; text: "remove"; danger: true; onClicked: { root.confirmRemove = false; root.profileMsg = hop.removeProfile() } }
        }

        // ---- about -------------------------------------------------------------------
        Label { text: "about"; color: Theme.accent; font { family: Theme.font; pixelSize: Theme.fs(24) } Layout.topMargin: 28 }
        Label {
            wrapMode: Text.WordWrap; Layout.fillWidth: true
            objectName: "aboutVersion"
            text: "hop-tunes " + hop.version + " · GPL-3.0-or-later · works offline, no accounts, no tracking"
            color: Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(15) }
        }
        Label { text: "keyboard"; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(17) } Layout.topMargin: 6 }
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
        Item { height: 18; width: 1 }
    }
    }
}
