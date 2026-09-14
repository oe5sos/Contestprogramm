#include "core/BroadcastPublisher.h"

#include "data/QsoRecord.h"

#include <QDateTime>
#include <QSysInfo>
#include <QUdpSocket>
#include <QXmlStreamWriter>

namespace Contestprogramm {

namespace {

// N1MM's own timestamp format in every example on the documentation
// page is "yyyy-MM-dd HH:mm:ss" (space-separated, no "T", no zone
// suffix) -- QsoRecord::timestampUtc is ISO-8601 ("...T...Z"), so it
// needs reformatting, not passthrough.
QString n1mmTimestamp(const QString& isoUtc)
{
    const QDateTime dt = QDateTime::fromString(isoUtc, Qt::ISODate);
    return dt.isValid() ? dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : isoUtc;
}

// The documentation page's "ADIF fields in ContactInfo" note states
// plainly: "in the contact info and radio info message, the frequency
// is exported in units of 10 Hz" -- confirmed by the RadioInfo Freq
// example (352211 for a signal described as "CW-80m", i.e. 3.52211
// MHz). QsoRecord::freqHz/RigctldClient::frequencyHz() are in Hz.
QString tensOfHzString(qint64 hz)
{
    return hz > 0 ? QString::number(hz / 10) : QString();
}

QString optionalIntString(const std::optional<int>& value)
{
    return value ? QString::number(*value) : QString();
}

QString optionalFreqString(const std::optional<qint64>& value)
{
    return value ? tensOfHzString(*value) : QString();
}

QString boolWord(bool value)
{
    // N1MM's own examples render bools as capitalized words
    // ("False"/"True" in the RadioInfo/ContactInfo samples), not
    // lowercase or "0"/"1".
    return value ? QStringLiteral("True") : QStringLiteral("False");
}

// Every field N1MM's own <contactinfo> example lists, in that same
// order -- see BroadcastPublisher.h's class comment for which of these
// this program has a real value for, and why the rest are written
// empty (never omitted -- DXLog.net's own N1MM-format broadcast keeps
// the full tag set too, just leaves the fields it does not support
// empty, the same discipline followed here).
void writeContactInfoFields(QXmlStreamWriter& writer, const QsoRecord& record, const QString& ownCallsign, const QString& contestName)
{
    writer.writeTextElement(QStringLiteral("app"), QStringLiteral("Contestprogramm"));
    writer.writeTextElement(QStringLiteral("contestname"), contestName);
    writer.writeTextElement(QStringLiteral("timestamp"), n1mmTimestamp(record.timestampUtc));
    writer.writeTextElement(QStringLiteral("mycall"), ownCallsign);
    // Passed through as-is: "144"/"432"/"1296" are already plain MHz
    // band designators in this project's own schema, the same
    // convention the documented HF examples use for their bands (e.g.
    // "3.5" for 80m) -- N1MM's page has no VHF/UHF <band> example to
    // confirm the literal string against, so this is a same-pattern
    // inference from the HF examples, not a directly quoted value.
    writer.writeTextElement(QStringLiteral("band"), record.band);
    writer.writeTextElement(QStringLiteral("rxfreq"), optionalFreqString(record.freqHz));
    writer.writeTextElement(QStringLiteral("txfreq"), optionalFreqString(record.freqHz));
    writer.writeTextElement(QStringLiteral("operator"), QString());
    writer.writeTextElement(QStringLiteral("mode"), BroadcastPublisher::n1mmModeCode(record.mode));
    writer.writeTextElement(QStringLiteral("call"), record.callsign);
    writer.writeTextElement(QStringLiteral("countryprefix"), QString());
    writer.writeTextElement(QStringLiteral("wpxprefix"), QString());
    writer.writeTextElement(QStringLiteral("stationprefix"), QString());
    writer.writeTextElement(QStringLiteral("continent"), QString());
    writer.writeTextElement(QStringLiteral("snt"), QString());
    writer.writeTextElement(QStringLiteral("sntnr"), optionalIntString(record.serialSent));
    writer.writeTextElement(QStringLiteral("rcv"), QString());
    writer.writeTextElement(QStringLiteral("rcvnr"), optionalIntString(record.serialRcvd));
    writer.writeTextElement(QStringLiteral("gridsquare"), record.gridSquare);
    writer.writeTextElement(QStringLiteral("section"), QString());
    writer.writeTextElement(QStringLiteral("comment"), QString());
    writer.writeTextElement(QStringLiteral("qth"), QString());
    writer.writeTextElement(QStringLiteral("name"), QString());
    writer.writeTextElement(QStringLiteral("power"), QString());
    writer.writeTextElement(QStringLiteral("misctext"), QString());
    writer.writeTextElement(QStringLiteral("zone"), QString());
    writer.writeTextElement(QStringLiteral("prec"), QString());
    writer.writeTextElement(QStringLiteral("ck"), QString());
    writer.writeTextElement(QStringLiteral("points"), QString());
    writer.writeTextElement(QStringLiteral("radionr"), QStringLiteral("1"));
    writer.writeTextElement(QStringLiteral("RoverLocation"), QString());
    writer.writeTextElement(QStringLiteral("IsOriginal"), boolWord(true));
    writer.writeTextElement(QStringLiteral("NetBiosName"), QString());
    writer.writeTextElement(QStringLiteral("StationName"), QSysInfo::machineHostName());
    // Not a GUID (N1MM's own format for this field) -- this program
    // has no GUID per QSO, only the SQLite row id. Using that real,
    // stable id rather than fabricating a random UUID: this class
    // never sends <contactreplace>/<contactdelete>, so ID's only
    // documented purpose (correlating an edit/delete with the
    // original record) does not apply here, and a stable-but-
    // differently-shaped value is preferable to a plausible-looking
    // but meaningless random one.
    writer.writeTextElement(QStringLiteral("ID"), record.id >= 0 ? QString::number(record.id) : QString());
    writer.writeTextElement(QStringLiteral("SentExchange"), record.exchangeSent);
}

void writeRadioInfoFields(QXmlStreamWriter& writer, const QString& ownCallsign, qint64 frequencyHz, const QString& mode, bool transmitting)
{
    writer.writeTextElement(QStringLiteral("app"), QStringLiteral("Contestprogramm"));
    writer.writeTextElement(QStringLiteral("StationName"), QSysInfo::machineHostName());
    writer.writeTextElement(QStringLiteral("RadioNr"), QStringLiteral("1"));
    writer.writeTextElement(QStringLiteral("Freq"), tensOfHzString(frequencyHz));
    writer.writeTextElement(QStringLiteral("TXFreq"), tensOfHzString(frequencyHz));
    writer.writeTextElement(QStringLiteral("Mode"), BroadcastPublisher::n1mmModeCode(mode));
    writer.writeTextElement(QStringLiteral("mycall"), ownCallsign);
    writer.writeTextElement(QStringLiteral("OpCall"), ownCallsign);
    writer.writeTextElement(QStringLiteral("IsRunning"), QString());
    writer.writeTextElement(QStringLiteral("Antenna"), QString());
    writer.writeTextElement(QStringLiteral("Rotors"), QString());
    writer.writeTextElement(QStringLiteral("ActiveRadioNr"), QStringLiteral("1"));
    // The only CAT-observable proxy this program has for N1MM's
    // "program has initiated a transmission" semantics is the rig's
    // own PTT state -- an honest, directly-read value rather than
    // trying to reproduce N1MM's program-vs-radio distinction (see
    // that page's own "Transmitting with <IsTransmitting> = False"
    // caveat, which this class does not attempt to replicate).
    writer.writeTextElement(QStringLiteral("IsTransmitting"), boolWord(transmitting));
    writer.writeTextElement(QStringLiteral("RadioName"), QString());
    writer.writeTextElement(QStringLiteral("IsConnected"), boolWord(true));
}

} // namespace

BroadcastPublisher::BroadcastPublisher(QObject* parent)
    : QObject(parent)
    , m_socket(new QUdpSocket(this))
{
}

void BroadcastPublisher::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

void BroadcastPublisher::setTarget(const QString& host, quint16 port)
{
    const QHostAddress address(host);
    m_hasTarget = !host.trimmed().isEmpty() && !address.isNull();
    m_targetHost = m_hasTarget ? address : QHostAddress();
    m_targetPort = port;
}

QString BroadcastPublisher::n1mmModeCode(const QString& mode)
{
    const QString m = mode.trimmed().toUpper();
    if (m == QStringLiteral("SSB")) {
        return QStringLiteral("USB");
    }
    return m;
}

QByteArray BroadcastPublisher::buildContactInfoXml(const QsoRecord& record, const QString& ownCallsign, const QString& contestName)
{
    QByteArray body;
    QXmlStreamWriter writer(&body);
    writer.writeStartElement(QStringLiteral("contactinfo"));
    writeContactInfoFields(writer, record, ownCallsign, contestName);
    writer.writeEndElement();
    return QByteArrayLiteral("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n") + body;
}

QByteArray BroadcastPublisher::buildRadioInfoXml(const QString& ownCallsign, qint64 frequencyHz, const QString& mode, bool transmitting)
{
    QByteArray body;
    QXmlStreamWriter writer(&body);
    writer.writeStartElement(QStringLiteral("RadioInfo"));
    writeRadioInfoFields(writer, ownCallsign, frequencyHz, mode, transmitting);
    writer.writeEndElement();
    return QByteArrayLiteral("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n") + body;
}

void BroadcastPublisher::publishContact(const QsoRecord& record, const QString& ownCallsign, const QString& contestName)
{
    if (!m_enabled || !m_hasTarget) {
        return;
    }
    send(buildContactInfoXml(record, ownCallsign, contestName));
}

void BroadcastPublisher::publishRadioInfo(const QString& ownCallsign, qint64 frequencyHz, const QString& mode, bool transmitting)
{
    if (!m_enabled || !m_hasTarget || ownCallsign.isEmpty()) {
        return;
    }
    send(buildRadioInfoXml(ownCallsign, frequencyHz, mode, transmitting));
}

void BroadcastPublisher::send(const QByteArray& xml)
{
    m_socket->writeDatagram(xml, m_targetHost, m_targetPort);
}

} // namespace Contestprogramm
