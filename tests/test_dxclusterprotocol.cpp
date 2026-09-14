#include <QtTest>

#include <QCoreApplication>
#include <QSignalSpy>

#include "core/DxClusterClient.h"
#include "core/SpotCandidate.h"
#include "mocks/MockDxClusterServer.h"

using namespace Contestprogramm;

class TestDxClusterProtocol : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void parseClassicDxDeLineDirect();
    void parseDxspiderLineDirect();
    void malformedLinesAreRejected();
    void loginPromptDetection();
    void telnetIacIsStrippedFromBuffer();

    void liveConnectSendsCallsignAndReceivesBothSpotFormats();
};

void TestDxClusterProtocol::initTestCase()
{
    // See test_on4kstprotocol.cpp's own initTestCase for why this is
    // registered under the unqualified name.
    qRegisterMetaType<SpotCandidate>("SpotCandidate");
}

void TestDxClusterProtocol::parseClassicDxDeLineDirect()
{
    SpotCandidate candidate;
    QVERIFY(DxClusterClient::parseDxSpotLineForTest(
        QStringLiteral("DX de W3LPL:     14025.0  JA1ABC       CW big signal       1824Z"), candidate));
    QCOMPARE(candidate.callsign, QStringLiteral("JA1ABC"));
    QCOMPARE(candidate.source, QStringLiteral("cluster"));
    QCOMPARE(candidate.freqHz, static_cast<qint64>(14025000)); // 14025.0 kHz -> Hz
    QVERIFY(candidate.grid.isEmpty()); // classic cluster spots carry no locator
}

void TestDxClusterProtocol::parseDxspiderLineDirect()
{
    SpotCandidate candidate;
    QVERIFY(DxClusterClient::parseDxSpotLineForTest(
        QStringLiteral("14310.0 VK2IO/P     12-May-2026 0449Z WWFF VKFF-5514     <OH0M>"), candidate));
    QCOMPARE(candidate.callsign, QStringLiteral("VK2IO/P"));
    QCOMPARE(candidate.source, QStringLiteral("cluster"));
    QCOMPARE(candidate.freqHz, static_cast<qint64>(14310000));
}

void TestDxClusterProtocol::malformedLinesAreRejected()
{
    SpotCandidate candidate;
    QVERIFY(!DxClusterClient::parseDxSpotLineForTest(QStringLiteral("not a spot line at all"), candidate));
    QVERIFY(!DxClusterClient::parseDxSpotLineForTest(QStringLiteral("DX de W3LPL: not-a-frequency JA1ABC hi Z"), candidate));
}

void TestDxClusterProtocol::loginPromptDetection()
{
    // Both real call sites (onReadyRead's no-newline-yet partial-buffer
    // branch, and handleLine's per-line branch) always trim() before
    // calling isLoginPrompt -- exercised untrimmed here on purpose,
    // matching that contract, not "login: " with a trailing space.
    QVERIFY(DxClusterClient::isLoginPromptForTest(QStringLiteral("login:")));
    QVERIFY(DxClusterClient::isLoginPromptForTest(QStringLiteral("Please enter your call:")));
    QVERIFY(DxClusterClient::isLoginPromptForTest(QStringLiteral("Callsign:")));
    QVERIFY(!DxClusterClient::isLoginPromptForTest(QStringLiteral("DX de W3LPL:     14025.0  JA1ABC")));
}

void TestDxClusterProtocol::telnetIacIsStrippedFromBuffer()
{
    QByteArray buf;
    buf.append(char(0xFF));
    buf.append(char(0xFB));
    buf.append(char(0x01));
    buf.append("login: ");
    DxClusterClient::stripTelnetIACForTest(buf);
    QCOMPARE(buf, QByteArrayLiteral("login: "));
}

void TestDxClusterProtocol::liveConnectSendsCallsignAndReceivesBothSpotFormats()
{
    MockDxClusterServer server;
    QVERIFY(server.startListening());

    DxClusterClient client;
    QSignalSpy connectedSpy(&client, &DxClusterClient::connected);
    QSignalSpy spotSpy(&client, &DxClusterClient::spotReceived);
    QSignalSpy spotsSentSpy(&server, &MockDxClusterServer::spotsSent);

    client.connectToCluster(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("OE5TST"));

    QVERIFY(QTest::qWaitFor([&]() { return !connectedSpy.isEmpty(); }, 2000));
    QVERIFY(QTest::qWaitFor([&]() { return !spotsSentSpy.isEmpty(); }, 2000));
    QCOMPARE(server.callsignReceived(), QByteArrayLiteral("OE5TST"));

    QVERIFY(QTest::qWaitFor([&]() { return spotSpy.size() >= 2; }, 2000));

    const SpotCandidate first = spotSpy.at(0).at(0).value<SpotCandidate>();
    QCOMPARE(first.callsign, QStringLiteral("JA1ABC"));
    QCOMPARE(first.source, QStringLiteral("cluster"));

    const SpotCandidate second = spotSpy.at(1).at(0).value<SpotCandidate>();
    QCOMPARE(second.callsign, QStringLiteral("VK2IO/P"));
    QCOMPARE(second.source, QStringLiteral("cluster"));

    QVERIFY(client.isConnected());
}

// Not QTEST_APPLESS_MAIN: the live-connect test needs QTest::qWaitFor's
// event-loop pumping, which requires a real QCoreApplication instance --
// same reasoning as test_on4kstprotocol.cpp's own hand-rolled main.
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    TestDxClusterProtocol tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_dxclusterprotocol.moc"
