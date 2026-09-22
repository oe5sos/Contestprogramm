#include "data/CabrilloExporter.h"

#include "BuildInfo.h"
#include "app/ContestSettings.h"
#include "core/BandUtils.h"
#include "data/ContestDatabase.h"
#include "data/ContestDefinition.h"
#include "data/QsoRecord.h"

#include <QDateTime>
#include <QSet>

#include <algorithm>

namespace Contestprogramm {

namespace {

// Cabrillo v3 mode codes: CW, PH, FM, RY, DG. Anything unrecognized
// passes through unchanged (best effort) rather than being silently
// dropped from the exported line.
QString cabrilloModeCode(const QString& mode)
{
    const QString m = mode.trimmed().toUpper();
    if (m == QStringLiteral("CW")) { return QStringLiteral("CW"); }
    if (m == QStringLiteral("FM")) { return QStringLiteral("FM"); }
    if (m == QStringLiteral("RTTY") || m == QStringLiteral("RY")) { return QStringLiteral("RY"); }
    if (m == QStringLiteral("SSB") || m == QStringLiteral("USB") || m == QStringLiteral("LSB")
        || m == QStringLiteral("PH") || m == QStringLiteral("PHONE")) {
        return QStringLiteral("PH");
    }
    if (m == QStringLiteral("DIGITAL") || m == QStringLiteral("DATA") || m == QStringLiteral("DG")
        || m == QStringLiteral("FT8") || m == QStringLiteral("FT4")) {
        return QStringLiteral("DG");
    }
    return m;
}

// The QSO line's first field. Cabrillo v3 wants the frequency in kHz
// below 30 MHz and a band designator above it -- and the designator is
// the ARRL/CQ spelling ("1.2G"), not this program's own band label
// ("1296"). A label the table below does not know passes through
// unchanged, the same best-effort rule cabrilloModeCode() follows.
QString cabrilloQsoBandField(const QsoRecord& record)
{
    const qint64 base = bandBaseHz(record.band);
    if (base > 0 && base < 30000000LL) {
        const qint64 hz = record.freqHz.value_or(base);
        return QString::number(hz / 1000);
    }
    static const struct { const char* band; const char* designator; } kDesignators[] = {
        {"1296", "1.2G"}, {"2320", "2.3G"}, {"3400", "3.4G"}, {"5760", "5.7G"}, {"10368", "10G"},
    };
    for (const auto& entry : kDesignators) {
        if (record.band == QLatin1String(entry.band)) {
            return QString::fromLatin1(entry.designator);
        }
    }
    return record.band;
}

// CATEGORY-BAND speaks wavelengths ("20M"), not the megahertz this
// program names its bands after.
QString cabrilloCategoryBand(const QString& band)
{
    static const struct { const char* band; const char* category; } kCategories[] = {
        {"1.8", "160M"}, {"3.5", "80M"}, {"7", "40M"}, {"10", "30M"}, {"14", "20M"},
        {"18", "17M"}, {"21", "15M"}, {"24", "12M"}, {"28", "10M"}, {"50", "6M"},
        {"70", "4M"}, {"144", "2M"}, {"432", "432"}, {"1296", "1.2G"}, {"2320", "2.3G"},
        {"3400", "3.4G"}, {"5760", "5.7G"}, {"10368", "10G"},
    };
    for (const auto& entry : kCategories) {
        if (band == QLatin1String(entry.band)) {
            return QString::fromLatin1(entry.category);
        }
    }
    return band;
}

QString formatDate(const QString& timestampUtc)
{
    const QDateTime dt = QDateTime::fromString(timestampUtc, Qt::ISODate);
    if (!dt.isValid()) {
        return timestampUtc;
    }
    return dt.date().toString(QStringLiteral("yyyy-MM-dd"));
}

QString formatTime(const QString& timestampUtc)
{
    const QDateTime dt = QDateTime::fromString(timestampUtc, Qt::ISODate);
    if (!dt.isValid()) {
        return QStringLiteral("0000");
    }
    return dt.time().toString(QStringLiteral("HHmm"));
}

} // namespace

CabrilloExporter::CabrilloExporter(ContestDatabase& database)
    : m_database(&database)
{
}

QString CabrilloExporter::exportContest(const QString& contestId,
                                        const ContestDefinition& definition,
                                        const ContestSettings& settings,
                                        const QString& categoryPower) const
{
    QVector<QsoRecord> records = m_database->qsosForContest(contestId);
    // A QSO marked invalid (see QsoRecord::isInvalid / this task's
    // report on DXLog.net's own "mark invalid, never delete" model)
    // never leaves the operator's own log, but it must not appear in a
    // submitted Cabrillo file -- excluded here, before it can influence
    // CATEGORY-BAND/CATEGORY-MODE either.
    records.erase(std::remove_if(records.begin(), records.end(), [](const QsoRecord& r) { return r.isInvalid; }),
                  records.end());

    QSet<QString> distinctBands;
    QSet<QString> distinctModes;
    for (const QsoRecord& record : records) {
        distinctBands.insert(record.band);
        distinctModes.insert(cabrilloModeCode(record.mode));
    }
    const QString categoryBand = distinctBands.size() == 1 ? cabrilloCategoryBand(*distinctBands.begin())
                                                          : QStringLiteral("ALL");
    const QString categoryMode = distinctModes.size() == 1 ? *distinctModes.begin() : QStringLiteral("MIXED");

    QStringList lines;
    lines << QStringLiteral("START-OF-LOG: 3.0");
    lines << QStringLiteral("CALLSIGN: %1").arg(settings.ownCallsign);
    // The robot reads this, so it must be the contest's own Cabrillo
    // name ("CQ-WW-CW"), not this program's internal id. A definition
    // without the key keeps the old behaviour -- better a wrong name
    // than an empty field, and the VHF/UHF contests submit EDI anyway.
    lines << QStringLiteral("CONTEST: %1").arg(definition.cabrilloName().isEmpty() ? definition.id()
                                                                                   : definition.cabrilloName());
    lines << QStringLiteral("CATEGORY-OPERATOR: SINGLE-OP");
    lines << QStringLiteral("CATEGORY-BAND: %1").arg(categoryBand);
    lines << QStringLiteral("CATEGORY-MODE: %1").arg(categoryMode);
    lines << QStringLiteral("CATEGORY-POWER: %1").arg(categoryPower);
    lines << QStringLiteral("CATEGORY-STATION: FIXED");
    // GRID-LOCATOR, not LOCATION: the latter carries a section/country
    // ("DX", "OE"), and a Maidenhead square in it is simply the wrong
    // field.
    lines << QStringLiteral("GRID-LOCATOR: %1").arg(settings.ownGrid);
    lines << QStringLiteral("CREATED-BY: Contestprogramm %1").arg(QStringLiteral(CONTESTPROGRAMM_VERSION));

    for (const QsoRecord& record : records) {
        lines << QStringLiteral("QSO: %1 %2 %3 %4 %5 %6 %7 %8")
                     .arg(cabrilloQsoBandField(record))
                     .arg(cabrilloModeCode(record.mode))
                     .arg(formatDate(record.timestampUtc))
                     .arg(formatTime(record.timestampUtc))
                     .arg(settings.ownCallsign)
                     .arg(record.exchangeSent)
                     .arg(record.callsign)
                     .arg(record.exchangeRcvd);
    }

    lines << QStringLiteral("END-OF-LOG:");

    return lines.join(QStringLiteral("\n")) + QStringLiteral("\n");
}

} // namespace Contestprogramm
