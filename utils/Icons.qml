pragma Singleton

import QtQuick
import Quickshell
import Quickshell.Services.Notifications
import Caelestia.Config

Singleton {
    id: root

    readonly property var weatherIcons: ({
            "0": "clear_day",
            "1": "clear_day",
            "2": "partly_cloudy_day",
            "3": "cloud",
            "45": "foggy",
            "48": "foggy",
            "51": "rainy",
            "53": "rainy",
            "55": "rainy",
            "56": "rainy",
            "57": "rainy",
            "61": "rainy",
            "63": "rainy",
            "65": "rainy",
            "66": "rainy",
            "67": "rainy",
            "71": "cloudy_snowing",
            "73": "cloudy_snowing",
            "75": "snowing_heavy",
            "77": "cloudy_snowing",
            "80": "rainy",
            "81": "rainy",
            "82": "rainy",
            "85": "cloudy_snowing",
            "86": "snowing_heavy",
            "95": "thunderstorm",
            "96": "thunderstorm",
            "99": "thunderstorm"
        })

    readonly property var categoryIcons: ({
            WebBrowser: "web",
            Printing: "print",
            Security: "security",
            Network: "chat",
            Archiving: "archive",
            Compression: "archive",
            Development: "code",
            IDE: "code",
            TextEditor: "edit_note",
            Audio: "music_note",
            Music: "music_note",
            Player: "music_note",
            Recorder: "mic",
            Game: "sports_esports",
            FileTools: "files",
            FileManager: "files",
            Filesystem: "files",
            FileTransfer: "files",
            Settings: "settings",
            DesktopSettings: "settings",
            HardwareSettings: "settings",
            TerminalEmulator: "terminal",
            ConsoleOnly: "terminal",
            Utility: "build",
            Monitor: "monitor_heart",
            Midi: "graphic_eq",
            Mixer: "graphic_eq",
            AudioVideoEditing: "video_settings",
            AudioVideo: "music_video",
            Video: "videocam",
            Building: "construction",
            Graphics: "photo_library",
            "2DGraphics": "photo_library",
            RasterGraphics: "photo_library",
            TV: "tv",
            System: "host",
            Office: "content_paste"
        })

    // qmlformat off
    readonly property list<string> networkIcons: [
        "signal_wifi_0_bar",
        "network_wifi_1_bar",
        "network_wifi_2_bar",
        "network_wifi_3_bar",
        "network_wifi"
    ]

    readonly property var bluetoothIconRules: [
        [["headset", "headphones"], "headphones"],
        [["audio"], "speaker"],
        [["phone"], "smartphone"],
        [["mouse"], "mouse"],
        [["keyboard"], "keyboard"]
    ]

    readonly property var notifIconRules: [
        [["reboot"], "restart_alt"],
        [["recording"], "screen_record"],
        [["battery"], "power"],
        [["screenshot"], "screenshot_monitor"],
        [["welcome"], "waving_hand"],
        [["time", "a break"], "schedule"],
        [["installed"], "download"],
        [["update"], "update"],
        [["unable to"], "deployed_code_alert"],
        [["profile"], "person"],
        [["file"], "folder_copy"]
    ]

    readonly property var appIconRules: [
        [["firefox", "chrome", "chromium", "brave", "browser", "webkit"], "public"],
        [["code", "vscode", "codium", "ide"], "code"],
        [["terminal", "kitty", "alacritty", "foot", "wezterm"], "terminal"],
        [["spotify", "music", "player"], "music_note"],
        [["discord", "telegram", "signal", "whatsapp", "chat"], "chat"],
        [["steam", "game"], "sports_esports"],
        [["gimp", "krita", "photo", "image"], "photo_library"],
        [["vlc", "mpv", "video"], "movie"],
        [["file", "nautilus", "dolphin", "thunar"], "files"],
        [["settings", "control"], "settings"],
        [["office", "libreoffice", "document"], "description"],
        [["mail", "thunderbird"], "mail"],
        [["calendar"], "calendar_month"],
        [["clock"], "schedule"],
        [["calculator"], "calculate"]
    ]

    readonly property var notifAppIconRules: [
        [["audio-headset", "headset", "headphone"], "headphones"],
        [["bluetooth", "blueman"], "bluetooth"],
        [["media-record", "recording", "screenrecord"], "screen_record"],
        [["screenshot", "spectacle"], "screenshot_monitor"],
        [["firefox", "chrome", "chromium", "browser"], "public"],
        [["instagram"], "photo_camera"],
        [["discord"], "forum"],
        [["telegram"], "send"],
        [["steam"], "sports_esports"],
        [["spotify"], "music_note"],
        [["code", "vscode", "codium"], "code"],
        [["terminal", "kitty", "alacritty"], "terminal"],
        [["mail", "thunderbird"], "mail"],
        [["calendar"], "calendar_month"]
    ]

    readonly property var menuIconRules: [
        [["about"], "info"],
        [["propert", "settings", "preferences", "config"], "settings"],
        [["bluetooth"], "bluetooth"],
        [["addon", "add-on", "extension", "plugin"], "extension"],
        [["exit", "quit", "close"], "logout"],
        [["copy"], "content_copy"],
        [["paste"], "content_paste"],
        [["reload", "refresh"], "refresh"],
        [["fullscreen"], "fullscreen"],
        [["mute", "volume"], "volume_up"],
        [["hide"], "visibility_off"],
        [["show"], "visibility"],
        [["new window", "new"], "add"],
        [["download"], "download"],
        [["upload"], "upload"],
        [["print"], "print"],
        [["help"], "help"],
        [["check", "update"], "update"]
    ]

    readonly property var trayIconRules: [
        [["bluetooth", "blueman"], "bluetooth"],
        [["network", "wifi", "ethernet"], "wifi"],
        [["volume", "audio", "sound"], "volume_up"],
        [["battery", "power"], "battery_full"],
        [["chrome", "chromium", "browser", "firefox"], "public"],
        [["discord"], "forum"],
        [["telegram"], "send"],
        [["steam"], "sports_esports"],
        [["spotify"], "music_note"],
        [["mail", "thunderbird"], "mail"],
        [["dropbox", "nextcloud", "cloud"], "cloud"],
        [["keyboard"], "keyboard"],
        [["screen", "record"], "screen_record"]
    ]
    // qmlformat on

    /**
     * Checks if a name matches an icon rule. See the IconRule type in the config module.
     */
    function matchIconRule(name: string, iconRule: var): bool {
        if (!iconRule.icon)
            return false;

        if (iconRule.regex) {
            const re = new RegExp(iconRule.regex, iconRule.flags ?? "");
            if (re.test(name))
                return true;
        } else if (iconRule.name === name) {
            return true;
        }

        return false;
    }

    function matchIconRuleList(name: string, rules: var): string {
        if (!rules)
            return "";

        for (const iconRule of rules.values)
            if (matchIconRule(name, iconRule))
                return iconRule.icon;

        return "";
    }

    function decodeIcon(value: string): string {
        try {
            return decodeURIComponent(value);
        } catch (e) {
            return value;
        }
    }

    function isUrl(source: string): bool {
        if (!source)
            return false;

        return source.startsWith("file://") || source.startsWith("image://") || source.startsWith("qrc:/") || source.startsWith("http://") || source.startsWith("https://") || source.startsWith("data:");
    }

    function isImageSource(source: string): bool {
        if (!source)
            return false;

        if (isUrl(source))
            return true;

        if (source.startsWith("/") || source.startsWith("~/"))
            return true;

        return /\.(png|jpe?g|webp|svg|gif|avif|bmp|ico)(\?.*)?$/i.test(source);
    }

    function toImageSource(source: string): string {
        if (!source)
            return "";

        if (source.startsWith("~"))
            return Qt.resolvedUrl(`${Quickshell.env("HOME")}${source.slice(1)}`);

        if (source.startsWith("/"))
            return Qt.resolvedUrl(source);

        return source;
    }

    /**
     * Resolves a theme icon name or file path to a source that will actually
     * load. Returns an empty string when nothing can be found, allowing the
     * caller to display its own fallback instead of Quickshell's missing-icon
     * texture.
     */
    function resolveIcon(source: string, fallback: string): string {
        if (!source)
            return fallback ? resolveIcon(fallback, "") : "";

        if (source.startsWith("image://icon/"))
            return resolveSource(source);

        if (isImageSource(source))
            return toImageSource(source);

        const path = Quickshell.iconPath(source, true);
        if (path)
            return path;

        if (fallback && fallback !== source)
            return resolveIcon(fallback, "");

        return "";
    }

    /**
     * Resolves any icon source, including Quickshell image-provider URLs such
     * as image://icon/name?fallback=other. Invalid provider URLs resolve to an
     * empty string rather than the purple/black missing texture.
     */
    function resolveSource(source: string): string {
        if (!source)
            return "";

        if (source.startsWith("image://icon/")) {
            const raw = source.slice("image://icon/".length);
            const pathIndex = raw.indexOf("?path=");
            if (pathIndex !== -1) {
                const name = raw.slice(0, pathIndex);
                const dir = raw.slice(pathIndex + "?path=".length).replace(/^\/+/, "");
                const file = name.slice(name.lastIndexOf("/") + 1);
                const themed = Quickshell.iconPath(file, true);
                return themed ? themed : toImageSource(dir ? `/${dir}/${file}` : `/${file}`);
            }

            const fallbackIndex = raw.indexOf("?fallback=");
            if (fallbackIndex !== -1) {
                const name = raw.slice(0, fallbackIndex);
                const fallback = raw.slice(fallbackIndex + "?fallback=".length);
                const resolved = resolveIcon(decodeIcon(name), "");
                return resolved ? resolved : resolveIcon(decodeIcon(fallback), "");
            }

            return resolveIcon(decodeIcon(raw), "");
        }

        if (isImageSource(source))
            return toImageSource(source);

        return resolveIcon(source, "");
    }

    function getMonogram(text: string): string {
        if (!text)
            return "";

        const cleaned = text.replace(/[^A-Za-z0-9\u00C0-\uFFFF ]/g, " ").trim();
        if (!cleaned)
            return "";

        const parts = cleaned.split(/\s+/).filter(part => part.length > 0);
        if (parts.length === 0)
            return "";

        if (parts.length === 1)
            return parts[0].slice(0, 1).toUpperCase();

        return `${parts[0].slice(0, 1)}${parts[1].slice(0, 1)}`.toUpperCase();
    }

    function getAppIcon(name: string, fallback: string): string {
        const icon = DesktopEntries.heuristicLookup(name)?.icon;
        return resolveIcon(icon, fallback === "undefined" ? "" : fallback);
    }

    function getAppFallbackIcon(name: string, categories: var, icon: string): string {
        const match = matchIcon(`${name} ${icon}`.toLowerCase(), appIconRules, "");
        if (match)
            return match;

        if (categories)
            for (const [key, value] of Object.entries(categoryIcons))
                if (categories.includes(key))
                    return value;

        return "apps";
    }

    function isCriticalUrgency(urgency: var): bool {
        return urgency === NotificationUrgency.Critical || urgency === "critical";
    }

    function getNotifAppIcon(appIcon: string, appName: string, summary: string, urgency: var): string {
        const match = matchIcon(`${appIcon} ${appName} ${summary}`.toLowerCase(), notifAppIconRules, "");
        return match || getNotifIcon(summary, urgency);
    }

    function getMenuIcon(icon: string, text: string): string {
        return matchIcon(`${text} ${icon}`.toLowerCase(), menuIconRules, "radio_button_checked");
    }

    function getTrayFallbackIcon(id: string, title: string): string {
        return matchIcon(`${id} ${title}`.toLowerCase(), trayIconRules, "widgets");
    }

    function getFileFallbackIcon(mimeType: string, name: string, isDir: bool): string {
        if (isDir)
            return "folder";

        const mime = (mimeType || "").toLowerCase();
        if (mime.startsWith("image/"))
            return "image";
        if (mime.startsWith("video/"))
            return "movie";
        if (mime.startsWith("audio/"))
            return "audio_file";
        if (mime.includes("zip") || mime.includes("compressed") || mime.includes("archive"))
            return "folder_zip";
        if (mime.includes("pdf"))
            return "picture_as_pdf";
        if (mime.includes("font"))
            return "font_download";
        if (mime.includes("executable"))
            return "terminal";
        if (mime.includes("javascript") || mime.includes("json") || mime.includes("xml") || mime.includes("text/x-"))
            return "code";
        if (mime.startsWith("text/"))
            return "description";

        return "draft";
    }

    function getAppCategoryIcon(name: string, fallback: string): string {
        const match = matchIconRuleList(name, GlobalConfig.bar.workspaces.windowIcons);
        if (match)
            return match;

        const categories = DesktopEntries.heuristicLookup(name)?.categories;
        if (categories)
            for (const [key, value] of Object.entries(categoryIcons))
                if (categories.includes(key))
                    return value;

        return fallback;
    }

    /**
     * Accepts a list of tables containing matching rules (strings) and their corresponding results.
     * If any of the strings are found in the text, returns the result associated with them. Otherwise returns the fallback.
     */
    function matchIcon(text: string, rules: var, fallback: string): string {
        for (const [needles, result] of rules)
            if (needles.some(n => text.includes(n)))
                return result;

        return fallback;
    }

    function getNetworkIcon(strength: int, isSecure = false): string {
        const level = Math.max(0, Math.min(4, Math.floor(strength / 20)));
        const icon = networkIcons[level];

        return isSecure && level > 0 ? `${icon}_locked` : icon;
    }

    function getBluetoothIcon(icon: string): string {
        return matchIcon(icon, bluetoothIconRules, "bluetooth");
    }

    function getWeatherIcon(code: string): string {
        return weatherIcons[code] ?? "air";
    }

    function getNotifIcon(summary: string, urgency: var): string {
        const fallback = isCriticalUrgency(urgency) ? "release_alert" : "chat";
        return matchIcon(summary.toLowerCase(), notifIconRules, fallback);
    }

    function getVolumeIcon(volume: real, isMuted: bool): string {
        if (isMuted)
            return "no_sound";
        if (volume >= 0.5)
            return "volume_up";
        if (volume > 0)
            return "volume_down";
        return "volume_mute";
    }

    function getMicVolumeIcon(volume: real, isMuted: bool): string {
        return !isMuted && volume > 0 ? "mic" : "mic_off";
    }

    function getTrayIcon(id: string, icon: string): string {
        for (const sub of GlobalConfig.bar.tray.iconSubs.values)
            if (sub.id === id)
                return sub.image ? toImageSource(sub.image) : resolveIcon(sub.icon, "");

        return resolveSource(icon);
    }

    function getBatteryIcon(percentage: real, charging = false): string {
        if (percentage === 1)
            return charging ? "battery_charging_full" : "battery_full";
        let level = Math.floor(percentage * 7);
        if (charging && (level === 4 || level === 1))
            level--;
        return charging ? `battery_charging_${(level + 3) * 10}` : `battery_${level}_bar`;
    }
}
