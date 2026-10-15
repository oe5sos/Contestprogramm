#include <QtTest>

#include <QCoreApplication>
#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>

#include "core/RotctldClient.h"

using namespace Contestprogramm;

// QTcpServer-based rotctld stand-in, same idea as test_rigctldprotocol.cpp's
// MockRigctldServer: listens on an ephemeral loopback port, accepts one
// client, and dispatches whatever line it receives to a canned reply -- no
// real rotctld/Hamlib/rotor involved.
class MockRotctldServer : public QObject
{
    Q_OBJECT

public:
    explicit MockRotctldServer(QObject* parent = nullptr) : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this, &MockRotctldServer::onNewConnection);
    }

    bool startListening() { return m_server.listen(QHostAddress::LocalHost, 0); }

    // A rotctld that answers the position query with one line MORE than
    // the two this client reads -- see the matching test below.
    void setExtraLineAfterPosition(const QByteArray& line) { m_extraAfterPosition = line; }
    quint16 port() const { return m_server.serverPort(); }

    QList<QByteArray> commandsReceived() const { return m_commandsReceived; }

    // "P 145.00 0.00" -> lastReportedPosition() reflects it, so a
    // subsequent "p" poll can be checked against what was actually
    // commanded.
    void setPosition(double azDeg, double elDeg)
    {
        m_azDeg = azDeg;
        m_elDeg = elDeg;
    }

signals:
    void commandReceived(const QByteArray& command);

private slots:
    void onNewConnection()
    {
        m_client = m_server.nextPendingConnection();
        if (!m_client) { return; }
        connect(m_client, &QTcpSocket::readyRead, this, &MockRotctldServer::onReadyRead);
    }

    void onReadyRead()
    {
        if (!m_client) { return; }
        m_buffer += m_client->readAll();
        while (true) {
            const int idx = m_buffer.indexOf('\n');
            if (idx < 0) { break; }
            const QByteArray line = m_buffer.left(idx).trimmed();
            m_buffer.remove(0, idx + 1);
            if (line.isEmpty()) { continue; }
            m_commandsReceived.append(line);
            emit commandReceived(line);
            respond(line);
        }
    }

private:
    void respond(const QByteArray& line)
    {
        if (!m_client) { return; }
        if (line == "p") {
            m_client->write(QStringLiteral("%1\n%2\n").arg(m_azDeg, 0, 'f', 6).arg(m_elDeg, 0, 'f', 6).toLatin1());
            if (!m_extraAfterPosition.isEmpty()) {
                m_client->write(m_extraAfterPosition);
            }
        } else if (line.startsWith("P ")) {
            const QList<QByteArray> parts = line.split(' ');
            if (parts.size() >= 3) {
                m_azDeg = parts.at(1).toDouble();
                m_elDeg = parts.at(2).toDouble();
            }
            m_client->write("RPRT 0\n");
        }
    }

    QTcpServer m_server;
    QTcpSocket* m_client = nullptr;
    QByteArray m_buffer;
    QByteArray m_extraAfterPosition;
    QList<QByteArray> m_commandsReceived;
    double m_azDeg = 145.0;
    double m_elDeg = 0.0;
};

class TestRotctldProtocol : public QObject
{
    Q_OBJECT

private slots:
    void positionReplyParses();
    void reportReplyParses();
    void errorReportIsNeverReadAsPosition();
    void setPositionCommandFormatsPlainDecimalsAndWrapsAzimuth();
    void setPositionCommandAlwaysSendsGivenElevation();

    void liveClientReceivesAzimuth();
    void liveClientSendsSetAzimuthWithZeroElevation();
    void leftoverReplyLineNeverShiftsTheNextAnswer();
};

void TestRotctldProtocol::positionReplyParses()
{
    double az = 0.0, el = 0.0;
    QVERIFY(RotctldClient::parsePosition(QByteArrayLiteral("145.000000\n0.000000\n"), az, el));
    QCOMPARE(az, 145.0);
    QCOMPARE(el, 0.0);
}

void TestRotctldProtocol::reportReplyParses()
{
    int code = 0;
    QVERIFY(RotctldClient::parseReport(QByteArrayLiteral("RPRT 0\n"), code));
    QCOMPARE(code, 0);

    QVERIFY(RotctldClient::parseReport(QByteArrayLiteral("RPRT -1\n"), code));
    QCOMPARE(code, -1);
}

void TestRotctldProtocol::errorReportIsNeverReadAsPosition()
{
    // "RPRT -1" must never be misread as a position -- the trailing
    // "-1" would otherwise parse as -1 degrees azimuth.
    double az = 0.0, el = 0.0;
    QVERIFY(!RotctldClient::parsePosition(QByteArrayLiteral("RPRT -1\n"), az, el));
}

void TestRotctldProtocol::setPositionCommandFormatsPlainDecimalsAndWrapsAzimuth()
{
    QCOMPARE(RotctldClient::setPositionCommand(145.0, 0.0), QByteArrayLiteral("P 145.00 0.00\n"));
    // Wrapped: -10 -> 350.
    QCOMPARE(RotctldClient::setPositionCommand(-10.0, 0.0), QByteArrayLiteral("P 350.00 0.00\n"));
    QCOMPARE(RotctldClient::setPositionCommand(370.0, 0.0), QByteArrayLiteral("P 10.00 0.00\n"));
}

void TestRotctldProtocol::setPositionCommandAlwaysSendsGivenElevation()
{
    // setAzimuth() always calls this with elevation 0 (azimuth-only
    // rotors) -- verified at the RigctldClient-analogous call site, but
    // the pure function itself just formats whatever it is given.
    QCOMPARE(RotctldClient::setPositionCommand(90.0, 0.0), QByteArrayLiteral("P 90.00 0.00\n"));
}

void TestRotctldProtocol::liveClientReceivesAzimuth()
{
    MockRotctldServer server;
    server.setPosition(145.0, 0.0);
    QVERIFY(server.startListening());

    RotctldClient client;
    QSignalSpy azSpy(&client, &RotctldClient::azimuthChanged);

    client.setTarget(QStringLiteral("127.0.0.1"), server.port());
    client.connectToRotor();

    QVERIFY(azSpy.wait(2000));
    QCOMPARE(client.azimuthDeg(), 145.0);
    QVERIFY(client.isConnected());
}

void TestRotctldProtocol::liveClientSendsSetAzimuthWithZeroElevation()
{
    MockRotctldServer server;
    server.setPosition(145.0, 0.0);
    QVERIFY(server.startListening());

    RotctldClient client;
    QSignalSpy azSpy(&client, &RotctldClient::azimuthChanged);

    client.setTarget(QStringLiteral("127.0.0.1"), server.port());
    client.connectToRotor();
    QVERIFY(azSpy.wait(2000)); // wait for the initial poll round to clear

    client.setAzimuth(210.0);

    // Queues behind the poll round already in flight -- same reasoning
    // as test_rigctldprotocol.cpp's equivalent -- poll for the exact
    // command rather than assuming which round-trip it lands on.
    const bool sawSetAzimuth = QTest::qWaitFor([&server]() {
        for (const QByteArray& cmd : server.commandsReceived()) {
            if (cmd == "P 210.00 0.00") {
                return true;
            }
        }
        return false;
    }, 3000);
    QVERIFY(sawSetAzimuth);
}

// Same class of fault as RigctldClient's own test of this name: an
// unread leftover line was handed to the next command as its reply.
// With a third NUMERIC line (a rotor/rotctld that reports more than
// azimuth+elevation) the shifted reply parses perfectly well -- as a
// heading of 0 degrees, while the rotor stands at 145. The dial then
// points north and the operator turns the antenna to correct it.
void TestRotctldProtocol::leftoverReplyLineNeverShiftsTheNextAnswer()
{
    MockRotctldServer server;
    server.setExtraLineAfterPosition(QByteArrayLiteral("0.000000\n"));
    QVERIFY(server.startListening());

    RotctldClient client;
    client.setPollIntervalMs(200);
    QSignalSpy azimuthSpy(&client, &RotctldClient::azimuthChanged);
    client.setTarget(QStringLiteral("127.0.0.1"), server.port());
    client.connectToRotor();

    const bool sawRounds = QTest::qWaitFor([&server]() {
        int positionQueries = 0;
        for (const QByteArray& cmd : server.commandsReceived()) {
            if (cmd == "p") { ++positionQueries; }
        }
        return positionQueries >= 6;
    }, 6000);
    QVERIFY(sawRounds);

    // The rotor never moved, so EVERY reported heading must be 145.
    QVERIFY(!azimuthSpy.isEmpty());
    for (const QList<QVariant>& emission : azimuthSpy) {
        QCOMPARE(emission.at(0).toDouble(), 145.0);
    }
    QCOMPARE(client.azimuthDeg(), 145.0);
    QVERIFY(client.isConnected());
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    TestRotctldProtocol tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_rotctldprotocol.moc"
