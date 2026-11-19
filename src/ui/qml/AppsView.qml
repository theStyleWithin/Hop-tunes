// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// The apps tab: small apps that are part of Hop-tunes and run inside its window, as pages of glass tiles (five across and three down on a
// computer, three across on a phone), page dots on top and a "custom themes" button at the top right. The page is pale in the light theme
// and dark in the dark theme; the person can lay one of their own pictures behind it instead (kept as a resized copy in their profile).
// To add an app: one entry in `allApps` below and its screen in AppHost.qml.
Item {
    id: root
    objectName: "appsView"
    signal openApp(string appId)

    // a picture of the person's own is behind the page: then the glass and the labels follow how bright it is (after the dimming)
    readonly property var customPicture: {
        const l = hop.appsBackgrounds
        for (let i = 0; i < l.length; ++i) if (l[i].id === hop.appsBackground) return l[i]
        return null
    }
    readonly property bool custom: customPicture !== null
    readonly property real effectiveLuma: custom ? customPicture.luma * (1 - hop.appsDim / 100) : (Theme.light ? 1 : 0)
    // "light" or "dark" glass, labels and dots
    readonly property string look: custom ? (effectiveLuma >= 0.5 ? "light" : "dark") : (Theme.light ? "light" : "dark")
    readonly property color labelColor: look === "light" ? "#26282b" : "#e9e9ec"
    readonly property color labelColor2: look === "light" ? "#3d4044" : "#c8c8ce"
    readonly property color labelDim: look === "light" ? "#6c7075" : "#9a9aa2"

    // id, the name under the tile, a line for the tooltip, an icon (a url, or "" for the plain glass)
    // net: the app uses the internet (it says so on its tile, and is left out of builds without networked apps)
    readonly property var allApps: [
        { id: "kick", name: "kick", blurb: "browse and watch live streams from kick.com. uses the internet only while it is open, and only after you allow it", icon: "qrc:/images/app-kick.png", net: true },
        { id: "odysee", name: "odysee", blurb: "browse, search and watch videos from odysee.com: follow channels, keep favourites and playlists, with your own blocked channels and words and an 18+ switch (off at the start). uses the internet only while it is open, and only after you allow it. not made or endorsed by odysee; some videos may not play outside odysee's own site", icon: "qrc:/images/app-odysee.png", net: true },
        { id: "archive", name: "internet archive", blurb: "find music, films and books on archive.org: play the music, watch the videos and read the books right here, with the media preserver normand64's picks first. uses the internet only while it is open, and only after you allow it. not made or endorsed by the internet archive", icon: "qrc:/images/app-archive.png", net: true },
        { id: "deviantart", name: "deviantart", blurb: "browse artwork from deviantart's public feeds: search, artists, favourites, your own blocked artists and words, and an 18+ switch (off at the start). uses the internet only while it is open, and only after you allow it. not made or endorsed by deviantart", icon: "qrc:/images/app-deviantart.png", net: true },
        { id: "nekocafe", name: "neko cafe", blurb: "a little game: run a cafe as a neko maid. take orders, fetch the coffee from the cook and serve the right customer, fast. works offline, nothing is sent anywhere", icon: "qrc:/images/app-neko-cafe.png", net: false }
    ]
    readonly property var apps: allApps.filter(function (a) { return !a.net || hop.netAppsBuilt })

    // the dark look's glass square: flat and dark with a soft edge and a faint rim (the light look uses the glass picture)
    component DarkGlass: Item {
        Rectangle { y: 3; width: parent.width; height: parent.height; radius: width * 0.2; color: "#0d0e10"; opacity: 0.7 }   // its soft shadow
        Rectangle {
            anchors.fill: parent; radius: width * 0.2
            gradient: Gradient { GradientStop { position: 0.0; color: "#26272b" } GradientStop { position: 1.0; color: "#1d1e22" } }
            border { width: 1.5; color: "#313238" }
            Rectangle { anchors.fill: parent; anchors.margins: parent.width * 0.05; radius: parent.radius * 0.8; color: "transparent"; border { width: 1; color: "#2b2c31" } }
        }
    }

    // one glass tile
    component AppTile: AbstractButton {
        id: tile
        required property var app
        objectName: "appTile_" + app.id
        property real side: 132
        width: side; height: side + 52
        hoverEnabled: true
        focusPolicy: Qt.StrongFocus
        Accessible.name: app.name
        // the description pops up on hover (or a long look on a touch screen) and goes away by itself; settings > apps can hide it for good
        ToolTip.visible: hop.appHints && hovered && !pressed && app.blurb !== ""
        ToolTip.delay: 600
        ToolTip.timeout: Theme.compact ? 3500 : 12000
        ToolTip.text: app.blurb
        onClicked: root.openApp(app.id)
        Keys.onReturnPressed: clicked()
        Keys.onEnterPressed: clicked()
        Keys.onSpacePressed: clicked()
        contentItem: Item {
            Item {
                id: glass
                width: tile.side; height: tile.side
                scale: tile.pressed ? 0.96 : (tile.hovered || tile.activeFocus ? 1.05 : 1.0)
                Behavior on scale { NumberAnimation { duration: Theme.reduceMotion ? 0 : 140; easing.type: Easing.OutQuad } }
                DarkGlass { anchors.fill: parent; visible: root.look === "dark" }
                Rectangle { visible: root.look === "light"; y: 4; width: parent.width; height: parent.height; radius: width * 0.2; color: "#26202428" }   // a soft shadow under the glass
                Image {   // the glass bubble (light theme)
                    anchors.fill: parent
                    visible: root.look === "light"
                    source: "qrc:/images/app-tile.png"
                    sourceSize.width: tile.side * 2; sourceSize.height: tile.side * 2
                    smooth: true; mipmap: true
                }
                Image {   // the app's own icon, centred on the glass
                    anchors.centerIn: parent
                    width: tile.side * 0.56; height: width
                    visible: tile.app.icon !== ""
                    source: tile.app.icon
                    sourceSize.width: width * 2; sourceSize.height: height * 2
                    fillMode: Image.PreserveAspectFit
                    smooth: true; mipmap: true
                }
                Rectangle {   // keyboard focus ring
                    anchors.fill: parent; anchors.margins: -4; radius: tile.side * 0.2
                    color: "transparent"; border { width: 2; color: Theme.accent }
                    visible: tile.activeFocus
                }
            }
            Label {
                anchors { top: glass.bottom; topMargin: 10; horizontalCenter: parent.horizontalCenter }
                width: tile.side
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                text: tile.app.name
                color: tile.hovered || tile.activeFocus ? root.labelColor : root.labelColor2
                font { family: Theme.font; pixelSize: Theme.fs(16) }
            }
            Label {   // honest about the network
                visible: tile.app.net === true
                anchors { top: glass.bottom; topMargin: 30; horizontalCenter: parent.horizontalCenter }
                text: "uses the internet"; color: root.labelDim; font { family: Theme.font; pixelSize: Theme.fs(11) }
            }
        }
        background: Item {}
    }

    Label {   // nothing to open here yet (a build without the networked apps has none)
        objectName: "appsEmpty"
        visible: root.apps.length === 0
        anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: 8 }
        wrapMode: Text.WordWrap; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(16) }
        text: "there are no apps in this build."
    }

    // ---- the grid: pages of glass tiles, page dots on top ----
    // Free spots are drawn as faded empty glass, so a page always looks full and shows there is room for more apps.
    readonly property int cols: Theme.compact ? 3 : 5
    readonly property int rows: 3
    readonly property int perPage: cols * rows
    readonly property int pageCount: Math.max(1, Math.ceil(apps.length / perPage))
    property int page: 0
    onPageCountChanged: if (page >= pageCount) page = pageCount - 1
    function goTo(i) { page = Math.max(0, Math.min(pageCount - 1, i)); pages.positionViewAtIndex(page, ListView.Beginning) }
    readonly property real dotsHeight: 40
    readonly property real cellW: width / cols
    readonly property real cellH: Math.max(1, (height - dotsHeight) / rows)
    readonly property real tileSide: Math.max(72, Math.min(150, Math.min(cellW - 28, cellH - 66)))

    Rectangle {   // the backdrop: the theme's frosted look, or the person's own picture
        id: backdrop
        objectName: "appsBackdrop"
        anchors.fill: parent
        radius: 18
        clip: true
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.light ? "#f6f7f5" : "#1b1c20" }
            GradientStop { position: 1.0; color: Theme.light ? "#e8ebe8" : "#27282d" }
        }
        Image {
            objectName: "appsBackdropPicture"
            anchors.fill: parent
            visible: root.custom
            source: root.custom ? customPicture.url : ""
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: false
            smooth: true
        }
        Rectangle { anchors.fill: parent; visible: root.custom; color: "black"; opacity: hop.appsDim / 100 }   // the dimming
        Repeater {   // a few soft glass shapes floating behind the tiles (only on the theme's own look)
            model: root.custom ? [] : [[0.06, 0.42, 70], [0.93, 0.08, 110], [0.88, 0.62, 60], [0.46, 0.93, 40], [0.12, 0.88, 34]]
            delegate: Rectangle {
                required property var modelData
                x: backdrop.width * modelData[0] - width / 2; y: backdrop.height * modelData[1] - height / 2
                width: modelData[2]; height: width; radius: width / 2
                color: Theme.light ? "#33ffffff" : "#14ffffff"
                border { width: 2; color: Theme.light ? "#55ffffff" : "#1fffffff" }
            }
        }
    }

    Rectangle {   // (a light strip behind the dots on a dark look, so the grey ones can be seen)
        visible: root.look === "dark"
        anchors { horizontalCenter: dots.horizontalCenter; verticalCenter: dots.verticalCenter }
        width: dots.width + 14; height: dots.height + 14; radius: 3; color: "#eceef0"
    }
    Row {   // the page dots: the page you are on is the blue one
        id: dots
        objectName: "appsDots"
        anchors { top: parent.top; topMargin: 14; horizontalCenter: parent.horizontalCenter }
        spacing: 12
        Repeater {
            model: root.pageCount
            delegate: Rectangle {
                required property int index
                objectName: "appsDot_" + index
                width: 14; height: 14; radius: 3
                color: index === root.page ? "#12a0f4" : "#b9bdc0"
                Accessible.name: "page " + (index + 1)
                MouseArea { anchors.fill: parent; anchors.margins: -8; onClicked: root.goTo(index) }
            }
        }
    }

    ListView {
        id: pages
        objectName: "appsPages"
        anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: root.dotsHeight; bottom: parent.bottom }
        orientation: ListView.Horizontal
        snapMode: ListView.SnapOneItem
        highlightRangeMode: ListView.StrictlyEnforceRange
        highlightMoveDuration: Theme.reduceMotion ? 0 : 220
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        model: root.pageCount
        currentIndex: root.page
        onCurrentIndexChanged: if (currentIndex !== root.page && !moving) root.page = currentIndex
        onMovementEnded: root.page = currentIndex
        delegate: Item {
            id: pageItem
            required property int index
            width: pages.width; height: pages.height
            Grid {
                anchors.centerIn: parent
                columns: root.cols
                Repeater {
                    model: root.perPage
                    delegate: Item {
                        id: slot
                        required property int index
                        readonly property int appIndex: pageItem.index * root.perPage + index
                        readonly property var app: appIndex < root.apps.length ? root.apps[appIndex] : null
                        width: root.cellW; height: root.cellH
                        DarkGlass {   // a free spot on a dark look
                            visible: slot.app === null && root.look === "dark"
                            anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: Math.max(0, (parent.height - (root.tileSide + 52)) / 2) }
                            width: root.tileSide; height: root.tileSide
                        }
                        Rectangle {   // (its soft shadow)
                            visible: slot.app === null && root.look === "light"
                            x: (parent.width - width) / 2; y: Math.max(0, (parent.height - (root.tileSide + 52)) / 2) + 4
                            width: root.tileSide; height: root.tileSide; radius: width * 0.2; color: "#1a202428"
                        }
                        Image {   // a free spot on a light look: faded empty glass
                            visible: slot.app === null && root.look === "light"
                            anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: Math.max(0, (parent.height - (root.tileSide + 52)) / 2) }
                            width: root.tileSide; height: root.tileSide
                            source: "qrc:/images/app-tile.png"
                            sourceSize.width: root.tileSide * 2; sourceSize.height: root.tileSide * 2
                            opacity: 0.75
                            smooth: true; mipmap: true
                        }
                        Loader {
                            active: slot.app !== null
                            anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: Math.max(0, (parent.height - (root.tileSide + 52)) / 2) }
                            sourceComponent: AppTile { app: slot.app; side: root.tileSide }
                        }
                    }
                }
            }
        }
    }

    WheelHandler {   // the mouse wheel (or a trackpad) turns the page
        property real cooldown: 0
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: function (e) {
            const now = Date.now()
            if (now < cooldown) return
            const d = Math.abs(e.angleDelta.y) >= Math.abs(e.angleDelta.x) ? e.angleDelta.y : e.angleDelta.x
            if (d === 0) return
            cooldown = now + 350
            root.goTo(root.page + (d < 0 ? 1 : -1))
        }
    }
    Keys.onLeftPressed: goTo(page - 1)
    Keys.onRightPressed: goTo(page + 1)
    Keys.onPressed: function (e) {
        if (e.key === Qt.Key_PageDown) { goTo(page + 1); e.accepted = true }
        else if (e.key === Qt.Key_PageUp) { goTo(page - 1); e.accepted = true }
    }

    // ---- custom themes: the person's own pictures behind the grid ----
    AbstractButton {
        id: themesButton
        objectName: "appsCustomThemes"
        anchors { top: parent.top; right: parent.right; topMargin: 10; rightMargin: 14 }
        hoverEnabled: true; focusPolicy: Qt.NoFocus
        implicitHeight: 30; implicitWidth: themesLabel.implicitWidth + 24
        Accessible.name: "custom themes"
        onClicked: themesPanel.opened = !themesPanel.opened
        background: Rectangle { radius: 15; color: themesButton.hovered || themesPanel.opened ? (root.look === "light" ? "#fff" : "#2b2c31") : (root.look === "light" ? "#d9ffffff" : "#992b2c31")
                                border { width: 1; color: root.look === "light" ? "#b8bcc0" : "#4a4b52" } }
        contentItem: Label { id: themesLabel; text: "custom themes"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                             color: root.labelColor; font { family: Theme.font; pixelSize: Theme.fs(13) } }
    }

    Rectangle {
        id: themesPanel
        objectName: "appsThemesPanel"
        property bool opened: false
        property string message: ""
        visible: opened
        anchors { top: themesButton.bottom; right: parent.right; topMargin: 8; rightMargin: 14 }
        width: Math.min(root.width - 28, 460)
        height: Math.min(root.height - 70, panelColumn.implicitHeight + 28)
        radius: 14
        color: Theme.light ? "#f4ffffff" : "#f2202226"
        border { width: 1; color: Theme.light ? "#c9ccd0" : "#3a3b42" }
        focus: opened
        Keys.onEscapePressed: opened = false
        MouseArea { anchors.fill: parent }   // (nothing under the panel hears the mouse)
        Flickable {
            anchors { fill: parent; margins: 14 }
            contentHeight: panelColumn.implicitHeight; clip: true
            boundsBehavior: Flickable.StopAtBounds
            ColumnLayout {
                id: panelColumn
                width: parent.width; spacing: 10
                Label { text: "custom themes"; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(20); weight: Font.Light } }
                Label {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(13) }
                    text: "put one of your own pictures behind the apps. hop-tunes keeps a resized copy in your profile on this computer (up to " + 12 + "); nothing is uploaded or shared."
                }
                Flow {
                    Layout.fillWidth: true; spacing: 10
                    Rectangle {   // the theme's own look
                        objectName: "appsThemeDefault"
                        width: 120; height: 68; radius: 8; clip: true
                        color: Theme.light ? "#eef0ee" : "#202126"
                        border { width: hop.appsBackground === "" ? 3 : 1; color: hop.appsBackground === "" ? Theme.accent : Theme.border }
                        Label { anchors.centerIn: parent; text: "the theme's own"; color: Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(12) } }
                        MouseArea { anchors.fill: parent; onClicked: hop.setAppsBackground("") }
                    }
                    Repeater {
                        model: hop.appsBackgrounds
                        delegate: Rectangle {
                            required property var modelData
                            objectName: "appsTheme_" + modelData.id
                            width: 120; height: 68; radius: 8; clip: true; color: Theme.panel
                            border { width: hop.appsBackground === modelData.id ? 3 : 1; color: hop.appsBackground === modelData.id ? Theme.accent : Theme.border }
                            Image { anchors.fill: parent; anchors.margins: 1; source: modelData.url; fillMode: Image.PreserveAspectCrop; asynchronous: true; cache: false; sourceSize.width: 240 }
                            MouseArea { anchors.fill: parent; onClicked: hop.setAppsBackground(modelData.id) }
                            AbstractButton {
                                objectName: "appsThemeRemove_" + modelData.id
                                anchors { top: parent.top; right: parent.right; margins: 4 }
                                width: 22; height: 22; focusPolicy: Qt.NoFocus; Accessible.name: "remove this picture"
                                onClicked: hop.removeAppsBackground(modelData.id)
                                background: Rectangle { radius: 11; color: "#cc000000" }
                                contentItem: Label { text: "\u00d7"; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 16 }
                            }
                        }
                    }
                }
                RowLayout {
                    spacing: 10
                    PlainButton { objectName: "appsThemeAdd"; text: "add a picture"; primary: true; implicitHeight: 38; onClicked: pictureDialog.open() }
                    PlainButton { text: "close"; implicitHeight: 38; onClicked: themesPanel.opened = false }
                }
                Label { objectName: "appsThemeMessage"; visible: themesPanel.message !== ""; Layout.fillWidth: true; wrapMode: Text.WordWrap
                        color: Theme.danger; text: themesPanel.message; font { family: Theme.font; pixelSize: Theme.fs(13) } }
                RowLayout {
                    visible: root.custom; Layout.fillWidth: true; spacing: 10
                    Label { text: "dim"; color: Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(14) } }
                    Slider {
                        objectName: "appsDimSlider"
                        Layout.fillWidth: true; from: 0; to: 60; stepSize: 5; snapMode: Slider.SnapAlways
                        value: hop.appsDim
                        onMoved: hop.appsDim = Math.round(value)
                    }
                    Label { text: hop.appsDim + "%"; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(13) } Layout.preferredWidth: 38 }
                }
            }
        }
    }
    FileDialog {
        id: pictureDialog
        title: "choose a picture for the apps page"
        nameFilters: ["pictures (*.png *.jpg *.jpeg *.webp *.bmp)"]
        onAccepted: themesPanel.message = hop.addAppsBackground(selectedFile)
    }
}
