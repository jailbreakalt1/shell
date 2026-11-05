pragma ComponentBehavior: Bound

import QtQuick
import Quickshell.Widgets
import Caelestia.Config
import qs.components
import qs.services
import qs.utils

Item {
    id: root

    // Easier (and more efficient) to ignore it than to check type and cast
    readonly property int status: loader.item?.status ?? Image.Null // qmllint disable missing-property
    readonly property real actualSize: Math.min(width, height)
    readonly property string resolvedSource: Icons.resolveSource(root.source.toString())
    readonly property bool isThemeIcon: resolvedSource.startsWith("image://icon/")
    property real implicitSize
    property url source
    property string fallbackIcon: "draft"
    property color fallbackColour: Colours.palette.m3onSurfaceVariant

    implicitWidth: implicitSize
    implicitHeight: implicitSize

    MaterialIcon {
        anchors.centerIn: parent

        visible: root.status !== Image.Ready
        text: root.fallbackIcon || "draft"
        color: root.fallbackColour
        fontStyle: Tokens.font.icon.size(Math.max(8, root.implicitSize * 0.55)).build()
    }

    Loader {
        id: loader

        asynchronous: true
        anchors.fill: parent
        active: root.resolvedSource !== ""
        sourceComponent: root.isThemeIcon ? iconImage : cachingImage
    }

    Component {
        id: cachingImage

        CachingImage {
            path: Paths.toLocalFile(root.resolvedSource)
            fillMode: Image.PreserveAspectFit
        }
    }

    Component {
        id: iconImage

        IconImage {
            source: root.resolvedSource
            asynchronous: true
        }
    }
}
