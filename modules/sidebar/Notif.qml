pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Quickshell.Services.Notifications
import Caelestia.Config
import Caelestia.I18n
import qs.components
import qs.services
import qs.utils

StyledRect {
    id: root

    required property NotifData modelData
    required property Props props
    required property bool expanded
    required property ScreenState screenState

    readonly property StyledText body: (expandedContent.item as ExpandedBody)?.body ?? null
    readonly property bool critical: modelData?.urgency === NotificationUrgency.Critical
    readonly property string displayBody: Strings.notificationBody(String(modelData?.body ?? ""))
    readonly property bool displayBodyHasMarkup: Strings.hasMarkup(displayBody)
    readonly property real nonAnimHeight: expanded ? summary.implicitHeight + expandedContent.implicitHeight + expandedContent.anchors.topMargin + Tokens.padding.medium * 2 : summaryHeightMetrics.height

    implicitHeight: nonAnimHeight

    radius: Tokens.rounding.medium
    color: !expanded ? "transparent" : root.critical ? Qt.alpha(Colours.disabledRed, 0.1) : Qt.alpha(Colours.enabledGreenMuted, 0.06)
    border.width: expanded ? 1 : 0
    border.color: root.critical ? Qt.alpha(Colours.disabledRed, 0.45) : Qt.alpha(Colours.enabledGreenMuted, 0.22)

    FontMetrics {
        id: bodyMetrics

        font: Tokens.font.body.small
    }

    state: expanded ? "expanded" : ""

    states: State {
        name: "expanded"

        PropertyChanges {
            summary.anchors.margins: root.Tokens.padding.medium
            dummySummary.anchors.margins: root.Tokens.padding.medium
            compactBody.anchors.margins: root.Tokens.padding.medium
            timeStr.anchors.margins: root.Tokens.padding.medium
            expandedContent.anchors.margins: root.Tokens.padding.medium
            summary.width: root.width - root.Tokens.padding.medium * 2 - timeStr.implicitWidth - root.Tokens.spacing.small
            summary.maximumLineCount: Number.MAX_SAFE_INTEGER
        }
    }

    transitions: Transition {
        Anim {
            properties: "margins,width,maximumLineCount"
        }
    }

    TextMetrics {
        id: summaryHeightMetrics

        font: summary.font
        text: " " // Use this height to prevent weird characters from changing the line height
    }

    StyledText {
        id: summary

        anchors.top: parent.top
        anchors.left: parent.left

        width: parent.width
        text: root.modelData?.summary ?? ""
        color: root.critical ? Colours.disabledRed : Colours.enabledGreen
        lineHeightMode: Text.FixedHeight
        lineHeight: bodyMetrics.lineSpacing
        elide: Text.ElideRight
        wrapMode: Text.WordWrap
        maximumLineCount: 1
    }

    StyledText {
        id: dummySummary

        anchors.top: parent.top
        anchors.left: parent.left

        visible: false
        text: root.modelData?.summary ?? ""
    }

    WrappedLoader {
        id: compactBody

        shouldBeActive: !root.expanded
        anchors.top: parent.top
        anchors.left: dummySummary.right
        anchors.right: parent.right
        anchors.leftMargin: Tokens.spacing.small

        sourceComponent: StyledText {
            text: Strings.notificationPreview(String(root.modelData?.body ?? "")).replace(/\n/g, " ")
            color: root.critical ? Colours.disabledRed : Colours.enabledGreenMuted
            lineHeightMode: Text.FixedHeight
            lineHeight: bodyMetrics.lineSpacing
            elide: Text.ElideRight
        }
    }

    WrappedLoader {
        id: timeStr

        shouldBeActive: root.expanded
        anchors.top: parent.top
        anchors.right: parent.right

        sourceComponent: StyledText {
            animate: true
            text: root.modelData?.timeStr ?? ""
            color: root.critical ? Qt.alpha(Colours.disabledRed, 0.75) : Colours.enabledGreenMuted
            font: Tokens.font.body.small
            lineHeightMode: Text.FixedHeight
            lineHeight: bodyMetrics.lineSpacing
        }
    }

    WrappedLoader {
        id: expandedContent

        shouldBeActive: root.expanded
        anchors.top: summary.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: Tokens.spacing.extraSmall

        sourceComponent: ExpandedBody {}
    }

    Behavior on implicitHeight {
        Anim {}
    }

    component ExpandedBody: ColumnLayout {
        readonly property alias body: bodyText

        spacing: Tokens.spacing.medium

        StyledText {
            id: bodyText

            Layout.fillWidth: true
            textFormat: root.displayBodyHasMarkup ? Text.MarkdownText : Text.PlainText
            text: root.displayBody || Tr.tr("No body here! :/")
            color: root.critical ? Qt.alpha(Colours.disabledRed, 0.85) : Colours.enabledGreenMuted
            linkColor: root.critical ? Colours.disabledRed : Colours.enabledGreen
            lineHeightMode: Text.FixedHeight
            lineHeight: bodyMetrics.lineSpacing
            wrapMode: Text.WordWrap

            onLinkActivated: link => {
                Qt.openUrlExternally(link);
                root.screenState.sidebar = false;
            }
        }

        NotifActionList {
            notif: root.modelData
        }
    }

    component WrappedLoader: Loader {
        id: comp

        required property bool shouldBeActive

        active: false
        opacity: 0

        // Makes the loader load on the same frame shouldBeActive becomes true, which ensures size is set
        states: State {
            name: "active"
            when: comp.shouldBeActive

            PropertyChanges {
                comp.opacity: 1
                comp.active: true
            }
        }

        transitions: [
            Transition {
                from: ""
                to: "active"

                SequentialAnimation {
                    PropertyAction {
                        property: "active"
                    }
                    Anim {
                        type: Anim.DefaultEffects
                        property: "opacity"
                    }
                }
            },
            Transition {
                from: "active"
                to: ""

                SequentialAnimation {
                    Anim {
                        type: Anim.DefaultEffects
                        property: "opacity"
                    }
                    PropertyAction {
                        property: "active"
                    }
                }
            }
        ]
    }
}
