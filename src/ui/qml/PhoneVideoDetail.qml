// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Phones: a film's page as in the mockups: the poster beside the title and the play button, then the boxes
// to fill in, then the cast and crew as a list. It reads and writes the same things as the desktop page.
Item {
    id: root
    readonly property var d: hop.videoDetail
    readonly property var cast: d.cast || []
    signal choosePoster()
    signal choosePhoto(int index)

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: page.implicitHeight + 24
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: page
            x: Theme.pad; y: 4
            width: flick.width - 2 * Theme.pad
            spacing: 12

            AbstractButton {
                objectName: "phoneDetailBack"
                implicitHeight: 44
                onClicked: hop.closeVideoDetail()
                Accessible.name: "back to videos"
                background: Item {}
                contentItem: Label { text: "‹  videos"; color: Theme.muted; verticalAlignment: Text.AlignVCenter; font { family: Theme.font; pixelSize: 18 } }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 16
                ColumnLayout {
                    Layout.alignment: Qt.AlignTop
                    spacing: 6
                    Rectangle {
                        Layout.preferredWidth: 130; Layout.preferredHeight: 195
                        radius: 6; color: Theme.panel; clip: true
                        Image {
                            anchors.fill: parent
                            visible: !!root.d.thumb
                            source: root.d.thumb || ""
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true; cache: false
                        }
                        Label {
                            anchors { left: parent.left; bottom: parent.bottom; margins: 8 }
                            visible: !root.d.thumb
                            text: "no poster"; color: Theme.muted; font { family: Theme.font; pixelSize: 13 }
                        }
                    }
                    RowLayout {
                        visible: !hop.guest
                        spacing: 12
                        AbstractButton {
                            objectName: "phoneDetailPoster"
                            implicitHeight: 28
                            onClicked: root.choosePoster()
                            background: Item {}
                            contentItem: Label { text: "change poster"; color: Theme.accent; verticalAlignment: Text.AlignVCenter; font { family: Theme.font; pixelSize: 14 } }
                        }
                        AbstractButton {
                            visible: !!root.d.customThumb
                            implicitHeight: 28
                            onClicked: hop.clearVideoThumb(root.d.row)
                            background: Item {}
                            contentItem: Label { text: "reset"; color: Theme.muted; verticalAlignment: Text.AlignVCenter; font { family: Theme.font; pixelSize: 14 } }
                        }
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; Layout.alignment: Qt.AlignTop; Layout.minimumWidth: 0
                    spacing: 10
                    Label {
                        text: (root.d.meta && root.d.meta.title) || root.d.fileName || ""
                        color: Theme.strong; wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.minimumWidth: 0
                        font { family: Theme.font; pixelSize: 24; weight: Font.Light }
                    }
                    Label {
                        text: [hop.videoCategory, root.d.length, root.d.resolution, root.d.hdr ? "HDR" : ""].filter(function (s) { return !!s }).join(" · ")
                        color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.minimumWidth: 0
                        font { family: Theme.font; pixelSize: 14 }
                    }
                    Button {
                        id: playBtn
                        objectName: "videoDetailPlay"
                        Layout.fillWidth: true
                        text: root.d.resumeText ? "▶  resume " + root.d.resumeText : "▶  play"
                        onClicked: hop.playVideo(root.d.row)
                        contentItem: Label { text: playBtn.text; color: Theme.ink; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                             font { family: Theme.font; pixelSize: 17; weight: Font.Medium } }
                        background: Rectangle { implicitHeight: 46; radius: 12; color: Theme.accent }
                    }
                    Button {
                        id: watchedBtn
                        objectName: "videoDetailWatched"
                        Layout.fillWidth: true
                        text: root.d.played ? "watched ✓" : "mark watched"
                        onClicked: hop.toggleVideoWatched()
                        contentItem: Label { text: watchedBtn.text; color: root.d.played ? Theme.accent : Theme.text2
                                             horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                             font { family: Theme.font; pixelSize: 16 } }
                        background: Rectangle { implicitHeight: 46; radius: 12; color: "transparent"; border { width: 1; color: Theme.borderSoft } }
                    }
                }
            }

            DetailField { objectName: "videoDetailTitle"; key: "title"; pixel: 16; Layout.fillWidth: true; Layout.minimumWidth: 0
                          placeholderText: root.d.fileName || "title" }
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                DetailField { objectName: "videoDetailYear"; key: "year"; pixel: 16; placeholderText: "year"; Layout.preferredWidth: 80; Layout.minimumWidth: 0 }
                DetailField { objectName: "videoDetailRating"; key: "rating"; pixel: 16; placeholderText: "rating"; Layout.fillWidth: true; Layout.minimumWidth: 0 }
                DetailField { objectName: "videoDetailScore"; key: "score"; pixel: 16; placeholderText: "score"; Layout.preferredWidth: 70; Layout.minimumWidth: 0 }
            }
            DetailField { objectName: "videoDetailTagline"; key: "tagline"; pixel: 16; placeholderText: "tagline"; Layout.fillWidth: true; Layout.minimumWidth: 0 }
            DetailArea { objectName: "videoDetailOverview"; key: "overview"; placeholderText: "what is it about?"; Layout.fillWidth: true; Layout.minimumWidth: 0 }
            GridLayout {
                columns: 2; columnSpacing: 8; rowSpacing: 10
                Layout.fillWidth: true
                Repeater {
                    model: [["genre", "genre"], ["director", "director"], ["writers", "writers"], ["studio", "studio"]]
                    delegate: DetailField {
                        required property var modelData
                        objectName: "videoDetail_" + modelData[0]
                        key: modelData[0]; pixel: 16; placeholderText: modelData[1]
                        Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 100
                    }
                }
                DetailField { objectName: "videoDetail_tags"; key: "tags"; pixel: 16; placeholderText: "tags, separated by commas"
                              Layout.columnSpan: 2; Layout.fillWidth: true; Layout.minimumWidth: 0 }
            }

            RowLayout {
                Layout.fillWidth: true; Layout.topMargin: 10
                Label { text: "cast & crew"; color: Theme.strong; font { family: Theme.font; pixelSize: 24; weight: Font.Light } Layout.fillWidth: true }
                AbstractButton {
                    objectName: "videoDetailAddPerson"
                    implicitHeight: 34; implicitWidth: addLbl.implicitWidth + 28
                    onClicked: hop.addCastMember()
                    Accessible.name: "add person"
                    background: Rectangle { radius: 17; color: Theme.accent }
                    contentItem: Label { id: addLbl; text: "+ add person"; color: Theme.ink; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                         font { family: Theme.font; pixelSize: 15 } }
                }
            }
            Label {
                visible: root.cast.length === 0
                text: "nobody added yet. add the people you want to remember, with a role and a photo if you like."
                color: Theme.dim; wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.minimumWidth: 0
                font { family: Theme.font; pixelSize: 14 }
            }
            Repeater {
                model: root.cast.length   // a count, so saving a name does not rebuild the row being typed in
                delegate: RowLayout {
                    id: person
                    required property int index
                    readonly property var p: root.cast[index] || ({})
                    Layout.fillWidth: true; Layout.minimumWidth: 0
                    spacing: 12
                    ProfileAvatar {
                        profile: ({ initial: String(person.p.name || "?").charAt(0).toUpperCase(), accent: Theme.panel, avatar: person.p.photo || "" })
                        size: 46; round: true; cornerColor: Theme.bg
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; Layout.minimumWidth: 0
                        spacing: 0
                        TextField {
                            id: nameField
                            objectName: "castName_" + person.index
                            Layout.fillWidth: true; Layout.minimumWidth: 0
                            placeholderText: "name"; text: person.p.name
                            selectByMouse: true; color: Theme.text; placeholderTextColor: Theme.dim
                            font { family: Theme.font; pixelSize: 17 }
                            leftPadding: 0; rightPadding: 0; topPadding: 2; bottomPadding: 2
                            onEditingFinished: hop.setCastMember(person.index, text, roleField.text)
                            background: Item {}
                        }
                        TextField {
                            id: roleField
                            objectName: "castRole_" + person.index
                            Layout.fillWidth: true; Layout.minimumWidth: 0
                            placeholderText: "role"; text: person.p.role
                            selectByMouse: true; color: Theme.muted; placeholderTextColor: Theme.dim
                            font { family: Theme.font; pixelSize: 14 }
                            leftPadding: 0; rightPadding: 0; topPadding: 0; bottomPadding: 2
                            onEditingFinished: hop.setCastMember(person.index, nameField.text, text)
                            background: Item {}
                        }
                    }
                    AbstractButton {
                        visible: !hop.guest
                        implicitHeight: 44; implicitWidth: 52
                        onClicked: root.choosePhoto(person.index)
                        Accessible.name: "photo"
                        background: Item {}
                        contentItem: Label { text: "photo"; color: Theme.muted; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font { family: Theme.font; pixelSize: 14 } }
                    }
                    AbstractButton {
                        objectName: "castRemove_" + person.index
                        implicitHeight: 44; implicitWidth: 64
                        onClicked: hop.removeCastMember(person.index)
                        Accessible.name: "remove"
                        background: Item {}
                        contentItem: Label { text: "remove"; color: Theme.muted; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font { family: Theme.font; pixelSize: 14 } }
                    }
                }
            }
            Label {
                text: "your notes, poster and people are saved on this device only"
                color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.topMargin: 10
                font { family: Theme.font; pixelSize: 14 }
            }
            Item { Layout.preferredHeight: 12 }
        }
    }
}
