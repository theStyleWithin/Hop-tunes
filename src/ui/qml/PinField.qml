// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import "Theme.js" as Theme

// 4–8 digit PIN entry, shown as dots.
TextField {
    id: f
    property string hint: "PIN"
    echoMode: TextInput.Password
    inputMethodHints: Qt.ImhDigitsOnly
    validator: RegularExpressionValidator { regularExpression: /[0-9]{0,8}/ }
    maximumLength: 8
    placeholderText: hint
    placeholderTextColor: Theme.dim
    color: Theme.text
    font { family: Theme.font; pixelSize: 20; letterSpacing: f.text.length ? 4 : 0 }
    implicitWidth: 220
    Accessible.name: hint
    background: Rectangle { implicitHeight: 48; color: Theme.panel; border { width: f.activeFocus ? 2 : 1; color: f.activeFocus ? hop.accent : Theme.line } }
}
