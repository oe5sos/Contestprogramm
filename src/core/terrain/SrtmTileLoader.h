#pragma once

#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

#include <optional>

class QNetworkAccessManager;
class QNetworkReply;

namespace Contestprogramm {

// Fetches, caches, and reads real NASA SRTM1 (1 arc-second, ~30m)
// elevation data -- the actual data source for Phase 2's terrain
// line-of-sight (see PathProfile.h/LineOfSight.h). No offline dataset
// ships with the app (SRTM tiles for a 500km operational radius would
// be many hundreds of megabytes -- see the plan's own "Cache pro
// Grid-Paar" note); tiles are fetched ON DEMAND the first time a
// candidate's bearing needs them, then cached to local disk
// permanently, so a real contest weekend builds up exactly the
// coverage actually used, not the whole planet.
//
// Data source: the public, unauthenticated AWS Open Data "Terrain
// Tiles" bucket's "skadi" format (elevation-tiles-prod.s3.amazonaws.com/
// skadi/<N|S>YY/<N|S>YY<E|W>XXX.hgt.gz), which is genuine gzip-
// compressed NASA SRTM1 .hgt data (3601x3601 samples, 16-bit signed
// big-endian, void=-32768) -- verified against a real downloaded tile
// for this task (N47E013, the Feuerkogel/JN67VV area): decompresses to
// exactly 3601*3601*2 = 25,934,402 bytes, the documented SRTM1 size.
// SRTM3 (the plan's original, lower-resolution suggestion) was
// superseded once this SRTM1 source was confirmed live and free --
// better resolution at no extra integration cost.
//
// Gzip decompression uses zlib directly (Qt's own qUncompress()
// expects Qt's own zlib-plus-4-byte-length-prefix wrapper, not a real
// gzip stream) -- see SrtmTileLoader.cpp's own comment.
class SrtmTileLoader : public QObject {
    Q_OBJECT

public:
    explicit SrtmTileLoader(QObject* parent = nullptr);

    // The SRTM tile name (e.g. "N47E013") covering a given lat/lon --
    // named by its south-west corner, the standard SRTM convention.
    // Public so PathProfile/tests can reason about which tiles a path
    // needs without duplicating this formula.
    static QString tileNameForLatLon(double lat, double lon);

    // True once `tileName` is parsed and held in the in-memory cache,
    // ready for elevationAt() to actually answer for points inside it.
    bool isTileLoaded(const QString& tileName) const;

    // Elevation in metres at the given point, bilinearly interpolated
    // from the covering tile's four nearest samples -- std::nullopt if
    // that tile isn't loaded yet (call ensureTileAvailable() first and
    // wait for tileLoaded()/tileLoadFailed()) or if every one of the
    // four nearest samples is a genuine SRTM void (open ocean far from
    // any coastline, the one case real data still doesn't cover).
    std::optional<double> elevationAt(double lat, double lon) const;

    // Kicks off loading the tile covering (lat, lon) if it isn't
    // already loaded or in flight: local disk cache first
    // (QStandardPaths::AppDataLocation + "/srtm/<tileName>.hgt", the
    // already-decompressed form, written by a previous successful
    // fetch), then a real HTTP GET + gzip inflate if not cached on
    // disk either. A no-op if the tile is already loaded or already
    // being fetched -- safe to call repeatedly (e.g. once per sample
    // along a path that crosses the same tile many times).
    void ensureTileAvailable(double lat, double lon);

    // Test-only direct injection of a tile's samples, bypassing both
    // the network and the disk cache entirely -- same "*ForTest"
    // naming convention On4kstClient::parseDxSpotLineForTest() already
    // established in this codebase for a unit-testable seam. `samples`
    // must be exactly kSrtmSize*kSrtmSize entries, row-major from the
    // tile's NW corner (the real .hgt row order).
    void injectTileForTest(const QString& tileName, const QVector<qint16>& samples);

    // SRTM1 tile dimensions -- 3601 samples per degree (1 arc-second),
    // both axes. Public so PathProfile/tests can size synthetic tiles
    // correctly for injectTileForTest().
    static constexpr int kSrtmSize = 3601;
    // SRTM's own void sentinel (NASA spec) -- a sample with no data,
    // never a real elevation.
    static constexpr qint16 kVoidValue = -32768;

signals:
    // `tileName` is now loaded and elevationAt() will answer for it --
    // emitted whether the tile came from the disk cache or a fresh
    // download.
    void tileLoaded(const QString& tileName);
    // The tile could not be obtained (network error, HTTP error status,
    // corrupt/undersized data after decompression) -- `error` is a
    // short human-readable reason. The caller's line-of-sight
    // computation should treat this exactly like "still pending
    // forever": Unknown, not Blocked (see LineOfSight.h's own doc
    // comment on that distinction) -- a tile fetch failure is never
    // evidence the path is obstructed.
    void tileLoadFailed(const QString& tileName, const QString& error);

private:
    void loadFromDiskCache(const QString& tileName);
    void startDownload(const QString& tileName);
    void handleDownloadFinished(QNetworkReply* reply, const QString& tileName);
    // `rawBytesToCache`: the original (decompressed) .hgt bytes to
    // persist to the disk cache, or an empty QByteArray to skip the
    // write (the tile already came FROM the disk cache, so writing it
    // straight back would be a redundant no-op).
    void finishLoadingTile(const QString& tileName, const QVector<qint16>& samples,
                            const QByteArray& rawBytesToCache);

    QNetworkAccessManager* m_network;
    QMap<QString, QVector<qint16>> m_tiles;
    QSet<QString> m_pendingDownloads;
    QString m_cacheDir;
};

} // namespace Contestprogramm
