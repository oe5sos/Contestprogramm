#include "core/terrain/SrtmTileLoader.h"

#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>

#include <zlib.h>

#include <algorithm>
#include <cmath>

namespace Contestprogramm {

namespace {

// The public, unauthenticated AWS Open Data "Terrain Tiles" bucket --
// see SrtmTileLoader.h's own doc comment for how this was verified
// (2026-09-12, a real N47E013 tile downloaded and checked against the
// documented SRTM1 byte count).
constexpr auto kSrtmBaseUrl = "https://s3.amazonaws.com/elevation-tiles-prod/skadi";

// Real gzip inflate via zlib directly -- windowBits = MAX_WBITS(15) + 16
// asks zlib to expect/strip a genuine gzip header+trailer (not the
// zlib-wrapped stream QByteArray::qUncompress() expects, and not raw
// deflate either -- Qt has no built-in real-gzip decoder). Returns an
// empty QByteArray on any error (corrupt stream, truncated download,
// ...) -- the caller treats that the same as a network failure.
QByteArray gzipInflate(const QByteArray& compressed)
{
    if (compressed.isEmpty()) {
        return {};
    }

    z_stream stream{};
    if (inflateInit2(&stream, 15 + 16) != Z_OK) {
        return {};
    }

    QByteArray output;
    output.resize(qMax<qsizetype>(compressed.size() * 4, 65536));
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressed.constData()));
    stream.avail_in = static_cast<uInt>(compressed.size());

    qsizetype totalOut = 0;
    int ret = Z_OK;
    while (ret != Z_STREAM_END) {
        if (totalOut == output.size()) {
            output.resize(output.size() * 2);
        }
        stream.next_out = reinterpret_cast<Bytef*>(output.data() + totalOut);
        const uInt availBefore = static_cast<uInt>(output.size() - totalOut);
        stream.avail_out = availBefore;

        ret = inflate(&stream, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            inflateEnd(&stream);
            return {};
        }
        totalOut += (availBefore - stream.avail_out);
        if (ret != Z_STREAM_END && stream.avail_in == 0 && availBefore == stream.avail_out) {
            // No progress and no more input left -- truncated/corrupt stream.
            inflateEnd(&stream);
            return {};
        }
    }

    inflateEnd(&stream);
    output.resize(totalOut);
    return output;
}

// Parses raw (already-decompressed) .hgt bytes into row-major 16-bit
// signed big-endian samples -- the real NASA SRTM1 on-disk format.
// Returns an empty vector if `raw` isn't exactly the expected size
// (SrtmTileLoader::kSrtmSize squared, 2 bytes each) -- a corrupt or
// truncated download/cache file is never silently misread as a
// differently-sized grid.
QVector<qint16> parseHgtBytes(const QByteArray& raw)
{
    const qsizetype expectedBytes =
        qsizetype(SrtmTileLoader::kSrtmSize) * qsizetype(SrtmTileLoader::kSrtmSize) * 2;
    if (raw.size() != expectedBytes) {
        return {};
    }
    QVector<qint16> samples(SrtmTileLoader::kSrtmSize * SrtmTileLoader::kSrtmSize);
    const auto* bytes = reinterpret_cast<const uchar*>(raw.constData());
    for (qsizetype i = 0; i < samples.size(); ++i) {
        // Big-endian: high byte first.
        samples[i] = static_cast<qint16>((bytes[i * 2] << 8) | bytes[i * 2 + 1]);
    }
    return samples;
}

} // namespace

SrtmTileLoader::SrtmTileLoader(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_cacheDir = base + QStringLiteral("/srtm");
    QDir().mkpath(m_cacheDir);
}

QString SrtmTileLoader::tileNameForLatLon(double lat, double lon)
{
    const int tileLat = static_cast<int>(std::floor(lat));
    const int tileLon = static_cast<int>(std::floor(lon));
    return QStringLiteral("%1%2%3%4")
        .arg(tileLat >= 0 ? QLatin1Char('N') : QLatin1Char('S'))
        .arg(qAbs(tileLat), 2, 10, QLatin1Char('0'))
        .arg(tileLon >= 0 ? QLatin1Char('E') : QLatin1Char('W'))
        .arg(qAbs(tileLon), 3, 10, QLatin1Char('0'));
}

bool SrtmTileLoader::isTileLoaded(const QString& tileName) const
{
    return m_tiles.contains(tileName);
}

std::optional<double> SrtmTileLoader::elevationAt(double lat, double lon) const
{
    const QString tileName = tileNameForLatLon(lat, lon);
    const auto it = m_tiles.constFind(tileName);
    if (it == m_tiles.constEnd()) {
        return std::nullopt;
    }
    const QVector<qint16>& samples = it.value();

    const int tileLat = static_cast<int>(std::floor(lat));
    const int tileLon = static_cast<int>(std::floor(lon));

    // Fractional pixel position -- row 0 is the tile's NORTH edge, col 0
    // is its WEST edge (the real .hgt row order), so row increases
    // southward while latitude decreases.
    const double colF = (lon - tileLon) * (kSrtmSize - 1);
    const double rowF = ((tileLat + 1) - lat) * (kSrtmSize - 1);

    const int col0 = std::clamp(static_cast<int>(std::floor(colF)), 0, kSrtmSize - 2);
    const int row0 = std::clamp(static_cast<int>(std::floor(rowF)), 0, kSrtmSize - 2);
    const double fx = std::clamp(colF - col0, 0.0, 1.0);
    const double fy = std::clamp(rowF - row0, 0.0, 1.0);

    const auto sampleAt = [&](int row, int col) -> std::optional<double> {
        const qint16 raw = samples.at(row * kSrtmSize + col);
        if (raw == kVoidValue) {
            return std::nullopt;
        }
        return static_cast<double>(raw);
    };

    const std::optional<double> topLeft = sampleAt(row0, col0);
    const std::optional<double> topRight = sampleAt(row0, col0 + 1);
    const std::optional<double> bottomLeft = sampleAt(row0 + 1, col0);
    const std::optional<double> bottomRight = sampleAt(row0 + 1, col0 + 1);

    // Bilinear when all four corners are real data (the overwhelming
    // common case); when some corners are void, renormalize over
    // whichever ARE real data so a lone void pixel doesn't blank out
    // an otherwise-answerable point; nullopt only when every corner is
    // void.
    double weightedSum = 0.0;
    double weightTotal = 0.0;
    if (topLeft) {
        weightedSum += *topLeft * (1 - fx) * (1 - fy);
        weightTotal += (1 - fx) * (1 - fy);
    }
    if (topRight) {
        weightedSum += *topRight * fx * (1 - fy);
        weightTotal += fx * (1 - fy);
    }
    if (bottomLeft) {
        weightedSum += *bottomLeft * (1 - fx) * fy;
        weightTotal += (1 - fx) * fy;
    }
    if (bottomRight) {
        weightedSum += *bottomRight * fx * fy;
        weightTotal += fx * fy;
    }

    if (weightTotal <= 0.0) {
        return std::nullopt;
    }
    return weightedSum / weightTotal;
}

void SrtmTileLoader::ensureTileAvailable(double lat, double lon)
{
    const QString tileName = tileNameForLatLon(lat, lon);
    if (m_tiles.contains(tileName) || m_pendingDownloads.contains(tileName)) {
        return;
    }
    loadFromDiskCache(tileName);
}

void SrtmTileLoader::loadFromDiskCache(const QString& tileName)
{
    QFile file(m_cacheDir + QLatin1Char('/') + tileName + QStringLiteral(".hgt"));
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        startDownload(tileName);
        return;
    }
    const QByteArray raw = file.readAll();
    file.close();
    const QVector<qint16> samples = parseHgtBytes(raw);
    if (samples.isEmpty()) {
        // Corrupt/incomplete cache file from a previous interrupted run
        // -- remove it and re-fetch rather than permanently failing.
        QFile::remove(file.fileName());
        startDownload(tileName);
        return;
    }
    finishLoadingTile(tileName, samples, QByteArray());
}

void SrtmTileLoader::startDownload(const QString& tileName)
{
    m_pendingDownloads.insert(tileName);
    const QString latDir = tileName.left(3); // "N47" / "S12" -- the skadi bucket's own directory scheme
    const QUrl url(QStringLiteral("%1/%2/%3.hgt.gz").arg(QLatin1String(kSrtmBaseUrl), latDir, tileName));
    QNetworkReply* reply = m_network->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, tileName] {
        handleDownloadFinished(reply, tileName);
    });
}

void SrtmTileLoader::handleDownloadFinished(QNetworkReply* reply, const QString& tileName)
{
    reply->deleteLater();
    m_pendingDownloads.remove(tileName);

    if (reply->error() != QNetworkReply::NoError) {
        emit tileLoadFailed(tileName, reply->errorString());
        return;
    }
    const QByteArray compressed = reply->readAll();
    const QByteArray raw = gzipInflate(compressed);
    const QVector<qint16> samples = parseHgtBytes(raw);
    if (samples.isEmpty()) {
        emit tileLoadFailed(tileName,
                             QStringLiteral("unexpected tile size after decompression (%1 bytes)").arg(raw.size()));
        return;
    }
    finishLoadingTile(tileName, samples, raw);
}

void SrtmTileLoader::finishLoadingTile(const QString& tileName, const QVector<qint16>& samples,
                                        const QByteArray& rawBytesToCache)
{
    m_tiles.insert(tileName, samples);
    if (!rawBytesToCache.isEmpty()) {
        QFile cacheFile(m_cacheDir + QLatin1Char('/') + tileName + QStringLiteral(".hgt"));
        if (cacheFile.open(QIODevice::WriteOnly)) {
            cacheFile.write(rawBytesToCache);
        }
    }
    emit tileLoaded(tileName);
}

void SrtmTileLoader::injectTileForTest(const QString& tileName, const QVector<qint16>& samples)
{
    m_tiles.insert(tileName, samples);
}

} // namespace Contestprogramm
