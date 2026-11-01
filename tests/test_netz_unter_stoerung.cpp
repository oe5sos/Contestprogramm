// Die Netzstrecken gegen ein böswilliges Gegenüber. Am Contest-Standort
// (Feuerkogel) ist die Verbindung wackelig, und ein Chat-Server, der
// annimmt und sofort wieder abbricht, ist dort kein Gedankenspiel.
//
// Geprüft wird nicht, DASS es einen Fehler gibt -- das wäre zu wenig --
// sondern dass das Programm die Störung aushält, ohne zu stürmen: ein
// Client, der zehnmal in der Sekunde neu wählt, bringt im Zweifel die
// letzte Bandbreite um, die noch da ist.

#include <QtTest>

#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include "core/DxClusterClient.h"
#include "core/On4kstClient.h"
#include "core/RigctldClient.h"

using namespace Contestprogramm;

namespace {

// Nimmt jede Verbindung an, schickt eine halbe Zeile und bricht hart ab.
class Abbrecher : public QObject
{
    Q_OBJECT
public:
    explicit Abbrecher(int abbruchNachMs, QObject* parent = nullptr)
        : QObject(parent), m_nach(abbruchNachMs)
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            QTcpSocket* s = m_server.nextPendingConnection();
            if (!s) {
                return;
            }
            ++m_verbindungen;
            // Eine angefangene Zeile, damit auch der Lesepfad etwas zu
            // tun bekommt -- und nie ein Zeilenende.
            s->write("DX de OE5XXX:    1443");
            QTimer::singleShot(m_nach, s, [s] {
                s->abort();
                s->deleteLater();
            });
        });
    }
    bool start() { return m_server.listen(QHostAddress::LocalHost, 0); }
    quint16 port() const { return m_server.serverPort(); }
    int verbindungen() const { return m_verbindungen; }

private:
    QTcpServer m_server;
    int m_nach;
    int m_verbindungen = 0;
};

void laufenLassen(int ms)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
}

} // namespace

class TestNetzUnterStoerung : public QObject
{
    Q_OBJECT

private slots:
    void dxClusterHaeltDauerabbruchAus();
    void on4kstHaeltDauerabbruchAus();
    void rigctldWachhundSchlaegtAnWennDasGeraetSchweigt();
};

void TestNetzUnterStoerung::dxClusterHaeltDauerabbruchAus()
{
    Abbrecher server(120);
    QVERIFY(server.start());

    DxClusterClient client;
    QSignalSpy fehler(&client, &DxClusterClient::connectionError);
    client.connectToCluster(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("OE5TST"));

    laufenLassen(6000);

    // Es wird überhaupt gewählt, und es wird gesagt, dass es nicht geht.
    QVERIFY2(server.verbindungen() >= 1,
             qPrintable(QStringLiteral("gar kein Verbindungsversuch in 6 s")));
    QVERIFY(!fehler.isEmpty());
    // Aber nicht im Sekundentakt: die Wartezeit zwischen zwei Versuchen
    // liegt bei 5 s und verdoppelt sich (siehe scheduleReconnect()).
    QVERIFY2(server.verbindungen() <= 20,
             qPrintable(QStringLiteral("%1 Versuche in 6 s -- das ist ein Sturm")
                            .arg(server.verbindungen())));
    QVERIFY(!client.isConnected());
}

void TestNetzUnterStoerung::on4kstHaeltDauerabbruchAus()
{
    Abbrecher server(120);
    QVERIFY(server.start());

    On4kstClient client;
    QSignalSpy fehler(&client, &On4kstClient::connectionError);
    client.connectAndLogin(QStringLiteral("127.0.0.1"), server.port(),
                           QStringLiteral("OE5TST"), QStringLiteral("geheim"), 2);

    laufenLassen(6000);

    QVERIFY2(server.verbindungen() >= 1,
             qPrintable(QStringLiteral("gar kein Verbindungsversuch in 6 s")));
    QVERIFY(!fehler.isEmpty());
    QVERIFY2(server.verbindungen() <= 20,
             qPrintable(QStringLiteral("%1 Versuche in 6 s -- das ist ein Sturm")
                            .arg(server.verbindungen())));
}

void TestNetzUnterStoerung::rigctldWachhundSchlaegtAnWennDasGeraetSchweigt()
{
    // Ein rigctld, das die Verbindung annimmt und dann nie antwortet --
    // der Fall, für den der Antwort-Wachhund da ist: ohne ihn bliebe
    // der Client für immer "verbunden" und stumm.
    QTcpServer stumm;
    QVERIFY(stumm.listen(QHostAddress::LocalHost, 0));
    QList<QTcpSocket*> angenommen;
    connect(&stumm, &QTcpServer::newConnection, this, [&stumm, &angenommen] {
        if (QTcpSocket* s = stumm.nextPendingConnection()) {
            angenommen << s; // annehmen, nichts sagen
        }
    });

    RigctldClient client;
    QSignalSpy fehler(&client, &RigctldClient::errorOccurred);
    client.setTarget(QStringLiteral("127.0.0.1"), stumm.serverPort());
    client.connectToRig();

    QVERIFY2(QTest::qWaitFor([&fehler] { return !fehler.isEmpty(); }, 15000),
             "der Wachhund hat in 15 s nicht angeschlagen");
    QVERIFY2(fehler.first().first().toString().contains(QStringLiteral("stopped answering")),
             qPrintable(fehler.first().first().toString()));
    qDeleteAll(angenommen);
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    TestNetzUnterStoerung tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_netz_unter_stoerung.moc"
