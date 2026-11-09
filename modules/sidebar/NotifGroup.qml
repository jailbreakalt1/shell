pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Services.Notifications
import Caelestia.Config
import qs.components
import qs.components.effects
import qs.components.images
import qs.services
import qs.utils

StyledRect {
    id: root

    required property string modelData
    required property Props props
    required property Flickable container
    required property ScreenState screenState

    readonly property list<var> notifs: Notifs.list.filter(n => n.appName === modelData)
    readonly property list<var> activeNotifs: notifs.filter(n => !n.closed)
    readonly property int notifCount: activeNotifs.length
    readonly property string image: activeNotifs.find(n => n.image.length > 0)?.image ?? ""
    readonly property string appIcon: activeNotifs.find(n => n.appIcon.length > 0)?.appIcon ?? ""
    readonly property int urgency: {
        if (activeNotifs.find(n => n.urgency === NotificationUrgency.Critical))
            return NotificationUrgency.Critical;
        if (activeNotifs.find(n => n.urgency === NotificationUrgency.Normal))
            return NotificationUrgency.Normal;
        return NotificationUrgency.Low;
    }
    readonly property bool critical: urgency === NotificationUrgency.Critical
    readonly property bool low: urgency === NotificationUrgency.Low
    readonly property color accentColour: critical ? Colours.disabledRed : low ? Colours.enabledGreenMuted : Colours.enabledGreen
    readonly property color mutedColour: critical ? Qt.alpha(Colours.disabledRed, 0.8) : Colours.enabledGreenMuted
    readonly property color iconBackground: critical ? Qt.alpha(Colours.disabledRed, 0.2) : Qt.alpha(accentColour, low ? 0.08 : 0.14)

    readonly property int nonAnimHeight: {
        const headerHeight = header.implicitHeight + (root.expanded ? Math.round(Tokens.spacing.extraSmall) : 0);
        const columnHeight = headerHeight + notifList.layoutHeight;
        return Math.round(Math.max(TokenConfig.sizes.notifs.image, columnHeight) + Tokens.padding.medium * 2);
    }
    readonly property bool expanded: props.expandedNotifs.includes(modelData)

    function toggleExpand(expand: bool): void {
        if (expand) {
            if (!expanded)
                props.expandedNotifs.push(modelData);
        } else if (expanded) {
            props.expandedNotifs.splice(props.expandedNotifs.indexOf(modelData), 1);
        }
    }

    Component.onDestruction: {
        if (notifCount === 0 && expanded)
            props.expandedNotifs.splice(props.expandedNotifs.indexOf(modelData), 1);
    }

    anchors.left: parent?.left
    anchors.right: parent?.right
    implicitHeight: nonAnimHeight

    clip: true
    radius: Tokens.rounding.large
    color: Colours.deepBlack
    border.width: 1
    border.color: root.critical ? Qt.alpha(Colours.disabledRed, 0.35) : Qt.alpha(Colours.enabledGreenMuted, 0.18)

    FontMetrics {
        id: bodyMetrics

        font: Tokens.font.body.small
    }

    Behavior on implicitHeight {
        Anim {}
    }

    RowLayout {
        id: content

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Tokens.padding.medium

        spacing: Tokens.spacing.medium

        Item {
            Layout.alignment: Qt.AlignLeft | Qt.AlignTop
            implicitWidth: TokenConfig.sizes.notifs.image
            implicitHeight: TokenConfig.sizes.notifs.image

            Component {
                id: imageComp

                SmartIcon {
                    implicitSize: TokenConfig.sizes.notifs.image
                    icon: root.image
                    fallbackIcon: "broken_image"
                    fallbackContentColour: root.accentColour
                    fillModeCrop: true
                    showFallbackBackground: false
                }
            }

            Component {
                id: appIconComp

                SmartIcon {
                    implicitSize: Math.round(TokenConfig.sizes.notifs.image * 0.6)
                    icon: root.appIcon
                    fallbackIcon: Icons.getNotifAppIcon(root.appIcon, root.modelData, root.activeNotifs[0]?.summary, root.urgency)
                    fallbackContentColour: root.accentColour
                    iconColour: fallbackContentColour
                    colorise: root.appIcon.endsWith("-symbolic")
                    showFallbackBackground: false
                }
            }

            Component {
                id: materialIconComp

                MaterialIcon {
                    text: Icons.getNotifIcon(root.activeNotifs[0]?.summary, root.urgency)
                    color: root.accentColour
                    fontStyle: Tokens.font.icon.medium
                }
            }

            StyledClippingRect {
                anchors.fill: parent
                color: root.iconBackground
                radius: Tokens.rounding.full

                Loader {
                    asynchronous: true
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: sourceComponent === materialIconComp ? 1 : 0
                    sourceComponent: root.image ? imageComp : root.appIcon ? appIconComp : materialIconComp
                }
            }

            Loader {
                asynchronous: true
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                active: root.appIcon && root.image

                sourceComponent: StyledRect {
                    implicitWidth: Tokens.sizes.notifs.badge
                    implicitHeight: Tokens.sizes.notifs.badge

                    color: root.iconBackground
                    radius: Tokens.rounding.full

                    SmartIcon {
                        anchors.centerIn: parent
                        implicitSize: Math.round(Tokens.sizes.notifs.badge * 0.6)
                        icon: root.appIcon
                        fallbackIcon: Icons.getNotifAppIcon(root.appIcon, root.modelData, root.activeNotifs[0]?.summary, root.urgency)
                        fallbackContentColour: root.accentColour
                        iconColour: fallbackContentColour
                        colorise: root.appIcon.endsWith("-symbolic")
                        showFallbackBackground: false
                    }
                }
            }
        }

        Column {
            id: column

            Layout.fillWidth: true
            spacing: root.expanded ? Math.round(Tokens.spacing.extraSmall) : 0

            Behavior on spacing {
                Anim {}
            }

            RowLayout {
                id: header

                anchors.left: parent.left
                anchors.right: parent.right
                spacing: Tokens.spacing.small

                StyledText {
                    Layout.fillWidth: true
                    text: root.modelData
                    color: root.mutedColour
                    font: Tokens.font.body.small
                    lineHeightMode: Text.FixedHeight
                    lineHeight: bodyMetrics.lineSpacing
                    elide: Text.ElideRight
                }

                StyledText {
                    animate: true
                    text: root.activeNotifs[0]?.timeStr ?? ""
                    color: root.mutedColour
                    font: Tokens.font.body.small
                    lineHeightMode: Text.FixedHeight
                    lineHeight: bodyMetrics.lineSpacing
                }

                StyledRect {
                    implicitWidth: expandBtn.implicitWidth + Tokens.padding.large
                    implicitHeight: groupCount.implicitHeight + Tokens.padding.extraSmall

                    color: root.critical ? Qt.alpha(Colours.disabledRed, 0.16) : Qt.alpha(Colours.enabledGreenMuted, 0.12)
                    radius: Tokens.rounding.full

                    StateLayer {
                        color: root.accentColour
                        onClicked: root.toggleExpand(!root.expanded)
                    }

                    RowLayout {
                        id: expandBtn

                        anchors.centerIn: parent
                        spacing: Tokens.spacing.extraSmall

                        StyledText {
                            id: groupCount

                            Layout.leftMargin: Tokens.padding.extraSmall / 2
                            animate: true
                            text: root.notifCount
                            color: root.accentColour
                            font: Tokens.font.body.small
                            lineHeightMode: Text.FixedHeight
                            lineHeight: bodyMetrics.lineSpacing
                        }

                        MaterialIcon {
                            Layout.rightMargin: -Tokens.padding.extraSmall / 2
                            text: "expand_more"
                            color: root.accentColour
                            rotation: root.expanded ? 180 : 0
                            Layout.topMargin: root.expanded ? -Math.floor(Tokens.padding.extraSmall) : 0

                            Behavior on rotation {
                                Anim {}
                            }

                            Behavior on Layout.topMargin {
                                Anim {}
                            }
                        }
                    }
                }
            }

            NotifGroupList {
                id: notifList

                props: root.props
                notifs: root.notifs
                expanded: root.expanded
                container: root.container
                screenState: root.screenState
                onRequestToggleExpand: expand => root.toggleExpand(expand)
            }
        }
    }
}
