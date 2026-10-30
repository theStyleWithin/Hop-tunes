// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// A movie's page (collection › videos › click a tile). Nothing plays until Play is pressed. The boxes
// are empty for the user to fill in; only the file facts (size, resolution, codec, length) are known.
Item {
    id: root
    readonly property var d: hop.videoDetail
    readonly property var cast: d.cast || []

    FileDialog {
        id: posterDialog
        title: "choose a poster"
        nameFilters: ["pictures (*.png *.jpg *.jpeg)"]
        onAccepted: hop.setVideoThumb(root.d.row, selectedFile)
    }
    FileDialog {
        id: castDialog
        property int index: -1
        title: "choose a photo"
        nameFilters: ["pictures (*.png *.jpg *.jpeg)"]
        onAccepted: hop.setCastPhoto(index, selectedFile)
    }

    PhoneVideoDetail {   // phones
        visible: Theme.compact
        anchors.fill: parent
        onChoosePoster: posterDialog.open()
        onChoosePhoto: function (i) { castDialog.index = i; castDialog.open() }
    }

    Flickable {
        id: flick
        visible: !Theme.compact
        anchors.fill: parent
        contentWidth: width
        contentHeight: page.implicitHeight + 40
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: page
            width: flick.width - 24
            spacing: 18

            AbstractButton {
                id: back
                objectName: "videoDetailBack"
                hoverEnabled: true
                implicitHeight: 28
                onClicked: hop.closeVideoDetail()
                Accessible.name: "back to videos"
                contentItem: Label {
                    text: "‹  videos"
                    color: back.hovered ? Theme.accent : Theme.muted
                    font { family: Theme.font; pixelSize: Theme.fs(17) }
                }
                background: Item {}
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 32

                // ---- poster ----
                ColumnLayout {
                    Layout.alignment: Qt.AlignTop
                    spacing: 6
                    Rectangle {
                        Layout.preferredWidth: 220; Layout.preferredHeight: 330
                        color: Theme.panel
                        clip: true
                        Image {
                            anchors.fill: parent
                            visible: !!root.d.thumb
                            source: root.d.thumb || ""
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            cache: false
                        }
                        Label {
                            anchors.centerIn: parent
                            visible: !root.d.thumb
                            text: "no poster"
                            color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(16) }
                        }
                    }
                    RowLayout {
                        spacing: 14
                        visible: !hop.guest   // a poster is a file kept in the profile; guest keeps nothing
                        AbstractButton {
                            id: posterBtn
                            objectName: "videoDetailPoster"
                            hoverEnabled: true
                            implicitHeight: 20
                            onClicked: posterDialog.open()
                            contentItem: Label { text: "change poster"; color: posterBtn.hovered ? Theme.accent : Theme.muted
                                                 font { family: Theme.font; pixelSize: Theme.fs(13); underline: posterBtn.hovered } }
                            background: Item {}
                        }
                        AbstractButton {
                            id: posterReset
                            visible: !!root.d.customThumb
                            hoverEnabled: true
                            implicitHeight: 20
                            onClicked: hop.clearVideoThumb(root.d.row)
                            contentItem: Label { text: "reset"; color: posterReset.hovered ? Theme.accent : Theme.muted
                                                 font { family: Theme.font; pixelSize: Theme.fs(13); underline: posterReset.hovered } }
                            background: Item {}
                        }
                    }
                }

                // ---- the details ----
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spacing: 12

                    DetailField {
                        objectName: "videoDetailTitle"
                        key: "title"; light: true; pixel: 34
                        Layout.fillWidth: true
                        placeholderText: root.d.fileName || "title"
                    }

                    RowLayout {
                        spacing: 10
                        DetailField { objectName: "videoDetailYear"; key: "year"; placeholderText: "year"; Layout.preferredWidth: 90 }
                        DetailField { objectName: "videoDetailRating"; key: "rating"; placeholderText: "rating (e.g. PG-13)"; Layout.preferredWidth: 170 }
                        DetailField { objectName: "videoDetailScore"; key: "score"; placeholderText: "score"; Layout.preferredWidth: 90 }
                        Label {
                            visible: !!root.d.length
                            text: root.d.length || ""
                            color: Theme.muted; font { family: Theme.font; pixelSize: Theme.fs(15) }
                            Layout.leftMargin: 6
                        }
                    }

                    // facts already known from the file itself
                    Label {
                        text: [root.d.resolution, root.d.codec, root.d.hdr ? "HDR" : "", root.d.container, root.d.size].filter(function (s) { return !!s }).join("  ·  ")
                        color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(13) }
                    }

                    RowLayout {
                        spacing: 12
                        Layout.topMargin: 4
                        Button {
                            id: playBtn
                            objectName: "videoDetailPlay"
                            text: root.d.resumeText ? "▶  resume from " + root.d.resumeText : "▶  play"
                            onClicked: hop.playVideo(root.d.row)
                            contentItem: Label { text: playBtn.text; color: Theme.ink; font { family: Theme.font; pixelSize: Theme.fs(17); weight: Font.Medium }
                                                 horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { implicitHeight: 46; implicitWidth: 200; color: playBtn.hovered ? Qt.lighter(Theme.accent, 1.08) : Theme.accent }
                        }
                        Button {
                            id: watchedBtn
                            objectName: "videoDetailWatched"
                            text: root.d.played ? "watched ✓" : "mark watched"
                            onClicked: hop.toggleVideoWatched()
                            contentItem: Label { text: watchedBtn.text; color: root.d.played ? Theme.accent : Theme.text
                                                 font { family: Theme.font; pixelSize: Theme.fs(15) }
                                                 horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { implicitHeight: 46; implicitWidth: 150; color: "transparent"; border { width: 2; color: Theme.border } }
                        }
                    }

                    DetailField {
                        objectName: "videoDetailTagline"
                        key: "tagline"; placeholderText: "tagline"
                        Layout.fillWidth: true; Layout.topMargin: 6
                    }
                    DetailArea {
                        objectName: "videoDetailOverview"
                        key: "overview"; placeholderText: "what is it about?"
                        Layout.fillWidth: true
                    }

                    GridLayout {
                        columns: 2
                        columnSpacing: 18; rowSpacing: 8
                        Layout.fillWidth: true
                        Layout.topMargin: 6
                        Repeater {
                            model: [["genre", "genre"], ["director", "director"], ["writers", "writers"],
                                    ["studio", "studio"], ["tags", "tags, separated by commas"]]
                            delegate: RowLayout {
                                required property var modelData
                                Layout.columnSpan: 2
                                Layout.fillWidth: true
                                spacing: 18
                                Label { text: modelData[0]; color: Theme.muted; Layout.preferredWidth: 90
                                        font { family: Theme.font; pixelSize: Theme.fs(15) } }
                                DetailField {
                                    objectName: "videoDetail_" + modelData[0]
                                    key: modelData[0]; placeholderText: modelData[1]
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }
            }

            // ---- cast & crew ----
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 10
                Label { text: "cast & crew"; color: Theme.text; font { family: Theme.font; pixelSize: Theme.fs(22) } }
                Item { Layout.fillWidth: true }
                AbstractButton {
                    id: addPerson
                    objectName: "videoDetailAddPerson"
                    hoverEnabled: true
                    implicitHeight: 26
                    onClicked: hop.addCastMember()
                    contentItem: Label { text: "+ add person"; color: addPerson.hovered ? Theme.accent : Theme.muted
                                         font { family: Theme.font; pixelSize: Theme.fs(15); underline: addPerson.hovered } }
                    background: Item {}
                }
            }
            Label {
                visible: root.cast.length === 0
                text: "nobody added yet. add the people you want to remember, with a role and a photo if you like."
                color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(14) }
            }
            Flow {
                Layout.fillWidth: true
                spacing: 18
                // The model is just the count, so saving a name does not rebuild the cards (which would
                // drop the focus from the box being typed in); each card reads its person by index.
                Repeater {
                    model: root.cast.length
                    delegate: ColumnLayout {
                        id: card
                        required property int index
                        readonly property var person: root.cast[index] || ({})
                        width: 150
                        spacing: 6
                        Rectangle {
                            Layout.preferredWidth: 150; Layout.preferredHeight: 180
                            color: Theme.panel
                            clip: true
                            Image {
                                anchors.fill: parent
                                visible: !!card.person.photo
                                source: card.person.photo || ""
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                cache: false
                            }
                            Label {
                                anchors.centerIn: parent
                                visible: !card.person.photo
                                text: (card.person.name || "?").charAt(0).toUpperCase()
                                color: Theme.dim; font { family: Theme.font; pixelSize: Theme.fs(54); weight: Font.Light }
                            }
                        }
                        TextField {
                            id: nameField
                            objectName: "castName_" + card.index
                            Layout.fillWidth: true
                            placeholderText: "name"
                            text: card.person.name
                            selectByMouse: true; color: Theme.text; placeholderTextColor: Theme.dim
                            font { family: Theme.font; pixelSize: Theme.fs(15) }
                            leftPadding: 8; rightPadding: 8; topPadding: 5; bottomPadding: 5
                            onEditingFinished: hop.setCastMember(card.index, text, roleField.text)
                            background: Rectangle { color: nameField.activeFocus ? Theme.panel : "transparent"; radius: 3
                                                    border { width: 1; color: nameField.activeFocus ? Theme.accent : Theme.borderSoft } }
                        }
                        TextField {
                            id: roleField
                            objectName: "castRole_" + card.index
                            Layout.fillWidth: true
                            placeholderText: "role"
                            text: card.person.role
                            selectByMouse: true; color: Theme.muted; placeholderTextColor: Theme.dim
                            font { family: Theme.font; pixelSize: Theme.fs(13) }
                            leftPadding: 8; rightPadding: 8; topPadding: 4; bottomPadding: 4
                            onEditingFinished: hop.setCastMember(card.index, nameField.text, text)
                            background: Rectangle { color: roleField.activeFocus ? Theme.panel : "transparent"; radius: 3
                                                    border { width: 1; color: roleField.activeFocus ? Theme.accent : Theme.borderSoft } }
                        }
                        RowLayout {
                            spacing: 12
                            AbstractButton {
                                id: photoBtn
                                visible: !hop.guest
                                hoverEnabled: true; implicitHeight: 18
                                onClicked: { castDialog.index = card.index; castDialog.open() }
                                contentItem: Label { text: "photo"; color: photoBtn.hovered ? Theme.accent : Theme.muted
                                                     font { family: Theme.font; pixelSize: Theme.fs(12); underline: photoBtn.hovered } }
                                background: Item {}
                            }
                            AbstractButton {
                                id: removeBtn
                                objectName: "castRemove_" + card.index
                                hoverEnabled: true; implicitHeight: 18
                                onClicked: hop.removeCastMember(card.index)
                                contentItem: Label { text: "remove"; color: removeBtn.hovered ? Theme.danger : Theme.muted
                                                     font { family: Theme.font; pixelSize: Theme.fs(12); underline: removeBtn.hovered } }
                                background: Item {}
                            }
                        }
                    }
                }
            }
            Item { Layout.preferredHeight: 20 }
        }
    }
}
