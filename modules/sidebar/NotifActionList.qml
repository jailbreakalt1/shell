pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Widgets
import Quickshell.Services.Notifications
import Caelestia.Config
import qs.components
import qs.components.containers
import qs.components.effects
import qs.components.images
import qs.services

Item {
    id: root

    required property NotifData notif

    readonly property bool critical: notif?.urgency === NotificationUrgency.Critical
    readonly property color accentColour: critical ? Colours.disabledRed : Colours.enabledGreen

    Layout.fillWidth: true
    implicitHeight: flickable.contentHeight

    FontMetrics {
        id: bodyMetrics

        font: Tokens.font.body.small
    }

    layer.enabled: true
    layer.smooth: true
    layer.effect: Mask {
        maskSource: gradientMask
    }

    Item {
        id: gradientMask

        anchors.fill: parent
        layer.enabled: true
        visible: false

        Rectangle {
            anchors.fill: parent

            gradient: Gradient {
                orientation: Gradient.Horizontal

                GradientStop {
                    position: 0
                    color: Qt.rgba(0, 0, 0, 0)
                }
                GradientStop {
                    position: 0.1
                    color: Qt.rgba(0, 0, 0, 1)
                }
                GradientStop {
                    position: 0.9
                    color: Qt.rgba(0, 0, 0, 1)
                }
                GradientStop {
                    position: 1
                    color: Qt.rgba(0, 0, 0, 0)
                }
            }
        }

        Rectangle {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left

            implicitWidth: parent.width / 2
            opacity: flickable.contentX > 0 ? 0 : 1

            Behavior on opacity {
                Anim {
                    type: Anim.DefaultEffects
                }
            }
        }

        Rectangle {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: parent.right

            implicitWidth: parent.width / 2
            opacity: flickable.contentX < flickable.contentWidth - parent.width ? 0 : 1

            Behavior on opacity {
                Anim {
                    type: Anim.DefaultEffects
                }
            }
        }
    }

    StyledFlickable {
        id: flickable

        anchors.fill: parent
        contentWidth: Math.max(width, actionList.implicitWidth)
        contentHeight: actionList.implicitHeight

        RowLayout {
            id: actionList

            anchors.fill: parent
            spacing: Tokens.spacing.small

            Repeater {
                model: [
                    {
                        isClose: true
                    },
                    ...(root.notif?.actions ?? []),
                    {
                        isCopy: true
                    }
                ]

                StyledRect {
                    id: action

                    required property var modelData

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    implicitWidth: actionInner.implicitWidth + Tokens.padding.medium * 2
                    implicitHeight: actionInner.implicitHeight + Tokens.padding.small

                    Layout.preferredWidth: implicitWidth + (actionStateLayer.pressed ? Tokens.padding.large : 0)
                    radius: actionStateLayer.pressed ? Tokens.rounding.medium / 2 : Tokens.rounding.medium
                    color: root.critical ? Qt.alpha(Colours.disabledRed, 0.12) : Qt.alpha(Colours.enabledGreenMuted, 0.1)

                    Timer {
                        id: copyTimer

                        interval: 3000
                        onTriggered: actionInner.item.text = "content_copy"
                    }

                    StateLayer {
                        id: actionStateLayer

                        color: root.accentColour

                        onClicked: {
                            if (action.modelData.isClose) {
                                root.notif.close();
                            } else if (action.modelData.isCopy) {
                                Quickshell.clipboardText = root.notif.body;
                                actionInner.item.text = "inventory";
                                copyTimer.start();
                            } else if (action.modelData.invoke) {
                                action.modelData.invoke();
                            } else if (!root.notif.resident) {
                                root.notif.close();
                            }
                        }
                    }

                    Loader {
                        id: actionInner

                        anchors.centerIn: parent
                        sourceComponent: action.modelData.isClose || action.modelData.isCopy ? iconBtn : root.notif?.hasActionIcons ? iconComp : textComp
                    }

                    Component {
                        id: iconBtn

                        MaterialIcon {
                            animate: action.modelData.isCopy ?? false
                            text: action.modelData.isCopy ? "content_copy" : "close"
                            color: root.accentColour
                        }
                    }

                    Component {
                        id: iconComp

                        SmartIcon {
                            implicitSize: Math.round(Tokens.font.icon.small.pointSize * 1.25)
                            icon: action.modelData.identifier
                            fallbackIcon: "touch_app"
                            iconColour: root.accentColour
                            showFallbackBackground: false
                        }
                    }

                    Component {
                        id: textComp

                        StyledText {
                            text: action.modelData.text
                            color: root.accentColour
                            lineHeightMode: Text.FixedHeight
                            lineHeight: bodyMetrics.lineSpacing
                        }
                    }

                    Behavior on Layout.preferredWidth {
                        Anim {
                            type: Anim.FastSpatial
                        }
                    }

                    Behavior on radius {
                        Anim {
                            type: Anim.FastSpatial
                        }
                    }
                }
            }
        }
    }
}
