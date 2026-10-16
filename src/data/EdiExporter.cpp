#include "data/EdiExporter.h"

#include "app/ContestSettings.h"
#include "core/AsciiText.h"
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

// Die Grenzen, die REG1TEST (Issue 1,1) selbst nennt. Abschnitt
// "Line length": "If line length is already specified it must not be
// exceeded, other lines must not exceed a length of 75 characters."
// Dazu aus der Tabelle der QSO-Spalten die Maximallängen -- und die
// Tabelle rechnet selbst vor, dass 61 Zeichen plus 14 Trenner genau
// die 75 ergeben. Wer die Spalten nicht begrenzt, sprengt also die
// Zeile mit.
constexpr int kLineLimit = 75;
constexpr int kMaxCallLength = 14;        // "Call = 3 to 14 characters"
constexpr int kMaxRstLength = 3;          // "0 or 2 or 3 characters"
constexpr int kMaxExchangeLength = 6;     // "0 or 1 to 6 characters"
constexpr int kMaxLocatorLength = 6;      // "0 or 4 or 6 characters"

// A header value is one line and must not contain the record
// separator; a stray ";" in a street name would otherwise shift every
// column a reader parses after it.
//
// asciiOnly(): die Spezifikation lässt nur die Zeichen 10, 13 und
// 32-127 zu (Abschnitt "Characters"), und Martins Land heißt
// "Österreich". Siehe core/AsciiText.h -- hier wird umgeschrieben,
// nicht hinausgeschrieben.
QString headerValue(const QString& raw)
{
    QString value = asciiOnly(raw).simplified();
    value.replace(QLatin1Char(';'), QLatin1Char(','));
    return value;
}

// Eine Kopfzeile "Schlüssel=Wert" mit der Zeilengrenze. Gekürzt wird
// der Wert, nicht der Schlüssel: eine ausführliche Antennen- oder
// Standortbeschreibung ist schnell länger als 75 Zeichen, und eine
// zu lange Zeile ist etwas, das die Spezifikation ausdrücklich
// verbietet -- eine gekürzte Beschreibung nicht.
QString headerLine(const QString& key, const QString& raw)
{
    QString value = headerValue(raw);
    const int room = kLineLimit - key.size() - 1;
    if (value.size() > room) {
        value.truncate(qMax(0, room));
        value = value.trimmed();
    }
    return key + QLatin1Char('=') + value;
}

// Dasselbe für eine SPALTE einer QSO-Zeile. Die Kopfzeilen waren seit
// jeher geschützt, die QSO-Zeilen nicht -- ein Semikolon im Rufzeichen,
// im RST oder im Locator schob jede Spalte dahinter um eine weiter,
// und der Robot liest die Zeile falsch oder lehnt die Einreichung ab.
// Tippbar ist es (das Rufzeichenfeld hat keinen Validator), und was
// über einen Import in die Datenbank kommt, hat diesen Weg gar nicht
// erst genommen. simplified() nimmt dabei auch Zeilenumbrüche mit, die
// eine Zeile in zwei teilen würden.
QString columnValue(const QString& raw)
{
    QString value = asciiOnly(raw).simplified();
    value.replace(QLatin1Char(';'), QLatin1Char(','));
    return value;
}

// Die Rufzeichenspalte. "Call = 3 to 14 characters" sagt die Tabelle,
// und ein Rufzeichen hat nie ein Leerzeichen -- über einen Import
// (data/LogFileReader.h) kann aber beides hereinkommen. Länger als 14
// Zeichen ist kein Rufzeichen mehr, sondern ein Versehen; abgeschnitten
// bleibt die Datei lesbar, in voller Länge sprengt sie zusätzlich die
// 75-Zeichen-Grenze der Zeile.
QString callColumn(const QString& raw)
{
    QString value = columnValue(raw).toUpper();
    value.remove(QLatin1Char(' '));
    if (value.size() > kMaxCallLength) {
        value.truncate(kMaxCallLength);
    }
    return value;
}

// RST: "0 or 2 or 3 characters". Das Eingabefeld lässt vier zu
// ("5999" tippt sich leicht), ein Import beliebig viele.
QString rstColumn(const QString& raw)
{
    QString value = columnValue(raw);
    value.remove(QLatin1Char(' '));
    if (value.size() > kMaxRstLength) {
        value.truncate(kMaxRstLength);
    }
    return value;
}

// Der Locator des Gegenübers: "0 or 4 or 6 characters, World Wide
// Locator" -- dazwischen gibt es nichts. Achtstellige Locatoren stehen
// in vielen Logs und kommen über den Journal-Import herein; ihre ersten
// sechs Stellen sind der richtige Locator. Was sich auch so nicht zu
// einem Locator machen lässt, bleibt leer: eine Spalte mit drei
// Zeichen wäre ein Formatfehler für die ganze Datei, und das QSO
// selbst meldet die Logprüfung ohnehin als "bad_locator".
QString locatorColumn(const QString& raw)
{
    if (const std::optional<QString> grid = normalizedGridSquare(raw)) {
        return *grid;
    }
    return QString();
}

// MOpe1 is the one header key where ";" IS the separator (a list of
// operator calls), so only line breaks are removed here -- und deshalb
// darf diese Zeile nicht durch headerLine() laufen, die Semikolons zu
// Kommas macht. Die Zeilengrenze gilt aber auch hier.
QString operatorList(const QString& raw)
{
    QStringList calls;
    for (const QString& part : asciiOnly(raw).split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
        const QString call = part.simplified().toUpper();
        if (!call.isEmpty()) {
            calls << call;
        }
    }
    QString value = calls.join(QLatin1Char(';'));
    const int room = kLineLimit - 6;   // "MOpe1=" davor
    if (value.size() > room) {
        // Lieber ein Rufzeichen weniger als eine zu lange Zeile: an
        // der letzten vollständigen Angabe abschneiden, nicht mitten
        // in einem Rufzeichen.
        value.truncate(room);
        const int lastSep = value.lastIndexOf(QLatin1Char(';'));
        value = lastSep > 0 ? value.left(lastSep) : value;
    }
    return value;
}

// "0 or 3 or 4 characters, with leading zeros". Eine Nummer jenseits
// von 9999 gibt es in 24 Stunden auf einem Band nicht; über einen
// Import kann sie trotzdem dastehen, und dann ist die leere Spalte die
// einzige, die das Format noch erlaubt.
QString serialText(const std::optional<int>& serial)
{
    if (!serial) {
        return QString();
    }
    if (*serial < 0 || *serial > 9999) {
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
    QString value = headerValue(tokens.join(QLatin1Char(' ')));
    if (value.size() > kMaxExchangeLength) {
        value.truncate(kMaxExchangeLength);
    }
    return value;
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
    // the two band files of one entry must agree on it. REG1TEST: the
    // CONTEST's first and last day, so the contest window wins (from the
    // definition's schedule, or the operator's contest end); only
    // without one do the QSO dates stand in. Vorher kam bei jemandem,
    // der nur am Samstag funkte, "20261003;20261003" heraus
    // (Pruefbericht 2026-09-25).
    QString tDate;
    const ContestWindow window = effectiveContestWindow(
        definition.schedule(), settings.contestEndUtc,
        all.isEmpty() ? QDateTime::currentDateTimeUtc() : parseTimestamp(all.first().timestampUtc));
    if (window.isValid()) {
        tDate = window.startUtc.date().toString(QStringLiteral("yyyyMMdd")) + QLatin1Char(';')
              + window.endUtc.date().toString(QStringLiteral("yyyyMMdd"));
    } else if (!all.isEmpty()) {
        const QDate first = parseTimestamp(all.first().timestampUtc).date();
        const QDate last = parseTimestamp(all.last().timestampUtc).date();
        tDate = first.toString(QStringLiteral("yyyyMMdd")) + QLatin1Char(';') + last.toString(QStringLiteral("yyyyMMdd"));
    } else {
        const QString today = QDateTime::currentDateTimeUtc().date().toString(QStringLiteral("yyyyMMdd"));
        tDate = today + QLatin1Char(';') + today;
    }

    const QString ownCall = settings.ownCallsign.trimmed().toUpper();
    const QString ownGrid = settings.ownGrid.trimmed().toUpper();
    // PWWLo: "Maximum length is six characters". Im
    // Einstellungsfenster lässt sich mehr eintippen, und ein
    // achtstelliger eigener Locator stand schon in mancher
    // Programmeinstellung; gerechnet wird ohnehin mit sechs Stellen.
    const QString ownGridColumn = normalizedGridSquare(ownGrid).value_or(ownGrid.left(6));

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
               << callColumn(record.callsign)
               << QString::number(modeCode(record.mode))
               << rstColumn(record.rstSent)
               << serialText(record.serialSent)
               << rstColumn(record.rstRcvd)
               << serialText(record.serialRcvd)
               << extraExchange(record)
               << locatorColumn(record.gridSquare)
               << QString::number(qsoPts)
               << QString()   // new exchange -- no exchange multipliers in these contests
               << newWwl
               << QString()   // new DXCC -- see the class comment
               << (record.isDupe ? QStringLiteral("D") : QString());
        qsoLines << fields.join(QLatin1Char(';'));
    }

    QStringList lines;
    lines << QStringLiteral("[REG1TEST;1]");
    lines << headerLine(QStringLiteral("TName"), definition.name());
    lines << QStringLiteral("TDate=%1").arg(tDate);
    lines << QStringLiteral("PCall=%1").arg(ownCall);
    lines << QStringLiteral("PWWLo=%1").arg(ownGridColumn);
    lines << QStringLiteral("PExch=");
    lines << headerLine(QStringLiteral("PAdr1"), station.locationLine1);
    lines << headerLine(QStringLiteral("PAdr2"), station.locationLine2);
    lines << headerLine(QStringLiteral("PSect"), station.section);
    lines << QStringLiteral("PBand=%1").arg(bandLabel(band));
    lines << headerLine(QStringLiteral("PClub"), station.club);
    lines << headerLine(QStringLiteral("RName"), station.name);
    lines << QStringLiteral("RCall=%1").arg(ownCall);
    lines << headerLine(QStringLiteral("RAdr1"), station.street);
    lines << QStringLiteral("RAdr2=");
    lines << headerLine(QStringLiteral("RPoCo"), station.postalCode);
    lines << headerLine(QStringLiteral("RCity"), station.city);
    lines << headerLine(QStringLiteral("RCoun"), station.country);
    lines << headerLine(QStringLiteral("RPhon"), station.phone);
    lines << headerLine(QStringLiteral("RHBBS"), station.email);
    lines << QStringLiteral("MOpe1=%1").arg(operatorList(station.operators));
    lines << QStringLiteral("MOpe2=");
    lines << headerLine(QStringLiteral("STXEq"), station.txEquipment);
    lines << QStringLiteral("SPowe=%1").arg(station.powerWatts > 0 ? QString::number(station.powerWatts) : QString());
    lines << headerLine(QStringLiteral("SRXEq"), station.rxEquipment);
    lines << headerLine(QStringLiteral("SAnte"), station.antenna);
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
