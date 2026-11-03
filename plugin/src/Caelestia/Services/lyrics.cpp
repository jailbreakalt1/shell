#include "lyrics.hpp"

#include <qdiriterator.h>
#include <qfileinfo.h>
#include <qjsonarray.h>
#include <qnetworkcookiejar.h>
#include <qnetworkrequest.h>
#include <qsavefile.h>
#include <qurlquery.h>
#include <qregularexpression.h>
#include <qvector.h>

#include <algorithm>
#include <cmath>
#include <limits>

#include "lyricsutils.hpp"

#include "config/rootnodes.hpp"
#include "config/serviceconfig.hpp"
#include "config/userpaths.hpp"

using Qt::StringLiterals::operator""_s;
using Qt::StringLiterals::operator""_ba;

namespace {

Q_LOGGING_CATEGORY(lcLyrics, "caelestia.lyrics", QtInfoMsg)

constexpr int k_loadDebounceMs = 50;
constexpr int k_transferTimeoutMs = 12000;
constexpr qreal k_indexFudge = 0.1;

[[nodiscard]] const QHash<QByteArray, QByteArray>& netEaseHeaders() {
    static const QHash<QByteArray, QByteArray> k_h = {
        { "User-Agent",
            "Mozilla/5.0 (X11; Linux x86_64; rv:120.0) Gecko/20100101 Firefox/120.0" },
        { "Referer", "https://music.163.com/" },
    };
    return k_h;
}

[[nodiscard]] const QHash<QByteArray, QByteArray>& lrclibHeaders() {
    static const QHash<QByteArray, QByteArray> k_h = {
        { "User-Agent", "caelestia-shell (https://github.com/caelestia-dots/shell)" },
    };
    return k_h;
}

[[nodiscard]] const QHash<QByteArray, QByteArray>& kugouHeaders() {
    static const QHash<QByteArray, QByteArray> k_h = {
        { "User-Agent",
            "Mozilla/5.0 (X11; Linux x86_64; rv:120.0) Gecko/20100101 Firefox/120.0" },
        { "Referer", "https://www.kugou.com/" },
    };
    return k_h;
}

[[nodiscard]] const QHash<QByteArray, QByteArray>& qqMusicHeaders() {
    static const QHash<QByteArray, QByteArray> k_h = {
        { "User-Agent",
            "Mozilla/5.0 (X11; Linux x86_64; rv:120.0) Gecko/20100101 Firefox/120.0" },
        { "Referer", "https://y.qq.com/" },
    };
    return k_h;
}

[[nodiscard]] QString sanitizeFilenamePart(const QString& s) {
    QString out;
    out.reserve(s.size());
    for (const QChar c : s) {
        if (c == u'/' || c == u'\0' || c == u'\\') {
            out.append(u'_');
        } else {
            out.append(c);
        }
    }
    return out;
}

[[nodiscard]] QString readTextFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    QByteArray bytes = f.readAll();
    if (bytes.startsWith("\xEF\xBB\xBF")) {
        bytes.remove(0, 3);
    }
    return QString::fromUtf8(bytes);
}

[[nodiscard]] QStringList artistNames(const QJsonArray& artists, const QString& key = u"name"_s) {
    QStringList out;
    out.reserve(artists.size());
    for (const auto& value : artists) {
        const QString name = value.toObject().value(key).toString().trimmed();
        if (!name.isEmpty()) {
            out.append(name);
        }
    }
    return out;
}

[[nodiscard]] QString kugouCandidateId(const QString& id, const QString& accessKey) {
    if (id.isEmpty() || accessKey.isEmpty()) {
        return {};
    }
    return u"%1|%2"_s.arg(id, accessKey);
}

[[nodiscard]] QString decodeBase64Content(const QString& content) {
    if (content.contains(u'[') && content.contains(u':')) {
        return content;
    }

    QString compact = content;
    compact.remove(QRegularExpression(uR"(\s+)"_s));

    const QByteArray decoded = QByteArray::fromBase64(compact.toLatin1());
    if (!decoded.isEmpty()) {
        return QString::fromUtf8(decoded);
    }
    return content;
}

[[nodiscard]] bool isCreditLine(const QString& text, qreal time) {
    if (time > 20.0) {
        return false;
    }

    static const QStringList keywords = {
        u"作词"_s,
        u"作曲"_s,
        u"编曲"_s,
        u"制作"_s,
        u"收录"_s,
        u"演奏"_s,
        u"词："_s,
        u"曲："_s,
        u"lyricist"_s,
        u"composer"_s,
        u"composed by"_s,
        u"arranger"_s,
        u"arranged by"_s,
        u"producer"_s,
        u"produced by"_s,
        u"written by"_s,
        u"lyrics by"_s,
        u"music by"_s,
        u"mixed by"_s,
        u"mastered by"_s,
        u"mixing"_s,
        u"mastering"_s,
    };

    const QString trimmed = text.trimmed();
    const bool hasColon = trimmed.contains(u':') || trimmed.contains(QChar(0xFF1A));
    for (const QString& keyword : keywords) {
        if (!trimmed.contains(keyword, Qt::CaseInsensitive)) {
            continue;
        }
        if (trimmed.startsWith(keyword, Qt::CaseInsensitive)) {
            return true;
        }
        if (hasColon && trimmed.indexOf(keyword, 0, Qt::CaseInsensitive) <= 12) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool isTrackHeaderLine(const QString& text, const QString& title, const QString& artist) {
    if (title.isEmpty() || artist.isEmpty()) {
        return false;
    }

    static const QRegularExpression separator(uR"(\s+[-–—]\s+)"_s);
    const QString trimmed = text.trimmed();
    const auto separatorMatch = separator.match(trimmed);
    if (!separatorMatch.hasMatch() || separatorMatch.capturedStart() == 0
        || separatorMatch.capturedEnd() >= trimmed.size()) {
        return false;
    }

    const QString left = trimmed.left(separatorMatch.capturedStart()).trimmed();
    const QString right = trimmed.mid(separatorMatch.capturedEnd()).trimmed();
    if (left.isEmpty() || right.isEmpty()) {
        return false;
    }
    const auto query = caelestia::services::lyricmatch::makeTrackQuery(artist, title, {}, 0.0);
    const auto matchesTitle = [&query](const QString& candidate) {
        return caelestia::services::lyricmatch::bestSimilarity(
                   query.titles, caelestia::services::lyricmatch::titleVariants(candidate))
            >= 0.9;
    };
    const auto matchesArtist = [&query](const QString& candidate) {
        return caelestia::services::lyricmatch::bestSimilarity(
                   query.artists, caelestia::services::lyricmatch::artistVariants(candidate))
            >= 0.9;
    };

    return (matchesTitle(left) && matchesArtist(right)) || (matchesArtist(left) && matchesTitle(right));
}

[[nodiscard]] QString findBestLocalLrc(const QString& dir, const QString& artist, const QString& title, qreal duration) {
    if (dir.isEmpty()) {
        return {};
    }

    const auto query = caelestia::services::lyricmatch::makeTrackQuery(artist, title, {}, duration);
    if (query.titles.isEmpty()) {
        return {};
    }

    const QDir root(dir);
    if (!root.exists()) {
        return {};
    }

    static const QRegularExpression separator(uR"(\s+[-–—]\s+)"_s);
    QDirIterator it(dir, QStringList{ u"*.lrc"_s }, QDir::Files | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);

    QString bestPath;
    qreal bestScore = 0.0;

    while (it.hasNext()) {
        const QString path = it.next();
        const QFileInfo info(path);
        const QString stem = info.completeBaseName();
        const QString relative = root.relativeFilePath(path);

        QStringList fileTitles = { stem };
        QStringList fileArtists = { info.dir().dirName() };

        const QStringList segments = relative.split(u'/', Qt::SkipEmptyParts);
        for (qsizetype i = 0; i + 1 < segments.size(); ++i) {
            fileArtists.append(segments.at(i));
        }

        const QStringList parts = stem.split(separator, Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            fileTitles.append(parts.first());
            fileTitles.append(parts.last());
            fileArtists.append(parts.first());
            fileArtists.append(parts.last());
        }

        QStringList fileTitleVariants;
        for (const QString& value : fileTitles) {
            fileTitleVariants.append(caelestia::services::lyricmatch::titleVariants(value));
        }
        QStringList fileArtistVariants;
        for (const QString& value : fileArtists) {
            fileArtistVariants.append(caelestia::services::lyricmatch::artistVariants(value));
        }

        const qreal titleScore =
            caelestia::services::lyricmatch::bestSimilarity(query.titles, fileTitleVariants);
        const qreal artistScore =
            caelestia::services::lyricmatch::bestSimilarity(query.artists, fileArtistVariants);

        qreal score = titleScore * (query.artists.isEmpty() ? 1.0 : 0.72);
        if (!query.artists.isEmpty()) {
            score += artistScore * 0.28;
        }

        const QString normalizedStem = caelestia::services::lyricmatch::normalizeKey(stem);
        const QString normalizedArtist = caelestia::services::lyricmatch::normalizeKey(query.artist);
        const QString normalizedTitle = caelestia::services::lyricmatch::normalizeKey(query.title);
        if (!normalizedTitle.isEmpty()
            && (normalizedStem == u"%1 %2"_s.arg(normalizedArtist, normalizedTitle).trimmed()
                || normalizedStem == u"%1 %2"_s.arg(normalizedTitle, normalizedArtist).trimmed())) {
            score += 0.15;
        }

        if (score > bestScore) {
            bestScore = score;
            bestPath = path;
        }
    }

    if (bestScore < 0.62) {
        qCDebug(lcLyrics) << "best local match too weak:" << bestPath << bestScore;
        return {};
    }
    return bestPath;
}

} // namespace

namespace caelestia::services {

using lyricmatch::makeTrackQuery;

Lyrics::Lyrics(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_loadDebounce(new QTimer(this)) {
    m_loadDebounce->setSingleShot(true);
    m_loadDebounce->setInterval(k_loadDebounceMs);
    QObject::connect(m_loadDebounce, &QTimer::timeout, this, &Lyrics::doLoad);

    m_nam->setCookieJar(new QNetworkCookieJar(m_nam));

    const auto* cfg = config::ConfigSingleton::instance();
    const auto* svcCfg = cfg->services();
    const auto* paths = cfg->paths();

    m_preferredBackend = svcCfg->lyricsBackend();

    QObject::connect(
        svcCfg, &config::ServiceConfig::lyricsBackendChanged, this, &Lyrics::onPreferredBackendConfigChanged);
    QObject::connect(paths, &config::UserPaths::lyricsDirChanged, this, &Lyrics::onLyricsDirChanged);

    loadLyricsMap();
}

QStringList Lyrics::lyrics() const {
    return m_lyrics;
}

LyricsBackend Lyrics::backend() const {
    return m_backend;
}

LyricsBackend Lyrics::preferredBackend() const {
    return m_preferredBackend;
}

void Lyrics::setPreferredBackend(LyricsBackend value) {
    if (m_preferredBackend == value) {
        return;
    }
    m_preferredBackend = value;
    emit preferredBackendChanged();

    config::ConfigSingleton::instance()->services()->set_lyricsBackend(value);

    scheduleLoad();
}

QList<LyricCandidate> Lyrics::lyricCandidates() const {
    return m_candidates;
}

LyricCandidate Lyrics::selectedCandidate() const {
    return m_selected;
}

void Lyrics::setSelectedCandidate(const LyricCandidate& value) {
    if (m_selected == value) {
        return;
    }
    m_selected = value;
    emit selectedCandidateChanged();

    if (!value.isValid()) {
        return;
    }

    const auto b = value.backend();
    setBackend(b);
    setLoading(true);

    const int reqId = newRequestId();
    cancelInFlight();
    const bool allowChain = m_settingFromPrefs && m_preferredBackend == LyricsBackend::Auto;

    if (b != LyricsBackend::Local && b != LyricsBackend::Auto) {
        const QString cached = readCachedLrc(b, value.id());
        if (!cached.isEmpty()) {
            const auto lines = parseLrc(cached);
            if (!lines.isEmpty()) {
                setLines(lines, b);
                setLoading(false);
                if (!m_settingFromPrefs) {
                    persistTrackPrefs();
                }
                return;
            }
        }
    }

    switch (b) {
    case LyricsBackend::LRCLIB:
        fetchLrclibById(value.id(), reqId, allowChain);
        break;
    case LyricsBackend::NetEase:
        fetchNetEaseLyricsById(value.id(), reqId, allowChain);
        break;
    case LyricsBackend::Kugou:
        fetchKugouLyricsById(value.id(), reqId, allowChain);
        break;
    case LyricsBackend::QQMusic:
        fetchQQMusicLyricsById(value.id(), reqId, allowChain);
        break;
    case LyricsBackend::Local: {
        const QString text = readTextFile(value.id());
        const auto lines = text.isEmpty() ? QVector<LyricLine>{} : parseLrc(text);
        if (lines.isEmpty()) {
            qCWarning(lcLyrics) << "selectedCandidate: cannot open or parse local file" << value.id();
            if (allowChain) {
                chainNext(LyricsBackend::Local, reqId);
            } else {
                setLoading(false);
            }
        } else {
            setLines(lines, LyricsBackend::Local);
            setLoading(false);
        }
        break;
    }
    case LyricsBackend::Auto:
    default:
        setLoading(false);
        return;
    }

    if (!m_settingFromPrefs) {
        persistTrackPrefs();
    }
}

bool Lyrics::loading() const {
    return m_loading;
}

bool Lyrics::hasLyrics() const {
    return m_hasLyrics;
}

qreal Lyrics::offset() const {
    return m_offset;
}

void Lyrics::setOffset(qreal value) {
    if (qFuzzyCompare(m_offset, value)) {
        return;
    }
    m_offset = value;
    emit offsetChanged();

    if (!m_settingFromPrefs) {
        persistTrackPrefs();
    }
}

QString Lyrics::trackArtist() const {
    return m_artist;
}

QString Lyrics::trackTitle() const {
    return m_title;
}

int Lyrics::indexForTime(qreal time) const {
    if (m_lines.isEmpty()) {
        return -1;
    }
    const qreal target = time - m_offset + k_indexFudge;
    qsizetype lo = 0;
    qsizetype hi = m_lines.size();
    while (lo < hi) {
        const qsizetype mid = lo + (hi - lo) / 2;
        if (m_lines.at(mid).time <= target) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return static_cast<int>(lo - 1);
}

qreal Lyrics::timeForIndex(int index) const {
    if (index < 0 || index >= m_lines.size()) {
        return -1.0;
    }
    return m_lines.at(index).time + m_offset;
}

void Lyrics::setTrack(const QString& artist, const QString& title, const QString& album, qreal duration) {
    const QString a = artist.trimmed();
    const QString t = title.trimmed();

    if (a == m_artist && t == m_title && album == m_album && qFuzzyCompare(duration + 1.0, m_duration + 1.0)) {
        return;
    }

    m_artist = a;
    m_title = t;
    m_album = album;
    m_duration = duration;
    emit trackChanged();

    scheduleLoad();
}

void Lyrics::clearTrack() {
    newRequestId();
    cancelInFlight();
    m_artist.clear();
    m_title.clear();
    m_album.clear();
    m_duration = 0.0;
    emit trackChanged();

    clearCandidates();
    clearLines();
    if (m_selected.isValid()) {
        m_selected = {};
        emit selectedCandidateChanged();
    }
    setLoading(false);
}

void Lyrics::refresh() {
    scheduleLoad();
}

void Lyrics::setBackend(LyricsBackend value) {
    if (m_backend == value) {
        return;
    }
    m_backend = value;
    emit backendChanged();
}

void Lyrics::setLoading(bool value) {
    if (m_loading == value) {
        return;
    }
    m_loading = value;
    emit loadingChanged();
}

void Lyrics::setLines(QVector<LyricLine> lines, LyricsBackend source) {
    lines.erase(
        std::remove_if(lines.begin(), lines.end(),
            [this](const LyricLine& line) { return line.time <= 1.0 && isTrackHeaderLine(line.text, m_title, m_artist); }),
        lines.end());

    std::ranges::sort(lines, [](const LyricLine& a, const LyricLine& b) {
        return a.time < b.time;
    });

    m_lines = std::move(lines);
    QStringList list;
    list.reserve(m_lines.size());
    for (const auto& line : std::as_const(m_lines)) {
        list.append(line.text);
    }
    m_lyrics = std::move(list);

    setBackend(source);
    emit lyricsChanged();

    const auto hasLyrics = !m_lines.isEmpty();
    if (hasLyrics != m_hasLyrics) {
        m_hasLyrics = hasLyrics;
        emit hasLyricsChanged();
    }
}

void Lyrics::clearLines() {
    // Doesn't actually clear lines, set a flag instead so anims can run
    m_hasLyrics = false;
    emit hasLyricsChanged();
}

void Lyrics::appendCandidates(const QList<LyricCandidate>& add) {
    if (add.isEmpty()) {
        return;
    }

    QList<LyricCandidate> merged = m_candidates;
    bool changed = false;
    for (const auto& candidate : add) {
        if (!merged.contains(candidate)) {
            merged.append(candidate);
            changed = true;
        }
    }
    if (!changed) {
        return;
    }

    std::ranges::stable_sort(merged, [this](const LyricCandidate& a, const LyricCandidate& b) {
        return scoreFor(a) > scoreFor(b);
    });
    if (merged.size() > 24) {
        merged = merged.mid(0, 24);
    }

    m_candidates = std::move(merged);
    emit lyricCandidatesChanged();
}

qreal Lyrics::scoreFor(const LyricCandidate& candidate) const {
    const auto query = makeTrackQuery(m_artist, m_title, m_album, m_duration);
    return lyricmatch::candidateScore(query, candidate.artist(), candidate.title(), candidate.duration());
}

void Lyrics::clearCandidates() {
    if (m_candidates.isEmpty()) {
        return;
    }
    m_candidates.clear();
    emit lyricCandidatesChanged();
}

void Lyrics::markSelected(LyricsBackend backend, const QString& id) {
    const LyricCandidate candidate(backend, id, m_title, m_artist, m_album, m_duration);
    if (!(m_selected == candidate)) {
        m_selected = candidate;
        emit selectedCandidateChanged();
    }
    if (!m_settingFromPrefs) {
        persistTrackPrefs();
    }
}

void Lyrics::scheduleLoad() {
    m_loadDebounce->start();
}

int Lyrics::newRequestId() {
    return ++m_currentRequestId;
}

void Lyrics::cancelInFlight() {
    for (auto it = m_pendingReplies.begin(); it != m_pendingReplies.end(); ++it) {
        for (auto& ptr : it.value()) {
            if (auto* reply = ptr.data()) {
                reply->abort();
                reply->deleteLater();
            }
        }
    }
    m_pendingReplies.clear();
}

void Lyrics::trackReply(int reqId, QNetworkReply* reply) {
    if (!reply) {
        return;
    }
    m_pendingReplies[reqId].append(QPointer<QNetworkReply>(reply));
}

void Lyrics::doLoad() {
    if (m_artist.isEmpty() && m_title.isEmpty()) {
        newRequestId();
        cancelInFlight();
        clearLines();
        clearCandidates();
        if (m_selected.isValid()) {
            m_selected = {};
            emit selectedCandidateChanged();
        }
        setLoading(false);
        return;
    }

    const int reqId = newRequestId();
    cancelInFlight();

    setLoading(true);
    clearLines();
    clearCandidates();
    if (m_selected.isValid()) {
        m_selected = {};
        emit selectedCandidateChanged();
    }

    // Restore per-track prefs (offset, last-selected backend/id)
    m_settingFromPrefs = true;
    const QJsonObject saved = m_lyricsMap.value(trackKey()).toObject();
    setOffset(saved.value(u"offset"_s).toDouble(0.0));
    LyricCandidate restored;
    const QString savedBackendKey = saved.value(u"backend"_s).toString();
    const QString savedId = saved.value(u"id"_s).toString();
    const LyricsBackend savedBackend = backendFromKey(savedBackendKey);
    if (!savedBackendKey.isEmpty() && !savedId.isEmpty() && savedBackend != LyricsBackend::Auto) {
        restored = LyricCandidate(savedBackend, savedId, m_title, m_artist, m_album, m_duration);
    }
    m_settingFromPrefs = false;

    if (restored.isValid()) {
        // Honor saved selection for this track. Start candidate searches afterwards because selecting a
        // candidate cancels any requests that are already in flight.
        m_settingFromPrefs = true;
        setSelectedCandidate(restored);
        m_settingFromPrefs = false;

        const int searchId = m_currentRequestId;
        searchLrclibCandidates(searchId);
        searchNetEaseCandidates(searchId);
        searchKugouCandidates(searchId);
        searchQQMusicCandidates(searchId);
        return;
    }

    // Populate online candidates for the picker while the primary backend is loading.
    searchLrclibCandidates(reqId);
    searchNetEaseCandidates(reqId);
    searchKugouCandidates(reqId);
    searchQQMusicCandidates(reqId);

    // Primary attempt by preferred backend
    switch (m_preferredBackend) {
    case LyricsBackend::Local:
        tryLocal(reqId);
        break;
    case LyricsBackend::LRCLIB:
        tryLrclib(reqId);
        break;
    case LyricsBackend::NetEase:
        tryNetEase(reqId);
        break;
    case LyricsBackend::Kugou:
        tryKugou(reqId);
        break;
    case LyricsBackend::QQMusic:
        tryQQMusic(reqId);
        break;
    case LyricsBackend::Auto:
    default:
        tryLocal(reqId);
        break;
    }
}

void Lyrics::chainNext(LyricsBackend justFailed, int reqId) {
    if (m_preferredBackend != LyricsBackend::Auto) {
        // Non-auto modes don't chain
        setLoading(false);
        return;
    }
    switch (justFailed) {
    case LyricsBackend::Local:
        tryLrclib(reqId);
        return;
    case LyricsBackend::LRCLIB:
        tryNetEase(reqId);
        return;
    case LyricsBackend::NetEase:
        tryKugou(reqId);
        return;
    case LyricsBackend::Kugou:
        tryQQMusic(reqId);
        return;
    case LyricsBackend::QQMusic:
    case LyricsBackend::Auto:
    default:
        setLoading(false);
        return;
    }
}

void Lyrics::tryLocal(int reqId) {
    if (reqId != m_currentRequestId) {
        return;
    }

    setBackend(LyricsBackend::Local);

    const QString dir = lyricsDir();
    if (dir.isEmpty()) {
        chainNext(LyricsBackend::Local, reqId);
        return;
    }

    const QString path = findBestLocalLrc(dir, m_artist, m_title, m_duration);
    if (!path.isEmpty()) {
        const QString text = readTextFile(path);
        const auto lines = parseLrc(text);
        if (!lines.isEmpty()) {
            const LyricCandidate candidate(LyricsBackend::Local, path, m_title, m_artist, m_album, m_duration);
            setLines(lines, LyricsBackend::Local);
            appendCandidates({ candidate });
            m_selected = candidate;
            emit selectedCandidateChanged();
            if (!m_settingFromPrefs) {
                persistTrackPrefs();
            }
            setLoading(false);
            return;
        }
    }

    qCDebug(lcLyrics) << "no local lrc for" << m_artist << "-" << m_title;
    chainNext(LyricsBackend::Local, reqId);
}

void Lyrics::tryLrclib(int reqId) {
    if (reqId != m_currentRequestId) {
        return;
    }

    setBackend(LyricsBackend::LRCLIB);

    const auto query = makeTrackQuery(m_artist, m_title, m_album, m_duration);
    QUrl url(u"https://lrclib.net/api/get"_s);
    QUrlQuery q;
    q.addQueryItem(u"track_name"_s, query.title);
    q.addQueryItem(u"artist_name"_s, query.artist);
    if (!query.album.isEmpty()) {
        q.addQueryItem(u"album_name"_s, query.album);
    }

    constexpr qreal k_maxDurationSecs = std::numeric_limits<int>::max();
    if (m_duration > 0 && qIsFinite(m_duration) && m_duration < k_maxDurationSecs) {
        q.addQueryItem(u"duration"_s, QString::number(qRound(m_duration)));
    }
    url.setQuery(q);

    auto* reply = getJson(url, lrclibHeaders());
    trackReply(reqId, reply);

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, reqId] {
        reply->deleteLater();
        if (reqId != m_currentRequestId) {
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            qCDebug(lcLyrics) << "lrclib /get error:" << reply->errorString();
            tryLrclibSearch(reqId);
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonObject obj = doc.object();
        const auto matchQuery = makeTrackQuery(m_artist, m_title, m_album, m_duration);
        const qreal score = lyricmatch::candidateScore(matchQuery, obj.value(u"artistName"_s).toString(),
            obj.value(u"trackName"_s).toString(), obj.value(u"duration"_s).toDouble());
        if (!lyricmatch::isPlausibleMatch(score)) {
            qCDebug(lcLyrics) << "lrclib /get: implausible match for" << m_artist << "-" << m_title << "score"
                              << score;
            tryLrclibSearch(reqId);
            return;
        }

        const QString synced = obj.value(u"syncedLyrics"_s).toString();
        if (synced.isEmpty()) {
            qCDebug(lcLyrics) << "lrclib: no syncedLyrics for" << m_artist << "-" << m_title;
            tryLrclibSearch(reqId);
            return;
        }

        const auto lines = parseLrc(synced);
        if (lines.isEmpty()) {
            tryLrclibSearch(reqId);
            return;
        }

        const QString id = QString::number(static_cast<qint64>(obj.value(u"id"_s).toDouble()));
        writeCachedLrc(LyricsBackend::LRCLIB, id, synced);
        setLines(lines, LyricsBackend::LRCLIB);
        const LyricCandidate candidate(LyricsBackend::LRCLIB, id, obj.value(u"trackName"_s).toString(),
            obj.value(u"artistName"_s).toString(), obj.value(u"albumName"_s).toString(),
            obj.value(u"duration"_s).toDouble());
        appendCandidates({ candidate });
        m_selected = candidate;
        emit selectedCandidateChanged();
        if (!m_settingFromPrefs) {
            persistTrackPrefs();
        }
        setLoading(false);
    });
}

void Lyrics::tryLrclibSearch(int reqId, bool usePrimaryArtist) {
    if (reqId != m_currentRequestId) {
        return;
    }

    const auto query = makeTrackQuery(m_artist, m_title, m_album, m_duration);
    if (query.title.isEmpty()) {
        chainNext(LyricsBackend::LRCLIB, reqId);
        return;
    }

    const QString fullArtist = query.artist;
    const QString primary = lyricmatch::primaryArtist(fullArtist);
    const QString searchArtist = usePrimaryArtist ? primary : fullArtist;
    const bool canRetry = !usePrimaryArtist && !primary.isEmpty() && primary != fullArtist;

    QUrl url(u"https://lrclib.net/api/search"_s);
    QUrlQuery q;
    q.addQueryItem(u"track_name"_s, query.title);
    if (!searchArtist.isEmpty()) {
        q.addQueryItem(u"artist_name"_s, searchArtist);
    }
    url.setQuery(q);

    auto* reply = getJson(url, lrclibHeaders());
    trackReply(reqId, reply);

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, reqId, canRetry] {
        reply->deleteLater();
        if (reqId != m_currentRequestId) {
            return;
        }
        const auto fail = [this, reqId, canRetry]() {
            if (canRetry) {
                // LRCLIB can reject comma-separated artist names; retry with the primary artist.
                tryLrclibSearch(reqId, true);
            } else {
                chainNext(LyricsBackend::LRCLIB, reqId);
            }
        };
        if (reply->error() != QNetworkReply::NoError) {
            qCDebug(lcLyrics) << "lrclib /search error:" << reply->errorString();
            fail();
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isArray()) {
            fail();
            return;
        }

        const auto matchQuery = makeTrackQuery(m_artist, m_title, m_album, m_duration);
        QJsonObject best;
        qreal bestScore = 0.0;

        for (const auto& value : doc.array()) {
            const QJsonObject obj = value.toObject();
            const QString synced = obj.value(u"syncedLyrics"_s).toString();
            if (synced.isEmpty()) {
                continue;
            }
            const qreal score = lyricmatch::candidateScore(matchQuery, obj.value(u"artistName"_s).toString(),
                obj.value(u"trackName"_s).toString(), obj.value(u"duration"_s).toDouble());
            if (score > bestScore) {
                bestScore = score;
                best = obj;
            }
        }

        if (best.isEmpty() || !lyricmatch::isPlausibleMatch(bestScore)) {
            qCDebug(lcLyrics) << "lrclib search: no plausible match for" << m_artist << "-" << m_title
                              << "best score" << bestScore;
            fail();
            return;
        }

        const QString synced = best.value(u"syncedLyrics"_s).toString();
        const auto lines = parseLrc(synced);
        if (lines.isEmpty()) {
            fail();
            return;
        }

        const QString id = QString::number(static_cast<qint64>(best.value(u"id"_s).toDouble()));
        writeCachedLrc(LyricsBackend::LRCLIB, id, synced);
        setLines(lines, LyricsBackend::LRCLIB);
        const LyricCandidate candidate(LyricsBackend::LRCLIB, id, best.value(u"trackName"_s).toString(),
            best.value(u"artistName"_s).toString(), best.value(u"albumName"_s).toString(),
            best.value(u"duration"_s).toDouble());
        appendCandidates({ candidate });
        m_selected = candidate;
        emit selectedCandidateChanged();
        if (!m_settingFromPrefs) {
            persistTrackPrefs();
        }
        setLoading(false);
    });
}

void Lyrics::tryNetEase(int reqId) {
    if (reqId != m_currentRequestId) {
        return;
    }

    setBackend(LyricsBackend::NetEase);

    const auto query = makeTrackQuery(m_artist, m_title, m_album, m_duration);
    QString text = u"%1 %2"_s.arg(query.artist, query.title).trimmed();
    if (text.isEmpty()) {
        text = query.title;
    }

    QUrl url(u"https://music.163.com/api/search/get"_s);
    QUrlQuery q;
    q.addQueryItem(u"s"_s, text);
    q.addQueryItem(u"type"_s, u"1"_s);
    q.addQueryItem(u"limit"_s, u"10"_s);
    url.setQuery(q);

    auto* reply = getJson(url, netEaseHeaders());
    trackReply(reqId, reply);

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, reqId] {
        reply->deleteLater();
        if (reqId != m_currentRequestId) {
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            qCDebug(lcLyrics) << "netease /search error:" << reply->errorString();
            chainNext(LyricsBackend::NetEase, reqId);
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonArray songs =
            doc.object().value(u"result"_s).toObject().value(u"songs"_s).toArray();

        const auto matchQuery = makeTrackQuery(m_artist, m_title, m_album, m_duration);
        QString bestId;
        qreal bestScore = 0.0;
        for (const auto& value : songs) {
            const QJsonObject song = value.toObject();
            const QStringList names = artistNames(song.value(u"artists"_s).toArray());
            const qreal score = lyricmatch::candidateScore(matchQuery, names.join(u", "_s),
                song.value(u"name"_s).toString(), song.value(u"duration"_s).toDouble() / 1000.0);
            if (score > bestScore) {
                bestScore = score;
                bestId = QString::number(static_cast<qint64>(song.value(u"id"_s).toDouble()));
            }
        }

        if (bestId.isEmpty() || !lyricmatch::isPlausibleMatch(bestScore)) {
            qCDebug(lcLyrics) << "netease: no plausible match for" << m_artist << "-" << m_title
                              << "best score" << bestScore;
            chainNext(LyricsBackend::NetEase, reqId);
            return;
        }

        fetchNetEaseLyricsById(bestId, reqId, true);
    });
}

void Lyrics::tryKugou(int reqId) {
    if (reqId != m_currentRequestId) {
        return;
    }

    setBackend(LyricsBackend::Kugou);

    const auto query = makeTrackQuery(m_artist, m_title, m_album, m_duration);
    QString text = query.artist.isEmpty() ? query.title : u"%1 - %2"_s.arg(query.artist, query.title);

    QUrl url(u"https://lyrics.kugou.com/search"_s);
    QUrlQuery q;
    q.addQueryItem(u"ver"_s, u"1"_s);
    q.addQueryItem(u"man"_s, u"yes"_s);
    q.addQueryItem(u"client"_s, u"pc"_s);
    q.addQueryItem(u"keyword"_s, text);
    if (m_duration > 0) {
        q.addQueryItem(u"duration"_s, QString::number(qRound(m_duration)));
    }
    url.setQuery(q);

    auto* reply = getJson(url, kugouHeaders());
    trackReply(reqId, reply);

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, reqId] {
        reply->deleteLater();
        if (reqId != m_currentRequestId) {
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            qCDebug(lcLyrics) << "kugou /search error:" << reply->errorString();
            chainNext(LyricsBackend::Kugou, reqId);
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonArray candidates = doc.object().value(u"candidates"_s).toArray();

        const auto matchQuery = makeTrackQuery(m_artist, m_title, m_album, m_duration);
        QString bestId;
        qreal bestScore = 0.0;
        for (const auto& value : candidates) {
            const QJsonObject candidate = value.toObject();
            const qreal score = lyricmatch::candidateScore(matchQuery, candidate.value(u"singer"_s).toString(),
                candidate.value(u"song"_s).toString(), candidate.value(u"duration"_s).toDouble() / 1000.0);
            if (score > bestScore) {
                const QString id = kugouCandidateId(candidate.value(u"id"_s).toString(),
                    candidate.value(u"accesskey"_s).toString());
                if (!id.isEmpty()) {
                    bestScore = score;
                    bestId = id;
                }
            }
        }

        if (bestId.isEmpty() || !lyricmatch::isPlausibleMatch(bestScore)) {
            qCDebug(lcLyrics) << "kugou: no plausible match for" << m_artist << "-" << m_title
                              << "best score" << bestScore;
            chainNext(LyricsBackend::Kugou, reqId);
            return;
        }

        fetchKugouLyricsById(bestId, reqId, true);
    });
}

void Lyrics::tryQQMusic(int reqId) {
    if (reqId != m_currentRequestId) {
        return;
    }

    setBackend(LyricsBackend::QQMusic);

    const auto query = makeTrackQuery(m_artist, m_title, m_album, m_duration);
    QString text = u"%1 %2"_s.arg(query.artist, query.title).trimmed();

    QUrl url(u"https://c.y.qq.com/soso/fcgi-bin/client_search_cp"_s);
    QUrlQuery q;
    q.addQueryItem(u"w"_s, text);
    q.addQueryItem(u"format"_s, u"json"_s);
    q.addQueryItem(u"n"_s, u"10"_s);
    q.addQueryItem(u"p"_s, u"1"_s);
    url.setQuery(q);

    auto* reply = getJson(url, qqMusicHeaders());
    trackReply(reqId, reply);

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, reqId] {
        reply->deleteLater();
        if (reqId != m_currentRequestId) {
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            qCDebug(lcLyrics) << "qq music /search error:" << reply->errorString();
            chainNext(LyricsBackend::QQMusic, reqId);
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonArray songs = doc.object()
                                     .value(u"data"_s)
                                     .toObject()
                                     .value(u"song"_s)
                                     .toObject()
                                     .value(u"list"_s)
                                     .toArray();

        const auto matchQuery = makeTrackQuery(m_artist, m_title, m_album, m_duration);
        QString bestId;
        qreal bestScore = 0.0;
        for (const auto& value : songs) {
            const QJsonObject song = value.toObject();
            const QStringList names = artistNames(song.value(u"singer"_s).toArray());
            const qreal score = lyricmatch::candidateScore(matchQuery, names.join(u", "_s),
                song.value(u"songname"_s).toString(), song.value(u"interval"_s).toDouble());
            if (score > bestScore) {
                const QString id = song.value(u"songmid"_s).toString();
                if (!id.isEmpty()) {
                    bestScore = score;
                    bestId = id;
                }
            }
        }

        if (bestId.isEmpty() || !lyricmatch::isPlausibleMatch(bestScore)) {
            qCDebug(lcLyrics) << "qq music: no plausible match for" << m_artist << "-" << m_title
                              << "best score" << bestScore;
            chainNext(LyricsBackend::QQMusic, reqId);
            return;
        }

        fetchQQMusicLyricsById(bestId, reqId, true);
    });
}

void Lyrics::searchLrclibCandidates(int reqId) {
    const auto query = makeTrackQuery(m_artist, m_title, m_album, m_duration);
    if (query.title.isEmpty()) {
        return;
    }

    const QString searchArtist = lyricmatch::primaryArtist(query.artist);
    QUrl url(u"https://lrclib.net/api/search"_s);
    QUrlQuery q;
    q.addQueryItem(u"track_name"_s, query.title);
    if (!searchArtist.isEmpty()) {
        q.addQueryItem(u"artist_name"_s, searchArtist);
    }
    url.setQuery(q);

    auto* reply = getJson(url, lrclibHeaders());
    trackReply(reqId, reply);

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, reqId] {
        reply->deleteLater();
        if (reqId != m_currentRequestId) {
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            qCDebug(lcLyrics) << "lrclib candidates error:" << reply->errorString();
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isArray()) {
            return;
        }

        const auto matchQuery = makeTrackQuery(m_artist, m_title, m_album, m_duration);
        QList<LyricCandidate> add;
        for (const auto& value : doc.array()) {
            const QJsonObject obj = value.toObject();
            if (obj.value(u"syncedLyrics"_s).toString().isEmpty()) {
                continue;
            }
            const QString id = QString::number(static_cast<qint64>(obj.value(u"id"_s).toDouble()));
            const QString title = obj.value(u"trackName"_s).toString();
            const QString artist = obj.value(u"artistName"_s).toString();
            const qreal duration = obj.value(u"duration"_s).toDouble();
            if (lyricmatch::isPlausibleMatch(lyricmatch::candidateScore(matchQuery, artist, title, duration))) {
                add.append(LyricCandidate(LyricsBackend::LRCLIB, id, title, artist,
                    obj.value(u"albumName"_s).toString(), duration));
            }
        }
        appendCandidates(add);
    });
}

void Lyrics::searchNetEaseCandidates(int reqId) {
    const auto query = makeTrackQuery(m_artist, m_title, m_album, m_duration);
    QString text = u"%1 %2"_s.arg(query.artist, query.title).trimmed();
    if (text.isEmpty()) {
        text = query.title;
    }

    QUrl url(u"https://music.163.com/api/search/get"_s);
    QUrlQuery q;
    q.addQueryItem(u"s"_s, text);
    q.addQueryItem(u"type"_s, u"1"_s);
    q.addQueryItem(u"limit"_s, u"10"_s);
    url.setQuery(q);

    auto* reply = getJson(url, netEaseHeaders());
    trackReply(reqId, reply);

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, reqId] {
        reply->deleteLater();
        if (reqId != m_currentRequestId) {
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            qCDebug(lcLyrics) << "netease candidates error:" << reply->errorString();
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonArray songs =
            doc.object().value(u"result"_s).toObject().value(u"songs"_s).toArray();

        const auto matchQuery = makeTrackQuery(m_artist, m_title, m_album, m_duration);
        QList<LyricCandidate> add;
        for (const auto& value : songs) {
            const QJsonObject song = value.toObject();
            const QStringList names = artistNames(song.value(u"artists"_s).toArray());
            const qreal duration = song.value(u"duration"_s).toDouble() / 1000.0;
            const QString title = song.value(u"name"_s).toString();
            const QString artist = names.join(u", "_s);
            if (lyricmatch::isPlausibleMatch(lyricmatch::candidateScore(matchQuery, artist, title, duration))) {
                add.append(LyricCandidate(LyricsBackend::NetEase,
                    QString::number(static_cast<qint64>(song.value(u"id"_s).toDouble())), title, artist, {},
                    duration));
            }
        }
        appendCandidates(add);
    });
}

void Lyrics::searchKugouCandidates(int reqId) {
    const auto query = makeTrackQuery(m_artist, m_title, m_album, m_duration);
    QString text = query.artist.isEmpty() ? query.title : u"%1 - %2"_s.arg(query.artist, query.title);

    QUrl url(u"https://lyrics.kugou.com/search"_s);
    QUrlQuery q;
    q.addQueryItem(u"ver"_s, u"1"_s);
    q.addQueryItem(u"man"_s, u"yes"_s);
    q.addQueryItem(u"client"_s, u"pc"_s);
    q.addQueryItem(u"keyword"_s, text);
    if (m_duration > 0) {
        q.addQueryItem(u"duration"_s, QString::number(qRound(m_duration)));
    }
    url.setQuery(q);

    auto* reply = getJson(url, kugouHeaders());
    trackReply(reqId, reply);

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, reqId] {
        reply->deleteLater();
        if (reqId != m_currentRequestId) {
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            qCDebug(lcLyrics) << "kugou candidates error:" << reply->errorString();
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonArray candidates = doc.object().value(u"candidates"_s).toArray();

        const auto matchQuery = makeTrackQuery(m_artist, m_title, m_album, m_duration);
        QList<LyricCandidate> add;
        for (const auto& value : candidates) {
            const QJsonObject candidate = value.toObject();
            const QString id =
                kugouCandidateId(candidate.value(u"id"_s).toString(), candidate.value(u"accesskey"_s).toString());
            if (id.isEmpty()) {
                continue;
            }
            const QString title = candidate.value(u"song"_s).toString();
            const QString artist = candidate.value(u"singer"_s).toString();
            const qreal duration = candidate.value(u"duration"_s).toDouble() / 1000.0;
            if (lyricmatch::isPlausibleMatch(lyricmatch::candidateScore(matchQuery, artist, title, duration))) {
                add.append(LyricCandidate(LyricsBackend::Kugou, id, title, artist, {}, duration));
            }
        }
        appendCandidates(add);
    });
}

void Lyrics::searchQQMusicCandidates(int reqId) {
    const auto query = makeTrackQuery(m_artist, m_title, m_album, m_duration);
    const QString text = u"%1 %2"_s.arg(query.artist, query.title).trimmed();

    QUrl url(u"https://c.y.qq.com/soso/fcgi-bin/client_search_cp"_s);
    QUrlQuery q;
    q.addQueryItem(u"w"_s, text);
    q.addQueryItem(u"format"_s, u"json"_s);
    q.addQueryItem(u"n"_s, u"10"_s);
    q.addQueryItem(u"p"_s, u"1"_s);
    url.setQuery(q);

    auto* reply = getJson(url, qqMusicHeaders());
    trackReply(reqId, reply);

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, reqId] {
        reply->deleteLater();
        if (reqId != m_currentRequestId) {
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            qCDebug(lcLyrics) << "qq music candidates error:" << reply->errorString();
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonArray songs = doc.object()
                                     .value(u"data"_s)
                                     .toObject()
                                     .value(u"song"_s)
                                     .toObject()
                                     .value(u"list"_s)
                                     .toArray();

        const auto matchQuery = makeTrackQuery(m_artist, m_title, m_album, m_duration);
        QList<LyricCandidate> add;
        for (const auto& value : songs) {
            const QJsonObject song = value.toObject();
            const QString id = song.value(u"songmid"_s).toString();
            if (id.isEmpty()) {
                continue;
            }
            const QStringList names = artistNames(song.value(u"singer"_s).toArray());
            const QString title = song.value(u"songname"_s).toString();
            const QString artist = names.join(u", "_s);
            const qreal duration = song.value(u"interval"_s).toDouble();
            if (lyricmatch::isPlausibleMatch(lyricmatch::candidateScore(matchQuery, artist, title, duration))) {
                add.append(LyricCandidate(LyricsBackend::QQMusic, id, title, artist,
                    song.value(u"albumname"_s).toString(), duration));
            }
        }
        appendCandidates(add);
    });
}

void Lyrics::fetchLrclibById(const QString& id, int reqId, bool chainOnFailure) {
    const QUrl url(u"https://lrclib.net/api/get/"_s + id);
    auto* reply = getJson(url, lrclibHeaders());
    trackReply(reqId, reply);

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, reqId, id, chainOnFailure] {
        reply->deleteLater();
        if (reqId != m_currentRequestId) {
            return;
        }
        const auto fail = [this, reqId, chainOnFailure]() {
            if (chainOnFailure) {
                chainNext(LyricsBackend::LRCLIB, reqId);
            } else if (m_preferredBackend == LyricsBackend::LRCLIB) {
                // LRCLIB's /get/{id} endpoint can fail while its search endpoint is healthy.
                tryLrclibSearch(reqId);
            } else {
                setLoading(false);
            }
        };
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcLyrics) << "lrclib /get/{id} error:" << reply->errorString();
            fail();
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QString synced = doc.object().value(u"syncedLyrics"_s).toString();
        if (synced.isEmpty()) {
            qCDebug(lcLyrics) << "lrclib /get/{id}: no syncedLyrics";
            fail();
            return;
        }
        const auto lines = parseLrc(synced);
        if (lines.isEmpty()) {
            qCDebug(lcLyrics) << "lrclib /get/{id}: no syncable lines";
            fail();
            return;
        }
        writeCachedLrc(LyricsBackend::LRCLIB, id, synced);
        setLines(lines, LyricsBackend::LRCLIB);
        markSelected(LyricsBackend::LRCLIB, id);
        setLoading(false);
    });
}

void Lyrics::fetchNetEaseLyricsById(const QString& id, int reqId, bool chainOnFailure) {
    QUrl url(u"https://music.163.com/api/song/lyric"_s);
    QUrlQuery q;
    q.addQueryItem(u"id"_s, id);
    q.addQueryItem(u"lv"_s, u"1"_s);
    q.addQueryItem(u"kv"_s, u"1"_s);
    q.addQueryItem(u"tv"_s, u"-1"_s);
    url.setQuery(q);

    auto* reply = getJson(url, netEaseHeaders());
    trackReply(reqId, reply);

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, reqId, id, chainOnFailure] {
        reply->deleteLater();
        if (reqId != m_currentRequestId) {
            return;
        }
        const auto fail = [this, reqId, chainOnFailure]() {
            if (chainOnFailure) {
                chainNext(LyricsBackend::NetEase, reqId);
            } else {
                setLoading(false);
            }
        };
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcLyrics) << "netease /lyric error:" << reply->errorString();
            fail();
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QString lrc = doc.object().value(u"lrc"_s).toObject().value(u"lyric"_s).toString();
        if (lrc.isEmpty()) {
            qCDebug(lcLyrics) << "netease /lyric: empty for id" << id;
            fail();
            return;
        }
        lrc = lyricmatch::decodeHtmlEntities(lrc);
        const auto lines = parseLrc(lrc);
        if (lines.isEmpty()) {
            qCDebug(lcLyrics) << "netease /lyric: no syncable lines for id" << id;
            fail();
            return;
        }
        writeCachedLrc(LyricsBackend::NetEase, id, lrc);
        setLines(lines, LyricsBackend::NetEase);
        markSelected(LyricsBackend::NetEase, id);
        setLoading(false);
    });
}

void Lyrics::fetchKugouLyricsById(const QString& id, int reqId, bool chainOnFailure) {
    const qsizetype separator = id.indexOf(u'|');
    if (separator <= 0 || separator + 1 >= id.size()) {
        qCWarning(lcLyrics) << "kugou: malformed candidate id" << id;
        if (chainOnFailure) {
            chainNext(LyricsBackend::Kugou, reqId);
        } else {
            setLoading(false);
        }
        return;
    }

    const QString downloadId = id.left(separator);
    const QString accessKey = id.mid(separator + 1);

    QUrl url(u"https://lyrics.kugou.com/download"_s);
    QUrlQuery q;
    q.addQueryItem(u"ver"_s, u"1"_s);
    q.addQueryItem(u"client"_s, u"pc"_s);
    q.addQueryItem(u"id"_s, downloadId);
    q.addQueryItem(u"accesskey"_s, accessKey);
    q.addQueryItem(u"fmt"_s, u"lrc"_s);
    q.addQueryItem(u"charset"_s, u"utf8"_s);
    url.setQuery(q);

    auto* reply = getJson(url, kugouHeaders());
    trackReply(reqId, reply);

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, reqId, id, downloadId, chainOnFailure] {
        reply->deleteLater();
        if (reqId != m_currentRequestId) {
            return;
        }
        const auto fail = [this, reqId, chainOnFailure]() {
            if (chainOnFailure) {
                chainNext(LyricsBackend::Kugou, reqId);
            } else {
                setLoading(false);
            }
        };
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcLyrics) << "kugou /download error:" << reply->errorString();
            fail();
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QString content = doc.object().value(u"content"_s).toString();
        if (content.isEmpty()) {
            qCDebug(lcLyrics) << "kugou /download: empty content for id" << downloadId;
            fail();
            return;
        }
        const QString lrc = decodeBase64Content(content);
        const auto lines = parseLrc(lrc);
        if (lines.isEmpty()) {
            qCDebug(lcLyrics) << "kugou /download: no syncable lines for id" << downloadId;
            fail();
            return;
        }
        writeCachedLrc(LyricsBackend::Kugou, id, lrc);
        setLines(lines, LyricsBackend::Kugou);
        markSelected(LyricsBackend::Kugou, id);
        setLoading(false);
    });
}

void Lyrics::fetchQQMusicLyricsById(const QString& id, int reqId, bool chainOnFailure) {
    QUrl url(u"https://c.y.qq.com/lyric/fcgi-bin/fcg_query_lyric_new.fcg"_s);
    QUrlQuery q;
    q.addQueryItem(u"songmid"_s, id);
    q.addQueryItem(u"format"_s, u"json"_s);
    q.addQueryItem(u"nobase64"_s, u"1"_s);
    url.setQuery(q);

    auto* reply = getJson(url, qqMusicHeaders());
    trackReply(reqId, reply);

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, reqId, id, chainOnFailure] {
        reply->deleteLater();
        if (reqId != m_currentRequestId) {
            return;
        }
        const auto fail = [this, reqId, chainOnFailure]() {
            if (chainOnFailure) {
                chainNext(LyricsBackend::QQMusic, reqId);
            } else {
                setLoading(false);
            }
        };
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcLyrics) << "qq music /lyric error:" << reply->errorString();
            fail();
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QString lrc = doc.object().value(u"lyric"_s).toString();
        if (lrc.isEmpty()) {
            qCDebug(lcLyrics) << "qq music /lyric: empty for id" << id;
            fail();
            return;
        }
        lrc = lyricmatch::decodeHtmlEntities(lrc);
        const auto lines = parseLrc(lrc);
        if (lines.isEmpty()) {
            qCDebug(lcLyrics) << "qq music /lyric: no syncable lines for id" << id;
            fail();
            return;
        }
        writeCachedLrc(LyricsBackend::QQMusic, id, lrc);
        setLines(lines, LyricsBackend::QQMusic);
        markSelected(LyricsBackend::QQMusic, id);
        setLoading(false);
    });
}

QNetworkReply* Lyrics::getJson(const QUrl& url, const QHash<QByteArray, QByteArray>& headers) {
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setMaximumRedirectsAllowed(5);
    req.setTransferTimeout(k_transferTimeoutMs);
    req.setRawHeader("Cache-Control", "no-cache, no-store");
    req.setRawHeader("Pragma", "no-cache");
    req.setRawHeader("Connection", "close");
    req.setRawHeader("Accept", "application/json");
    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it) {
        req.setRawHeader(it.key(), it.value());
    }
    return m_nam->get(req);
}

void Lyrics::onPreferredBackendConfigChanged() {
    const LyricsBackend desired = config::ConfigSingleton::instance()->services()->lyricsBackend();
    if (desired == m_preferredBackend) {
        return;
    }
    m_preferredBackend = desired;
    emit preferredBackendChanged();
    scheduleLoad();
}

void Lyrics::onLyricsDirChanged() {
    scheduleLoad();
}

void Lyrics::loadLyricsMap() {
    m_lyricsMap = {};
    m_lyricsMapLoaded = false;

    QFile f(lyricsMapPath());
    if (!f.open(QIODevice::ReadOnly)) {
        m_lyricsMapLoaded = true;
        return;
    }
    const QByteArray bytes = f.readAll();
    f.close();

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
    if (err.error != QJsonParseError::NoError) {
        qCWarning(lcLyrics) << "lyrics_map.json parse error:" << err.errorString();
        m_lyricsMapLoaded = true;
        return;
    }
    m_lyricsMap = doc.object();
    m_lyricsMapLoaded = true;
}

void Lyrics::persistTrackPrefs() {
    if (!m_lyricsMapLoaded || trackKey().isEmpty()) {
        return;
    }
    const QString key = trackKey();
    QJsonObject entry = m_lyricsMap.value(key).toObject();
    entry.insert(u"offset"_s, m_offset);
    if (m_selected.isValid()) {
        entry.insert(u"backend"_s, backendKey(m_selected.backend()));
        entry.insert(u"id"_s, m_selected.id());
    }
    m_lyricsMap.insert(key, entry);

    QDir().mkpath(stateDir());

    QSaveFile out(lyricsMapPath());
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCWarning(lcLyrics) << "cannot open" << lyricsMapPath() << "for write:" << out.errorString();
        return;
    }
    const QByteArray bytes = QJsonDocument(m_lyricsMap).toJson(QJsonDocument::Compact);
    if (out.write(bytes) != bytes.size()) {
        qCWarning(lcLyrics) << "short write to" << lyricsMapPath();
        out.cancelWriting();
        return;
    }
    if (!out.commit()) {
        qCWarning(lcLyrics) << "commit failed for" << lyricsMapPath() << ":" << out.errorString();
    }
}

QString Lyrics::lyricsDir() {
    QString dir = config::ConfigSingleton::instance()->paths()->lyricsDir();
    if (dir.isEmpty()) {
        return {};
    }
    if (dir == u"~"_s) {
        dir = QDir::homePath();
    } else if (dir.startsWith(u"~/"_s)) {
        dir.replace(0, 1, QDir::homePath());
    }
    while (dir.endsWith(u'/') && dir.size() > 1) {
        dir.chop(1);
    }
    return dir;
}

QString Lyrics::lyricsMapPath() {
    return stateDir() + u"/lyrics_map.json"_s;
}

QString Lyrics::trackKey() const {
    if (m_artist.isEmpty() && m_title.isEmpty()) {
        return {};
    }
    return u"%1 - %2"_s.arg(m_artist, m_title);
}

QString Lyrics::backendKey(LyricsBackend value) {
    switch (value) {
    case LyricsBackend::Local:
        return u"Local"_s;
    case LyricsBackend::LRCLIB:
        return u"LRCLIB"_s;
    case LyricsBackend::NetEase:
        return u"NetEase"_s;
    case LyricsBackend::Kugou:
        return u"Kugou"_s;
    case LyricsBackend::QQMusic:
        return u"QQMusic"_s;
    case LyricsBackend::Auto:
    default:
        return u"Auto"_s;
    }
}

LyricsBackend Lyrics::backendFromKey(const QString& key) {
    if (key.compare(u"Local"_s, Qt::CaseInsensitive) == 0) {
        return LyricsBackend::Local;
    }
    if (key.compare(u"LRCLIB"_s, Qt::CaseInsensitive) == 0) {
        return LyricsBackend::LRCLIB;
    }
    if (key.compare(u"NetEase"_s, Qt::CaseInsensitive) == 0) {
        return LyricsBackend::NetEase;
    }
    if (key.compare(u"Kugou"_s, Qt::CaseInsensitive) == 0) {
        return LyricsBackend::Kugou;
    }
    if (key.compare(u"QQMusic"_s, Qt::CaseInsensitive) == 0
        || key.compare(u"QQ"_s, Qt::CaseInsensitive) == 0) {
        return LyricsBackend::QQMusic;
    }
    return LyricsBackend::Auto;
}

const QString& Lyrics::stateDir() {
    static const QString k_dir = [] {
        QString state = qEnvironmentVariable("XDG_STATE_HOME");
        if (state.isEmpty()) {
            state = QDir::homePath() + u"/.local/state"_s;
        }
        return state + u"/caelestia/lyrics"_s;
    }();
    return k_dir;
}

const QString& Lyrics::cacheDir() {
    static const QString k_dir = [] {
        QString cache = qEnvironmentVariable("XDG_CACHE_HOME");
        if (cache.isEmpty()) {
            cache = QDir::homePath() + u"/.cache"_s;
        }
        return cache + u"/caelestia/lyrics"_s;
    }();
    return k_dir;
}

QString Lyrics::cachePathFor(LyricsBackend backend, const QString& id) {
    if (id.isEmpty() || backend == LyricsBackend::Auto || backend == LyricsBackend::Local) {
        return {};
    }
    return u"%1/%2/%3.lrc"_s.arg(cacheDir(), backendKey(backend), sanitizeFilenamePart(id));
}

QString Lyrics::readCachedLrc(LyricsBackend backend, const QString& id) {
    const QString path = cachePathFor(backend, id);
    if (path.isEmpty()) {
        return {};
    }
    return readTextFile(path);
}

void Lyrics::writeCachedLrc(LyricsBackend backend, const QString& id, const QString& text) {
    if (text.isEmpty()) {
        return;
    }
    const QString path = cachePathFor(backend, id);
    if (path.isEmpty()) {
        return;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());

    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCWarning(lcLyrics) << "cannot open" << path << "for write:" << out.errorString();
        return;
    }
    const QByteArray bytes = text.toUtf8();
    if (out.write(bytes) != bytes.size()) {
        qCWarning(lcLyrics) << "short write to" << path;
        out.cancelWriting();
        return;
    }
    if (!out.commit()) {
        qCWarning(lcLyrics) << "commit failed for" << path << ":" << out.errorString();
    }
}

QVector<LyricLine> Lyrics::parseLrc(const QString& text) {
    QVector<LyricLine> result;
    if (text.isEmpty()) {
        return result;
    }

    static const QRegularExpression offsetRe(
        uR"(\[offset:\s*([+-]?\d+)\s*\])"_s, QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression timeRe(
        uR"(\[(?:(\d{1,2}):)?(\d{1,3}):(\d{2})(?:[.:,](\d{1,3}))?\])"_s);
    static const QRegularExpression wordTimeRe(
        uR"(<\d{1,3}:\d{2}(?:[.:,]\d{1,3})?>)"_s);

    qreal globalOffset = 0.0;
    const auto offsetMatch = offsetRe.match(text);
    if (offsetMatch.hasMatch()) {
        globalOffset = offsetMatch.captured(1).toDouble() / 1000.0;
    }

    const QStringList lines = text.split(QRegularExpression(uR"(\r\n|\n|\r)"_s));
    for (const QString& line : lines) {
        QList<QRegularExpressionMatch> matches;
        auto it = timeRe.globalMatch(line);
        while (it.hasNext()) {
            matches.append(it.next());
        }
        if (matches.isEmpty()) {
            continue;
        }

        QString lyric = line;
        lyric.remove(timeRe);
        lyric.remove(wordTimeRe);
        lyric = lyric.trimmed();

        if (lyric.isEmpty() && (result.isEmpty() || result.last().text.isEmpty())) {
            continue;
        }

        const auto first = matches.first();
        const qreal firstTime = first.captured(1).isEmpty()
            ? first.captured(2).toInt() * 60.0 + first.captured(3).toDouble()
            : first.captured(1).toInt() * 3600.0 + first.captured(2).toInt() * 60.0
                + first.captured(3).toDouble();

        if (isCreditLine(lyric, firstTime + globalOffset)) {
            continue;
        }

        for (const auto& match : matches) {
            qreal time = 0.0;
            if (match.captured(1).isEmpty()) {
                time = match.captured(2).toInt() * 60.0 + match.captured(3).toDouble();
            } else {
                time = match.captured(1).toInt() * 3600.0 + match.captured(2).toInt() * 60.0
                    + match.captured(3).toDouble();
            }

            const QString fractionText = match.captured(4);
            if (!fractionText.isEmpty()) {
                int fraction = fractionText.toInt();
                switch (fractionText.size()) {
                case 1:
                    fraction *= 100;
                    break;
                case 2:
                    fraction *= 10;
                    break;
                default:
                    break;
                }
                time += static_cast<qreal>(fraction) / 1000.0;
            }
            time += globalOffset;

            if (!result.isEmpty() && qFuzzyCompare(result.last().time, time) && result.last().text == lyric) {
                continue;
            }
            result.append(LyricLine{ .time = time, .text = lyric });
        }
    }

    while (!result.isEmpty() && result.last().text.isEmpty()) {
        result.removeLast();
    }

    std::ranges::stable_sort(result, [](const LyricLine& a, const LyricLine& b) {
        return a.time < b.time;
    });

    return result;
}

} // namespace caelestia::services
