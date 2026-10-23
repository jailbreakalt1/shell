pragma Singleton

import Quickshell
import Caelestia.I18n

Singleton {
    property var _regexCache: ({})

    // Notification bodies can contain the small HTML subset from the
    // freedesktop spec. Elided previews must not measure or display raw tags.
    function hasMarkup(text: string): bool {
        return /<[a-z!\/][^>]*>/i.test(text);
    }

    function _decodeEntities(text: string): string {
        return text.replace(/&#x([0-9a-f]+);/gi, (_, hex) => String.fromCodePoint(parseInt(hex, 16))).replace(/&#([0-9]+);/g, (_, dec) => String.fromCodePoint(parseInt(dec, 10))).replace(/&nbsp;/gi, " ").replace(/&quot;/gi, '"').replace(/&apos;/gi, "'").replace(/&lt;/gi, "<").replace(/&gt;/gi, ">").replace(/&amp;/gi, "&");
    }

    function stripMarkup(text: string): string {
        if (!hasMarkup(text))
            return text;
        return _decodeEntities(text.replace(/<br\s*\/?>/gi, " ").replace(/<[^>]+>/g, "")).replace(/\s+/g, " ").trim();
    }

    // Chrome/web notifications often start the body with a link to the site,
    // followed by the actual message. The preview should show the message.
    function notificationPreview(text: string): string {
        if (!hasMarkup(text))
            return text;

        const withoutLeadingLink = text.replace(/^\s*<a\b[^>]*>[\s\S]*?<\/a>\s*(?:\r?\n\s*)+/i, "");
        const preview = stripMarkup(withoutLeadingLink);
        return preview.length > 0 ? preview : stripMarkup(text);
    }

    function escapeHtml(text: string): string {
        return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;").replace(/'/g, "&#39;");
    }

    function percent(value: int): string {
        // TRANSLATORS: %1 = a number
        return Tr.tr("%1%").arg(value);
    }

    function percentOne(value: real): string {
        return percent(Math.round(value * 100));
    }

    function testRegexList(filterList: list<string>, target: string): bool {
        const regexChecker = /^\^.*\$$/;
        for (const filter of filterList) {
            if (regexChecker.test(filter)) {
                let re = _regexCache[filter];
                if (!re) {
                    re = new RegExp(filter);
                    _regexCache[filter] = re;
                }
                if (re.test(target))
                    return true;
            } else {
                if (filter === target)
                    return true;
            }
        }
        return false;
    }
}
