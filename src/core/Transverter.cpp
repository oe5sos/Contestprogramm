#include "core/Transverter.h"

#include "core/BandUtils.h"
#include "data/ContestDatabase.h"

namespace Contestprogramm {

namespace {
const QString kEnabledKey = QStringLiteral("transverter_enabled");
const QString kIfBandKey = QStringLiteral("transverter_if_band");
const QString kRfBandKey = QStringLiteral("transverter_rf_band");
const QString kOffsetKey = QStringLiteral("transverter_offset_hz");
} // namespace

qint64 TransverterSetup::rfFrequencyHz(qint64 rigHz) const
{
    qint64 low = 0;
    qint64 high = 0;
    if (!active() || !bandRangeHz(ifBand, low, high) || rigHz < low || rigHz > high) {
        return rigHz;
    }
    return rigHz + offsetHz;
}

qint64 TransverterSetup::rigFrequencyHz(qint64 rfHz) const
{
    qint64 low = 0;
    qint64 high = 0;
    if (!active() || !bandRangeHz(rfBand, low, high) || rfHz < low || rfHz > high) {
        return rfHz;
    }
    return rfHz - offsetHz;
}

QString TransverterSetup::describe() const
{
    if (!configured()) {
        return QString();
    }
    const double offsetMHz = offsetHz / 1e6;
    const QString offsetText = qFuzzyCompare(offsetMHz, double(qRound(offsetMHz)))
        ? QString::number(qRound(offsetMHz))
        : QString::number(offsetMHz, 'f', 3);
    return QStringLiteral("%1 → %2 (%3%4 MHz)").arg(ifBand, rfBand, offsetHz > 0 ? QStringLiteral("+") : QString(), offsetText);
}

qint64 TransverterSetup::defaultOffsetHz(const QString& ifBand, const QString& rfBand)
{
    const qint64 ifBase = bandBaseHz(ifBand);
    const qint64 rfBase = bandBaseHz(rfBand);
    if (ifBase == 0 || rfBase == 0 || ifBase == rfBase) {
        return 0;
    }
    return rfBase - ifBase;
}

TransverterSetup TransverterSetup::load(const ContestDatabase& database)
{
    TransverterSetup setup;
    setup.enabled = database.settingValue(kEnabledKey) == QStringLiteral("1");
    setup.ifBand = database.settingValue(kIfBandKey).trimmed();
    setup.rfBand = database.settingValue(kRfBandKey).trimmed();
    bool ok = false;
    const qint64 offset = database.settingValue(kOffsetKey).toLongLong(&ok);
    setup.offsetHz = ok ? offset : 0;
    return setup;
}

void TransverterSetup::save(ContestDatabase& database) const
{
    database.setSettingValue(kEnabledKey, enabled ? QStringLiteral("1") : QStringLiteral("0"));
    database.setSettingValue(kIfBandKey, ifBand);
    database.setSettingValue(kRfBandKey, rfBand);
    database.setSettingValue(kOffsetKey, QString::number(offsetHz));
}

} // namespace Contestprogramm
