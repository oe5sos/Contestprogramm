#include "data/AdifExporter.h"

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

// ADIF's BAND enumeration uses "2m"/"70cm"/"23cm", not the raw MHz
// numbers QsoRecord::band stores ("144"/"432"/a future "1296" per the
// plan's third-band note). Best-effort passthrough for anything not in
// this program's two current contest definitions, rather than dropping
// an unrecognized band silently.
QString adifBandCode(const QString& band)
{
    const QString b = band.trimmed();
    if (b == QStringLiteral("144")) {
        return QStringLiteral("2m");
    }
    if (b == QStringLiteral("432")) {
        return QStringLiteral("70cm");
    }
    if (b == QStringLiteral("1296")) {
        return QStringLiteral("23cm");
    }
    return b;
}

QString toAdifRecord(const QsoRecord& record)
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

    field(r, QStringLiteral("GRIDSQUARE"), record.gridSquare);
    field(r, QStringLiteral("CONTEST_ID"), record.contestId);
    if (record.serialSent.has_value()) {
        field(r, QStringLiteral("STX"), QString::number(*record.serialSent));
    }
    if (record.serialRcvd.has_value()) {
        field(r, QStringLiteral("SRX"), QString::number(*record.serialRcvd));
    }

    r += QStringLiteral("<EOR>");
    return r;
}

} // namespace

AdifExporter::AdifExporter(ContestDatabase& database)
    : m_database(&database)
{
}

QString AdifExporter::exportContest(const QString& contestId) const
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
        out += toAdifRecord(record) + QStringLiteral("\n");
    }
    return out;
}

} // namespace Contestprogramm
