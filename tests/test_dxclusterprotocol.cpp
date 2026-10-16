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
    void aPeerThatNeverSendsANewlineCannotGrowTheBufferForever();
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

// 24 hours is a long time to keep appending to a buffer nothing ever
// drains. A peer that sends data with no newline in it (a wedged link,
// a port that is not a cluster at all, binary noise) used to grow
// m_readBuffer without any limit; past the cap the partial line is
// dropped, the operator is told, and the connection carries on.
void TestDxClusterProtocol::aPeerThatNeverSendsANewlineCannotGrowTheBufferForever()
{
    MockDxClusterServer server;
    QVERIFY(server.startListening());

    DxClusterClient client;
    QSignalSpy connectedSpy(&client, &DxClusterClient::connected);
    QSignalSpy errorSpy(&client, &DxClusterClient::connectionError);
    QSignalSpy spotSpy(&client, &DxClusterClient::spotReceived);

    client.connectToCluster(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("OE5TST"));
    QVERIFY(QTest::qWaitFor([&]() { return !connectedSpy.isEmpty(); }, 2000));

    // 64 KB at a time, not one newline among them, and kept up until
    // the cap actually bites. Deliberately NOT a fixed number of
    // helpings: how much of a local TCP write has reached the reader by
    // the time the next one goes out is a platform matter (a fixed 1.5
    // MB passed on macOS and Linux and ran out the clock on Windows --
    // CI, 2026-10-15). qWaitFor pumps the event loop between calls, so
    // the client gets to read in between.
    const QByteArray chunk(64 * 1024, 'x');
    QVERIFY2(QTest::qWaitFor([&]() {
                 if (errorSpy.isEmpty()) {
                     server.sendRaw(chunk);
                 }
                 return !errorSpy.isEmpty();
             }, 30000),
             "der Puffer-Deckel hat nicht gegriffen");
    const QString message = errorSpy.at(0).at(0).toString();
    QVERIFY2(message.contains(QStringLiteral("verworfen")), qPrintable(message));

    // Still connected, and once the junk line is finally terminated the
    // next proper spot is parsed as usual. (The junk itself, and
    // anything stuck to it before the first newline, is gone -- that
    // part was never a readable line to begin with.)
    //
    // Counted from where the spy stands NOW, not from zero: the mock
    // server sends its own two spots on connect, and whether those
    // arrive before the junk or get swallowed by it is a matter of
    // timing -- not something this test is about.
    QVERIFY(client.isConnected());
    const int spotsBefore = spotSpy.size();
    QVERIFY(server.sendRaw(QByteArrayLiteral("\r\n")));
    QVERIFY(server.sendRaw(QByteArrayLiteral("DX de OE5XXX:    144300.0  OE3ABC        JN88TC         1210Z\r\n")));
    QVERIFY(QTest::qWaitFor([&]() { return spotSpy.size() > spotsBefore; }, 3000));
    QCOMPARE(spotSpy.at(spotsBefore).at(0).value<SpotCandidate>().callsign, QStringLiteral("OE3ABC"));
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
