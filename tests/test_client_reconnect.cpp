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

QTEST_MAIN(TestClientReconnect)
#include "test_client_reconnect.moc"
