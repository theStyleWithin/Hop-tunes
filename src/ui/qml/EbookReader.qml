// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The EPUB reader: one chapter at a time as rich text, with a table of contents, adjustable
// text size and a saved reading position. Left/right arrows change chapter; space and the
// page keys scroll; escape closes.
Popup {
    id: root
    property int fontSize: 20

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: parent ? parent.width : 0
    height: parent ? parent.height : 0
    modal: true
    padding: 0
    closePolicy: Popup.CloseOnEscape
    background: Rectangle { color: Theme.bg }

    readonly property real fraction: flick.contentHeight > flick.height
        ? Math.max(0, Math.min(1, flick.contentY / (flick.contentHeight - flick.height))) : 0

    function restore() {
        if (flick.contentHeight > flick.height)
            flick.contentY = hop.bookResumeFraction * (flick.contentHeight - flick.height)
        else flick.contentY = 0
    }

    Connections {
        target: hop
        function onBookChanged() {
            if (hop.bookOpen && !root.opened) root.open()
            else if (!hop.bookOpen && root.opened) root.close()
        }
    }
    onOpened: scope.forceActiveFocus()
    onClosed: if (hop.bookOpen) hop.closeBook(root.fraction)

    Timer { interval: 5000; running: root.opened; repeat: true; onTriggered: hop.saveBookProgress(root.fraction) }

    contentItem: FocusScope {
        id: scope
        focus: true
        Keys.onPressed: function (e) {
            if (e.key === Qt.Key_Right) { hop.saveBookProgress(root.fraction); hop.goToBookChapter(hop.bookChapterIndex + 1); e.accepted = true }
            else if (e.key === Qt.Key_Left) { hop.goToBookChapter(hop.bookChapterIndex - 1); e.accepted = true }
            else if (e.key === Qt.Key_Space || e.key === Qt.Key_PageDown) { flick.contentY = Math.min(flick.contentHeight - flick.height, flick.contentY + flick.height * 0.9); e.accepted = true }
            else if (e.key === Qt.Key_PageUp) { flick.contentY = Math.max(0, flick.contentY - flick.height * 0.9); e.accepted = true }
        }

        RowLayout {
            id: header
            anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: 28; rightMargin: 28; topMargin: 16 }
            spacing: 12
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                Label { text: hop.bookTitle; color: Theme.text; elide: Text.ElideRight; Layout.fillWidth: true
                        font { family: Theme.font; pixelSize: Theme.fs(18) } }
                Label { text: (hop.bookAuthor !== "" ? hop.bookAuthor + "  ·  " : "") + "chapter " + (hop.bookChapterIndex + 1) + " of " + hop.bookChapterCount
                        color: Theme.muted; elide: Text.ElideRight; Layout.fillWidth: true
                        font { family: Theme.font; pixelSize: Theme.fs(13) } }
            }
            AbstractButton {
                objectName: "bookSmaller"
                implicitWidth: 40; implicitHeight: 40
                onClicked: root.fontSize = Math.max(12, root.fontSize - 2)
                Accessible.name: "smaller text"
                background: Rectangle { radius: 6; color: parent.hovered ? Theme.hover : "transparent"; border { width: 1; color: Theme.borderSoft } }
                contentItem: Label { text: "A−"; color: Theme.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font { family: Theme.font; pixelSize: Theme.fs(14) } }
            }
            AbstractButton {
                objectName: "bookLarger"
                implicitWidth: 40; implicitHeight: 40
                onClicked: root.fontSize = Math.min(40, root.fontSize + 2)
                Accessible.name: "larger text"
                background: Rectangle { radius: 6; color: parent.hovered ? Theme.hover : "transparent"; border { width: 1; color: Theme.borderSoft } }
                contentItem: Label { text: "A+"; color: Theme.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font { family: Theme.font; pixelSize: Theme.fs(18) } }
            }
            AbstractButton {
                objectName: "bookContents"
                implicitWidth: 100; implicitHeight: 40
                onClicked: toc.open()
                Accessible.name: "contents"
                background: Rectangle { radius: 6; color: parent.hovered ? Theme.hover : "transparent"; border { width: 1; color: Theme.borderSoft } }
                contentItem: Label { text: "contents"; color: Theme.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font { family: Theme.font; pixelSize: Theme.fs(14) } }
            }
            AbstractButton {
                objectName: "bookClose"
                implicitWidth: 44; implicitHeight: 44
                onClicked: root.close()
                Accessible.name: "close"
                background: Rectangle { radius: 22; color: parent.hovered ? Theme.hover : "transparent"; border { width: 2; color: Theme.border } }
                contentItem: Label { text: "×"; color: Theme.text; font.pixelSize: Theme.fs(26); horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            }
        }

        Flickable {
            id: flick
            anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: footer.top; topMargin: 10 }
            clip: true
            contentWidth: width
            contentHeight: page.height + 80
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            onContentHeightChanged: if (root.opened) Qt.callLater(root.restore)

            Text {
                id: page
                objectName: "bookText"
                x: Math.max(24, (flick.width - width) / 2)
                y: 20
                width: Math.min(760, flick.width - 48)
                text: hop.bookChapterHtml
                textFormat: Text.RichText
                wrapMode: Text.WordWrap
                color: Theme.text
                linkColor: hop.accent
                font { family: Theme.font; pixelSize: root.fontSize }
                lineHeight: 1.05
            }
        }

        RowLayout {
            id: footer
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 28; rightMargin: 28; bottomMargin: 14 }
            AbstractButton {
                objectName: "bookPrev"
                enabled: hop.bookChapterIndex > 0
                implicitWidth: 120; implicitHeight: 40
                onClicked: hop.goToBookChapter(hop.bookChapterIndex - 1)
                background: Rectangle { radius: 6; color: parent.hovered ? Theme.hover : "transparent"; border { width: 1; color: Theme.borderSoft } }
                contentItem: Label { text: "‹ previous"; opacity: parent.enabled ? 1 : 0.4; color: Theme.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font { family: Theme.font; pixelSize: Theme.fs(14) } }
            }
            Item { Layout.fillWidth: true }
            Label { text: Math.round(((hop.bookChapterIndex + root.fraction) / Math.max(1, hop.bookChapterCount)) * 100) + "%"
                    color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(14) } }
            Item { Layout.fillWidth: true }
            AbstractButton {
                objectName: "bookNext"
                enabled: hop.bookChapterIndex < hop.bookChapterCount - 1
                implicitWidth: 120; implicitHeight: 40
                onClicked: { hop.saveBookProgress(root.fraction); hop.goToBookChapter(hop.bookChapterIndex + 1) }
                background: Rectangle { radius: 6; color: parent.hovered ? Theme.hover : "transparent"; border { width: 1; color: Theme.borderSoft } }
                contentItem: Label { text: "next ›"; opacity: parent.enabled ? 1 : 0.4; color: Theme.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font { family: Theme.font; pixelSize: Theme.fs(14) } }
            }
        }

        Popup {
            id: toc
            parent: Overlay.overlay
            anchors.centerIn: parent
            width: Math.min(560, root.width - 60)
            height: Math.min(520, root.height - 80)
            modal: true
            padding: 8
            background: Rectangle { color: Theme.panel; border { width: 1; color: Theme.border } }
            contentItem: ListView {
                id: tocList
                objectName: "bookToc"
                clip: true
                model: toc.opened ? hop.bookChapterTitles : []
                currentIndex: hop.bookChapterIndex
                ScrollBar.vertical: ScrollBar {}
                delegate: AbstractButton {
                    required property int index
                    required property string modelData
                    width: tocList.width; height: 40
                    hoverEnabled: true
                    onClicked: { const i = index; hop.goToBookChapter(i); toc.close() }  // change chapter first: closing empties the list and destroys this delegate
                    background: Rectangle { color: hovered ? Theme.hover : "transparent" }
                    contentItem: Label {
                        text: (index + 1) + ".  " + modelData
                        color: index === hop.bookChapterIndex ? hop.accent : Theme.text
                        elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
                        font { family: Theme.font; pixelSize: Theme.fs(16) }
                    }
                }
            }
        }
    }
}
