#include "data/EdiExporter.h"

#include "app/ContestSettings.h"
#include "core/Maidenhead.h"
#include "data/ContestDatabase.h"
#include "data/ContestDefinition.h"
#include "data/ContestScoring.h"
#include "data/QsoRecord.h"

#include <QDateTime>
#include <QSet>
#include <QVector>

#include <algorithm>
#include <cmath>

namespace Contestprogramm {

namespace {

const QString kCrLf = QStringLiteral("\r\n");

QDateTime parseTimestamp(const QString& timestampUtc)
{
    return QDateTime::fromString(timestampUtc, Qt::ISODate);
}

// A header value is one line and must not contain the record
// separator; a stray ";" in a street name would otherwise shift every
// column a reader parses after it.
QString headerValue(const QString& raw)
{
    QString value = raw.simplified();
    value.replace(QLatin1Char(';'), QLatin1Char(','));
    return value;
}

// MOpe1 is the one header key where ";" IS the separator (a list of
// operator calls), so only line breaks are removed here.
QString operatorList(const QString& raw)
{
    QStringList calls;
    for (const QString& part : raw.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
        const QString call = part.simplified().toUpper();
        if (!call.isEmpty()) {
            calls << call;
        }
    }
    return calls.join(QLatin1Char(';'));
}

QString serialText(const std::optional<int>& serial)
{
    if (!serial) {
        return QString();
    }
    return QStringLiteral("%1").arg(*serial, 3, 10, QLatin1Char('0'));
}

// REG1TEST carries RST, serial and locator in columns of their own,
// so the free "received exchange" column is whatever else the contest
// asked for -- the composed exchange text minus those three parts.
// Empty for both shipped definitions (RST + Nr. + Grid is all they
// exchange); only a user-added extra field ends up here.
QString extraExchange(const QsoRecord& record)
{
    QStringList tokens = record.exchangeRcvd.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString& part : {record.rstRcvd, record.gridSquare}) {
        if (part.isEmpty()) {
            continue;
        }
        const int idx = tokens.indexOf(part, 0, Qt::CaseInsensitive);
        if (idx >= 0) {
            tokens.removeAt(idx);
        }
    }
    // The serial is matched by value, not by text: the composed
    // exchange keeps whatever the operator typed ("3" as well as
    // "003"), the column keeps the number.
    if (record.serialRcvd) {
        for (int i = 0; i < tokens.size(); ++i) {
            bool isNumber = false;
            if (tokens.at(i).toInt(&isNumber) == *record.serialRcvd && isNumber) {
                tokens.removeAt(i);
                break;
            }
        }
    }
    return headerValue(tokens.join(QLatin1Char(' ')));
}

QString largeSquare(const QString& grid)
{
    return grid.left(4).toUpper();
}

bool earlierThan(const QsoRecord& a, const QsoRecord& b)
{
    if (a.timestampUtc != b.timestampUtc) {
        return a.timestampUtc < b.timestampUtc;
    }
    return a.id < b.id;
}

} // namespace

// ── EdiStationInfo ─────────────────────────────────────────────────

void EdiStationInfo::loadFrom(const ContestDatabase& database)
{
    section = database.settingValue(QStringLiteral("edi_section"), section);
    club = database.settingValue(QStringLiteral("edi_club"), club);
    locationLine1 = database.settingValue(QStringLiteral("edi_location_1"), locationLine1);
    locationLine2 = database.settingValue(QStringLiteral("edi_location_2"), locationLine2);
    operators = database.settingValue(QStringLiteral("edi_operators"), operators);
    name = database.settingValue(QStringLiteral("edi_name"), name);
    street = database.settingValue(QStringLiteral("edi_street"), street);
    postalCode = database.settingValue(QStringLiteral("edi_postal_code"), postalCode);
    city = database.settingValue(QStringLiteral("edi_city"), city);
    country = database.settingValue(QStringLiteral("edi_country"), country);
    phone = database.settingValue(QStringLiteral("edi_phone"), phone);
    email = database.settingValue(QStringLiteral("edi_email"), email);
    txEquipment = database.settingValue(QStringLiteral("edi_tx_equipment"), txEquipment);
    powerWatts = database.settingValue(QStringLiteral("edi_power_w"), QString::number(powerWatts)).toInt();
    rxEquipment = database.settingValue(QStringLiteral("edi_rx_equipment"), rxEquipment);
    antenna = database.settingValue(QStringLiteral("edi_antenna"), antenna);
}

void EdiStationInfo::saveTo(ContestDatabase& database) const
{
    database.setSettingValue(QStringLiteral("edi_section"), section);
    database.setSettingValue(QStringLiteral("edi_club"), club);
    database.setSettingValue(QStringLiteral("edi_location_1"), locationLine1);
    database.setSettingValue(QStringLiteral("edi_location_2"), locationLine2);
    database.setSettingValue(QStringLiteral("edi_operators"), operators);
    database.setSettingValue(QStringLiteral("edi_name"), name);
    database.setSettingValue(QStringLiteral("edi_street"), street);
    database.setSettingValue(QStringLiteral("edi_postal_code"), postalCode);
    database.setSettingValue(QStringLiteral("edi_city"), city);
    database.setSettingValue(QStringLiteral("edi_country"), country);
    database.setSettingValue(QStringLiteral("edi_phone"), phone);
    database.setSettingValue(QStringLiteral("edi_email"), email);
    database.setSettingValue(QStringLiteral("edi_tx_equipment"), txEquipment);
    database.setSettingValue(QStringLiteral("edi_power_w"), QString::number(powerWatts));
    database.setSettingValue(QStringLiteral("edi_rx_equipment"), rxEquipment);
    database.setSettingValue(QStringLiteral("edi_antenna"), antenna);
}

// ── EdiExporter ────────────────────────────────────────────────────

EdiExporter::EdiExporter(ContestDatabase& database)
    : m_database(&database)
{
}

QStringList EdiExporter::bandsWithQsos(const QString& contestId, const ContestDefinition& definition) const
{
    QSet<QString> seen;
    QStringList unlisted;
    for (const QsoRecord& record : m_database->qsosForContest(contestId)) {
        if (record.isInvalid || seen.contains(record.band)) {
            continue;
        }
        seen.insert(record.band);
        if (!definition.bands().contains(record.band)) {
            unlisted << record.band;
        }
    }
    QStringList result;
    for (const QString& band : definition.bands()) {
        if (seen.contains(band)) {
            result << band;
        }
    }
    result << unlisted;
    return result;
}

QString EdiExporter::exportBand(const QString& contestId,
                                const QString& band,
                                const ContestDefinition& definition,
                                const ContestSettings& settings,
                                const EdiStationInfo& station) const
{
    QVector<QsoRecord> all = m_database->qsosForContest(contestId);
    all.erase(std::remove_if(all.begin(), all.end(), [](const QsoRecord& r) { return r.isInvalid; }), all.end());
    std::stable_sort(all.begin(), all.end(), earlierThan);

    // TDate spans the whole contest, not just this band's activity --
    // the two band files of one entry must agree on it.
    QString tDate;
    if (!all.isEmpty()) {
        const QDate first = parseTimestamp(all.first().timestampUtc).date();
        const QDate last = parseTimestamp(all.last().timestampUtc).date();
        tDate = first.toString(QStringLiteral("yyyyMMdd")) + QLatin1Char(';') + last.toString(QStringLiteral("yyyyMMdd"));
    } else {
        const QString today = QDateTime::currentDateTimeUtc().date().toString(QStringLiteral("yyyyMMdd"));
        tDate = today + QLatin1Char(';') + today;
    }

    const QString ownCall = settings.ownCallsign.trimmed().toUpper();
    const QString ownGrid = settings.ownGrid.trimmed().toUpper();

    QVector<QsoRecord> records;
    for (const QsoRecord& record : all) {
        if (record.band == band) {
            records << record;
        }
    }
    // The header's claimed numbers, from the one scoring function the
    // live panel uses too (see the class comment).
    const ContestScore score = computeContestScore(records, ownGrid, {band}, definition.scoring());
    const BandScore* bandScore = score.band(band);
    const int validCount = bandScore ? bandScore->validQsos : 0;
    const qint64 points = bandScore ? bandScore->points : 0;
    const int squares = bandScore ? bandScore->largeSquares : 0;
    const QString odxCall = bandScore ? bandScore->odxCall : QString();
    const QString odxGrid = bandScore ? bandScore->odxGrid : QString();
    const int odxKm = bandScore ? bandScore->odxKm : 0;

    QSet<QString> worked4;
    QStringList qsoLines;
    for (const QsoRecord& record : records) {
        const int qsoPts = qsoPoints(record, ownGrid, definition.scoring());
        QString newWwl;
        if (!record.isDupe && isValidGridSquare(record.gridSquare)) {
            const QString square = largeSquare(record.gridSquare);
            if (!worked4.contains(square)) {
                worked4.insert(square);
                newWwl = QStringLiteral("N");
            }
        }

        const QDateTime when = parseTimestamp(record.timestampUtc);
        QStringList fields;
        fields << when.date().toString(QStringLiteral("yyMMdd"))
               << when.time().toString(QStringLiteral("HHmm"))
               << record.callsign.trimmed().toUpper()
               << QString::number(modeCode(record.mode))
               << record.rstSent.trimmed()
               << serialText(record.serialSent)
               << record.rstRcvd.trimmed()
               << serialText(record.serialRcvd)
               << extraExchange(record)
               << record.gridSquare.trimmed().toUpper()
               << QString::number(qsoPts)
               << QString()   // new exchange -- no exchange multipliers in these contests
               << newWwl
               << QString()   // new DXCC -- see the class comment
               << (record.isDupe ? QStringLiteral("D") : QString());
        qsoLines << fields.join(QLatin1Char(';'));
    }

    QStringList lines;
    lines << QStringLiteral("[REG1TEST;1]");
    lines << QStringLiteral("TName=%1").arg(headerValue(definition.name()));
    lines << QStringLiteral("TDate=%1").arg(tDate);
    lines << QStringLiteral("PCall=%1").arg(ownCall);
    lines << QStringLiteral("PWWLo=%1").arg(ownGrid);
    lines << QStringLiteral("PExch=");
    lines << QStringLiteral("PAdr1=%1").arg(headerValue(station.locationLine1));
    lines << QStringLiteral("PAdr2=%1").arg(headerValue(station.locationLine2));
    lines << QStringLiteral("PSect=%1").arg(headerValue(station.section));
    lines << QStringLiteral("PBand=%1").arg(bandLabel(band));
    lines << QStringLiteral("PClub=%1").arg(headerValue(station.club));
    lines << QStringLiteral("RName=%1").arg(headerValue(station.name));
    lines << QStringLiteral("RCall=%1").arg(ownCall);
    lines << QStringLiteral("RAdr1=%1").arg(headerValue(station.street));
    lines << QStringLiteral("RAdr2=");
    lines << QStringLiteral("RPoCo=%1").arg(headerValue(station.postalCode));
    lines << QStringLiteral("RCity=%1").arg(headerValue(station.city));
    lines << QStringLiteral("RCoun=%1").arg(headerValue(station.country));
    lines << QStringLiteral("RPhon=%1").arg(headerValue(station.phone));
    lines << QStringLiteral("RHBBS=%1").arg(headerValue(station.email));
    lines << QStringLiteral("MOpe1=%1").arg(operatorList(station.operators));
    lines << QStringLiteral("MOpe2=");
    lines << QStringLiteral("STXEq=%1").arg(headerValue(station.txEquipment));
    lines << QStringLiteral("SPowe=%1").arg(station.powerWatts > 0 ? QString::number(station.powerWatts) : QString());
    lines << QStringLiteral("SRXEq=%1").arg(headerValue(station.rxEquipment));
    lines << QStringLiteral("SAnte=%1").arg(headerValue(station.antenna));
    lines << QStringLiteral("SAntH=%1;%2")
                 .arg(static_cast<int>(std::lround(settings.antennaHeightM)))
                 .arg(static_cast<int>(std::lround(settings.ownElevationM)));
    lines << QStringLiteral("CQSOs=%1;1").arg(validCount);
    lines << QStringLiteral("CQSOP=%1").arg(points);
    lines << QStringLiteral("CWWLs=%1;0;1").arg(squares);
    lines << QStringLiteral("CWWLB=0");
    lines << QStringLiteral("CExcs=0;0;1");
    lines << QStringLiteral("CExcB=0");
    lines << QStringLiteral("CDXCs=0;0;1");
    lines << QStringLiteral("CDXCB=0");
    lines << QStringLiteral("CToSc=%1").arg(points);
    lines << QStringLiteral("CODXC=%1;%2;%3").arg(odxCall, odxGrid).arg(odxKm);
    lines << QStringLiteral("[Remarks]");
    lines << QStringLiteral("Created by Contestprogramm");
    lines << QStringLiteral("[QSORecords;%1]").arg(qsoLines.size());
    lines << qsoLines;

    return lines.join(kCrLf) + kCrLf;
}

QString EdiExporter::bandLabel(const QString& band)
{
    struct Entry {
        const char* band;
        const char* label;
    };
    static const Entry kBands[] = {
        {"50", "50 MHz"},      {"70", "70 MHz"},      {"144", "144 MHz"},    {"432", "432 MHz"},
        {"1296", "1,3 GHz"},   {"2320", "2,3 GHz"},   {"3400", "3,4 GHz"},   {"5760", "5,7 GHz"},
        {"10368", "10 GHz"},   {"24048", "24 GHz"},   {"47088", "47 GHz"},   {"76032", "76 GHz"},
    };
    const QString key = band.trimmed();
    for (const Entry& entry : kBands) {
        if (key == QLatin1String(entry.band)) {
            return QLatin1String(entry.label);
        }
    }
    return key + QStringLiteral(" MHz");
}

int EdiExporter::modeCode(const QString& mode)
{
    const QString m = mode.trimmed().toUpper();
    if (m == QStringLiteral("SSB") || m == QStringLiteral("USB") || m == QStringLiteral("LSB")
        || m == QStringLiteral("PH") || m == QStringLiteral("PHONE")) {
        return 1;
    }
    if (m == QStringLiteral("CW")) { return 2; }
    if (m == QStringLiteral("AM")) { return 5; }
    if (m == QStringLiteral("FM")) { return 6; }
    if (m == QStringLiteral("RTTY") || m == QStringLiteral("RY")) { return 7; }
    if (m == QStringLiteral("SSTV")) { return 8; }
    if (m == QStringLiteral("ATV")) { return 9; }
    return 0;
}

QString EdiExporter::suggestedFileName(const QString& ownCallsign, const QString& band)
{
    QString call = ownCallsign.trimmed().toUpper();
    call.replace(QLatin1Char('/'), QLatin1Char('-'));
    if (call.isEmpty()) {
        call = QStringLiteral("log");
    }
    return QStringLiteral("%1_%2MHz.edi").arg(call, band.trimmed());
}

} // namespace Contestprogramm
