import QtQuick
import qs.services

Item {
    id: root

    readonly property string artUrl: Players.getArtUrl(Players.active)

    // Second most dominant usable colour, falling back to the brightest usable one
    property color secondaryColour: "transparent"
    property color brightestColour: "transparent"

    visible: false
    implicitWidth: 64
    implicitHeight: 64

    onArtUrlChanged: {
        secondaryColour = "transparent";
        brightestColour = "transparent";
        refresh();
    }

    function refresh(): void {
        if (!canvas.available)
            return;

        if (artUrl.length)
            canvas.loadImage(artUrl);
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

            // Skip near-white and near-black buckets
            if (saturation < 0.12 || lightness < 0.1 || (lightness > 0.9 && saturation < 0.35))
                continue;

            entries.push({
                colour,
                count,
                lightness
            });
        }

        if (!entries.length) {
            root.secondaryColour = "transparent";
            root.brightestColour = "transparent";
            return;
        }

        entries.sort((a, b) => b.count - a.count);

        const threshold = Math.max(2, opaque * 0.002);
        const common = entries.filter(e => e.count >= threshold);
        const candidates = common.length ? common : entries;

        const secondary = candidates.length > 1 ? candidates[1] : null;
        const brightest = [...candidates].sort((a, b) => b.lightness - a.lightness)[0] ?? null;

        const chosen = secondary ?? brightest;
        root.secondaryColour = chosen ? Colours.mediaAccent(chosen.colour) : "transparent";
        root.brightestColour = brightest ? Colours.mediaAccent(brightest.colour) : "transparent";
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
