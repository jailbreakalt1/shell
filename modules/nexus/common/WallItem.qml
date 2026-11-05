pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Quickshell
import Caelestia.Config
import qs.components
import qs.components.controls
import qs.services

Item {
    id: root

    property alias source: img.source
    property alias text: label.text
    property alias radius: imgWrapper.radius
    property alias imgHeight: imgWrapper.implicitHeight
    property bool fillLabel: true

    signal clicked

    Layout.fillWidth: true
    implicitHeight: layout.implicitHeight

    ColumnLayout {
        id: layout

        anchors.fill: parent
        spacing: Tokens.spacing.small

        StyledClippingRect {
            id: imgWrapper

            Layout.fillWidth: true
            implicitHeight: width
            radius: Tokens.rounding.largeIncreased
            color: Colours.tPalette.m3surfaceContainer

            readonly property int colourSeed: {
                let hash = 0;
                const value = label.text || "";
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
                Colours.palette.term12 || Colours.palette.m3tertiary
            ]
            readonly property color fallbackColour: fallbackPalette[colourSeed % fallbackPalette.length]

            Loader {
                anchors.centerIn: parent

                opacity: img.status === Image.Ready ? 0 : 1
                active: img.source.toString() !== "" && (img.status === Image.Loading || img.status === Image.Null)

                sourceComponent: StyledRect {
                    implicitWidth: loadingIndicator.implicitSize + Tokens.padding.large * 2
                    implicitHeight: loadingIndicator.implicitSize + Tokens.padding.large * 2

                    color: Colours.palette.m3primaryContainer
                    radius: Tokens.rounding.full

                    LoadingIndicator {
                        id: loadingIndicator

                        anchors.centerIn: parent
                        containsIcon: true
                        implicitSize: Math.min(imgWrapper.width, imgWrapper.height) * 0.3
                    }
                }

                Behavior on opacity {
                    Anim {
                        type: Anim.DefaultEffects
                    }
                }
            }

            Rectangle {
                anchors.fill: parent

                visible: !img.source.toString() || img.status === Image.Error
                gradient: Gradient {
                    GradientStop {
                        position: 0
                        color: Qt.rgba(imgWrapper.fallbackColour.r * 0.22, imgWrapper.fallbackColour.g * 0.22, imgWrapper.fallbackColour.b * 0.22, 1)
                    }
                    GradientStop {
                        position: 1
                        color: Colours.palette.m3surfaceContainer
                    }
                }

                MaterialIcon {
                    anchors.centerIn: parent

                    text: "image"
                    color: Qt.lighter(imgWrapper.fallbackColour, 1.2)
                    fontStyle: Tokens.font.icon.size(parent.width * 0.25).build()
                }
            }

            Image {
                id: img

                anchors.fill: parent
                asynchronous: true
                fillMode: Image.PreserveAspectCrop
                sourceSize: {
                    const dpr = (QsWindow.window as QsWindow)?.devicePixelRatio ?? 1;
                    return Qt.size(width * dpr, height * dpr);
                }
                retainWhileLoading: true
                opacity: status === Image.Ready ? 1 : 0

                Behavior on opacity {
                    Anim {
                        type: Anim.SlowEffects
                    }
                }
            }
        }

        StyledText {
            id: label

            Layout.bottomMargin: Tokens.padding.small
            Layout.fillWidth: true
            color: Colours.palette.m3onSurfaceVariant
            font: Tokens.font.label.builders.small.weight(Font.Medium).build()
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }
    }

    StateLayer {
        anchors.bottomMargin: root.fillLabel ? 0 : layout.implicitHeight - imgWrapper.implicitHeight
        onClicked: root.clicked()
    }
}
