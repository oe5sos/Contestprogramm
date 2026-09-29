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
    void switchingRoomsLogsInAgainWithTheNewChatId();
    void aRejectedLoginIsTriedAgainInsteadOfGivingUp();
    void aRoomCanBeChosenWhileDisconnectedAndIsUsedOnTheNextTry();
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

// Martin, 2026-09-29: "bitte kontrolliere ob dieser chat auch wirklich
// den raum ändert." Er ändert ihn jetzt auf dem einzigen Weg, den das
// Protokoll dafür kennt -- ein neues Login mit der gewünschten
// chat_id. Geprüft wird beides: dass das Login die neue Nummer trägt,
// und dass die Zeilen danach aus diesem Raum kommen (roomObserved).
void TestOn4kstProtocol::switchingRoomsLogsInAgainWithTheNewChatId()
{
    MockOn4kstServer server;
    QVERIFY(server.startListening());

    On4kstClient client;
    QSignalSpy raumSpy(&client, &On4kstClient::roomObserved);
    client.connectAndLogin(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("OE5SOS"),
                           QStringLiteral("geheim"), On4kstClient::kChatIdVhfUhf);
    QTRY_VERIFY_WITH_TIMEOUT(!raumSpy.isEmpty(), 5000);
    QCOMPARE(raumSpy.takeFirst().at(0).toInt(), On4kstClient::kChatIdVhfUhf);
    QCOMPARE(server.lastChatId(), On4kstClient::kChatIdVhfUhf);
    QCOMPARE(client.currentChatId(), On4kstClient::kChatIdVhfUhf);

    // In den Mikrowellenraum wechseln.
    client.switchRoom(3);
    QTRY_COMPARE_WITH_TIMEOUT(server.lastChatId(), 3, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!raumSpy.isEmpty(), 5000);
    QCOMPARE(raumSpy.takeLast().at(0).toInt(), 3);
    QCOMPARE(client.currentChatId(), 3);

    // Derselbe Raum noch einmal: kein zweites Login, sonst risse jeder
    // Bandabgleich die Verbindung ohne Not ab.
    const int vorher = server.lastChatId();
    client.switchRoom(3);
    QCoreApplication::processEvents();
    QCOMPARE(server.lastChatId(), vorher);
}

// Ein abgelehnter Login hat den Chat bis zum Programmstart
// stillgelegt. Gedacht war das gegen falsche Zugangsdaten, es traf
// aber genauso "Sitzung noch offen" oder "Server mag gerade nicht" --
// am 2026-09-29 live erlebt: nach etlichen Neustarts blieb die
// Statusleiste grau und der Chat leer, bis das Programm neu startete.
// Mitten im Contest ist das schlimmer als ein Versuch alle paar
// Minuten.
void TestOn4kstProtocol::aRejectedLoginIsTriedAgainInsteadOfGivingUp()
{
    MockOn4kstServer server;
    QVERIFY(server.startListening());
    server.rejectNextLogin();

    On4kstClient client;
    QSignalSpy abgelehntSpy(&client, &On4kstClient::loginFailed);
    QSignalSpy angemeldetSpy(&client, &On4kstClient::loggedIn);
    client.connectAndLogin(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("OE5SOS"),
                           QStringLiteral("geheim"), On4kstClient::kChatIdVhfUhf);

    QTRY_VERIFY_WITH_TIMEOUT(!abgelehntSpy.isEmpty(), 5000);
    QCOMPARE(server.loginAttempts(), 1);
    qInfo().noquote() << "erster Versuch abgewiesen:" << abgelehntSpy.first().at(1).toString();

    // Früher war hier Schluss. Jetzt steht ein neuer Anlauf an -- er
    // kommt in Ruhe (eine Minute), deshalb wird hier nur geprüft, DASS
    // einer ansteht, statt eine Minute zu warten.
    // Warten, nicht sofort prüfen: der neue Anlauf wird erst gestellt,
    // wenn die Leitung wirklich unten ist (onDisconnected). Allein
    // gelaufen war der Prüfstand grün, in der Reihe rot -- ein
    // Zeitfehler im Prüfstand, nicht im Programm.
    QTRY_VERIFY_WITH_TIMEOUT(client.hasPendingRetryForTest(), 5000);
    qInfo().noquote() << "nächster Anlauf in" << client.pendingRetryDelayMsForTest() / 1000 << "s";
    QVERIFY(client.pendingRetryDelayMsForTest() >= 60000);
}

// Martin, 2026-09-29, mit Bild: "kann nicht anklicken". Das ganze
// Raummenü hing an "angemeldet" -- und angemeldet war er gerade
// nicht, weil ON4KST an dem Morgen schlicht nicht erreichbar war
// (aus seiner eigenen Shell: "connectx to www.on4kst.org port 23001
// failed: Operation timed out"). Gerade dann will man den Raum
// wählen können: die Wahl soll für den nächsten Anlauf gelten, und
// der soll gleich genommen werden.
void TestOn4kstProtocol::aRoomCanBeChosenWhileDisconnectedAndIsUsedOnTheNextTry()
{
    MockOn4kstServer server;
    QVERIFY(server.startListening());

    On4kstClient client;
    QSignalSpy raumSpy(&client, &On4kstClient::roomObserved);
    client.connectAndLogin(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("OE5SOS"),
                           QStringLiteral("geheim"), On4kstClient::kChatIdVhfUhf);
    QTRY_VERIFY_WITH_TIMEOUT(!raumSpy.isEmpty(), 5000);

    // Verbindung weg, wie bei einem Server, der nicht antwortet.
    client.disconnectFromServer();
    QTRY_VERIFY_WITH_TIMEOUT(!client.isConnected(), 5000);
    const int versucheVorher = server.loginAttempts();

    // Jetzt den Raum wählen -- getrennt.
    client.switchRoom(3);
    QTRY_COMPARE_WITH_TIMEOUT(server.lastChatId(), 3, 5000);
    QVERIFY2(server.loginAttempts() > versucheVorher,
             "Die Raumwahl im getrennten Zustand hat keinen neuen Anlauf ausgelöst");
    QCOMPARE(client.currentChatId(), 3);
    qInfo().noquote() << "getrennt Raum 3 gewählt -> Server sah Login für Raum" << server.lastChatId();

    // Und "Jetzt neu verbinden" nimmt ebenfalls sofort einen Anlauf.
    client.disconnectFromServer();
    QTRY_VERIFY_WITH_TIMEOUT(!client.isConnected(), 5000);
    const int vorNeuverbinden = server.loginAttempts();
    client.reconnectNow();
    QTRY_VERIFY_WITH_TIMEOUT(server.loginAttempts() > vorNeuverbinden, 5000);
    qInfo().noquote() << "neu verbinden -> Anmeldeversuche:" << server.loginAttempts();
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    TestOn4kstProtocol tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_on4kstprotocol.moc"
