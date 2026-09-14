#include <QtTest>

#include <QCoreApplication>
#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>

#include "core/RigctldClient.h"

using namespace Contestprogramm;

// QTcpServer-based rigctld stand-in, same idea as the plan's
// MockOn4kstServer for the ON4KST client: listens on an ephemeral
// loopback port, accepts one client, and dispatches whatever line it
// receives to a canned reply -- no real rigctld/Hamlib/radio involved.
class MockRigctldServer : public QObject
{
    Q_OBJECT

public:
    explicit MockRigctldServer(QObject* parent = nullptr) : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this, &MockRigctldServer::onNewConnection);
    }

    bool startListening() { return m_server.listen(QHostAddress::LocalHost, 0); }
    quint16 port() const { return m_server.serverPort(); }

    QList<QByteArray> commandsReceived() const { return m_commandsReceived; }

signals:
    void commandReceived(const QByteArray& command);

private slots:
    void onNewConnection()
    {
        m_client = m_server.nextPendingConnection();
        if (!m_client) { return; }
        connect(m_client, &QTcpSocket::readyRead, this, &MockRigctldServer::onReadyRead);
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
        if (line == "f") {
            m_client->write("145200000\n");
        } else if (line.startsWith("F ")) {
            m_client->write("RPRT 0\n");
        } else if (line == "m") {
            m_client->write("USB\n2400\n");
        } else if (line == "t") {
            m_client->write("0\n");
        } else if (line.startsWith("T ")) {
            m_client->write("RPRT 0\n");
        }
    }

    QTcpServer m_server;
    QTcpSocket* m_client = nullptr;
    QByteArray m_buffer;
    QList<QByteArray> m_commandsReceived;
};

class TestRigctldProtocol : public QObject
{
    Q_OBJECT

private slots:
    void frequencyReplyParses();
    void modeReplyParses();
    void pttReplyParses();
    void reportReplyParses();
    void errorReportIsNeverReadAsFrequency();
    void errorReportIsNeverReadAsPtt();
    void setFrequencyCommandFormatsPlainInteger();
    void setPttCommandFormatsZeroOrOne();

    void liveClientReceivesFrequencyModeAndPtt();
    void liveClientSendsSetFrequencyCommand();
};

void TestRigctldProtocol::frequencyReplyParses()
{
    qint64 hz = 0;
    QVERIFY(RigctldClient::parseFrequency(QByteArrayLiteral("145200000\n"), hz));
    QCOMPARE(hz, static_cast<qint64>(145200000));
}

void TestRigctldProtocol::modeReplyParses()
{
    QString mode;
    int passbandHz = 0;
    QVERIFY(RigctldClient::parseMode(QByteArrayLiteral("USB\n2400\n"), mode, passbandHz));
    QCOMPARE(mode, QStringLiteral("USB"));
    QCOMPARE(passbandHz, 2400);
}

void TestRigctldProtocol::pttReplyParses()
{
    bool active = true;
    QVERIFY(RigctldClient::parsePtt(QByteArrayLiteral("0\n"), active));
    QVERIFY(!active);

    QVERIFY(RigctldClient::parsePtt(QByteArrayLiteral("1\n"), active));
    QVERIFY(active);
}

void TestRigctldProtocol::reportReplyParses()
{
    int code = 0;
    QVERIFY(RigctldClient::parseReport(QByteArrayLiteral("RPRT 0\n"), code));
    QCOMPARE(code, 0);

    QVERIFY(RigctldClient::parseReport(QByteArrayLiteral("RPRT -6\n"), code));
    QCOMPARE(code, -6);
}

void TestRigctldProtocol::errorReportIsNeverReadAsFrequency()
{
    // Same reasoning as RotctldClient::parsePosition: an RPRT error
    // line must never be misread as a numeric value -- "RPRT -6" would
    // otherwise parse its trailing "-6" as -6 Hz.
    qint64 hz = 0;
    QVERIFY(!RigctldClient::parseFrequency(QByteArrayLiteral("RPRT -6\n"), hz));
}

void TestRigctldProtocol::errorReportIsNeverReadAsPtt()
{
    bool active = false;
    QVERIFY(!RigctldClient::parsePtt(QByteArrayLiteral("RPRT -1\n"), active));
}

void TestRigctldProtocol::setFrequencyCommandFormatsPlainInteger()
{
    QCOMPARE(RigctldClient::setFrequencyCommand(145500000), QByteArrayLiteral("F 145500000\n"));
}

void TestRigctldProtocol::setPttCommandFormatsZeroOrOne()
{
    QCOMPARE(RigctldClient::setPttCommand(true), QByteArrayLiteral("T 1\n"));
    QCOMPARE(RigctldClient::setPttCommand(false), QByteArrayLiteral("T 0\n"));
}

void TestRigctldProtocol::liveClientReceivesFrequencyModeAndPtt()
{
    MockRigctldServer server;
    QVERIFY(server.startListening());

    RigctldClient client;
    QSignalSpy freqSpy(&client, &RigctldClient::frequencyChanged);
    QSignalSpy modeSpy(&client, &RigctldClient::modeChanged);

    client.setTarget(QStringLiteral("127.0.0.1"), server.port());
    client.connectToRig();

    QVERIFY(freqSpy.wait(2000));
    QVERIFY(modeSpy.wait(2000));

    QCOMPARE(client.frequencyHz(), static_cast<qint64>(145200000));
    QCOMPARE(client.mode(), QStringLiteral("USB"));
    QCOMPARE(client.passbandHz(), 2400);
    QVERIFY(client.isConnected());
    QVERIFY(!client.pttActive());
}

void TestRigctldProtocol::liveClientSendsSetFrequencyCommand()
{
    MockRigctldServer server;
    QVERIFY(server.startListening());

    RigctldClient client;
    QSignalSpy freqSpy(&client, &RigctldClient::frequencyChanged);

    client.setTarget(QStringLiteral("127.0.0.1"), server.port());
    client.connectToRig();
    QVERIFY(freqSpy.wait(2000)); // wait for the initial poll round to clear

    client.setFrequency(145500000);

    // The set-frequency command queues behind the still-outstanding
    // mode/PTT queries from the initial poll round (one command
    // outstanding at a time -- see RigctldClient::pump), so it can take
    // more than one signal/round-trip before it actually reaches the
    // server. Poll for it directly instead of waiting for exactly one
    // more commandReceived emission.
    const bool sawSetFrequency = QTest::qWaitFor([&server]() {
        for (const QByteArray& cmd : server.commandsReceived()) {
            if (cmd == "F 145500000") {
                return true;
            }
        }
        return false;
    }, 3000);
    QVERIFY(sawSetFrequency);
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    TestRigctldProtocol tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_rigctldprotocol.moc"
