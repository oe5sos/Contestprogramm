// core/ClockCheck.h: the machine's clock against a server's Date
// header -- offset sign and size, several hosts tried, offline reported
// as such -- and the Startcheck line built from it.

#include <QtTest>

#include <QTcpServer>
#include <QTcpSocket>
#include <QTimeZone>

#include "core/ClockCheck.h"
#include "data/ReadinessCheck.h"

using namespace Contestprogramm;

namespace {

// The smallest web server: answers every request with a Date header
// `skewSecs` away from the true time (positive = the server's clock is
// ahead, so the local clock reads as behind).
class DateServer : public QObject
{
public:
    explicit DateServer(qint64 skewSecs)
        : m_skewSecs(skewSecs)
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            QTcpSocket* socket = m_server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                if (!socket->readAll().contains("\r\n\r\n")) {
                    return;
                }
                const QDateTime stamp = QDateTime::currentDateTimeUtc().addSecs(m_skewSecs);
                const QByteArray date = QLocale::c().toString(stamp, QStringLiteral("ddd, dd MMM yyyy HH:mm:ss")).toLatin1() + " GMT";
                socket->write("HTTP/1.1 200 OK\r\nDate: " + date + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                socket->disconnectFromHost();
            });
        });
        m_server.listen(QHostAddress::LocalHost, 0);
    }
    QString url() const { return QStringLiteral("http://127.0.0.1:%1/").arg(m_server.serverPort()); }

private:
    QTcpServer m_server;
    qint64 m_skewSecs;
};

quint16 closedPort()
{
    QTcpServer probe;
    probe.listen(QHostAddress::LocalHost, 0);
    const quint16 port = probe.serverPort();
    probe.close();
    return port;
}

const ReadinessItem* clockItem(const ReadinessResult& result)
{
    for (const ReadinessItem& item : result.items) {
        if (item.code == QStringLiteral("clock")) {
            return &item;
        }
    }
    return nullptr;
}

} // namespace

class TestClockCheck : public QObject
{
    Q_OBJECT

private slots:
    void parsesTheHttpDateHeader();
    void reportsTheOffsetAgainstTheServer();
    void triesTheNextHostAndReportsOffline();
    void startcheckLineFollowsTheOffset();
};

void TestClockCheck::parsesTheHttpDateHeader()
{
    const QDateTime parsed = ClockCheck::parseHttpDate(QStringLiteral("Sat, 03 Oct 2026 14:00:00 GMT"));
    QVERIFY(parsed.isValid());
    QCOMPARE(parsed, QDateTime(QDate(2026, 10, 3), QTime(14, 0), QTimeZone::utc()));
    QVERIFY(!ClockCheck::parseHttpDate(QString()).isValid());
    QVERIFY(!ClockCheck::parseHttpDate(QStringLiteral("gestern")).isValid());
}

void TestClockCheck::reportsTheOffsetAgainstTheServer()
{
    // A server 120 s ahead: this machine reads 120 s behind (-120).
    DateServer ahead(120);
    ClockCheck check;
    check.setUrls({ahead.url()});
    QSignalSpy spy(&check, &ClockCheck::finished);
    check.start();
    QVERIFY(check.isRunning());
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 5000);
    QVERIFY(spy.first().at(0).toBool());
    const qint64 offset = spy.first().at(1).toLongLong();
    QVERIFY2(offset <= -118 && offset >= -122, qPrintable(QString::number(offset)));
    QVERIFY(check.hasResult());
    QVERIFY(check.reachable());
    QCOMPARE(check.offsetSecs(), offset);
    QCOMPARE(check.source(), QStringLiteral("127.0.0.1"));

    // In sync: within a second.
    DateServer exact(0);
    check.setUrls({exact.url()});
    check.start();
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 2, 5000);
    QVERIFY(std::llabs(spy.at(1).at(1).toLongLong()) <= 1);
}

void TestClockCheck::triesTheNextHostAndReportsOffline()
{
    DateServer exact(0);
    ClockCheck check;
    // The first host is dead: the second answers.
    check.setUrls({QStringLiteral("http://127.0.0.1:%1/").arg(closedPort()), exact.url()});
    QSignalSpy spy(&check, &ClockCheck::finished);
    check.start();
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 8000);
    QVERIFY(spy.first().at(0).toBool());

    // Nobody answers: offline, no offset claimed.
    check.setUrls({QStringLiteral("http://127.0.0.1:%1/").arg(closedPort())});
    check.start();
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 2, 8000);
    QVERIFY(!spy.at(1).at(0).toBool());
    QVERIFY(check.hasResult());
    QVERIFY(!check.reachable());
}

void TestClockCheck::startcheckLineFollowsTheOffset()
{
    ReadinessContext ctx;
    ctx.nowUtc = QDateTime::currentDateTimeUtc();
    ctx.ownCallsign = QStringLiteral("OE5SOS");
    ctx.ownGrid = QStringLiteral("JN67UT");
    QCOMPARE(clockItem(checkReadiness(ctx))->level, ReadinessItem::Level::Hint); // still checking

    ctx.clockChecked = true;
    ctx.clockReachable = false;
    QCOMPARE(clockItem(checkReadiness(ctx))->level, ReadinessItem::Level::Hint);
    QVERIFY(clockItem(checkReadiness(ctx))->detail.contains(QStringLiteral("kein Internet")));

    ctx.clockReachable = true;
    ctx.clockSource = QStringLiteral("www.google.com");
    ctx.clockOffsetSecs = 2;
    QCOMPARE(clockItem(checkReadiness(ctx))->level, ReadinessItem::Level::Ok);
    QCOMPARE(clockItem(checkReadiness(ctx))->detail, QStringLiteral("Stimmt (+2 s gegen www.google.com)"));

    ctx.clockOffsetSecs = -40;
    QCOMPARE(clockItem(checkReadiness(ctx))->level, ReadinessItem::Level::Warning);
    QVERIFY(clockItem(checkReadiness(ctx))->detail.startsWith(QStringLiteral("Geht 40 s nach")));

    ctx.clockOffsetSecs = 200;
    QCOMPARE(clockItem(checkReadiness(ctx))->level, ReadinessItem::Level::Error);
    QVERIFY(clockItem(checkReadiness(ctx))->detail.startsWith(QStringLiteral("Geht 3 min vor")));
}

QTEST_MAIN(TestClockCheck)
#include "test_clock_check.moc"
