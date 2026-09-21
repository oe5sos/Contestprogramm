// A rotctld/rigctld that is not up yet when the client first dials --
// the everyday case at the contest site, where the program starts
// before the Hamlib daemons -- must be found once it appears. Qt
// reports a refused connect through errorOccurred() alone, never
// disconnected(), which is where both clients used to arm their
// retry: they sat in Connecting for good (observed 2026-09-21, the
// status bar's permanent "CAT: verbindet...").

#include <QtTest>

#include <QTcpServer>
#include <QTcpSocket>

#include "core/DxClusterClient.h"
#include "core/On4kstClient.h"
#include "core/RigctldClient.h"
#include "core/RotctldClient.h"

using namespace Contestprogramm;

namespace {

// A port nothing listens on right now, found by binding and releasing.
quint16 freePort()
{
    QTcpServer probe;
    probe.listen(QHostAddress::LocalHost, 0);
    const quint16 port = probe.serverPort();
    probe.close();
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
    QTRY_COMPARE_WITH_TIMEOUT(client.state(), RotctldClient::State::Disconnected, 3000);

    FakeDaemon daemon;
    QVERIFY(daemon.listen(port));
    QTRY_VERIFY_WITH_TIMEOUT(client.isConnected(), 8000);
    QTRY_COMPARE_WITH_TIMEOUT(client.azimuthDeg(), 123.0, 3000);
}

void TestClientReconnect::rigClientFindsARigctldThatAppearsLater()
{
    const quint16 port = freePort();
    RigctldClient client;
    client.setTarget(QStringLiteral("127.0.0.1"), port);
    client.connectToRig();
    QCOMPARE(client.state(), RigctldClient::State::Connecting);
    QTRY_COMPARE_WITH_TIMEOUT(client.state(), RigctldClient::State::Disconnected, 3000);

    FakeDaemon daemon;
    QVERIFY(daemon.listen(port));
    QTRY_VERIFY_WITH_TIMEOUT(client.isConnected(), 8000);
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
    QTRY_VERIFY_WITH_TIMEOUT(errors.count() >= 1, 3000);
    QVERIFY(!client.isConnected());

    FakeDaemon daemon;
    QVERIFY(daemon.listen(port));
    QTRY_VERIFY_WITH_TIMEOUT(client.isConnected(), 9000);
    QCOMPARE(connected.count(), 1);

    // A deliberate disconnect ends the retries.
    client.disconnectFromServer();
    QTRY_VERIFY_WITH_TIMEOUT(!client.isConnected(), 3000);
    QTest::qWait(200);
    QVERIFY(!client.isConnected());
}

void TestClientReconnect::clusterClientDialsAgainAfterARefusedConnect()
{
    const quint16 port = freePort();
    DxClusterClient client;
    QSignalSpy errors(&client, &DxClusterClient::connectionError);
    client.connectToCluster(QStringLiteral("127.0.0.1"), port, QStringLiteral("OE5SOS"));
    QTRY_VERIFY_WITH_TIMEOUT(errors.count() >= 1, 3000);
    QVERIFY(!client.isConnected());

    FakeDaemon daemon;
    QVERIFY(daemon.listen(port));
    QTRY_VERIFY_WITH_TIMEOUT(client.isConnected(), 9000);
}

QTEST_MAIN(TestClientReconnect)
#include "test_client_reconnect.moc"
