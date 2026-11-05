pragma ComponentBehavior: Bound

import QtQuick
import Quickshell.Services.SystemTray
import Caelestia.Config
import qs.components.effects
import qs.components.images
import qs.services
import qs.utils

MouseArea {
    id: root

    required property SystemTrayItem modelData

    acceptedButtons: Qt.LeftButton | Qt.RightButton
    implicitWidth: Tokens.font.body.small.pointSize * 2
    implicitHeight: Tokens.font.body.small.pointSize * 2

    onClicked: event => {
        if (event.button === Qt.LeftButton)
            modelData.activate();
        else
            modelData.secondaryActivate();
    }

    SmartIcon {
        id: icon

        anchors.fill: parent
        icon: Icons.getTrayIcon(root.modelData.id, root.modelData.icon)
        fallbackIcon: Icons.getTrayFallbackIcon(root.modelData.id, root.modelData.title)
        iconColour: Colours.palette.m3secondary
        colorise: Config.bar.tray.recolour
        showFallbackBackground: false
    }
}
