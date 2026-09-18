#include <QtTest>

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>

#include "core/OnlineScoreboard.h"
#include "data/QsoRecord.h"

using namespace Contestprogramm;

namespace {

QsoRecord makeQso(const QString& call, const QString& band, const QString& mode, double km)
{
    QsoRecord r;
    r.callsign = call;
    r.band = band;
    r.mode = mode;
    r.timestampUtc = QStringLiteral("2026-10-03T14:01:00Z");
    r.gridSquare = QStringLiteral("JN58SD");
    r.distanceKm = km;
    r.contestId = QStringLiteral("IARU_R1_VHF_UHF");
    return r;
}

ScoreboardConfig sampleConfig()
{
    ScoreboardConfig c;
    c.enabled = true;
    c.contestName = QStringLiteral("IARU-R1-VHF");
    c.callsign = QStringLiteral("oe5sos");
    c.grid = QStringLiteral("JN67UT");
    c.club = QStringLiteral("ADL 505");
    c.bandOrder = {QStringLiteral("144"), QStringLiteral("432")};
    return c;
}

// The smallest HTTP server that can receive one POST: reads until the
// body arrived, remembers request line/headers/body, answers 200.
class OnePostServer : public QObject {
public:
    OnePostServer()
    {
        server.listen(QHostAddress::LocalHost, 0);
        QObject::connect(&server, &QTcpServer::newConnection, [this]() {
            QTcpSocket* socket = server.nextPendingConnection();
            QObject::connect(socket, &QTcpSocket::readyRead, [this, socket]() {
                buffer += socket->readAll();
                const int headerEnd = buffer.indexOf("\r\n\r\n");
                if (headerEnd < 0) {
                    return;
                }
                const QByteArray headers = buffer.left(headerEnd);
                int contentLength = 0;
                for (const QByteArray& line : headers.split('\n')) {
                    if (line.toLower().startsWith("content-length:")) {
                        contentLength = line.mid(15).trimmed().toInt();
                    }
                }
                if (buffer.size() - (headerEnd + 4) < contentLength) {
                    return;
                }
                request = headers;
                body = buffer.mid(headerEnd + 4, contentLength);
                socket->write("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nOK");
                socket->flush();
                socket->disconnectFromHost();
                received = true;
            });
        });
    }
    QUrl url() const { return QUrl(QStringLiteral("http://127.0.0.1:%1/post/").arg(server.serverPort())); }

    QTcpServer server;
    QByteArray buffer;
    QByteArray request;
    QByteArray body;
    bool received = false;
};

} // namespace

class TestOnlineScoreboard : public QObject
{
    Q_OBJECT

private slots:
    void xmlCarriesBreakdownScoreAndStation();
    void postsWithBasicAuthAndReportsSuccess();
    void disabledOrUnconfiguredSendsNothing();
};

void TestOnlineScoreboard::xmlCarriesBreakdownScoreAndStation()
{
    QVector<QsoRecord> records = {
        makeQso(QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("SSB"), 187.4),
        makeQso(QStringLiteral("OE3XYZ"), QStringLiteral("144"), QStringLiteral("CW"), 214.6),
        makeQso(QStringLiteral("OE5XYZ"), QStringLiteral("432"), QStringLiteral("FM"), 50.2),
        makeQso(QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("SSB"), 187.4),
    };
    records[3].isDupe = true;
    const QDateTime stamp(QDate(2026, 10, 3), QTime(14, 5, 0), QTimeZone::utc());
    const QString xml = QString::fromUtf8(OnlineScoreboard::buildXml(records, sampleConfig(), stamp));

    QVERIFY2(xml.contains(QStringLiteral("<dynamicresults>")), qPrintable(xml));
    QVERIFY2(xml.contains(QStringLiteral("<contest>IARU-R1-VHF</contest>")), qPrintable(xml));
    QVERIFY2(xml.contains(QStringLiteral("<call>OE5SOS</call>")), qPrintable(xml));
    QVERIFY2(xml.contains(QStringLiteral("<club>ADL 505</club>")), qPrintable(xml));
    QVERIFY2(xml.contains(QStringLiteral("<grid6>JN67UT</grid6>")), qPrintable(xml));
    QVERIFY2(xml.contains(QStringLiteral("<qso band=\"144\" mode=\"CW\">1</qso>")), qPrintable(xml));
    QVERIFY2(xml.contains(QStringLiteral("<qso band=\"144\" mode=\"PH\">1</qso>")), qPrintable(xml)); // the dupe does not count
    QVERIFY2(xml.contains(QStringLiteral("<point band=\"144\" mode=\"ALL\">403</point>")), qPrintable(xml));
    QVERIFY2(xml.contains(QStringLiteral("<qso band=\"432\" mode=\"PH\">1</qso>")), qPrintable(xml));
    QVERIFY2(xml.contains(QStringLiteral("<point band=\"432\" mode=\"ALL\">51</point>")), qPrintable(xml));
    QVERIFY2(xml.contains(QStringLiteral("<qso band=\"total\" mode=\"ALL\">3</qso>")), qPrintable(xml));
    QVERIFY2(xml.contains(QStringLiteral("<point band=\"total\" mode=\"ALL\">454</point>")), qPrintable(xml));
    QVERIFY2(xml.contains(QStringLiteral("<score>454</score>")), qPrintable(xml));
    QVERIFY2(xml.contains(QStringLiteral("<timestamp>2026-10-03 14:05:00</timestamp>")), qPrintable(xml));
    QVERIFY2(xml.contains(QStringLiteral("bands=\"ALL\"")), qPrintable(xml));
}

void TestOnlineScoreboard::postsWithBasicAuthAndReportsSuccess()
{
    OnePostServer server;
    QVERIFY(server.server.isListening());

    OnlineScoreboard board;
    ScoreboardConfig config = sampleConfig();
    config.url = server.url();
    config.username = QStringLiteral("oe5sos");
    config.password = QStringLiteral("geheim");
    board.setConfig(config);
    board.setRecords({makeQso(QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("SSB"), 187.4)});

    QSignalSpy posted(&board, &OnlineScoreboard::posted);
    QSignalSpy failed(&board, &OnlineScoreboard::failed);
    QString error;
    QVERIFY2(board.postNow(&error), qPrintable(error));
    QTRY_VERIFY_WITH_TIMEOUT(server.received, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(posted.size(), 1, 5000);
    QCOMPARE(failed.size(), 0);

    QVERIFY2(server.request.startsWith("POST /post/ HTTP/1.1"), server.request.constData());
    // "oe5sos:geheim" in Base64.
    QVERIFY2(server.request.contains("Authorization: Basic b2U1c29zOmdlaGVpbQ=="), server.request.constData());
    QVERIFY2(server.request.toLower().contains("content-type: text/xml"), server.request.constData());
    QVERIFY2(server.body.contains("<score>188</score>"), server.body.constData());
}

void TestOnlineScoreboard::disabledOrUnconfiguredSendsNothing()
{
    OnlineScoreboard board;
    ScoreboardConfig config = sampleConfig();
    config.enabled = false;
    config.url = QUrl(QStringLiteral("http://127.0.0.1:1/post/"));
    board.setConfig(config);
    QString error;
    QVERIFY(!board.postNow(&error));
    QVERIFY(error.isEmpty()); // switched off is not a fault

    config.enabled = true;
    config.url = QUrl();
    board.setConfig(config);
    QVERIFY(!board.postNow(&error));
    QVERIFY(!error.isEmpty());
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    TestOnlineScoreboard tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_online_scoreboard.moc"
