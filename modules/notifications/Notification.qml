pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes
import Quickshell
import Quickshell.Services.Notifications
import Caelestia.Components
import Caelestia.Config
import qs.components
import qs.components.controls
import qs.components.images
import qs.services
import qs.utils

StyledRect {
    id: root

    required property NotifData modelData
    readonly property bool hasImage: modelData.image.length > 0
    readonly property bool hasAppIcon: modelData.appIcon.length > 0
    readonly property bool critical: modelData.urgency === NotificationUrgency.Critical
    readonly property bool low: modelData.urgency === NotificationUrgency.Low
    readonly property color accentColour: critical ? Colours.disabledRed : Colours.enabledGreen
    readonly property color mutedColour: critical ? Colours.disabledRed : Colours.enabledGreenMuted
    readonly property string displayBody: Strings.notificationBody(modelData.body)
    readonly property int bodyTextFormat: /[<*_`#\[\]]/.test(displayBody) ? Text.MarkdownText : Text.PlainText
    readonly property bool bodyHasMarkup: Strings.hasMarkup(modelData.body)
    // TextMetrics elides raw text without understanding textFormat, so the
    // preview must be markup-free. The expanded body drops the source-site
    // paragraph Chromium prepends, but keeps links in the message.
    readonly property string bodyPreviewText: bodyHasMarkup ? Strings.notificationPreview(modelData.body) : modelData.body
    readonly property int bodyPreviewTextFormat: bodyHasMarkup ? Text.PlainText : bodyTextFormat
    readonly property int nonAnimHeight: summary.implicitHeight + (root.expanded ? Tokens.spacing.extraSmall * 2 + appName.height + body.height + actions.height + actions.anchors.topMargin : bodyPreview.height) + inner.anchors.margins * 2
    property bool expanded: Config.notifs.openExpanded

    color: Colours.deepBlack
    radius: Tokens.rounding.large
    border.width: 1
    border.color: root.critical ? Qt.alpha(Colours.disabledRed, 0.55) : Qt.alpha(Colours.enabledGreenMuted, 0.32)

    implicitHeight: inner.implicitHeight

    // Some emoji fonts (notably the 3D Noto variant) report a huge line
    // height even though the glyph itself is normal sized. Pinning the line
    // height keeps message cards from growing to several hundred pixels.
    FontMetrics {
        id: bodyMetrics

        font: Tokens.font.body.small
    }

    FontMetrics {
        id: labelMetrics

        font: Tokens.font.label.medium
    }

    x: implicitWidth
    Component.onCompleted: {
        x = 0;
        modelData.lock(this);
    }
    Component.onDestruction: modelData.unlock(this)

    Behavior on x {
        Anim {
            easing: Tokens.anim.emphasizedDecel
        }
    }

    MouseArea {
        property int startY

        anchors.fill: parent
        hoverEnabled: true
        cursorShape: root.expanded && body.hoveredLink ? Qt.PointingHandCursor : pressed ? Qt.ClosedHandCursor : undefined
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton
        preventStealing: true

        onEntered: root.modelData.timer.stop()
        onExited: {
            if (!pressed)
                root.modelData.timer.start();
        }

        drag.target: parent
        drag.axis: Drag.XAxis

        onPressed: event => {
            root.modelData.timer.stop();
            startY = event.y;
            if (event.button === Qt.MiddleButton)
                root.modelData.close();
        }
        onReleased: event => {
            if (!containsMouse)
                root.modelData.timer.start();

            if (Math.abs(root.x) < root.implicitWidth * Config.notifs.clearThreshold)
                root.x = 0;
            else
                root.modelData.popup = false;
        }
        onPositionChanged: event => {
            if (pressed) {
                const diffY = event.y - startY;
                if (Math.abs(diffY) > Config.notifs.expandThreshold)
                    root.expanded = diffY > 0;
            }
        }
        onClicked: event => {
            if (!GlobalConfig.notifs.actionOnClick || event.button !== Qt.LeftButton)
                return;

            const actions = root.modelData.actions;
            if (actions.length === 1)
                actions[0].invoke();
        }

        Item {
            id: inner

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Tokens.padding.medium

            implicitHeight: root.nonAnimHeight

            Behavior on implicitHeight {
                Anim {}
            }

            Loader {
                id: image

                asynchronous: true
                active: root.hasImage

                anchors.left: parent.left
                anchors.top: parent.top
                width: TokenConfig.sizes.notifs.image
                height: TokenConfig.sizes.notifs.image
                visible: root.hasImage || root.hasAppIcon

                sourceComponent: StyledClippingRect {
                    radius: Tokens.rounding.full
                    color: root.critical ? Qt.alpha(Colours.disabledRed, 0.22) : root.low ? Qt.alpha(Colours.enabledGreenMuted, 0.1) : Qt.alpha(Colours.enabledGreen, 0.16)
                    implicitWidth: TokenConfig.sizes.notifs.image
                    implicitHeight: TokenConfig.sizes.notifs.image

                    SmartIcon {
                        anchors.fill: parent

                        icon: root.modelData.image
                        fallbackIcon: "broken_image"
                        fallbackContentColour: root.critical ? Colours.disabledRed : root.low ? Colours.enabledGreenMuted : Colours.enabledGreen
                        fillModeCrop: true
                        showFallbackBackground: false
                    }
                }
            }

            Loader {
                id: appIcon

                asynchronous: true
                active: root.hasAppIcon || !root.hasImage

                anchors.horizontalCenter: root.hasImage ? undefined : image.horizontalCenter
                anchors.verticalCenter: root.hasImage ? undefined : image.verticalCenter
                anchors.right: root.hasImage ? image.right : undefined
                anchors.bottom: root.hasImage ? image.bottom : undefined

                sourceComponent: StyledRect {
                    radius: Tokens.rounding.full
                    color: root.critical ? Qt.alpha(Colours.disabledRed, 0.22) : root.low ? Qt.alpha(Colours.enabledGreenMuted, 0.1) : Qt.alpha(Colours.enabledGreen, 0.16)
                    implicitWidth: root.hasImage ? Tokens.sizes.notifs.badge : TokenConfig.sizes.notifs.image
                    implicitHeight: root.hasImage ? Tokens.sizes.notifs.badge : TokenConfig.sizes.notifs.image

                    SmartIcon {
                        anchors.centerIn: parent

                        implicitSize: Math.round(parent.width * 0.6)
                        icon: root.modelData.appIcon
                        fallbackIcon: Icons.getNotifAppIcon(root.modelData.appIcon, root.modelData.appName, root.modelData.summary, root.modelData.urgency)
                        fallbackContentColour: root.critical ? Colours.disabledRed : root.low ? Colours.enabledGreenMuted : Colours.enabledGreen
                        iconColour: fallbackContentColour
                        colorise: root.modelData.appIcon.endsWith("-symbolic")
                        showFallbackBackground: false
                    }
                }
            }

            Shape {
                id: progressIndicator

                anchors.centerIn: appIcon
                width: appIcon.implicitWidth + progressShape.strokeWidth * 2
                height: appIcon.implicitHeight + progressShape.strokeWidth * 2
                preferredRendererType: Shape.CurveRenderer

                ShapePath {
                    id: progressShape

                    capStyle: ShapePath.RoundCap
                    fillColor: "transparent"
                    strokeWidth: 2
                    strokeColor: root.accentColour

                    PathAngleArc {
                        id: progressArc

                        radiusX: progressIndicator.width / 2 - root.Tokens.padding.extraSmall / 2
                        centerX: progressIndicator.width / 2
                        radiusY: progressIndicator.height / 2 - root.Tokens.padding.extraSmall / 2
                        centerY: progressIndicator.height / 2

                        startAngle: -90
                        sweepAngle: ((root.modelData.hints.value ?? 0) / 100) * 360

                        Behavior on sweepAngle {
                            Anim {
                                easing: Tokens.anim.emphasizedDecel
                            }
                        }
                    }
                }
            }

            StyledText {
                id: appName

                anchors.top: parent.top
                anchors.left: image.right
                anchors.leftMargin: Tokens.spacing.medium

                animate: true
                text: appNameMetrics.elidedText
                maximumLineCount: 1
                color: root.mutedColour
                font: Tokens.font.label.medium
                lineHeightMode: Text.FixedHeight
                lineHeight: labelMetrics.lineSpacing

                opacity: root.expanded ? 1 : 0

                Behavior on opacity {
                    Anim {
                        type: Anim.DefaultEffects
                    }
                }
            }

            TextMetrics {
                id: appNameMetrics

                text: root.modelData.appName
                font: appName.font
                elide: Text.ElideRight
                elideWidth: expandBtn.x - time.width - timeSep.width - summary.x - root.Tokens.spacing.small * 3
            }

            StyledText {
                id: summary

                anchors.top: parent.top
                anchors.left: image.right
                anchors.leftMargin: Tokens.spacing.medium

                animate: true
                text: summaryMetrics.elidedText
                maximumLineCount: 1
                height: implicitHeight
                color: root.critical ? Colours.disabledRed : Colours.enabledGreen
                lineHeightMode: Text.FixedHeight
                lineHeight: bodyMetrics.lineSpacing

                states: State {
                    name: "expanded"
                    when: root.expanded

                    PropertyChanges {
                        summary.maximumLineCount: undefined
                        summary.anchors.topMargin: root.Tokens.spacing.extraSmall
                        bodyPreview.anchors.topMargin: root.Tokens.spacing.extraSmall
                        body.anchors.topMargin: root.Tokens.spacing.extraSmall
                    }

                    AnchorChanges {
                        target: summary
                        anchors.top: appName.bottom
                    }
                }

                transitions: Transition {
                    PropertyAction {
                        target: summary
                        property: "maximumLineCount"
                    }
                    Anim {
                        property: "topMargin"
                    }
                    AnchorAnim {}
                }

                Behavior on height {
                    Anim {}
                }
            }

            TextMetrics {
                id: summaryMetrics

                text: root.modelData.summary
                font: summary.font
                elide: Text.ElideRight
                elideWidth: expandBtn.x - time.width - timeSep.width - summary.x - root.Tokens.spacing.small * 3
            }

            StyledText {
                id: timeSep

                anchors.top: parent.top
                anchors.left: summary.right
                anchors.leftMargin: Tokens.spacing.small

                text: "•"
                color: root.mutedColour
                font: Tokens.font.body.small
                lineHeightMode: Text.FixedHeight
                lineHeight: bodyMetrics.lineSpacing

                states: State {
                    name: "expanded"
                    when: root.expanded

                    AnchorChanges {
                        target: timeSep
                        anchors.left: appName.right
                    }
                }

                transitions: Transition {
                    AnchorAnim {}
                }
            }

            StyledText {
                id: time

                anchors.top: parent.top
                anchors.left: timeSep.right
                anchors.leftMargin: Tokens.spacing.small

                animate: true
                horizontalAlignment: Text.AlignLeft
                text: root.modelData.timeStr
                color: root.mutedColour
                font: Tokens.font.body.small
                lineHeightMode: Text.FixedHeight
                lineHeight: bodyMetrics.lineSpacing
            }

            Item {
                id: expandBtn

                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: -Tokens.padding.extraSmall

                implicitWidth: expandIcon.implicitHeight
                implicitHeight: expandIcon.implicitHeight

                StateLayer {
                    radius: Tokens.rounding.full
                    color: root.accentColour
                    onClicked: root.expanded = !root.expanded
                }

                MaterialIcon {
                    id: expandIcon

                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: root.expanded ? -1 : 1
                    text: "expand_more"
                    color: root.mutedColour
                    fontStyle: Tokens.font.icon.medium
                    rotation: root.expanded ? 180 : 0

                    Behavior on anchors.verticalCenterOffset {
                        Anim {}
                    }

                    Behavior on rotation {
                        Anim {}
                    }
                }
            }

            StyledText {
                id: bodyPreview

                anchors.left: summary.left
                anchors.right: expandBtn.left
                anchors.top: summary.bottom
                anchors.rightMargin: Tokens.spacing.small

                animate: true
                textFormat: root.bodyPreviewTextFormat
                text: root.bodyPreviewText
                color: root.mutedColour
                font: Tokens.font.body.small
                lineHeightMode: Text.FixedHeight
                lineHeight: bodyMetrics.lineSpacing
                elide: Text.ElideRight
                maximumLineCount: 1
                height: text ? implicitHeight : 0

                opacity: root.expanded ? 0 : 1

                Behavior on opacity {
                    Anim {
                        type: Anim.DefaultEffects
                    }
                }
            }

            StyledText {
                id: body

                anchors.left: summary.left
                anchors.right: expandBtn.left
                anchors.top: summary.bottom
                anchors.rightMargin: Tokens.spacing.small

                animate: true
                textFormat: root.bodyTextFormat
                text: root.displayBody
                color: root.mutedColour
                linkColor: root.accentColour
                font: Tokens.font.body.small
                lineHeightMode: Text.FixedHeight
                lineHeight: bodyMetrics.lineSpacing
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                height: text ? implicitHeight : 0

                onLinkActivated: link => {
                    if (!root.expanded)
                        return;

                    Qt.openUrlExternally(link);
                    root.modelData.popup = false;
                }

                opacity: root.expanded ? 1 : 0

                Behavior on opacity {
                    Anim {
                        type: Anim.DefaultEffects
                    }
                }
            }

            ButtonRow {
                id: actions

                anchors.left: body.left
                anchors.right: body.right
                anchors.top: body.bottom
                anchors.topMargin: Tokens.spacing.small

                spacing: Tokens.spacing.extraSmall
                opacity: root.expanded ? 1 : 0

                Behavior on opacity {
                    Anim {
                        type: Anim.DefaultEffects
                    }
                }

                IconButton {
                    isRound: true
                    shapeMorph: true
                    fillWidth: root.modelData.actions.length === 0
                    inactiveColour: root.critical ? Qt.alpha(Colours.disabledRed, 0.14) : Qt.alpha(Colours.enabledGreenMuted, 0.12)
                    inactiveOnColour: root.accentColour
                    icon: "close"
                    padding: Tokens.padding.extraSmall
                    onClicked: root.modelData.close()
                }

                Repeater {
                    model: root.modelData.actions

                    TextButton {
                        required property var modelData

                        isRound: true
                        shapeMorph: true
                        fillWidth: true
                        inactiveColour: root.critical ? Qt.alpha(Colours.disabledRed, 0.14) : Qt.alpha(Colours.enabledGreenMuted, 0.12)
                        inactiveOnColour: root.accentColour
                        text: modelData.text
                        onClicked: modelData.invoke()

                        label.horizontalAlignment: Text.AlignHCenter
                        label.anchors.left: left
                        label.anchors.right: right
                        label.anchors.verticalCenter: verticalCenter
                        label.anchors.centerIn: undefined
                        label.anchors.margins: Tokens.padding.medium
                        label.elide: Text.ElideRight
                        label.lineHeightMode: Text.FixedHeight
                        label.lineHeight: bodyMetrics.lineSpacing
                    }
                }

                IconButton {
                    isRound: true
                    shapeMorph: true
                    fillWidth: root.modelData.actions.length === 0
                    inactiveColour: root.critical ? Qt.alpha(Colours.disabledRed, 0.14) : Qt.alpha(Colours.enabledGreenMuted, 0.12)
                    inactiveOnColour: root.accentColour
                    icon: copyTimer.running ? "inventory" : "content_copy"
                    padding: Tokens.padding.extraSmall
                    onClicked: {
                        Quickshell.clipboardText = root.modelData.body;
                        copyTimer.restart();
                    }
                    label.animate: true

                    Timer {
                        id: copyTimer

                        interval: 3000
                    }
                }
            }
        }
    }
}
