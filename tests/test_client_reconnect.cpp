// A rotctld/rigctld that is not up yet when the client first dials --
// the everyday case at the contest site, where the program starts
// before the Hamlib daemons -- must be found once it appears. Qt
// reports a refused connect through errorOccurred() alone, never
// disconnected(), which is where both clients used to arm their
// retry: they sat in Connecting for good (observed 2026-09-21, the
// status bar's permanent "CAT: verbindet...").

#include <QtTest>

#include <QElapsedTimer>
#include <QTcpServer>
#include <QTcpSocket>

#include "core/DxClusterClient.h"
#include "core/On4kstClient.h"
#include "core/RigctldClient.h"
#include "core/RotctldClient.h"

using namespace Contestprogramm;

namespace {

// A port nothing listens on right now, found by binding and releasing.
// Ein Port, auf dem niemand lauscht -- und das wird belegt, nicht
// angenommen. Der frühere Weg (Server öffnen, Nummer merken, schließen)
// besorgte nur die NUMMER; hier wird zusätzlich so lange probiert, bis
// ein Verbindungsversuch dorthin wirklich abgewiesen wird.
quint16 freePort()
{
    QTcpServer probe;
    probe.listen(QHostAddress::LocalHost, 0);
    const quint16 port = probe.serverPort();
    probe.close();

    QElapsedTimer frist;
    frist.start();
    while (frist.elapsed() < 5000) {
        QTcpSocket sonde;
        sonde.connectToHost(QHostAddress::LocalHost, port);
        if (!sonde.waitForConnected(200)) {
            return port;   // abgewiesen -- der Port ist wirklich frei
        }
        sonde.abort();
        QTest::qWait(50);
    }
    // Nicht stillschweigend weitermachen: wer hier landet, misst gegen
    // einen fremden Dienst und bekommt unerklärliche Zahlen.
    qWarning("freePort: Port %u nimmt nach 5 s immer noch Verbindungen an", port);
    return port;
}

// The smallest rotctld/rigctld: answers "p"/"f"/"m" style polls with
// something parseable so the client stays Connected.
class FakeDaemon : public QObject
{
public:
    bool listen(quint16 port)
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            QTcpSocket* socket = m_server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [socket] {
                const QByteArray request = socket->readAll();
                if (request.startsWith("p")) {
                    socket->write("123.0\n0.0\n");
                } else if (request.startsWith("f")) {
                    socket->write("144300000\n");
                } else if (request.startsWith("m")) {
                    socket->write("USB\n2400\n");
                } else if (request.startsWith("t")) {
                    socket->write("0\n");
                } else {
                    socket->write("RPRT 0\n");
                }
            });
        });
        return m_server.listen(QHostAddress::LocalHost, port);
    }

private:
    QTcpServer m_server;
};

} // namespace

class TestClientReconnect : public QObject
{
    Q_OBJECT

private slots:
    void rotorClientFindsARotctldThatAppearsLater();
    void rigClientFindsARigctldThatAppearsLater();
    void on4kstClientDialsAgainAfterARefusedConnect();
    void clusterClientDialsAgainAfterARefusedConnect();
};

void TestClientReconnect::rotorClientFindsARotctldThatAppearsLater()
{
    const quint16 port = freePort();
    RotctldClient client;
    client.setTarget(QStringLiteral("127.0.0.1"), port);
    client.connectToRotor();
    QCOMPARE(client.state(), RotctldClient::State::Connecting);

    // Refused: back to Disconnected, retry armed -- not stuck.
    QTRY_COMPARE_WITH_TIMEOUT(client.state(), RotctldClient::State::Disconnected, 10000);

    FakeDaemon daemon;
    QVERIFY(daemon.listen(port));
    QTRY_VERIFY_WITH_TIMEOUT(client.isConnected(), 15000);
    QTRY_COMPARE_WITH_TIMEOUT(client.azimuthDeg(), 123.0, 10000);
}

void TestClientReconnect::rigClientFindsARigctldThatAppearsLater()
{
    const quint16 port = freePort();
    RigctldClient client;
    client.setTarget(QStringLiteral("127.0.0.1"), port);
    client.connectToRig();
    QCOMPARE(client.state(), RigctldClient::State::Connecting);
    QTRY_COMPARE_WITH_TIMEOUT(client.state(), RigctldClient::State::Disconnected, 10000);

    FakeDaemon daemon;
    QVERIFY(daemon.listen(port));
    QTRY_VERIFY_WITH_TIMEOUT(client.isConnected(), 15000);
}

// The chat and cluster clients had the same gap with their own
// exponential backoff: armed only from disconnected(), which a refused
// connect never produces. The first retry comes after the initial
// 5 s delay.
void TestClientReconnect::on4kstClientDialsAgainAfterARefusedConnect()
{
    const quint16 port = freePort();
    On4kstClient client;
    QSignalSpy errors(&client, &On4kstClient::connectionError);
    QSignalSpy connected(&client, &On4kstClient::connected);
    client.connectAndLogin(QStringLiteral("127.0.0.1"), port, QStringLiteral("OE5SOS"), QStringLiteral("pw"));
    QTRY_VERIFY_WITH_TIMEOUT(errors.count() >= 1, 10000);
    QVERIFY(!client.isConnected());

    FakeDaemon daemon;
    QVERIFY(daemon.listen(port));
    // Gezählt wird AB HIER, nicht seit dem Start des Prüfstands. Worum
    // es geht, ist die Doppelwahl: erscheint der Server, darf sich der
    // Client genau EINMAL verbinden, nicht zweimal parallel (auf dem
    // echten ON4KST-Server hieße das zwei Anmeldungen mit demselben
    // Rufzeichen).
    //
    // Die absolute Zahl taugte dafür nicht. Am 2026-10-20 fiel dieser
    // Prüfstand auf dem MacBook 6 von 6 Mal durch, in der CI nie --
    // gemessen mit Zeitstempeln und `lsof -a -p <pid> -iTCP`: schon VOR
    // dem Start des Daemons kam eine Verbindung auf den freien Port
    // zustande (nach rund 30 ms, sichtbar als 127.0.0.1:x->127.0.0.1:port
    // im Zustand CLOSED) und wurde Millisekunden später von der
    // Gegenseite weggeworfen ("The remote host closed the connection"),
    // obwohl dort nachweislich niemand lauschte (lsof auf den Port:
    // leer, eine eigene Sonde davor: "Connection refused"). Der Client
    // verhielt sich dabei richtig -- Fehler gemeldet, Wiederanwahl
    // geplant, nach 5 s sauber verbunden. Gezählt hat der Prüfstand
    // trotzdem zwei. Der Mechanismus dieser Phantomverbindung ist nicht
    // abschließend geklärt und trat nur auf diesem Rechner auf; gegen
    // ihn hilft, die Zählung dort beginnen zu lassen, wo die Aussage
    // des Prüfstands anfängt.
    const int vorDemDaemon = connected.count();
    QTRY_VERIFY_WITH_TIMEOUT(client.isConnected(), 15000);
    QCOMPARE(connected.count(), vorDemDaemon + 1);

    // A deliberate disconnect ends the retries.
    client.disconnectFromServer();
    QTRY_VERIFY_WITH_TIMEOUT(!client.isConnected(), 10000);
    QTest::qWait(200);
    QVERIFY(!client.isConnected());
}

void TestClientReconnect::clusterClientDialsAgainAfterARefusedConnect()
{
    const quint16 port = freePort();
    DxClusterClient client;
    QSignalSpy errors(&client, &DxClusterClient::connectionError);
    client.connectToCluster(QStringLiteral("127.0.0.1"), port, QStringLiteral("OE5SOS"));
    QTRY_VERIFY_WITH_TIMEOUT(errors.count() >= 1, 10000);
    QVERIFY(!client.isConnected());

    FakeDaemon daemon;
    QVERIFY(daemon.listen(port));
    QTRY_VERIFY_WITH_TIMEOUT(client.isConnected(), 15000);
}

QTEST_MAIN(TestClientReconnect)
#include "test_client_reconnect.moc"
