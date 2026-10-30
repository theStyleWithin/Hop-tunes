// SPDX-License-Identifier: GPL-3.0-or-later
pragma Singleton
import QtQuick

// Design tokens (spec §4.5), for the dark and light themes. The theme and motion settings
// belong to the profile (hop.lightTheme, hop.reduceMotion).
QtObject {
    readonly property bool light: typeof hop !== "undefined" && hop !== null && hop.lightTheme
    readonly property bool reduceMotion: typeof hop !== "undefined" && hop !== null && hop.reduceMotion
    // The accent: the profile's colour on desktop and tablets; the design's yellow on phones.
    readonly property color accent: compact ? "#f2c94c" : (typeof hop !== "undefined" && hop !== null ? hop.accent : "#4fd1c5")
    property bool compact: false        // phones (set by Main.qml): tighter margins
    readonly property int pad: compact ? 16 : 48
    // Type sizes: unchanged on desktop and tablets, scaled down on phones (big titles most).
    function fs(px) { return !compact ? px : (px >= 40 ? Math.round(px * 0.55) : px >= 26 ? Math.round(px * 0.7) : Math.max(12, Math.round(px * 0.9))) }
    readonly property int motion: reduceMotion ? 0 : 180          // pivot transitions, ms (≤ 200)

    readonly property color bg:      light ? "#f4f4f6" : "#111113"
    readonly property color bgDeep:  light ? "#ececf0" : "#0b0b0d"
    readonly property color panel:   light ? "#ffffff" : "#1c1c20"
    readonly property color bar:     light ? "#fafafb" : "#18181b"
    readonly property color hover:   light ? "#e6e6ea" : "#1f1f24"
    readonly property color line:    light ? "#dcdce2" : "#2a2a2f"
    readonly property color text:    light ? "#1a1a1e" : "#e9e9ec"
    readonly property color text2:   light ? "#3a3a42" : "#c8c8ce"
    readonly property color muted:   light ? "#5e5e68" : "#a3a3ab"
    readonly property color dim:     light ? "#8a8a94" : "#6e6e77"
    readonly property color faint:   light ? "#b0b0b8" : "#4f4f57"
    readonly property color ink:     light ? "#ffffff" : "#111113"   // text on an accent fill
    readonly property color strong:  light ? "#0d0d10" : "#ffffff"   // headings, selected pivots
    readonly property color border:  light ? "#b8b8c0" : "#5b5b63"
    readonly property color borderSoft: light ? "#cfcfd6" : "#3a3a42"
    readonly property color tabIdle: light ? "#6e6e78" : "#8a8a93"
    readonly property color rowLine: light ? "#ebebef" : "#1d1d22"
    readonly property color track:   light ? "#d4d4da" : "#34343a"   // slider groove
    readonly property color danger:  light ? "#c43c3c" : "#ff8a8a"
    readonly property color dangerLine: light ? "#e2a4a4" : "#7a3a3a"
    readonly property color scrim:   "#a0000000"
    function fade(a) { return Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, a) }   // bgDeep at alpha a

    readonly property string font: "Jost"   // bundled; falls back to the system sans

    readonly property var palette: [["#2f6f73","#f2c94c"],["#3b2f5c","#ff8a4c"],["#7a3341","#e9d8c4"],["#1f3b57","#7cc4ff"],
                                    ["#4a5a2a","#d9e3a5"],["#5a2f2f","#ffb38a"],["#2b2b3a","#b69cff"],["#6b4f2a","#f7e3b0"],
                                    ["#203a33","#8fe0c9"],["#264653","#e76f51"],["#6b3e26","#f4a261"]]

    // Stable two-colour placeholder art for albums without a cover.
    function artColors(key) {
        var h = 0
        var s = String(key || "")
        for (var i = 0; i < s.length; ++i) h = (h * 31 + s.charCodeAt(i)) | 0
        return palette[Math.abs(h) % palette.length]
    }
}
