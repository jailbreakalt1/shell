pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import Caelestia.Config
import qs.components
import qs.components.effects
import qs.services
import qs.utils

// An icon that never falls back to Quickshell's purple/black missing-icon
// texture. Missing theme icons, broken image files and empty icon names all
// resolve to a distinct fallback: a deterministic monogram for apps, or a
// Material symbol for everything else.
Item {
    id: root

    property string icon
    property string fallbackIcon: "help"
    property string fallbackText
    property color fallbackColour: "transparent"
    property color fallbackContentColour: "transparent"
    property color iconColour: Colours.palette.m3onSurface
    property bool colorise: icon.endsWith("-symbolic")
    property bool circular: false
    property bool showFallbackBackground: true
    property bool fillModeCrop: false
    property real implicitSize: 32

    implicitWidth: implicitSize
    implicitHeight: implicitSize

    readonly property string resolvedSource: Icons.resolveSource(icon)
    readonly property int imageStatus: loader.item?.status ?? Image.Null
    readonly property bool hasImage: resolvedSource !== "" && imageStatus === Image.Ready
    readonly property bool showFallback: !hasImage
    readonly property string monogram: Icons.getMonogram(fallbackText)

    readonly property int seed: {
        const value = `${fallbackText || ""}${icon || ""}${fallbackIcon || ""}`;
        let hash = 0;
        for (let i = 0; i < value.length; i++)
            hash = ((hash << 5) - hash + value.charCodeAt(i)) | 0;
        return Math.abs(hash);
    }

    readonly property var fallbackPalette: [
        Colours.palette.term4 || Colours.palette.m3primary,
        Colours.palette.term5 || Colours.palette.m3tertiary,
        Colours.palette.term6 || Colours.palette.m3secondary,
        Colours.palette.term2 || Colours.palette.m3primary,
        Colours.palette.term9 || Colours.palette.m3error,
        Colours.palette.term12 || Colours.palette.m3tertiary,
        Colours.palette.term13 || Colours.palette.m3primary,
        Colours.palette.term14 || Colours.palette.m3secondary
    ]

    readonly property color accentColour: fallbackPalette[seed % fallbackPalette.length]
    readonly property color generatedFallbackColour: Colours.light ? Qt.rgba(0.55 + accentColour.r * 0.45, 0.55 + accentColour.g * 0.45, 0.55 + accentColour.b * 0.45, 1) : Qt.rgba(accentColour.r * 0.28, accentColour.g * 0.28, accentColour.b * 0.28, 1)
    readonly property color generatedContentColour: Colours.light ? Qt.darker(accentColour, 1.8) : Qt.lighter(accentColour, 1.25)
    readonly property color effectiveFallbackColour: fallbackColour.a > 0 ? fallbackColour : generatedFallbackColour
    readonly property color effectiveContentColour: fallbackContentColour.a > 0 ? fallbackContentColour : generatedContentColour
    readonly property real fallbackRadius: circular ? width / 2 : Math.min(width, height) * 0.22

    Rectangle {
        anchors.fill: parent

        visible: root.showFallback && root.showFallbackBackground
        radius: root.fallbackRadius
        gradient: Gradient {
            GradientStop {
                position: 0
                color: Qt.lighter(root.effectiveFallbackColour, 1.2)
            }
            GradientStop {
                position: 1
                color: Qt.darker(root.effectiveFallbackColour, 1.25)
            }
        }
    }

    StyledText {
        anchors.centerIn: parent

        visible: root.showFallback && root.monogram !== ""
        text: root.monogram
        color: root.effectiveContentColour
        font: Tokens.font.title.builders.large.scale(Math.max(0.35, root.width / 72)).build()
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    MaterialIcon {
        anchors.centerIn: parent

        visible: root.showFallback && root.monogram === ""
        text: root.fallbackIcon || "help"
        color: root.effectiveContentColour
        fontStyle: Tokens.font.icon.size(Math.max(8, root.width * 0.52)).build()
    }

    Loader {
        id: loader

        anchors.fill: parent
        active: root.resolvedSource !== ""
        sourceComponent: root.colorise ? colouredImage : plainImage
    }

    Component {
        id: plainImage

        Image {
            anchors.fill: parent

            source: root.resolvedSource
            fillMode: root.fillModeCrop ? Image.PreserveAspectCrop : Image.PreserveAspectFit
            asynchronous: true
            smooth: true
            visible: status === Image.Ready
            sourceSize.width: Math.max(1, Math.round(root.width * ((QsWindow.window as QsWindow)?.devicePixelRatio ?? 1)))
            sourceSize.height: Math.max(1, Math.round(root.height * ((QsWindow.window as QsWindow)?.devicePixelRatio ?? 1)))
        }
    }

    Component {
        id: colouredImage

        ColouredIcon {
            anchors.fill: parent

            source: root.resolvedSource
            colour: root.iconColour
        }
    }
}
