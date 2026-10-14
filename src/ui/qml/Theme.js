// SPDX-License-Identifier: GPL-3.0-or-later
// Design tokens, matching design/mockups/style.css.
.pragma library

var bg = "#111113"
var bgDeep = "#0b0b0d"
var panel = "#1c1c20"
var bar = "#18181b"
var hover = "#1f1f24"
var line = "#2a2a2f"
var text = "#e9e9ec"
var text2 = "#c8c8ce"
var muted = "#a3a3ab"
var dim = "#6e6e77"
var faint = "#4f4f57"
var ink = "#111113"
var accent = "#4fd1c5"

var font = "Jost"          // falls back to the system sans if not installed

var palette = [["#2f6f73","#f2c94c"],["#3b2f5c","#ff8a4c"],["#7a3341","#e9d8c4"],["#1f3b57","#7cc4ff"],
               ["#4a5a2a","#d9e3a5"],["#5a2f2f","#ffb38a"],["#2b2b3a","#b69cff"],["#6b4f2a","#f7e3b0"],
               ["#203a33","#8fe0c9"],["#264653","#e76f51"],["#6b3e26","#f4a261"]]

// Stable two-colour placeholder art per album until real cover art lands (M2).
function artColors(key) {
    var h = 0
    var s = String(key || "")
    for (var i = 0; i < s.length; ++i) h = (h * 31 + s.charCodeAt(i)) | 0
    return palette[Math.abs(h) % palette.length]
}
