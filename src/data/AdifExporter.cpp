#include "data/AdifExporter.h"

#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"
#include "data/QsoRecord.h"

#include <QDateTime>

#include <algorithm>

namespace Contestprogramm {

namespace {

// <NAME:byteLen>value , per ADIF's length-prefixed field convention --
// same rule NereusSDR's LogEntry.cpp::field() applies, ported here for
// QsoRecord's own, different set of fields. Trimmed; an empty value is
// omitted rather than written as a zero-length field, which is what
// gives this exporter its "a NULL freq_hz degrades gracefully" contract
// -- nothing here can be a required, always-present field.
void field(QString& out, const QString& name, const QString& value)
{
    const QString v = value.trimmed();
    if (v.isEmpty()) {
        return;
    }
    out += QStringLiteral("<%1:%2>%3 ").arg(name).arg(v.toUtf8().size()).arg(v);
}

QString formatAdifDate(const QString& timestampUtc)
{
    const QDateTime dt = QDateTime::fromString(timestampUtc, Qt::ISODate);
    return dt.isValid() ? dt.date().toString(QStringLiteral("yyyyMMdd")) : QString();
}

QString formatAdifTime(const QString& timestampUtc)
{
    const QDateTime dt = QDateTime::fromString(timestampUtc, Qt::ISODate);
    return dt.isValid() ? dt.time().toString(QStringLiteral("HHmmss")) : QString();
}

// Die Bandnamen in ADIF sind Wellenlängen ("20m", "70cm"), nicht die
// Megahertz-Zahlen, unter denen dieses Programm ein Band führt
// ("14", "432" -- siehe core/BandUtils.cpp). Jede Zeile dieser Tabelle
// hat ihre Entsprechung dort; wer dort ein Band ergänzt, ergänzt es
// hier mit, sonst wandert eine nackte Zahl in die Datei und das
// Logbuch am anderen Ende weist sie zurück. Genau das wäre bis
// 2026-09-23 bei jedem Kurzwellen-QSO passiert: BAND=14 statt 20m.
//
// Ein unbekanntes Band geht weiterhin unverändert durch -- lieber eine
// Zeile, die auffällt, als ein stillschweigend verschlucktes QSO.
struct AdifBand {
    const char* band;       // wie das Programm es nennt
    const char* adifCode;   // wie ADIF es nennt
};

constexpr AdifBand kAdifBands[] = {
    {"1.8", "160m"}, {"3.5", "80m"},  {"7", "40m"},    {"10", "30m"},
    {"14", "20m"},   {"18", "17m"},   {"21", "15m"},   {"24", "12m"},
    {"28", "10m"},   {"50", "6m"},    {"70", "4m"},    {"144", "2m"},
    {"432", "70cm"}, {"1296", "23cm"}, {"2320", "13cm"}, {"3400", "9cm"},
    {"5760", "6cm"}, {"10368", "3cm"},
};

QString adifBandCode(const QString& band)
{
    const QString b = band.trimmed();
    for (const AdifBand& entry : kAdifBands) {
        if (b == QLatin1String(entry.band)) {
            return QString::fromLatin1(entry.adifCode);
        }
    }
    return b;
}

QString toAdifRecord(const QsoRecord& record, const ContestSettings& settings)
{
    QString r;
    field(r, QStringLiteral("CALL"), record.callsign);
    field(r, QStringLiteral("QSO_DATE"), formatAdifDate(record.timestampUtc));
    field(r, QStringLiteral("TIME_ON"), formatAdifTime(record.timestampUtc));
    field(r, QStringLiteral("BAND"), adifBandCode(record.band));
    // record.mode ("SSB"/"CW"/"FM"/"RTTY") already matches ADIF's MODE
    // enumeration directly -- unlike Cabrillo, which needs its own
    // PH/FM/RY/DG codes (see CabrilloExporter::cabrilloModeCode).
    field(r, QStringLiteral("MODE"), record.mode.trimmed().toUpper());

    // The whole point of this exporter (see AdifExporter.h): the exact
    // frequency, Hz -> MHz, only ever written here, never in Cabrillo
    // and never surfaced as its own UI field. Omitted entirely -- not
    // written as FREQ:0 or similar -- when freq_hz is NULL.
    if (record.freqHz.has_value()) {
        const double freqMhz = static_cast<double>(*record.freqHz) / 1000000.0;
        field(r, QStringLiteral("FREQ"), QString::number(freqMhz, 'f', 6));
    }

    // Der Rapport steht seit 2026-09-23 mit drin. Er ist ein eigenes
    // Feld im Log (QsoRecord::rstSent/rstRcvd) und gehört in jede
    // ADIF-Zeile -- ein Logbuch, das ein QSO ohne Rapport übernimmt,
    // trägt hinterher 59 ein, ob es stimmt oder nicht.
    field(r, QStringLiteral("RST_SENT"), record.rstSent);
    field(r, QStringLiteral("RST_RCVD"), record.rstRcvd);

    field(r, QStringLiteral("GRIDSQUARE"), record.gridSquare);
    field(r, QStringLiteral("CONTEST_ID"), record.contestId);
    if (record.serialSent.has_value()) {
        field(r, QStringLiteral("STX"), QString::number(*record.serialSent));
    }
    if (record.serialRcvd.has_value()) {
        field(r, QStringLiteral("SRX"), QString::number(*record.serialRcvd));
    }
    // Der ganze getauschte Text, so wie er im Log steht. STX/SRX oben
    // führen nur die Nummer; was ein Contest sonst noch tauscht (Zone,
    // Bezirk, Name, Leistung), passt in kein Zahlenfeld und wäre sonst
    // beim Import verloren.
    field(r, QStringLiteral("STX_STRING"), record.exchangeSent);
    field(r, QStringLiteral("SRX_STRING"), record.exchangeRcvd);

    // Wer das QSO gefahren hat und von wo. TQSL braucht STATION_CALLSIGN,
    // und der eigene Locator macht aus der Datei erst eine, aus der die
    // Gegenstelle ihre Entfernung nachrechnen kann. Leer bleibende
    // Einstellungen lassen die Felder weg -- wie überall hier.
    field(r, QStringLiteral("STATION_CALLSIGN"), settings.ownCallsign.trimmed().toUpper());
    field(r, QStringLiteral("OPERATOR"), settings.ownCallsign.trimmed().toUpper());
    field(r, QStringLiteral("MY_GRIDSQUARE"), settings.ownGrid.trimmed().toUpper());

    r += QStringLiteral("<EOR>");
    return r;
}

} // namespace

AdifExporter::AdifExporter(ContestDatabase& database)
    : m_database(&database)
{
}

QString AdifExporter::exportContest(const QString& contestId, const ContestSettings& settings) const
{
    QVector<QsoRecord> records = m_database->qsosForContest(contestId);
    // Same exclusion as CabrilloExporter -- a QSO marked invalid (see
    // QsoRecord::isInvalid) stays in the operator's own log but must not
    // appear in an exported ADIF file either.
    records.erase(std::remove_if(records.begin(), records.end(), [](const QsoRecord& r) { return r.isInvalid; }),
                  records.end());

    QString header;
    field(header, QStringLiteral("ADIF_VER"), QStringLiteral("3.1.4"));
    field(header, QStringLiteral("PROGRAMID"), QStringLiteral("Contestprogramm"));

    QString out = QStringLiteral("Contestprogramm ADIF export\n") + header + QStringLiteral("<EOH>\n");
    for (const QsoRecord& record : records) {
        out += toAdifRecord(record, settings) + QStringLiteral("\n");
    }
    return out;
}

} // namespace Contestprogramm
