#pragma once

#include <qglobal.h>
#include <qstring.h>
#include <qstringlist.h>

namespace caelestia::services::lyricmatch {

struct TrackQuery {
    QString artist;
    QString title;
    QString album;
    qreal duration = 0.0;
    QStringList artists;
    QStringList titles;
    QStringList queries;
};

[[nodiscard]] QString decodeHtmlEntities(QString text);
[[nodiscard]] QString removeBracketGroups(const QString& text);
[[nodiscard]] QString cleanTitle(const QString& title);
[[nodiscard]] QString cleanArtist(const QString& artist);
[[nodiscard]] QString cleanAlbum(const QString& album);
[[nodiscard]] QString normalizeKey(const QString& text);
[[nodiscard]] QString primaryArtist(const QString& artist);
[[nodiscard]] QStringList artistVariants(const QString& artist);
[[nodiscard]] QStringList titleVariants(const QString& title);
[[nodiscard]] QStringList searchQueries(const QString& artist, const QString& title);
[[nodiscard]] TrackQuery makeTrackQuery(const QString& artist, const QString& title, const QString& album, qreal duration);
[[nodiscard]] qreal textSimilarity(const QString& left, const QString& right);
[[nodiscard]] qreal bestSimilarity(const QStringList& left, const QStringList& right);
[[nodiscard]] qreal durationSimilarity(qreal left, qreal right);
[[nodiscard]] qreal candidateScore(
    const TrackQuery& query, const QString& candidateArtist, const QString& candidateTitle, qreal candidateDuration);
[[nodiscard]] bool isPlausibleMatch(qreal score);

} // namespace caelestia::services::lyricmatch
