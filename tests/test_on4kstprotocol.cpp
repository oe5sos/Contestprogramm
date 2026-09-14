#include <QtTest>

#include <QCoreApplication>
#include <QSignalSpy>

#include "core/On4kstClient.h"
#include "core/SpotCandidate.h"
#include "mocks/MockOn4kstServer.h"

using namespace Contestprogramm;

class TestOn4kstProtocol : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void parseDxSpotLineDirect();
    void parseChatLineDirectWithGrid();
    void parseChatLineDirectWithoutGrid();
    void malformedLinesAreRejected();
    void telnetIacIsStrippedFromBuffer();

    void liveLoginReceivesSpotChatAndKeepalive();
};

void TestOn4kstProtocol::initTestCase()
{
    // Registered under the unqualified name -- moc records signal
    // parameter types exactly as written in the header ("SpotCandidate",
    // since On4kstClient::spotReceived/chatLineReceived are declared
    // inside namespace Contestprogramm itself, with no explicit
    // namespace prefix), and QSignalSpy resolves argument types by that
    // literal string.
    qRegisterMetaType<SpotCandidate>("SpotCandidate");
}

void TestOn4kstProtocol::parseDxSpotLineDirect()
{
    SpotCandidate candidate;
    QVERIFY(On4kstClient::parseDxSpotLineForTest(
        QStringLiteral("DL|1700000000|1200Z|OE1TST|144300.0|OE3TST|FT8 -12dB|JN78|JN77|"),
        candidate));
    QCOMPARE(candidate.callsign, QStringLiteral("OE3TST"));
    QCOMPARE(candidate.grid, QStringLiteral("JN77"));
    QCOMPARE(candidate.source, QStringLiteral("on4kst"));
    QCOMPARE(candidate.freqHz, static_cast<qint64>(144300000)); // 144300.0 kHz -> Hz
}

void TestOn4kstProtocol::parseChatLineDirectWithGrid()
{
    SpotCandidate candidate;
    QVERIFY(On4kstClient::parseChatLineForTest(
        QStringLiteral("CH|2|1200Z|OE7TST|Hans|ALL|CQ CQ JN88TC|0|"),
        candidate));
    QCOMPARE(candidate.callsign, QStringLiteral("OE7TST"));
    QCOMPARE(candidate.grid, QStringLiteral("JN88TC"));
}

void TestOn4kstProtocol::parseChatLineDirectWithoutGrid()
{
    SpotCandidate candidate;
    QVERIFY(On4kstClient::parseChatLineForTest(
        QStringLiteral("CH|2|1200Z|OE7TST|Hans|ALL|anyone hearing me?|0|"),
        candidate));
    QCOMPARE(candidate.callsign, QStringLiteral("OE7TST"));
    // No embedded locator token in the message -- absent grid is not a
    // parse failure, per SpotParser::parseChatLine's contract.
    QVERIFY(candidate.grid.isEmpty());
}

void TestOn4kstProtocol::malformedLinesAreRejected()
{
    SpotCandidate candidate;
    QVERIFY(!On4kstClient::parseDxSpotLineForTest(QStringLiteral("DL|only|three|fields"), candidate));
    QVERIFY(!On4kstClient::parseChatLineForTest(QStringLiteral("SDONE|2|"), candidate));
}

void TestOn4kstProtocol::telnetIacIsStrippedFromBuffer()
{
    QByteArray buf;
    buf.append(char(0xFF));
    buf.append(char(0xFB));
    buf.append(char(0x01));
    buf.append("CK|\r\n");
    On4kstClient::stripTelnetIACForTest(buf);
    QCOMPARE(buf, QByteArrayLiteral("CK|\r\n"));
}

void TestOn4kstProtocol::liveLoginReceivesSpotChatAndKeepalive()
{
    MockOn4kstServer server;
    QVERIFY(server.startListening());

    On4kstClient client;
    QSignalSpy loggedInSpy(&client, &On4kstClient::loggedIn);
    QSignalSpy spotSpy(&client, &On4kstClient::spotReceived);
    QSignalSpy chatSpy(&client, &On4kstClient::chatLineReceived);
    QSignalSpy keepaliveSpy(&server, &MockOn4kstServer::keepaliveSent);

    client.connectAndLogin(QStringLiteral("127.0.0.1"), server.port(),
                            QStringLiteral("OE5TST"), QStringLiteral("secret"));

    // The mock server writes SDONE|/DL|/CH|/CK| back-to-back as soon as
    // it sees LOGIN, so on the client side loggedIn/spotReceived/
    // chatLineReceived typically all fire within the same buffered read
    // -- waiting on each spy's wait() *sequentially* is unreliable here
    // (a later spy's signal can already have fired, with its own
    // baseline already past, before its wait() call even starts; see
    // spotSpy.wait() racing loggedInSpy.wait() below the fixed version
    // once did). Poll for all three client-side signals together.
    QVERIFY(QTest::qWaitFor([&]() {
        return !loggedInSpy.isEmpty() && !spotSpy.isEmpty() && !chatSpy.isEmpty();
    }, 2000));
    // The server writes CK| before the client can possibly react to
    // anything (it happens synchronously while handling LOGIN, before
    // any reply is even sent back), so by the time the client-side
    // signals above have fired, this has necessarily already fired too.
    QVERIFY(!keepaliveSpy.isEmpty());

    QCOMPARE(loggedInSpy.at(0).at(0).toInt(), On4kstClient::kChatIdVhfUhf);

    // Give the client's reply to the keepalive a moment to reach the
    // mock server.
    QVERIFY(QTest::qWaitFor([&server]() {
        return !server.bytesReceivedAfterKeepalive().isEmpty();
    }, 2000));
    QCOMPARE(server.bytesReceivedAfterKeepalive(), QByteArrayLiteral("\r\n"));

    const SpotCandidate spot = spotSpy.at(0).at(0).value<SpotCandidate>();
    QCOMPARE(spot.callsign, QStringLiteral("OE3TST"));
    QCOMPARE(spot.grid, QStringLiteral("JN77"));
    QCOMPARE(spot.source, QStringLiteral("on4kst"));

    const SpotCandidate chat = chatSpy.at(0).at(0).value<SpotCandidate>();
    QCOMPARE(chat.callsign, QStringLiteral("OE7TST"));
    QCOMPARE(chat.grid, QStringLiteral("JN88TC"));

    QVERIFY(client.isLoggedIn());
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    TestOn4kstProtocol tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_on4kstprotocol.moc"
