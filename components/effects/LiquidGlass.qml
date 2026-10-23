pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import Caelestia.Config
import qs.components.images
import qs.services

Item {
    id: root

    property real radius: 0
    property real morph: 0
    property bool animated: true
    property color tint: "transparent"
    property real tintStrength: 0.55
    property real glassOpacity: 0.9
    property real chroma: 0.5
    property real refraction: 0.05
    property real bevelDepth: 0.12
    property real bevelWidth: 0.08
    property real frost: 1
    property real specularIntensity: 1
    property string wallpaper: GlobalConfig.background.wallpaperEnabled ? Wallpapers.current : ""

    readonly property var screen: (QsWindow.window as QsWindow)?.screen
    readonly property int screenWidth: screen?.width ?? 0
    readonly property int screenHeight: screen?.height ?? 0
    readonly property point screenPos: {
        // Touch every ancestor position so this binding also follows the
        // notification while it slides and while the popup list scrolls.
        let p = root.mapToItem(null, 0, 0);
        let item = root;
        while (item) {
            const dx = item.x;
            const dy = item.y;
            p.x += dx - dx;
            p.y += dy - dy;
            item = item.parent;
        }
        return p;
    }
    readonly property int pad: Math.ceil(Math.max(20, Math.min(root.width, root.height) * 0.1))
    readonly property rect captureRect: Qt.rect(root.screenPos.x - root.pad, root.screenPos.y - root.pad, root.width + root.pad * 2, root.height + root.pad * 2)

    property real iTime: 0

    Item {
        anchors.fill: parent
        clip: true

        CachingImage {
            id: wallpaperImage

            x: -root.screenPos.x
            y: -root.screenPos.y
            width: root.screenWidth
            height: root.screenHeight
            path: root.wallpaper
        }

        ShaderEffectSource {
            id: wallpaperTexture

            width: root.width
            height: root.height
            sourceItem: wallpaperImage
            sourceRect: root.captureRect
            hideSource: true
            live: true
            recursive: false
        }
    }

    ShaderEffect {
        id: shader

        anchors.fill: parent

        property variant source: wallpaperTexture
        property vector2d resolution: Qt.vector2d(root.width, root.height)
        property real radius: root.radius
        property real morph: root.morph
        property real iTime: root.iTime
        property real refraction: root.refraction
        property real bevelDepth: root.bevelDepth
        property real bevelWidth: root.bevelWidth
        property real frost: root.frost
        property real specularIntensity: root.specularIntensity
        property real pad: root.pad
        property color tint: root.tint
        property real tintStrength: root.tintStrength
        property real glassOpacity: root.glassOpacity
        property real chroma: root.chroma

        fragmentShader: "shaders/liquidglass.frag.qsb"
    }

    FrameAnimation {
        running: root.visible && root.animated && root.width > 0 && root.height > 0 && !GameMode.enabled
        onTriggered: root.iTime += frameTime
    }
}
