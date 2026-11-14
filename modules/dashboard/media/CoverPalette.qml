import QtQuick
import qs.services

Item {
    id: root

    readonly property string artUrl: Players.getArtUrl(Players.active)

    // Most dominant usable colour, used for the focused lyric
    property color dominantColour: "transparent"
    // Next dominant distinct colour, used for every other lyric line
    property color secondaryColour: "transparent"

    visible: false
    implicitWidth: 64
    implicitHeight: 64

    onArtUrlChanged: {
        dominantColour = "transparent";
        secondaryColour = "transparent";
        refresh();
    }

    function refresh(): void {
        if (!canvas.available)
            return;

        if (artUrl.length)
            canvas.loadImage(artUrl);
    }

    function hueDistance(a: real, b: real): real {
        const d = Math.abs(a - b);
        return Math.min(d, 1 - d);
    }

    function distinct(a: var, b: var): bool {
        const aGray = a.saturation < 0.1;
        const bGray = b.saturation < 0.1;

        if (aGray && bGray)
            return Math.abs(a.lightness - b.lightness) > 0.25;
        if (aGray !== bGray)
            return true;
        return hueDistance(a.hue, b.hue) > 0.083;
    }

    function focusColour(c: var): color {
        if (c.saturation < 0.1)
            return Qt.hsla(0, 0, Math.max(0.62, Math.min(0.78, c.lightness)), 1);

        return Colours.mediaAccent(c.colour);
    }

    function secondaryColourFor(c: var): color {
        if (c.saturation < 0.1)
            return Qt.hsla(0, 0, Math.max(0.55, Math.min(0.7, c.lightness)), 1);

        return Qt.hsla(c.hue, Math.max(0.35, Math.min(0.85, c.saturation)), Math.max(0.48, Math.min(0.66, c.lightness)), 1);
    }

    function analyse(canvasItem: var): void {
        const ctx = canvasItem.getContext("2d");
        ctx.clearRect(0, 0, canvasItem.width, canvasItem.height);

        if (!artUrl.length)
            return;

        try {
            ctx.drawImage(artUrl, 0, 0, canvasItem.width, canvasItem.height);
        } catch (e) {
            return;
        }

        const data = ctx.getImageData(0, 0, canvasItem.width, canvasItem.height).data;
        const size = canvasItem.width * canvasItem.height;
        const buckets = new Map();
        let opaque = 0;

        for (let i = 0; i < data.length; i += 4) {
            if (data[i + 3] < 200)
                continue;

            opaque++;

            const key = ((data[i] >> 3) << 10) | ((data[i + 1] >> 3) << 5) | (data[i + 2] >> 3);
            buckets.set(key, (buckets.get(key) ?? 0) + 1);
        }

        // Image has not been drawn yet
        if (opaque < size * 0.5)
            return;

        const entries = [];
        for (const [key, count] of buckets) {
            const r = ((key >> 10) & 31) << 3;
            const g = ((key >> 5) & 31) << 3;
            const b = (key & 31) << 3;
            const colour = Qt.rgba(r / 255, g / 255, b / 255, 1);
            const saturation = colour.hslSaturation;
            const lightness = colour.hslLightness;

            // Skip near-white and pure black buckets
            if (lightness > 0.9 && saturation < 0.35)
                continue;
            if (lightness < 0.05 && saturation < 0.1)
                continue;

            entries.push({
                colour,
                count,
                hue: colour.hslHue,
                saturation,
                lightness
            });
        }

        if (!entries.length) {
            root.dominantColour = "transparent";
            root.secondaryColour = "transparent";
            return;
        }

        entries.sort((a, b) => b.count - a.count);

        const dominant = entries[0];
        const threshold = Math.max(2, opaque * 0.002);

        let secondary = null;
        for (let i = 1; i < entries.length; i++) {
            const entry = entries[i];
            if (entry.count < threshold)
                break;
            if (distinct(dominant, entry)) {
                secondary = entry;
                break;
            }
        }

        root.dominantColour = focusColour(dominant);
        root.secondaryColour = secondary ? secondaryColourFor(secondary) : "transparent";
    }

    Canvas {
        id: canvas

        width: 64
        height: 64

        onAvailableChanged: {
            if (available)
                root.refresh();
        }

        onImageLoaded: requestPaint()
        onPaint: root.analyse(canvas)
    }
}
