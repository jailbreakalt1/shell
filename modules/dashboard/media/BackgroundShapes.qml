pragma ComponentBehavior: Bound

import QtQuick
import M3Shapes
import qs.components
import qs.services

Item {
    id: root

    readonly property list<int> shapePool: [MaterialShape.Circle, MaterialShape.Cookie4Sided, MaterialShape.Cookie6Sided, MaterialShape.Cookie7Sided, MaterialShape.Cookie9Sided, MaterialShape.Cookie12Sided, MaterialShape.Sunny, MaterialShape.VerySunny, MaterialShape.SoftBurst, MaterialShape.Pentagon, MaterialShape.Gem, MaterialShape.Arch, MaterialShape.Arrow, MaterialShape.Pill, MaterialShape.Triangle, MaterialShape.Fan, MaterialShape.Oval]

    property int count: 14
    property real minSize: 36
    property real maxSize: 124
    property real minSpeed: 4
    property real maxSpeed: 18
    property real minRotSpeed: -12
    property real maxRotSpeed: 12
    property real minMotionScale: 0.25
    property real maxMotionScale: 2
    property color accentColour: Colours.mediaNeutral
    property list<real> lightOpacities: [0.34, 0.34, 0.08, 0.2]
    property list<real> darkOpacities: [0.16, 0.16, 0.04, 0.16]

    function rand(min: real, max: real): real {
        return min + Math.random() * (max - min);
    }

    function signedRand(min: real, max: real): real {
        return rand(min, max) * (Math.random() < 0.5 ? -1 : 1);
    }

    clip: true
    Component.onCompleted: shapes.model = count

    Repeater {
        id: shapes

        DriftingShape {}
    }

    FrameAnimation {
        running: root.visible && root.width > 0 && root.height > 0 && (Players.active?.isPlaying ?? false)
        onTriggered: {
            const dt = frameTime;
            const values = Audio.cava.values;
            const barCount = values?.length ?? 0;
            let level = 0;

            if (barCount > 0) {
                for (let i = 0; i < barCount; i++)
                    level += values[i];
                level /= barCount;
            }

            for (let i = 0; i < shapes.count; i++) {
                const s = shapes.itemAt(i) as DriftingShape;
                if (!s)
                    continue;

                const bar = barCount > 0 ? Math.max(0, Math.min(1, values[i % barCount])) : 0;
                const energy = Math.max(0, Math.min(1, level * 0.4 + bar * 0.6));
                const smooth = Math.min(1, dt * 10);
                s.energy += (energy - s.energy) * smooth;

                const motion = root.minMotionScale + s.energy * (root.maxMotionScale - root.minMotionScale);
                s.x += s.vx * dt * motion;
                s.y += s.vy * dt * motion;
                s.rotation += s.vr * dt * motion;

                if (s.x + s.width < 0)
                    s.x = root.width;
                else if (s.x > root.width)
                    s.x = -s.width;

                if (s.y + s.height < 0)
                    s.y = root.height;
                else if (s.y > root.height)
                    s.y = -s.height;
            }
        }
    }

    component DriftingShape: MaterialShape {
        id: shapeItem

        required property int index

        property real vx: root.signedRand(root.minSpeed, root.maxSpeed)
        property real vy: root.signedRand(root.minSpeed, root.maxSpeed)
        property real vr: root.rand(root.minRotSpeed, root.maxRotSpeed)
        property real energy: 0
        readonly property int colourIdx: Math.floor(Math.random() * 4)

        implicitSize: root.minSize + (index / root.count) * (root.maxSize - root.minSize)
        shape: root.shapePool[Math.floor(Math.random() * root.shapePool.length)]
        color: [Qt.lighter(root.accentColour, 1.3), root.accentColour, Qt.lighter(root.accentColour, 1.65), Qt.darker(root.accentColour, 1.2)][colourIdx]
        opacity: Colours.light ? root.lightOpacities[colourIdx] : root.darkOpacities[colourIdx]
        rotation: root.rand(0, 360)

        Component.onCompleted: {
            x = root.rand(0, root.width - implicitSize);
            y = root.rand(0, root.height - implicitSize);
        }

        Behavior on color {
            CAnim {}
        }

        Behavior on opacity {
            Anim {
                type: Anim.SlowEffects
            }
        }
    }
}
