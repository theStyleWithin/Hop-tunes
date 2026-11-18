// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The apps tab: small apps that are part of Hop-tunes and run inside its window, each one a glass tile.
// To add an app: one entry in `apps` below and its screen in AppHost.qml.
Item {
    id: root
    objectName: "appsView"
    signal openApp(string appId)
    // how the page looks: "light" and "dark" follow the theme, "coffee" is the anime coffee look (settings > apps settings > apps page)
    readonly property string look: hop.appsLook === "coffee" ? "coffee" : (Theme.light ? "light" : "dark")

    // id, the name under the tile, a line for the tooltip, an icon (a url, or "" for the plain glass)
    // net: the app uses the internet (it says so on its tile, and is left out of builds without networked apps)
    readonly property var allApps: [
        { id: "kick", name: "kick", blurb: "browse and watch live streams from kick.com. uses the internet only while it is open, and only after you allow it", icon: "qrc:/images/app-kick.png", net: true },
        { id: "nekocafe", name: "neko cafe", blurb: "a little game: run a cafe as a neko maid. take orders, fetch the coffee from the cook and serve the right customer, fast. works offline, nothing is sent anywhere", icon: "qrc:/images/app-neko-cafe.png", net: false }
    ]
    readonly property var apps: allApps.filter(function (a) { return !a.net || hop.netAppsBuilt })

    // the dark theme's glass square: flat and dark with a soft edge and a faint rim (the light theme uses the glass picture)
    component DarkGlass: Item {
        Rectangle { y: 3; width: parent.width; height: parent.height; radius: width * 0.2; color: "#0d0e10"; opacity: 0.7 }   // its soft shadow
        Rectangle {
            anchors.fill: parent; radius: width * 0.2
            gradient: Gradient { GradientStop { position: 0.0; color: "#26272b" } GradientStop { position: 1.0; color: "#1d1e22" } }
            border { width: 1.5; color: "#313238" }
            Rectangle { anchors.fill: parent; anchors.margins: parent.width * 0.05; radius: parent.radius * 0.8; color: "transparent"; border { width: 1; color: "#2b2c31" } }
        }
    }

    // the coffee look's glass square: warm, see-through cream glass with a bright rim
    component CoffeeGlass: Item {
        Rectangle { y: 3; width: parent.width; height: parent.height; radius: width * 0.2; color: "#66401a00"; }   // its soft shadow
        Rectangle {
            anchors.fill: parent; radius: width * 0.2
            gradient: Gradient { GradientStop { position: 0.0; color: "#70fff3df" } GradientStop { position: 1.0; color: "#30ffd9a0" } }
            border { width: 2; color: "#b8ffffff" }
            Rectangle { anchors.fill: parent; anchors.margins: parent.width * 0.05; radius: parent.radius * 0.8; color: "transparent"; border { width: 1.5; color: "#55ffffff" } }
            Rectangle { x: parent.width * 0.12; y: parent.height * 0.08; width: parent.width * 0.5; height: parent.height * 0.12; radius: height / 2; color: "#40ffffff" }   // a glint
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
                CoffeeGlass { anchors.fill: parent; visible: root.look === "coffee" }
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
                color: root.look === "coffee" ? "#4b2a12" : (tile.hovered || tile.activeFocus ? Theme.text : Theme.text2)
                font { family: Theme.font; pixelSize: Theme.fs(16) }
            }
            Label {   // honest about the network
                visible: tile.app.net === true
                anchors { top: glass.bottom; topMargin: 30; horizontalCenter: parent.horizontalCenter }
                text: "uses the internet"; color: root.look === "coffee" ? "#7a4a22" : Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(11) }
            }
        }
        background: Item {}
    }

    Label {   // nothing to open here yet (a build without the networked apps has none)
        objectName: "appsEmpty"
        visible: root.apps.length === 0
        anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: 8 }
        wrapMode: Text.WordWrap; color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(16) }
        text: "there are no apps in this build. kick needs the internet and was left out of this version."
    }

    // ---- the grid: pages of glass tiles (five across and three down on a computer, three across on a phone), page dots on top ----
    // Free spots are drawn as faded empty glass, so a page always looks full and shows there is room for more apps.
    readonly property int cols: Theme.compact ? 3 : 5
    readonly property int rows: Theme.compact ? 3 : 3
    readonly property int perPage: cols * rows
    readonly property int pageCount: Math.max(1, Math.ceil(apps.length / perPage))
    property int page: 0
    onPageCountChanged: if (page >= pageCount) page = pageCount - 1
    function goTo(i) { page = Math.max(0, Math.min(pageCount - 1, i)); pages.positionViewAtIndex(page, ListView.Beginning) }
    readonly property real dotsHeight: 40
    readonly property real cellW: width / cols
    readonly property real cellH: Math.max(1, (height - dotsHeight) / rows)
    readonly property real tileSide: Math.max(72, Math.min(150, Math.min(cellW - 28, cellH - 66)))

    Rectangle {   // the frosted backdrop (it follows the theme: pale in the light theme, a dark frost in the dark one)
        anchors.fill: parent
        radius: 18
        gradient: Gradient {
            GradientStop { position: 0.0; color: root.look === "coffee" ? "#e98a2c" : (Theme.light ? "#f6f7f5" : "#1b1c20") }
            GradientStop { position: 1.0; color: root.look === "coffee" ? "#f2ad58" : (Theme.light ? "#e8ebe8" : "#27282d") }
        }
        // a few soft glass shapes floating behind the tiles
        Canvas {   // the anime coffee look: wood, a big cup and saucer, steam (all drawn here, nothing borrowed)
            id: coffee
            anchors.fill: parent
            visible: root.look === "coffee"
            onVisibleChanged: if (visible) requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d"), w = width, h = height
                ctx.reset()
                function ell(cx, cy, rx, ry) { ctx.save(); ctx.translate(cx, cy); ctx.scale(rx, ry); ctx.arc(0, 0, 1, 0, Math.PI * 2); ctx.restore() }
                // keep to the rounded corners of the page
                const r = 18
                ctx.beginPath(); ctx.moveTo(r, 0); ctx.lineTo(w - r, 0); ctx.quadraticCurveTo(w, 0, w, r); ctx.lineTo(w, h - r); ctx.quadraticCurveTo(w, h, w - r, h)
                ctx.lineTo(r, h); ctx.quadraticCurveTo(0, h, 0, h - r); ctx.lineTo(0, r); ctx.quadraticCurveTo(0, 0, r, 0); ctx.closePath(); ctx.clip()
                // wood grain
                for (let i = 0; i < 70; ++i) {
                    const y = (i + 0.5) * h / 70, a = 0.03 + 0.05 * Math.abs(Math.sin(i * 12.9898))
                    ctx.fillStyle = (i % 3 === 0) ? "rgba(120,50,0," + a + ")" : "rgba(255,240,200," + a + ")"
                    ctx.fillRect(0, y, w, 1 + (i * 7) % 3)
                }
                const line = Math.max(2, w / 520), outline = "#6b4352"
                ctx.lineWidth = line; ctx.strokeStyle = outline
                // the saucer
                ctx.beginPath(); ell(w * 0.5, h * 0.80, w * 0.46, h * 0.20); ctx.fillStyle = "#f7f0e8"; ctx.fill(); ctx.stroke()
                ctx.beginPath(); ell(w * 0.5, h * 0.79, w * 0.30, h * 0.12); ctx.fillStyle = "#ebe0d4"; ctx.fill(); ctx.stroke()
                // the spoon
                ctx.save(); ctx.translate(w * 0.78, h * 0.93); ctx.rotate(-0.07)
                ctx.beginPath(); ctx.moveTo(-w * 0.25, -h * 0.012); ctx.lineTo(w * 0.17, -h * 0.02); ctx.lineTo(w * 0.17, h * 0.02); ctx.lineTo(-w * 0.25, h * 0.012); ctx.closePath()
                ctx.fillStyle = "#f3ece4"; ctx.fill(); ctx.stroke(); ctx.restore()
                // the handle
                const cx = w * 0.5, cy = h * 0.40, rx = w * 0.27, ry = h * 0.115
                ctx.beginPath(); ell(cx + rx * 0.97, cy + h * 0.15, w * 0.075, h * 0.12)
                ctx.lineWidth = h * 0.050; ctx.strokeStyle = outline; ctx.stroke()
                ctx.lineWidth = h * 0.034; ctx.strokeStyle = "#fbf7f2"; ctx.stroke()
                // the cup
                ctx.lineWidth = line; ctx.strokeStyle = outline
                ctx.beginPath(); ctx.moveTo(cx - rx, cy)
                ctx.bezierCurveTo(cx - rx, cy + h * 0.31, cx - w * 0.14, cy + h * 0.43, cx, cy + h * 0.43)
                ctx.bezierCurveTo(cx + w * 0.14, cy + h * 0.43, cx + rx, cy + h * 0.31, cx + rx, cy); ctx.closePath()
                ctx.fillStyle = "#fbf7f2"; ctx.fill(); ctx.stroke()
                ctx.beginPath(); ell(cx, cy, rx, ry); ctx.fillStyle = "#fffaf5"; ctx.fill(); ctx.stroke()
                ctx.beginPath(); ell(cx, cy + ry * 0.05, rx * 0.9, ry * 0.8); ctx.fillStyle = "#5a2f22"; ctx.fill()
                ctx.beginPath(); ell(cx - rx * 0.25, cy - ry * 0.1, rx * 0.35, ry * 0.25); ctx.fillStyle = "rgba(255,210,170,0.28)"; ctx.fill()
                ctx.beginPath(); ctx.moveTo(cx - rx * 0.4, cy + ry * 0.1); ctx.bezierCurveTo(cx - rx * 0.1, cy - ry * 0.35, cx + rx * 0.1, cy + ry * 0.55, cx + rx * 0.45, cy)
                ctx.lineWidth = line * 2; ctx.strokeStyle = "rgba(255,225,190,0.35)"; ctx.stroke()
                // the steam
                ctx.lineCap = "round"; ctx.lineWidth = w * 0.011; ctx.strokeStyle = "rgba(255,255,255,0.55)"
                for (let i = 0; i < 3; ++i) {
                    const x = cx + (i - 1) * w * 0.12
                    ctx.beginPath(); ctx.moveTo(x, cy - ry * 1.1)
                    ctx.bezierCurveTo(x - w * 0.04, cy - h * 0.22, x + w * 0.05, cy - h * 0.30, x - w * 0.01, cy - h * 0.40); ctx.stroke()
                }
            }
        }
        Repeater {
            visible: root.look !== "coffee"
            model: root.look === "coffee" ? [] : [[0.06, 0.42, 70], [0.93, 0.08, 110], [0.88, 0.62, 60], [0.46, 0.93, 40], [0.12, 0.88, 34]]
            delegate: Rectangle {
                required property var modelData
                x: parent.width * modelData[0] - width / 2; y: parent.height * modelData[1] - height / 2
                width: modelData[2]; height: width; radius: width / 2
                color: Theme.light ? "#33ffffff" : "#14ffffff"
                border { width: 2; color: Theme.light ? "#55ffffff" : "#1fffffff" }
            }
        }
    }

    Rectangle {   // (a light strip behind the dots in the dark look, so the grey ones can be seen)
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
                color: root.look === "coffee" ? (index === root.page ? "#3f4aa0" : "#8a5a2a") : (index === root.page ? "#12a0f4" : "#b9bdc0")
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
                        DarkGlass {   // a free spot in the dark theme
                            visible: slot.app === null && root.look === "dark"
                            anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: Math.max(0, (parent.height - (root.tileSide + 52)) / 2) }
                            width: root.tileSide; height: root.tileSide
                        }
                        CoffeeGlass {   // a free spot in the coffee look
                            visible: slot.app === null && root.look === "coffee"
                            anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: Math.max(0, (parent.height - (root.tileSide + 52)) / 2) }
                            width: root.tileSide; height: root.tileSide
                            opacity: 0.8
                        }
                        Image {   // a free spot in the light theme: faded empty glass
                            visible: slot.app === null && root.look === "light"
                            anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: Math.max(0, (parent.height - (root.tileSide + 52)) / 2) }
                            width: root.tileSide; height: root.tileSide
                            source: "qrc:/images/app-tile.png"
                            sourceSize.width: root.tileSide * 2; sourceSize.height: root.tileSide * 2
                            opacity: 0.5
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
}
