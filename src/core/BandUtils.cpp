#include "core/BandUtils.h"

namespace Contestprogramm {

namespace {

struct BandRange {
    const char* label;
    qint64 baseHz;
    qint64 lowHz;
    qint64 highHz;
};

// Base = the band as it is named; the range tolerates a rig sweeping a
// little past the allocation.
constexpr BandRange kBands[] = {
    {"28", 28000000LL, 28000000LL, 29700000LL},
    {"50", 50000000LL, 50000000LL, 54000000LL},
    {"70", 70000000LL, 70000000LL, 70500000LL},
    {"144", 144000000LL, 144000000LL, 148000000LL},
    {"432", 432000000LL, 420000000LL, 450000000LL},
    {"1296", 1296000000LL, 1240000000LL, 1300000000LL},
    {"2320", 2320000000LL, 2300000000LL, 2450000000LL},
    {"3400", 3400000000LL, 3400000000LL, 3475000000LL},
    {"5760", 5760000000LL, 5650000000LL, 5850000000LL},
    {"10368", 10368000000LL, 10000000000LL, 10500000000LL},
};

} // namespace

QString bandLabelForFrequencyHz(qint64 hz)
{
    for (const BandRange& band : kBands) {
        if (hz >= band.lowHz && hz <= band.highHz) {
            return QString::fromLatin1(band.label);
        }
    }
    return QString();
}

QStringList knownBands()
{
    QStringList bands;
    for (const BandRange& band : kBands) {
        bands << QString::fromLatin1(band.label);
    }
    return bands;
}

bool bandRangeHz(const QString& band, qint64& lowHz, qint64& highHz)
{
    for (const BandRange& range : kBands) {
        if (band == QLatin1String(range.label)) {
            lowHz = range.lowHz;
            highHz = range.highHz;
            return true;
        }
    }
    return false;
}

qint64 bandBaseHz(const QString& band)
{
    for (const BandRange& range : kBands) {
        if (band == QLatin1String(range.label)) {
            return range.baseHz;
        }
    }
    return 0;
}

} // namespace Contestprogramm
