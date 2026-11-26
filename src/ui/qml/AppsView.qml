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

    // ---- where the apps are: the layout (hop.appsLayout) remembers it per profile ----
    readonly property var layout: hop.appsLayout
    Binding { target: hop.appsLayout; property: "form"; value: Theme.compact ? "phone" : "wide" }   // the wide and the phone layouts are kept apart
    function appById(id) { for (let i = 0; i < allApps.length; ++i) if (allApps[i].id === id) return allApps[i]; return null }
    function folderById(id) { const l = layout.folders; for (let i = 0; i < l.length; ++i) if (l[i].id === id) return l[i]; return null }
    Component.onCompleted: { layout.setKnownApps(apps.map(function (a) { return a.id })); syncPages() }

    property string openFolder: ""             // the id of the folder that is open ("" = the main grid)
    readonly property var openFolderData: { layout.revision; return openFolder === "" ? null : folderById(openFolder) }
    onOpenFolderDataChanged: if (openFolder !== "" && openFolderData === null) { openFolder = "" }   // (a folder that was deleted)
    property bool arranging: false             // touch screens: a long press arranges (then a drag moves a tile); a mouse can always drag
    property string menuItem: ""               // what the open context menu is for ("app:x", "folder:y")
    property string editFolder: ""             // the folder whose editor is open
    property string moveApp: ""                // the app whose "move to a folder" list is open

    // what is in slot i of the page being shown: "app:x", "folder:y" or "" (free)
    function itemAtSlot(i) {
        layout.revision
        if (openFolder === "") return layout.itemAt(i)
        const f = openFolderData
        return f && i < f.members.length ? "app:" + f.members[i] : ""
    }
    readonly property int usedSlots: { layout.revision; return openFolder === "" ? layout.usedSlots() : (openFolderData ? openFolderData.members.length : 0) }
    function openTheFolder(id) { openFolder = id; page = 0; pages.positionViewAtIndex(0, ListView.Beginning) }
    function closeFolder() { openFolder = ""; page = 0; pages.positionViewAtIndex(0, ListView.Beginning) }
    function goBack() { if (editFolder !== "" || moveApp !== "") { editFolder = ""; moveApp = ""; return true } if (arranging) { arranging = false; return true } if (openFolder !== "") { closeFolder(); return true } return false }

    // ---- the glass: one square, and what is drawn on it ----
    component Glass: Item {
        DarkGlass { anchors.fill: parent; visible: root.look === "dark" }
        Rectangle { visible: root.look === "light"; y: 4; width: parent.width; height: parent.height; radius: width * 0.2; color: "#26202428" }   // a soft shadow under the glass
        Image {   // the glass bubble (light theme)
            anchors.fill: parent
            visible: root.look === "light"
            source: "qrc:/images/app-tile.png"
            sourceSize.width: parent.width * 2; sourceSize.height: parent.height * 2
            smooth: true; mipmap: true
        }
    }
    component AppFace: Item {   // an app: glass with its own icon on it
        property var app
        property real side: 132
        width: side; height: side
        Glass { anchors.fill: parent }
        Image {
            anchors.centerIn: parent
            width: parent.side * 0.56; height: width
            visible: !!parent.app && parent.app.icon !== ""
            source: parent.app ? parent.app.icon : ""
            sourceSize.width: width * 2; sourceSize.height: height * 2
            fillMode: Image.PreserveAspectFit
            smooth: true; mipmap: true
        }
    }
    // a folder: clear glass (tinted by its colour) with the first apps in it, or the person's own picture
    component FolderFace: Item {
        id: ff
        property var folder
        property real side: 132
        readonly property string pic: folder && folder.picture !== "" ? hop.appsFolderPictureUrl(folder.picture) : ""
        readonly property color tint: folder && folder.color !== "" ? folder.color : "transparent"
        width: side; height: side
        Glass { anchors.fill: parent }
        Rectangle { anchors.fill: parent; radius: width * 0.2; color: ff.tint; opacity: ff.folder && ff.folder.color !== "" ? 0.38 : 0 }
        Rectangle {   // the clear look: a faint wash, so an empty folder reads as glass
            anchors { fill: parent; margins: ff.side * 0.05 }
            radius: width * 0.18
            color: root.look === "light" ? "#22ffffff" : "#12ffffff"; border { width: 1; color: root.look === "light" ? "#66ffffff" : "#26ffffff" }
        }
        Image {   // the person's own picture (a copy with rounded corners, kept in the profile)
            visible: ff.pic !== ""
            anchors { fill: parent; margins: ff.side * 0.08 }
            source: ff.pic; asynchronous: true; cache: false; smooth: true
            sourceSize.width: ff.side * 2; sourceSize.height: ff.side * 2
            fillMode: Image.PreserveAspectCrop
        }
        Grid {   // the first four apps inside
            visible: ff.pic === ""
            anchors.centerIn: parent
            columns: 2; spacing: ff.side * 0.06
            Repeater {
                model: ff.folder ? Math.min(4, ff.folder.members.length) : 0
                delegate: Rectangle {
                    required property int index
                    readonly property var inner: root.appById(ff.folder.members[index])
                    width: ff.side * 0.3; height: width; radius: width * 0.24
                    color: root.look === "light" ? "#bbffffff" : "#33ffffff"; border { width: 1; color: root.look === "light" ? "#88ffffff" : "#33ffffff" }
                    Image {
                        anchors.centerIn: parent; width: parent.width * 0.72; height: width
                        visible: !!parent.inner; source: parent.inner ? parent.inner.icon : ""
                        sourceSize.width: width * 2; sourceSize.height: height * 2; fillMode: Image.PreserveAspectFit; smooth: true
                    }
                }
            }
        }
        Rectangle {   // a soft light across the top: the glass
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: ff.side * 0.04 }
            height: ff.side * 0.42; radius: ff.side * 0.16
            gradient: Gradient { GradientStop { position: 0.0; color: "#40ffffff" } GradientStop { position: 1.0; color: "#00ffffff" } }
        }
    }

    // one tile on the grid: an app or a folder. Click opens it; a right click opens its menu; a drag moves it (touch: long press, then drag)
    component GridTile: AbstractButton {
        id: tile
        property var app: null
        property var folder: null
        readonly property string itemName: app ? "app:" + app.id : (folder ? "folder:" + folder.id : "")
        property real side: 132
        objectName: app ? "appTile_" + app.id : (folder ? "appFolder_" + folder.id : "")
        width: side; height: side + 52
        hoverEnabled: true
        focusPolicy: Qt.StrongFocus
        opacity: root.dragItem === itemName ? 0.25 : 1
        Accessible.name: app ? app.name : (folder ? "folder " + folder.name : "")
        // the description pops up on hover (or a long look on a touch screen) and goes away by itself; settings > apps can hide it for good
        ToolTip.visible: !!app && hop.appHints && hovered && !pressed && !root.dragActive && app.blurb !== ""
        ToolTip.delay: 600
        ToolTip.timeout: Theme.compact ? 3500 : 12000
        ToolTip.text: app ? app.blurb : ""
        onClicked: { if (root.arranging) return; if (app) root.openApp(app.id); else if (folder) root.openTheFolder(folder.id) }
        Keys.onReturnPressed: clicked()
        Keys.onEnterPressed: clicked()
        Keys.onSpacePressed: clicked()
        readonly property bool dropHover: root.dragActive && root.hoverItem === itemName && !!folder && root.dragItem.indexOf("app:") === 0
        contentItem: Item {
            Item {
                id: glass
                width: tile.side; height: tile.side
                scale: tile.pressed ? 0.96 : (tile.hovered || tile.activeFocus || tile.dropHover ? 1.05 : 1.0)
                Behavior on scale { NumberAnimation { duration: Theme.reduceMotion ? 0 : 140; easing.type: Easing.OutQuad } }
                AppFace { anchors.fill: parent; visible: !!tile.app; app: tile.app; side: tile.side }
                FolderFace { anchors.fill: parent; visible: !!tile.folder; folder: tile.folder; side: tile.side }
                Rectangle {   // keyboard focus ring, and the ring on a folder an app is about to be dropped into
                    anchors.fill: parent; anchors.margins: -4; radius: tile.side * 0.2
                    color: "transparent"; border { width: 2; color: Theme.accent }
                    visible: tile.activeFocus || tile.dropHover
                }
                SequentialAnimation on rotation {   // arranging: the tiles shiver a little (not with reduced motion)
                    running: root.arranging && !Theme.reduceMotion && root.dragItem !== tile.itemName
                    loops: Animation.Infinite
                    NumberAnimation { to: 1.6; duration: 110 } NumberAnimation { to: -1.6; duration: 220 } NumberAnimation { to: 0; duration: 110 }
                    onRunningChanged: if (!running) glass.rotation = 0
                }
            }
            Label {
                anchors { top: glass.bottom; topMargin: 10; horizontalCenter: parent.horizontalCenter }
                width: tile.side
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                text: tile.app ? tile.app.name : (tile.folder ? tile.folder.name : "")
                color: tile.hovered || tile.activeFocus ? root.labelColor : root.labelColor2
                font { family: Theme.font; pixelSize: Theme.fs(16) }
            }
            Label {   // honest about the network
                visible: !!tile.app && tile.app.net === true
                anchors { top: glass.bottom; topMargin: 30; horizontalCenter: parent.horizontalCenter }
                text: "uses the internet"; color: root.labelDim; font { family: Theme.font; pixelSize: Theme.fs(11) }
            }
            Label {   // a folder says how many apps it holds
                visible: !!tile.folder
                anchors { top: glass.bottom; topMargin: 30; horizontalCenter: parent.horizontalCenter }
                text: tile.folder ? (tile.folder.members.length === 1 ? "1 app" : tile.folder.members.length + " apps") : ""
                color: root.labelDim; font { family: Theme.font; pixelSize: Theme.fs(11) }
            }
        }
        background: Item {}
        // the mouse drags at once; a touch screen drags while arranging (a long press starts that)
        DragHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            target: null; dragThreshold: 8
            grabPermissions: PointerHandler.CanTakeOverFromAnything
            onActiveChanged: { if (active) root.beginDrag(tile.itemName, centroid.scenePosition); else root.endDrag() }
            onCentroidChanged: if (active) root.dragMove(centroid.scenePosition)
        }
        DragHandler {
            acceptedDevices: PointerDevice.TouchScreen | PointerDevice.Stylus
            enabled: root.arranging
            target: null; dragThreshold: 4
            grabPermissions: PointerHandler.CanTakeOverFromAnything
            onActiveChanged: { if (active) root.beginDrag(tile.itemName, centroid.scenePosition); else root.endDrag() }
            onCentroidChanged: if (active) root.dragMove(centroid.scenePosition)
        }
        TapHandler {
            acceptedDevices: PointerDevice.TouchScreen
            longPressThreshold: 0.5
            onLongPressed: root.arranging = true
        }
        MouseArea {   // a right click: the tile's menu
            anchors.fill: parent; acceptedButtons: Qt.RightButton
            onClicked: function (m) { root.openTileMenu(tile.itemName, tile.mapToItem(root, m.x, m.y)) }
        }
    }

    Label {   // nothing to open here yet (a build without the networked apps has none)
        objectName: "appsEmpty"
        visible: root.apps.length === 0
        anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: 8 }
        wrapMode: Text.WordWrap; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(16) }
        text: "there are no apps in this build."
    }

    // ---- the grid: pages of glass tiles, page dots on top ----
    // Free spots are drawn as faded empty glass, so a page always looks full and shows there is room for more apps; they are also where a tile can be dropped.
    readonly property int cols: Theme.compact ? 3 : 5
    readonly property int rows: 3
    readonly property int perPage: cols * rows
    readonly property int maxPages: Math.floor(120 / perPage)
    readonly property int pageCount: Math.min(maxPages, Math.max(1, Math.ceil(usedSlots / perPage)) + ((arranging || dragActive) ? 1 : 0))
    property int page: 0
    ListModel { id: pageModel }   // (not a plain number: a number as a model rebuilds every page when it changes, which would destroy the tile being dragged)
    function syncPages() {
        while (pageModel.count < pageCount) pageModel.append({})
        while (pageModel.count > pageCount) pageModel.remove(pageModel.count - 1)
    }
    onPageCountChanged: { syncPages(); if (page >= pageCount) page = pageCount - 1 }
    function goTo(i) { page = Math.max(0, Math.min(pageCount - 1, i)); pages.positionViewAtIndex(page, ListView.Beginning) }
    readonly property real dotsHeight: 40
    readonly property real cellW: width / cols
    readonly property real cellH: Math.max(1, (height - dotsHeight) / rows)
    readonly property real tileSide: Math.max(72, Math.min(150, Math.min(cellW - 28, cellH - 66)))

    // ---- dragging: a tile follows the pointer, the spot under it is lit, and it is dropped where the pointer is let go ----
    property string dragItem: ""               // "app:x" or "folder:y" while a drag is going on
    readonly property bool dragActive: dragItem !== ""
    property real dragX: 0
    property real dragY: 0
    readonly property int hoverSlot: dragActive ? slotAt(dragX, dragY) : -1
    readonly property string hoverItem: hoverSlot >= 0 ? itemAtSlot(hoverSlot) : ""
    function slotAt(px, py) {
        const c = Math.floor(px / cellW), r = Math.floor((py - dotsHeight) / cellH)
        if (c < 0 || c >= cols || r < 0 || r >= rows) return -1
        return page * perPage + r * cols + c
    }
    function dotAt(px, py) {   // the page dot under the pointer, or -1
        for (let i = 0; i < pageCount; ++i) {
            let d = null
            for (let k = 0; k < dots.children.length; ++k) if (dots.children[k].objectName === "appsDot_" + i) d = dots.children[k]
            if (!d) continue
            const p = d.mapToItem(root, 0, 0)
            if (px >= p.x - 10 && px <= p.x + d.width + 10 && py >= p.y - 10 && py <= p.y + d.height + 10) return i
        }
        return -1
    }
    function beginDrag(item, scenePos) {
        const p = root.mapFromItem(null, scenePos.x, scenePos.y)
        dragX = p.x; dragY = p.y
        dragItem = item
        edgeTimer.stop()
    }
    function dragMove(scenePos) {
        const p = root.mapFromItem(null, scenePos.x, scenePos.y)
        dragX = p.x; dragY = p.y
        const nearEdge = dragY > dotsHeight && (dragX < 36 || dragX > width - 36)
        if (nearEdge) { if (!edgeTimer.running) edgeTimer.start() } else edgeTimer.stop()
    }
    function endDrag() {
        const item = dragItem
        edgeTimer.stop()
        if (item === "") return
        drop(item)
        dragItem = ""
    }
    function drop(item) {
        const dot = dotAt(dragX, dragY)
        if (openFolder !== "") {   // inside a folder: onto "all apps" takes the app out; onto another spot of the folder reorders
            const app = item.substring(4)
            if (backButton.contains(backButton.mapFromItem(root, dragX, dragY))) {
                layout.removeFromFolder(app, layout.freeSlotNear(0))
                return
            }
            const s = hoverSlot
            if (s >= 0) layout.moveInFolder(openFolder, app, s)
            return
        }
        let slot = hoverSlot
        if (slot < 0 && dot >= 0) slot = layout.freeSlotNear(dot * perPage)                       // dropped on a page dot: the first free spot near that page
        if (slot < 0) return                                                                       // let go over nothing: it stays where it was
        const target = layout.itemAt(slot)
        if (target.indexOf("folder:") === 0 && item.indexOf("app:") === 0) layout.addToFolder(target.substring(7), item.substring(4))
        else layout.moveItem(item, slot)
    }
    Timer {   // holding a tile near the left or right edge turns the page
        id: edgeTimer
        interval: 650; repeat: true
        onTriggered: { if (root.dragX < 36) root.goTo(root.page - 1); else if (root.dragX > root.width - 36) root.goTo(root.page + 1) }
    }

    // ---- menus ----
    function openTileMenu(item, pos) {
        menuItem = item
        if (item.indexOf("folder:") === 0) folderMenu.popup(pos.x, pos.y)
        else appMenu.popup(pos.x, pos.y)
    }
    function newFolder(slot) {
        const id = layout.createFolder("folder", slot === undefined ? -1 : slot)
        if (id !== "") editFolder = id
        return id
    }
    Menu {
        id: appMenu
        background: Rectangle { implicitWidth: 240; color: Theme.panel; border { width: 1; color: Theme.line } }
        MenuItem {
            objectName: "appsMenuOpen"
            text: "open"
            onTriggered: root.openApp(root.menuItem.substring(4))
            contentItem: Label { text: parent.text; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(15) } leftPadding: 6 }
            background: Rectangle { implicitHeight: 36; color: parent.highlighted ? Theme.hover : "transparent" }
        }
        MenuItem {
            objectName: "appsMenuMoveToFolder"
            text: "move to a folder…"
            onTriggered: root.moveApp = root.menuItem.substring(4)
            contentItem: Label { text: parent.text; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(15) } leftPadding: 6 }
            background: Rectangle { implicitHeight: 36; color: parent.highlighted ? Theme.hover : "transparent" }
        }
        MenuItem {
            objectName: "appsMenuTakeOut"
            visible: root.openFolder !== ""
            height: visible ? implicitHeight : 0
            text: "take out of this folder"
            onTriggered: root.layout.removeFromFolder(root.menuItem.substring(4), root.layout.freeSlotNear(0))
            contentItem: Label { text: parent.text; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(15) } leftPadding: 6 }
            background: Rectangle { implicitHeight: 36; color: parent.highlighted ? Theme.hover : "transparent" }
        }
    }
    Menu {
        id: folderMenu
        background: Rectangle { implicitWidth: 240; color: Theme.panel; border { width: 1; color: Theme.line } }
        MenuItem {
            objectName: "appsMenuFolderOpen"
            text: "open"
            onTriggered: root.openTheFolder(root.menuItem.substring(7))
            contentItem: Label { text: parent.text; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(15) } leftPadding: 6 }
            background: Rectangle { implicitHeight: 36; color: parent.highlighted ? Theme.hover : "transparent" }
        }
        MenuItem {
            objectName: "appsMenuFolderEdit"
            text: "rename, colour or picture…"
            onTriggered: root.editFolder = root.menuItem.substring(7)
            contentItem: Label { text: parent.text; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(15) } leftPadding: 6 }
            background: Rectangle { implicitHeight: 36; color: parent.highlighted ? Theme.hover : "transparent" }
        }
        MenuItem {
            objectName: "appsMenuFolderDelete"
            text: "delete folder (its apps stay)"
            onTriggered: root.layout.deleteFolder(root.menuItem.substring(7))
            contentItem: Label { text: parent.text; color: Theme.danger; font { family: Theme.font; pixelSize: Theme.fs(15) } leftPadding: 6 }
            background: Rectangle { implicitHeight: 36; color: parent.highlighted ? Theme.hover : "transparent" }
        }
    }
    Menu {
        id: emptyMenu
        property int slot: -1
        background: Rectangle { implicitWidth: 240; color: Theme.panel; border { width: 1; color: Theme.line } }
        MenuItem {
            objectName: "appsMenuNewFolder"
            text: "new folder here"
            onTriggered: root.newFolder(emptyMenu.slot)
            contentItem: Label { text: parent.text; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(15) } leftPadding: 6 }
            background: Rectangle { implicitHeight: 36; color: parent.highlighted ? Theme.hover : "transparent" }
        }
    }

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
        anchors { top: parent.top; topMargin: 14 }
        x: Theme.compact ? 22 : Math.round((root.width - width) / 2)   // (a phone has the buttons across the top: the dots go to the left)
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
        interactive: !root.arranging && !root.dragActive
        model: pageModel
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
                        readonly property int slotIndex: pageItem.index * root.perPage + index
                        readonly property string slotItem: { root.layout.revision; root.openFolder; return root.itemAtSlot(slotIndex) }
                        readonly property var app: slotItem.indexOf("app:") === 0 ? root.appById(slotItem.substring(4)) : null
                        readonly property var folder: slotItem.indexOf("folder:") === 0 ? root.folderById(slotItem.substring(7)) : null
                        readonly property bool free: app === null && folder === null
                        width: root.cellW; height: root.cellH
                        DarkGlass {   // a free spot on a dark look
                            visible: slot.free && root.look === "dark"
                            anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: Math.max(0, (parent.height - (root.tileSide + 52)) / 2) }
                            width: root.tileSide; height: root.tileSide
                        }
                        Rectangle {   // (its soft shadow)
                            visible: slot.free && root.look === "light"
                            x: (parent.width - width) / 2; y: Math.max(0, (parent.height - (root.tileSide + 52)) / 2) + 4
                            width: root.tileSide; height: root.tileSide; radius: width * 0.2; color: "#1a202428"
                        }
                        Image {   // a free spot on a light look: faded empty glass
                            visible: slot.free && root.look === "light"
                            anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: Math.max(0, (parent.height - (root.tileSide + 52)) / 2) }
                            width: root.tileSide; height: root.tileSide
                            source: "qrc:/images/app-tile.png"
                            sourceSize.width: root.tileSide * 2; sourceSize.height: root.tileSide * 2
                            opacity: 0.75
                            smooth: true; mipmap: true
                        }
                        MouseArea {   // a right click on a free spot: a new folder there
                            anchors.fill: parent; acceptedButtons: Qt.RightButton; enabled: slot.free && root.openFolder === ""
                            onClicked: function (m) { emptyMenu.slot = slot.slotIndex; emptyMenu.popup(slot.mapToItem(root, m.x, m.y).x, slot.mapToItem(root, m.x, m.y).y) }
                        }
                        Loader {
                            active: !slot.free
                            anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: Math.max(0, (parent.height - (root.tileSide + 52)) / 2) }
                            sourceComponent: GridTile { app: slot.app; folder: slot.folder; side: root.tileSide }
                        }
                    }
                }
            }
        }
    }

    Rectangle {   // the spot under a dragged tile is lit
        objectName: "appsDropHighlight"
        visible: root.dragActive && root.hoverSlot >= 0
        x: (root.hoverSlot % root.perPage % root.cols) * root.cellW + 6
        y: root.dotsHeight + Math.floor((root.hoverSlot % root.perPage) / root.cols) * root.cellH + 6
        width: root.cellW - 12; height: root.cellH - 12; radius: 16
        color: "transparent"; border { width: 2; color: Theme.accent }
        opacity: 0.8
    }
    Item {   // the tile that follows the pointer
        id: dragProxy
        objectName: "appsDragProxy"
        z: 60
        visible: root.dragActive
        width: root.tileSide; height: root.tileSide
        x: root.dragX - width / 2; y: root.dragY - height / 2
        opacity: 0.92; scale: 1.08
        AppFace { anchors.fill: parent; visible: root.dragItem.indexOf("app:") === 0; app: root.dragItem.indexOf("app:") === 0 ? root.appById(root.dragItem.substring(4)) : null; side: root.tileSide }
        FolderFace { anchors.fill: parent; visible: root.dragItem.indexOf("folder:") === 0; folder: root.dragItem.indexOf("folder:") === 0 ? root.folderById(root.dragItem.substring(7)) : null; side: root.tileSide }
    }

    // ---- a folder is open: its name and the way back (dropping an app on it takes the app out) ----
    Rectangle {
        id: folderHeader
        objectName: "appsFolderHeader"
        visible: root.openFolder !== ""
        anchors { left: parent.left; top: parent.top; margins: 8 }
        height: 36; width: backRow.implicitWidth + 28; radius: 18
        color: backButton.hovered ? (root.look === "light" ? "#fff" : "#2b2c31") : (root.look === "light" ? "#d9ffffff" : "#cc2b2c31")
        border { width: 1; color: root.look === "light" ? "#b8bcc0" : "#4a4b52" }
        Row {
            id: backRow
            anchors.centerIn: parent; spacing: 8
            Label { text: "‹ all apps"; color: root.labelColor; font { family: Theme.font; pixelSize: Theme.fs(14) } anchors.verticalCenter: parent.verticalCenter }
            Label { objectName: "appsFolderTitle"; text: root.openFolderData ? "· " + root.openFolderData.name : ""; color: root.labelDim; elide: Text.ElideRight
                    width: Math.min(implicitWidth, root.width * (Theme.compact ? 0.25 : 0.4)); font { family: Theme.font; pixelSize: Theme.fs(14) } anchors.verticalCenter: parent.verticalCenter }
        }
        AbstractButton {
            id: backButton
            objectName: "appsFolderBack"
            anchors.fill: parent; hoverEnabled: true; focusPolicy: Qt.NoFocus
            Accessible.name: "back to all apps"
            onClicked: root.closeFolder()
            background: Item {}
        }
    }

    Keys.onEscapePressed: function (e) { if (root.goBack()) e.accepted = true }

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

    // ---- the buttons at the top right: arranging, a new folder, custom themes ----
    Row {
        id: topButtons
        anchors { top: parent.top; right: parent.right; topMargin: 10; rightMargin: 14 }
        spacing: 8
        component TopButton: AbstractButton {
            id: tb
            property string label: ""
            hoverEnabled: true; focusPolicy: Qt.NoFocus
            implicitHeight: 30; implicitWidth: tbLabel.implicitWidth + 24
            Accessible.name: tb.label
            background: Rectangle { radius: 15; color: tb.hovered || tb.checked ? (root.look === "light" ? "#fff" : "#2b2c31") : (root.look === "light" ? "#d9ffffff" : "#992b2c31")
                                    border { width: 1; color: root.look === "light" ? "#b8bcc0" : "#4a4b52" } }
            contentItem: Label { id: tbLabel; text: tb.label; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                 color: root.labelColor; font { family: Theme.font; pixelSize: Theme.fs(13) } }
        }
        TopButton {
            objectName: "appsArrangeDone"
            visible: root.arranging
            label: "done"
            onClicked: root.arranging = false
        }
        TopButton {
            objectName: "appsArrange"
            visible: !root.arranging && root.openFolder === "" && Theme.compact
            label: "arrange"
            onClicked: root.arranging = true
        }
        TopButton {
            objectName: "appsFolderEdit"
            visible: root.openFolder !== "" && !root.arranging
            label: "edit folder"
            onClicked: root.editFolder = root.openFolder
        }
        TopButton {
            objectName: "appsNewFolder"
            visible: !root.arranging && root.openFolder === ""
            label: Theme.compact ? "+ folder" : "new folder"
            onClicked: root.newFolder()
        }
        TopButton {
            id: themesButton
            objectName: "appsCustomThemes"
            visible: !root.arranging && root.openFolder === ""
            label: Theme.compact ? "themes" : "custom themes"
            onClicked: themesPanel.opened = !themesPanel.opened
            checked: themesPanel.opened
            checkable: false
        }
    }

    Rectangle {
        id: themesPanel
        objectName: "appsThemesPanel"
        property bool opened: false
        property string message: ""
        visible: opened
        anchors { top: topButtons.bottom; right: parent.right; topMargin: 8; rightMargin: 14 }
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
    // ---- the folder editor: name, colour, picture, delete ----
    Rectangle {
        id: folderEditor
        objectName: "appsFolderEditor"
        visible: root.editFolder !== "" && !!root.folderById(root.editFolder)
        readonly property var fd: root.editFolder === "" ? null : (root.layout.revision >= 0 ? root.folderById(root.editFolder) : null)
        property string message: ""
        anchors.centerIn: parent
        width: Math.min(root.width - 28, 440)
        height: Math.min(root.height - 30, editorColumn.implicitHeight + 28)
        z: 70
        radius: 14
        color: Theme.light ? "#f8ffffff" : "#f5202226"
        border { width: 1; color: Theme.light ? "#c9ccd0" : "#3a3b42" }
        focus: visible
        Keys.onEscapePressed: root.editFolder = ""
        onVisibleChanged: if (visible) { const f = root.folderById(root.editFolder); folderNameField.text = f ? f.name : ""; message = ""; folderNameField.forceActiveFocus(); folderNameField.selectAll() }
        MouseArea { anchors.fill: parent }   // (nothing under the panel hears the mouse)
        Flickable {
            anchors { fill: parent; margins: 14 }
            contentHeight: editorColumn.implicitHeight; clip: true
            boundsBehavior: Flickable.StopAtBounds
            ColumnLayout {
                id: editorColumn
                width: parent.width; spacing: 10
                Label { text: "folder"; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(20); weight: Font.Light } }
                RowLayout {
                    Layout.fillWidth: true; spacing: 8
                    TextField {
                        id: folderNameField
                        objectName: "appsFolderName"
                        Layout.fillWidth: true; maximumLength: 40; selectByMouse: true
                        placeholderText: "folder name"
                        font { family: Theme.font; pixelSize: Theme.fs(16) } color: Theme.text; placeholderTextColor: Theme.dim
                        background: Rectangle { color: Theme.panel; border { width: 1; color: Theme.border } }
                        onAccepted: if (folderEditor.fd) root.layout.renameFolder(folderEditor.fd.id, text)
                    }
                    PlainButton { objectName: "appsFolderRename"; text: "rename"; implicitHeight: 36; enabled: folderNameField.text.trim() !== ""
                                  onClicked: if (folderEditor.fd) root.layout.renameFolder(folderEditor.fd.id, folderNameField.text) }
                }
                Label { text: "colour"; color: Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(14) } }
                Flow {
                    Layout.fillWidth: true; spacing: 10
                    Rectangle {   // clear glass
                        objectName: "appsFolderColor_clear"
                        width: 34; height: 34; radius: 8; color: "transparent"
                        border { width: folderEditor.fd && folderEditor.fd.color === "" ? 3 : 1; color: folderEditor.fd && folderEditor.fd.color === "" ? Theme.accent : Theme.border }
                        Label { anchors.centerIn: parent; text: "×"; color: Theme.dim; font.pixelSize: 18 }
                        Accessible.name: "clear glass"
                        MouseArea { anchors.fill: parent; onClicked: if (folderEditor.fd) root.layout.setFolderColor(folderEditor.fd.id, "") }
                    }
                    Repeater {
                        model: hop.accents
                        delegate: Rectangle {
                            required property string modelData
                            objectName: "appsFolderColor_" + modelData
                            width: 34; height: 34; radius: 8; color: modelData
                            border { width: folderEditor.fd && folderEditor.fd.color === modelData ? 3 : 1; color: folderEditor.fd && folderEditor.fd.color === modelData ? Theme.strong : Theme.border }
                            Accessible.name: "colour " + modelData
                            MouseArea { anchors.fill: parent; onClicked: if (folderEditor.fd) root.layout.setFolderColor(folderEditor.fd.id, modelData) }
                        }
                    }
                }
                Label { text: "picture"; color: Theme.text2; font { family: Theme.font; pixelSize: Theme.fs(14) } }
                RowLayout {
                    spacing: 10
                    PlainButton { objectName: "appsFolderPicture"; text: "choose a picture…"; implicitHeight: 36; onClicked: folderPictureDialog.open() }
                    PlainButton { objectName: "appsFolderPictureRemove"; text: "remove picture"; implicitHeight: 36; visible: !!folderEditor.fd && folderEditor.fd.picture !== ""
                                  onClicked: hop.removeAppsFolderPicture(folderEditor.fd.id) }
                }
                Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(12) }
                        text: "hop-tunes keeps a small square copy in your profile on this computer; the photo itself is not kept and nothing is uploaded." }
                Label { objectName: "appsFolderMessage"; visible: folderEditor.message !== ""; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.danger
                        text: folderEditor.message; font { family: Theme.font; pixelSize: Theme.fs(13) } }
                RowLayout {
                    spacing: 10
                    PlainButton { objectName: "appsFolderDone"; text: "done"; primary: true; implicitHeight: 38; onClicked: { if (folderEditor.fd) root.layout.renameFolder(folderEditor.fd.id, folderNameField.text); root.editFolder = "" } }
                    PlainButton { objectName: "appsFolderDelete"; text: "delete folder"; danger: true; implicitHeight: 38
                                  onClicked: { const id = folderEditor.fd ? folderEditor.fd.id : ""; root.editFolder = ""; if (id !== "") root.layout.deleteFolder(id) } }
                }
                Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(12) }
                        text: "deleting a folder only removes the folder: its apps go back on the grid." }
            }
        }
    }
    FileDialog {
        id: folderPictureDialog
        title: "choose a picture for the folder"
        nameFilters: ["pictures (*.png *.jpg *.jpeg *.webp *.bmp)"]
        onAccepted: folderEditor.message = folderEditor.fd ? hop.setAppsFolderPicture(folderEditor.fd.id, selectedFile) : ""
    }

    // ---- "move to a folder…": a list of the folders (and a new one), for a touch screen or the keyboard ----
    Rectangle {
        id: folderPicker
        objectName: "appsFolderPicker"
        visible: root.moveApp !== ""
        anchors.centerIn: parent
        width: Math.min(root.width - 28, 360)
        height: Math.min(root.height - 30, pickerColumn.implicitHeight + 28)
        z: 70
        radius: 14
        color: Theme.light ? "#f8ffffff" : "#f5202226"
        border { width: 1; color: Theme.light ? "#c9ccd0" : "#3a3b42" }
        focus: visible
        Keys.onEscapePressed: root.moveApp = ""
        MouseArea { anchors.fill: parent }
        Flickable {
            anchors { fill: parent; margins: 14 }
            contentHeight: pickerColumn.implicitHeight; clip: true
            boundsBehavior: Flickable.StopAtBounds
            ColumnLayout {
                id: pickerColumn
                width: parent.width; spacing: 8
                Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.strong; font { family: Theme.font; pixelSize: Theme.fs(18); weight: Font.Light }
                        text: root.appById(root.moveApp) ? "move “" + root.appById(root.moveApp).name + "” to a folder" : "" }
                Repeater {
                    model: root.layout.folders
                    delegate: PlainButton {
                        required property var modelData
                        objectName: "appsMoveTo_" + modelData.id
                        Layout.fillWidth: true; implicitHeight: 38
                        text: modelData.name + (modelData.members.indexOf(root.moveApp) >= 0 ? "  (it is in here)" : "")
                        enabled: modelData.members.indexOf(root.moveApp) < 0
                        onClicked: { const app = root.moveApp, folderId = modelData.id; root.moveApp = ""; root.layout.addToFolder(folderId, app) }   // (the list is rebuilt by the change, so close it first)
                    }
                }
                PlainButton {
                    objectName: "appsMoveToNew"
                    Layout.fillWidth: true; implicitHeight: 38; text: "+ a new folder"
                    onClicked: { const app = root.moveApp; root.moveApp = ""; const id = root.layout.createFolder("folder"); if (id !== "") { root.layout.addToFolder(id, app); root.editFolder = id } }
                }
                PlainButton { objectName: "appsMoveCancel"; Layout.fillWidth: true; implicitHeight: 38; text: "cancel"; onClicked: root.moveApp = "" }
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
