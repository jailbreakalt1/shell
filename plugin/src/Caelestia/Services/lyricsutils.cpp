#include "lyricsutils.hpp"

#include <qhash.h>
#include <qregularexpression.h>
#include <qset.h>

#include <algorithm>
#include <cmath>

namespace caelestia::services::lyricmatch {

using Qt::StringLiterals::operator""_s;

namespace {

constexpr qreal k_matchThreshold = 0.55;

[[nodiscard]] QString stripLeadingTrackNumber(const QString& text) {
    static const QRegularExpression re(
        uR"(^\s*(?:\[?\d{1,3}\]?\s*[-–—.)]\s+|\d{1,3}\.\s+))"_s, QRegularExpression::CaseInsensitiveOption);
    QString out = text;
    out.remove(re);
    return out;
}

[[nodiscard]] QString stripNoise(QString text) {
    static const QRegularExpression featRe(
        uR"(\s+(?:feat\.?|ft\.?|featuring|prod\.?|prod\s+by|produced\s+by)\s+.*$)"_s,
        QRegularExpression::CaseInsensitiveOption);
    text.remove(featRe);

    static const QRegularExpression suffixRe(
        uR"(\s*[-–—|:]\s*(?:official\s*(?:music\s*)?(?:video|audio)|(?:official\s*)?lyric(?:s)?\s*video|lyrics?|visuali[sz]er|audio|video|m/v|mv|hd|hq|4k|8k|remaster(?:ed)?(?:\s*\d{4})?|live(?:\s+(?:at|from|in|on)\s+.*)?|acoustic(?:\s+version)?|demo(?:\s+version)?|radio\s*edit|extended(?:\s*mix)?|club\s*mix|remix|instrumental|karaoke|cover|explicit|clean|bonus\s*track|deluxe(?:\s*edition)?|single\s*version|album\s*version|mono|stereo|topic|from\s+.*)$)"_s,
        QRegularExpression::CaseInsensitiveOption);
    text.remove(suffixRe);

    static const QRegularExpression noiseRe(
        uR"(\b(?:official\s*(?:music\s*)?video|official\s*audio|lyric\s*video|visuali[sz]er)\b)"_s,
        QRegularExpression::CaseInsensitiveOption);
    text.remove(noiseRe);

    static const QRegularExpression topicRe(
        uR"(\s*[-–—]\s*topic$|\s+vevo$)"_s, QRegularExpression::CaseInsensitiveOption);
    text.remove(topicRe);

    return text.simplified();
}

[[nodiscard]] QStringList uniqueStrings(const QStringList& values) {
    QStringList out;
    QSet<QString> seen;
    out.reserve(values.size());
    for (const QString& value : values) {
        const QString trimmed = value.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        const QString key = normalizeKey(trimmed);
        if (key.isEmpty() || seen.contains(key)) {
            continue;
        }
        seen.insert(key);
        out.append(trimmed);
    }
    return out;
}

[[nodiscard]] qsizetype levenshteinDistance(const QString& left, const QString& right) {
    const qsizetype n = left.size();
    const qsizetype m = right.size();
    if (n == 0) {
        return m;
    }
    if (m == 0) {
        return n;
    }

    QVector<qsizetype> previous(static_cast<qsizetype>(m + 1));
    QVector<qsizetype> current(static_cast<qsizetype>(m + 1));
    for (qsizetype j = 0; j <= m; ++j) {
        previous[j] = j;
    }

    for (qsizetype i = 1; i <= n; ++i) {
        current[0] = i;
        for (qsizetype j = 1; j <= m; ++j) {
            const qsizetype substitution = previous[j - 1] + (left.at(i - 1) == right.at(j - 1) ? 0 : 1);
            const qsizetype insertion = current[j - 1] + 1;
            const qsizetype deletion = previous[j] + 1;
            current[j] = std::min({ substitution, insertion, deletion });
        }
        previous.swap(current);
    }

    return previous[m];
}

[[nodiscard]] QStringList splitArtistList(const QString& artist) {
    QString text = decodeHtmlEntities(artist);
    text = removeBracketGroups(text);
    text = stripNoise(text);
    text.replace(
        QRegularExpression(
            uR"(\s+(?:feat\.?|ft\.?|featuring|prod\.?|produced\s+by)\s+)"_s,
            QRegularExpression::CaseInsensitiveOption),
        u", "_s);

    static const QRegularExpression separators(
        uR"(\s*(?:,|;|&|\+|/|\s+x\s+|\s+×\s+|\s+vs\.?\s+|\s+with\s+)\s*)"_s,
        QRegularExpression::CaseInsensitiveOption);

    QStringList out;
    for (const QString& part : text.split(separators, Qt::SkipEmptyParts)) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
            out.append(trimmed);
        }
    }
    return uniqueStrings(out);
}

} // namespace

QString decodeHtmlEntities(QString text) {
    static const QHash<QString, QString> named = {
        { u"&amp;"_s, u"&"_s },
        { u"&apos;"_s, u"'"_s },
        { u"&#39;"_s, u"'"_s },
        { u"&quot;"_s, u"\""_s },
        { u"&lt;"_s, u"<"_s },
        { u"&gt;"_s, u">"_s },
        { u"&nbsp;"_s, u" "_s },
        { u"&ndash;"_s, u"–"_s },
        { u"&mdash;"_s, u"—"_s },
        { u"&hellip;"_s, u"…"_s },
        { u"&lsquo;"_s, u"‘"_s },
        { u"&rsquo;"_s, u"’"_s },
        { u"&ldquo;"_s, u"“"_s },
        { u"&rdquo;"_s, u"”"_s },
    };

    for (auto it = named.constBegin(); it != named.constEnd(); ++it) {
        text.replace(it.key(), it.value(), Qt::CaseInsensitive);
    }

    static const QRegularExpression numeric(uR"(&#(x?[0-9a-fA-F]+);)"_s);
    QString out;
    out.reserve(text.size());
    qsizetype last = 0;
    auto it = numeric.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        out.append(text.mid(last, match.capturedStart() - last));

        bool ok = false;
        const QString code = match.captured(1);
        const uint value = code.startsWith(u'x', Qt::CaseInsensitive) ? code.mid(1).toUInt(&ok, 16)
                                                                      : code.toUInt(&ok, 10);
        if (ok && value > 0 && value <= 0xFFFF) {
            out.append(QChar(static_cast<char16_t>(value)));
        } else {
            out.append(match.captured(0));
        }
        last = match.capturedEnd();
    }
    out.append(text.mid(last));
    return out;
}

QString removeBracketGroups(const QString& text) {
    static const QString opens = u"([{<【（「『〔〈《"_s;
    static const QString closes = u")]}>】）」』〕〉》"_s;
    if (opens.size() != closes.size()) {
        return text;
    }

    QVector<QPair<QChar, qsizetype>> stack;
    QVector<QPair<qsizetype, qsizetype>> ranges;
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        const qsizetype openIndex = opens.indexOf(c);
        if (openIndex >= 0) {
            stack.append({ closes.at(openIndex), i });
            continue;
        }
        if (!stack.isEmpty() && c == stack.last().first) {
            const qsizetype start = stack.last().second;
            stack.removeLast();
            if (stack.isEmpty()) {
                ranges.append({ start, i });
            }
        }
    }

    if (ranges.isEmpty()) {
        return text.simplified();
    }

    QString out;
    out.reserve(text.size());
    qsizetype rangeIndex = 0;
    for (qsizetype i = 0; i < text.size(); ++i) {
        if (rangeIndex < ranges.size() && i == ranges.at(rangeIndex).first) {
            out.append(u' ');
            i = ranges.at(rangeIndex).second;
            ++rangeIndex;
            continue;
        }
        out.append(text.at(i));
    }
    return out.simplified();
}

QString cleanTitle(const QString& title) {
    const QString decoded = decodeHtmlEntities(title).trimmed();
    QString out = removeBracketGroups(decoded);
    out = stripLeadingTrackNumber(out);
    out = stripNoise(out);
    if (out.isEmpty()) {
        out = removeBracketGroups(decoded);
    }
    return out.simplified();
}

QString cleanArtist(const QString& artist) {
    const QString decoded = decodeHtmlEntities(artist).trimmed();
    QString out = removeBracketGroups(decoded);
    out = stripNoise(out);
    if (out.isEmpty()) {
        out = removeBracketGroups(decoded);
    }
    return out.simplified();
}

QString cleanAlbum(const QString& album) {
    QString out = cleanTitle(album);
    if (out.contains(u"VEVO"_s, Qt::CaseInsensitive) || out.endsWith(u"Topic"_s, Qt::CaseInsensitive)) {
        return {};
    }
    return out;
}

QString normalizeKey(const QString& text) {
    const QString normalized = decodeHtmlEntities(text).normalized(QString::NormalizationForm_KD);
    QString out;
    out.reserve(normalized.size());
    for (const QChar c : normalized) {
        if (c.isMark()) {
            continue;
        }
        if (c == u'&') {
            out.append(u" and "_s);
            continue;
        }
        if (c == u'\'' || c == u'’' || c == u'‘' || c == u'`') {
            continue;
        }
        if (c.isLetterOrNumber()) {
            out.append(c.toLower());
        } else {
            out.append(u' ');
        }
    }
    return out.simplified();
}

QString primaryArtist(const QString& artist) {
    const QStringList parts = splitArtistList(artist);
    if (!parts.isEmpty()) {
        return parts.first();
    }
    return cleanArtist(artist);
}

QStringList artistVariants(const QString& artist) {
    QStringList out;
    const QString decoded = decodeHtmlEntities(artist).trimmed();
    const QString cleaned = cleanArtist(decoded);
    out.append(cleaned);
    out.append(decoded);

    const QStringList parts = splitArtistList(decoded);
    out.append(parts);
    if (!parts.isEmpty()) {
        out.append(parts.first());
    }

    static const QRegularExpression thePrefix(uR"(^\s*the\s+)"_s, QRegularExpression::CaseInsensitiveOption);
    QString withoutThe = cleaned;
    withoutThe.remove(thePrefix);
    out.append(withoutThe);

    return uniqueStrings(out);
}

QStringList titleVariants(const QString& title) {
    QStringList out;
    const QString decoded = decodeHtmlEntities(title).trimmed();
    const QString cleaned = cleanTitle(decoded);
    out.append(cleaned);
    out.append(decoded);
    out.append(removeBracketGroups(decoded));

    static const QRegularExpression separator(uR"(\s+[-–—]\s+)"_s);
    const QStringList parts = cleaned.split(separator, Qt::SkipEmptyParts);
    if (parts.size() >= 2) {
        out.append(parts.first());
        out.append(parts.last());
    }

    static const QRegularExpression featRe(
        uR"(\s+(?:feat\.?|ft\.?|featuring|with|prod\.?|prod\s+by|produced\s+by)\s+.*$)"_s,
        QRegularExpression::CaseInsensitiveOption);
    QString withoutFeat = cleaned;
    withoutFeat.remove(featRe);
    out.append(withoutFeat);

    return uniqueStrings(out);
}

QStringList searchQueries(const QString& artist, const QString& title) {
    const QStringList artists = artistVariants(artist);
    const QStringList titles = titleVariants(title);
    QStringList out;

    const qsizetype artistCount = std::min<qsizetype>(artists.size(), 3);
    const qsizetype titleCount = std::min<qsizetype>(titles.size(), 4);
    for (qsizetype i = 0; i < artistCount; ++i) {
        for (qsizetype j = 0; j < titleCount; ++j) {
            out.append(u"%1 %2"_s.arg(artists.at(i), titles.at(j)));
            out.append(u"%1 - %2"_s.arg(artists.at(i), titles.at(j)));
            out.append(u"%1 %2"_s.arg(titles.at(j), artists.at(i)));
        }
    }

    for (qsizetype i = 0; i < titleCount; ++i) {
        out.append(titles.at(i));
    }

    return uniqueStrings(out);
}

TrackQuery makeTrackQuery(const QString& artist, const QString& title, const QString& album, qreal duration) {
    TrackQuery query;
    query.artist = cleanArtist(artist);
    query.title = cleanTitle(title);
    query.album = cleanAlbum(album);
    query.duration = duration;
    query.artists = artistVariants(artist);
    query.titles = titleVariants(title);
    query.queries = searchQueries(artist, title);

    if (query.artist.isEmpty() && !query.artists.isEmpty()) {
        query.artist = query.artists.first();
    }
    if (query.title.isEmpty() && !query.titles.isEmpty()) {
        query.title = query.titles.first();
    }
    return query;
}

qreal textSimilarity(const QString& left, const QString& right) {
    const QString a = normalizeKey(left);
    const QString b = normalizeKey(right);
    if (a.isEmpty() || b.isEmpty()) {
        return 0.0;
    }
    if (a == b) {
        return 1.0;
    }

    const QStringList aTokens = a.split(u' ', Qt::SkipEmptyParts);
    const QStringList bTokens = b.split(u' ', Qt::SkipEmptyParts);
    QSet<QString> aSet;
    aSet.reserve(static_cast<qsizetype>(aTokens.size()));
    for (const QString& token : aTokens) {
        aSet.insert(token);
    }
    qsizetype overlap = 0;
    for (const QString& token : bTokens) {
        if (aSet.contains(token)) {
            ++overlap;
        }
    }
    const qreal dice = 2.0 * static_cast<qreal>(overlap)
        / static_cast<qreal>(std::max<qsizetype>(1, aTokens.size() + bTokens.size()));

    qreal containment = 0.0;
    if (a.contains(b) || b.contains(a)) {
        const qsizetype shorter = std::min<qsizetype>(a.size(), b.size());
        const qsizetype longer = std::max<qsizetype>(a.size(), b.size());
        containment = 0.78 + 0.22 * static_cast<qreal>(shorter) / static_cast<qreal>(std::max<qsizetype>(1, longer));
    }

    const qsizetype distance = levenshteinDistance(a, b);
    const qreal similarity = 1.0
        - static_cast<qreal>(distance) / static_cast<qreal>(std::max<qsizetype>(1, std::max(a.size(), b.size())));

    return std::clamp(std::max({ dice, containment, similarity * 0.94 }), 0.0, 1.0);
}

qreal bestSimilarity(const QStringList& left, const QStringList& right) {
    qreal best = 0.0;
    for (const QString& a : left) {
        for (const QString& b : right) {
            best = std::max(best, textSimilarity(a, b));
        }
    }
    return best;
}

qreal durationSimilarity(qreal left, qreal right) {
    if (left <= 0.0 || right <= 0.0) {
        return 0.5;
    }
    const qreal tolerance = std::max<qreal>(1.5, std::min(left, right) * 0.025);
    const qreal difference = std::abs(left - right);
    if (difference <= tolerance) {
        return 1.0;
    }
    if (difference <= tolerance * 2.0) {
        return 0.8;
    }
    if (difference <= tolerance * 4.0) {
        return 0.45;
    }
    return 0.0;
}

qreal candidateScore(
    const TrackQuery& query, const QString& candidateArtist, const QString& candidateTitle, qreal candidateDuration) {
    const qreal titleScore = bestSimilarity(query.titles, titleVariants(candidateTitle));
    const qreal artistScore = bestSimilarity(query.artists, artistVariants(candidateArtist));
    const qreal durationScore = durationSimilarity(query.duration, candidateDuration);

    qreal score = titleScore * 0.62 + artistScore * 0.28 + durationScore * 0.10;
    if (query.artists.isEmpty() && candidateArtist.trimmed().isEmpty()) {
        score += 0.04;
    }
    if (titleScore < 0.45) {
        score *= 0.75;
    }
    if (!query.artists.isEmpty() && !candidateArtist.trimmed().isEmpty() && artistScore < 0.3) {
        score *= 0.65;
    }
    return std::clamp(score, 0.0, 1.0);
}

bool isPlausibleMatch(qreal score) {
    return score >= k_matchThreshold;
}

} // namespace caelestia::services::lyricmatch
