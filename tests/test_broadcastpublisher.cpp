#include <QtTest>

#include <QCoreApplication>
#include <QHostAddress>
#include <QNetworkDatagram>
#include <QUdpSocket>
#include <QXmlStreamReader>

#include "core/BroadcastPublisher.h"
#include "data/QsoRecord.h"

using namespace Contestprogramm;

namespace {

// Reads every top-level text element of one XML document into a
// name->text map -- simpler than asserting against the whole
// serialized string, and robust against attribute/whitespace details
// QXmlStreamWriter might choose differently across Qt versions.
QHash<QString, QString> parseFlatFields(const QByteArray& xml, const QString& expectedRootTag)
{
    QHash<QString, QString> fields;
    QXmlStreamReader reader(xml);
    bool sawRoot = false;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement()) {
            if (!sawRoot) {
                sawRoot = (reader.name().toString() == expectedRootTag);
                continue;
            }
            const QString tag = reader.name().toString();
            fields.insert(tag, reader.readElementText());
        }
    }
    return fields;
}

QsoRecord sampleQso()
{
    QsoRecord record;
    record.id = 42;
    record.callsign = QStringLiteral("OE1ABC");
    record.band = QStringLiteral("144");
    record.mode = QStringLiteral("SSB");
    record.timestampUtc = QStringLiteral("2026-06-13T12:05:00Z");
    record.freqHz = 145200000;
    record.gridSquare = QStringLiteral("JN88TC");
    record.serialSent = 1;
    record.serialRcvd = 2;
    record.exchangeSent = QStringLiteral("001 JN77");
    record.contestId = QStringLiteral("OE_VHF_UHF");
    return record;
}

} // namespace

class TestBroadcastPublisher : public QObject
{
    Q_OBJECT

private slots:
    void contactInfoXmlHasExpectedFields();
    void contactInfoXmlMapsSsbToUsb();
    void radioInfoXmlHasExpectedFieldsAndFrequencyUnits();
    void liveSocketReceivesContactDatagramWhenEnabled();
    void liveSocketReceivesRadioInfoDatagramWhenEnabled();
    void disabledPublisherSendsNothing();
    void publisherWithNoTargetSendsNothing();
};

void TestBroadcastPublisher::contactInfoXmlHasExpectedFields()
{
    const QByteArray xml = BroadcastPublisher::buildContactInfoXml(sampleQso(), QStringLiteral("OE5SOS"), QStringLiteral("OE_VHF_UHF"));

    QVERIFY(xml.startsWith("<?xml version=\"1.0\" encoding=\"utf-8\"?>"));

    const QHash<QString, QString> fields = parseFlatFields(xml, QStringLiteral("contactinfo"));
    QCOMPARE(fields.value(QStringLiteral("app")), QStringLiteral("Contestprogramm"));
    QCOMPARE(fields.value(QStringLiteral("contestname")), QStringLiteral("OE_VHF_UHF"));
    QCOMPARE(fields.value(QStringLiteral("mycall")), QStringLiteral("OE5SOS"));
    QCOMPARE(fields.value(QStringLiteral("call")), QStringLiteral("OE1ABC"));
    QCOMPARE(fields.value(QStringLiteral("band")), QStringLiteral("144"));
    // N1MM's own timestamp form: "yyyy-MM-dd HH:mm:ss", not ISO-8601.
    QCOMPARE(fields.value(QStringLiteral("timestamp")), QStringLiteral("2026-06-13 12:05:00"));
    // 145200000 Hz -> tens-of-Hz units, per the documentation page's own
    // "frequency is exported in units of 10 Hz" note.
    QCOMPARE(fields.value(QStringLiteral("rxfreq")), QStringLiteral("14520000"));
    QCOMPARE(fields.value(QStringLiteral("txfreq")), QStringLiteral("14520000"));
    QCOMPARE(fields.value(QStringLiteral("gridsquare")), QStringLiteral("JN88TC"));
    QCOMPARE(fields.value(QStringLiteral("sntnr")), QStringLiteral("1"));
    QCOMPARE(fields.value(QStringLiteral("rcvnr")), QStringLiteral("2"));
    QCOMPARE(fields.value(QStringLiteral("SentExchange")), QStringLiteral("001 JN77"));
    QCOMPARE(fields.value(QStringLiteral("ID")), QStringLiteral("42"));
    QCOMPARE(fields.value(QStringLiteral("IsOriginal")), QStringLiteral("True"));
    QCOMPARE(fields.value(QStringLiteral("radionr")), QStringLiteral("1"));

    // Fields this program has no real value for stay present, but
    // empty -- never omitted, never fabricated (see BroadcastPublisher.h's
    // class comment).
    QVERIFY(fields.contains(QStringLiteral("qth")));
    QCOMPARE(fields.value(QStringLiteral("qth")), QString());
    QVERIFY(fields.contains(QStringLiteral("points")));
    QCOMPARE(fields.value(QStringLiteral("points")), QString());
}

void TestBroadcastPublisher::contactInfoXmlMapsSsbToUsb()
{
    QsoRecord record = sampleQso();
    record.mode = QStringLiteral("SSB");
    const QHash<QString, QString> fields = parseFlatFields(
        BroadcastPublisher::buildContactInfoXml(record, QStringLiteral("OE5SOS"), QStringLiteral("OE_VHF_UHF")),
        QStringLiteral("contactinfo"));
    // N1MM's own Mode enumeration has no plain "SSB" -- only USB/LSB.
    QCOMPARE(fields.value(QStringLiteral("mode")), QStringLiteral("USB"));

    record.mode = QStringLiteral("CW");
    const QHash<QString, QString> cwFields = parseFlatFields(
        BroadcastPublisher::buildContactInfoXml(record, QStringLiteral("OE5SOS"), QStringLiteral("OE_VHF_UHF")),
        QStringLiteral("contactinfo"));
    QCOMPARE(cwFields.value(QStringLiteral("mode")), QStringLiteral("CW"));
}

void TestBroadcastPublisher::radioInfoXmlHasExpectedFieldsAndFrequencyUnits()
{
    const QByteArray xml = BroadcastPublisher::buildRadioInfoXml(QStringLiteral("OE5SOS"), 145200000, QStringLiteral("CW"), true);

    QVERIFY(xml.startsWith("<?xml version=\"1.0\" encoding=\"utf-8\"?>"));

    const QHash<QString, QString> fields = parseFlatFields(xml, QStringLiteral("RadioInfo"));
    QCOMPARE(fields.value(QStringLiteral("app")), QStringLiteral("Contestprogramm"));
    QCOMPARE(fields.value(QStringLiteral("mycall")), QStringLiteral("OE5SOS"));
    QCOMPARE(fields.value(QStringLiteral("Mode")), QStringLiteral("CW"));
    QCOMPARE(fields.value(QStringLiteral("Freq")), QStringLiteral("14520000"));
    QCOMPARE(fields.value(QStringLiteral("TXFreq")), QStringLiteral("14520000"));
    QCOMPARE(fields.value(QStringLiteral("IsTransmitting")), QStringLiteral("True"));
    QCOMPARE(fields.value(QStringLiteral("RadioNr")), QStringLiteral("1"));
}

void TestBroadcastPublisher::liveSocketReceivesContactDatagramWhenEnabled()
{
    QUdpSocket receiver;
    QVERIFY(receiver.bind(QHostAddress::LocalHost, 0));

    BroadcastPublisher publisher;
    publisher.setEnabled(true);
    publisher.setTarget(QStringLiteral("127.0.0.1"), receiver.localPort());

    publisher.publishContact(sampleQso(), QStringLiteral("OE5SOS"), QStringLiteral("OE_VHF_UHF"));

    QVERIFY(QTest::qWaitFor([&receiver]() { return receiver.hasPendingDatagrams(); }, 2000));

    const QByteArray datagram = receiver.receiveDatagram().data();
    QXmlStreamReader reader(datagram);
    // Well-formed: parsing must not report an error at any point.
    while (!reader.atEnd()) {
        reader.readNext();
    }
    QVERIFY(!reader.hasError());

    const QHash<QString, QString> fields = parseFlatFields(datagram, QStringLiteral("contactinfo"));
    QCOMPARE(fields.value(QStringLiteral("call")), QStringLiteral("OE1ABC"));
    QCOMPARE(fields.value(QStringLiteral("mycall")), QStringLiteral("OE5SOS"));
}

void TestBroadcastPublisher::liveSocketReceivesRadioInfoDatagramWhenEnabled()
{
    QUdpSocket receiver;
    QVERIFY(receiver.bind(QHostAddress::LocalHost, 0));

    BroadcastPublisher publisher;
    publisher.setEnabled(true);
    publisher.setTarget(QStringLiteral("127.0.0.1"), receiver.localPort());

    publisher.publishRadioInfo(QStringLiteral("OE5SOS"), 432100000, QStringLiteral("FM"), false);

    QVERIFY(QTest::qWaitFor([&receiver]() { return receiver.hasPendingDatagrams(); }, 2000));

    const QByteArray datagram = receiver.receiveDatagram().data();
    const QHash<QString, QString> fields = parseFlatFields(datagram, QStringLiteral("RadioInfo"));
    QCOMPARE(fields.value(QStringLiteral("Mode")), QStringLiteral("FM"));
    QCOMPARE(fields.value(QStringLiteral("Freq")), QStringLiteral("43210000"));
    QCOMPARE(fields.value(QStringLiteral("IsTransmitting")), QStringLiteral("False"));
}

void TestBroadcastPublisher::disabledPublisherSendsNothing()
{
    QUdpSocket receiver;
    QVERIFY(receiver.bind(QHostAddress::LocalHost, 0));

    BroadcastPublisher publisher;
    // Enabled defaults to false -- an operator who never opted in must
    // never see traffic on the wire (ContestSettings::broadcastEnabled).
    publisher.setTarget(QStringLiteral("127.0.0.1"), receiver.localPort());
    publisher.publishContact(sampleQso(), QStringLiteral("OE5SOS"), QStringLiteral("OE_VHF_UHF"));

    QVERIFY(!QTest::qWaitFor([&receiver]() { return receiver.hasPendingDatagrams(); }, 300));
}

void TestBroadcastPublisher::publisherWithNoTargetSendsNothing()
{
    QUdpSocket receiver;
    QVERIFY(receiver.bind(QHostAddress::LocalHost, 0));

    BroadcastPublisher publisher;
    publisher.setEnabled(true);
    // No setTarget() call at all -- must not fall back to some default
    // destination.
    publisher.publishContact(sampleQso(), QStringLiteral("OE5SOS"), QStringLiteral("OE_VHF_UHF"));

    QVERIFY(!QTest::qWaitFor([&receiver]() { return receiver.hasPendingDatagrams(); }, 300));
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    TestBroadcastPublisher tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_broadcastpublisher.moc"
