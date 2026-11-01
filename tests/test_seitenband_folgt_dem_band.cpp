// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/test_seitenband_folgt_dem_band.cpp  (Contestprogramm)
// =================================================================
//
// Betreiber am 2026-10-23: „80 und 40 meter sollte immer automatisch
// LSB sein."
//
// Die Regel gab es (rigctldModeForContestMode in MainWindow.cpp:
// unter 10 MHz LSB, darüber USB) -- gerufen wurde sie aber nur in
// chooseMode(). Wer auf 20 m in SSB stand und das BAND auf 40
// wechselte, bekam die neue Frequenz und das ALTE Seitenband: USB auf
// 40 m, und niemand sagte es.
//
// Geprüft wird gegen ein Gerät, das mitschreibt: ein rigctld-
// Doppelgänger auf einem freien Port, der die Abfragen beantwortet
// und jede empfangene Zeile aufhebt. Dass das Programm die Regel
// KENNT, sagt nichts darüber, ob sie beim Bandwechsel auch hinausgeht.
//
// Modification history (Contestprogramm):
//   2026-10-23 — Original von Martin Fischer (OE5SOS), KI-gestützt
//                 über Anthropic Claude.
// =================================================================

#include <QtTest>

#include <QAction>
#include <QElapsedTimer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "ui/MainWindow.h"

#include <memory>

using namespace Contestprogramm;

namespace {

// Ein rigctld, das antwortet und mitschreibt. Antworten muss es:
// RigctldClient schickt den nächsten Befehl erst, wenn der vorige
// beantwortet ist -- ein stummer Doppelgänger ließe das `M LSB 0`
// einfach in der Schlange stehen, und der Prüflauf wäre grün, ohne
// irgendetwas belegt zu haben.
class MitschreibendesGeraet : public QObject
{
    Q_OBJECT
public:
    explicit MitschreibendesGeraet(QObject* parent = nullptr) : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            QTcpSocket* s = m_server.nextPendingConnection();
            if (!s) { return; }
            connect(s, &QTcpSocket::readyRead, this, [this, s] {
                m_rest.append(s->readAll());
                int nl = -1;
                while ((nl = m_rest.indexOf('\n')) >= 0) {
                    const QByteArray zeile = m_rest.left(nl).trimmed();
                    m_rest.remove(0, nl + 1);
                    if (zeile.isEmpty()) { continue; }
                    // Nur BEFEHLE aufheben, nicht die Abfragen des
                    // Takts. „f", „m" und „t" laufen im Sekundentakt
                    // weiter und machten jede Aussage über „es kam
                    // nichts mehr" unmöglich.
                    if (zeile.size() > 1) {
                        m_empfangen.append(QString::fromLatin1(zeile));
                    }
                    merken(zeile);
                    s->write(antwortAuf(zeile));
                }
            });
        });
    }

    bool start() { return m_server.listen(QHostAddress::LocalHost, 0); }
    quint16 port() const { return m_server.serverPort(); }
    QStringList empfangen() const { return m_empfangen; }
    void vergessen() { m_empfangen.clear(); }

    /// Warten, bis `stillMs` lang kein BEFEHL mehr ankam. Die Abfragen
    /// des Takts zählen nicht mit (siehe oben), sonst würde es nie
    /// still.
    void ruhe(int stillMs)
    {
        QElapsedTimer seitdem;
        seitdem.start();
        int zuletzt = m_empfangen.size();
        while (seitdem.elapsed() < stillMs) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            if (m_empfangen.size() != zuletzt) {
                zuletzt = m_empfangen.size();
                seitdem.restart();
            }
        }
    }

private:
    // ── Der Doppelgänger merkt sich, was man ihm sagt ────────────────
    //
    // Erst antwortete er auf jedes „f" mit derselben festen Frequenz.
    // Damit zog die CAT-Abfrage das Band im Programm ständig auf 20 m
    // zurück, und der Lauf fiel mal so und mal so aus -- ein Rennen
    // zwischen Abfragetakt und Prüfung. Ein Funkgerät, das die
    // gesetzte Frequenz vergisst, gibt es nicht; der Doppelgänger
    // darf sie also auch nicht vergessen.
    void merken(const QByteArray& zeile)
    {
        if (zeile.startsWith("F ")) {
            bool ok = false;
            const qint64 hz = zeile.mid(2).trimmed().toLongLong(&ok);
            if (ok) { m_frequenzHz = hz; }
        } else if (zeile.startsWith("M ")) {
            m_modus = QString::fromLatin1(zeile.mid(2).trimmed().split(' ').first());
        }
    }

    QByteArray antwortAuf(const QByteArray& zeile) const
    {
        // Die drei Abfragen, die der Klient im Takt stellt.
        if (zeile == "f") {
            return (QString::number(m_frequenzHz) + QLatin1Char('\n')).toLatin1();
        }
        if (zeile == "m") { return (m_modus + QStringLiteral("\n2700\n")).toLatin1(); }
        if (zeile == "t") { return QByteArrayLiteral("0\n"); }
        // Alles andere ist ein Befehl und wird quittiert.
        return QByteArrayLiteral("RPRT 0\n");
    }

    QTcpServer m_server;
    QByteArray m_rest;
    QStringList m_empfangen;
    qint64 m_frequenzHz = 14100000;
    QString m_modus = QStringLiteral("USB");
};

std::unique_ptr<AppController> baueSteuerung(QTemporaryDir& dir, quint16 port)
{
    auto c = std::make_unique<AppController>();
    if (!c->openDatabase(dir.filePath(QStringLiteral("seitenband.sqlite")))) {
        return nullptr;
    }
    ContestSettings s = c->settings();
    s.ownCallsign     = QStringLiteral("OE5SOS");
    s.ownGrid         = QStringLiteral("JN67UT");
    // Das Übungslog ist der einzige ausgelieferte Contest mit Bändern
    // auf BEIDEN Seiten der 10-MHz-Grenze -- und genau darum geht es.
    s.activeContestId = QStringLiteral("KW_UEBUNG");
    s.rigctldHost     = QStringLiteral("127.0.0.1");
    s.rigctldPort     = port;
    s.rotor1Enabled   = false;
    s.rotor2Enabled   = false;
    s.esmEnabled      = false;
    c->setSettings(s);
    return c;
}

} // namespace

class TestSeitenbandFolgtDemBand : public QObject
{
    Q_OBJECT

private slots:
    void beimWechselAufVierzigGehtLsbHinaus();
};

void TestSeitenbandFolgtDemBand::beimWechselAufVierzigGehtLsbHinaus()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    MitschreibendesGeraet geraet;
    QVERIFY2(geraet.start(), "der rigctld-Doppelgänger hört nicht zu");

    auto c = baueSteuerung(dir, geraet.port());
    QVERIFY(c);

    MainWindow fenster(*c);
    // 15 s, nicht 5: auf dem Windows-Laeufer hat der Verbindungsaufbau
    // die kurze Frist zweimal gerissen und den ganzen Lauf rot gemacht,
    // waehrend der Pruefstand selbst nichts gefunden hatte. Die uebrigen
    // Netz-Pruefstaende des Hauses (test_client_reconnect,
    // test_rigctld_supervision) warten seit je 15 s.
    QTRY_VERIFY_WITH_TIMEOUT(c->rigctldClient().isConnected(), 15000);

    // Erst auf 20 m und SSB -- die Lage, aus der heraus es schiefging.
    auto* zuZwanzig = fenster.findChild<QAction*>(QStringLiteral("bandAction_14"));
    QVERIFY2(zuZwanzig, "kein Bandeintrag für 14 MHz");
    zuZwanzig->trigger();
    auto* zuSsb = fenster.findChild<QAction*>(QStringLiteral("modeAction_SSB"));
    QVERIFY2(zuSsb, "kein Eintrag für SSB");
    zuSsb->trigger();
    QCoreApplication::processEvents();
    // SSB ist im Übungslog die Vorbelegung, der Griff oben also oft
    // ein Nulldurchgang (chooseMode kehrt bei gleicher Betriebsart
    // sofort um). Entscheidend ist nur, dass das Programm JETZT in
    // SSB steht -- sonst sagt der Rest nichts.
    QCOMPARE(fenster.currentMode(), QStringLiteral("SSB"));
    qInfo().noquote() << "bis hierher empfangen:"
                      << geraet.empfangen().join(QStringLiteral(" | "));

    // ── Jetzt der Bandwechsel, und NUR er ───────────────────────────
    geraet.vergessen();
    auto* zuVierzig = fenster.findChild<QAction*>(QStringLiteral("bandAction_7"));
    QVERIFY2(zuVierzig, "kein Bandeintrag für 7 MHz");
    zuVierzig->trigger();

    QTRY_VERIFY2_WITH_TIMEOUT(
        geraet.empfangen().contains(QStringLiteral("M LSB 0")),
        qPrintable(QStringLiteral("nach dem Wechsel auf 40 m kam kein LSB; "
                                  "empfangen wurde: %1")
                       .arg(geraet.empfangen().join(QStringLiteral(" | ")))),
        5000);

    // Und die Frequenz ging auch hinaus -- sonst hätte der Lauf nur
    // belegt, dass irgendein Befehl ankommt.
    QVERIFY2(geraet.empfangen().contains(QStringLiteral("F 7000000")),
             qPrintable(QStringLiteral("empfangen: %1")
                            .arg(geraet.empfangen().join(QStringLiteral(" | ")))));

    // ── Und auf 80 m ebenso ─────────────────────────────────────────
    //
    // Betreiber: „80 und 40 meter sollte immer automatisch LSB sein."
    // Also beide, und nicht nur das eine, bei dem sich das Seitenband
    // gegenueber vorher aendert.
    //
    // Geprueft wird ueber die REIHENFOLGE: das „M LSB 0" aus dem
    // 80-m-Wechsel steht hinter dem „F 3500000". Ein Zeitfenster
    // haette geraten, wann vom Schritt davor nichts mehr nachkommt.
    geraet.ruhe(400);
    auto* zuAchtzig = fenster.findChild<QAction*>(QStringLiteral("bandAction_3.5"));
    QVERIFY2(zuAchtzig, "kein Bandeintrag für 3,5 MHz");
    zuAchtzig->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(
        geraet.empfangen().contains(QStringLiteral("F 3500000")), 5000);
    geraet.ruhe(400);

    const QStringList befehle = geraet.empfangen();
    const int abAchtzig = befehle.lastIndexOf(QStringLiteral("F 3500000"));
    QVERIFY(abAchtzig >= 0);
    QVERIFY2(befehle.mid(abAchtzig + 1).contains(QStringLiteral("M LSB 0")),
             qPrintable(QStringLiteral("nach dem Wechsel auf 80 m kam kein LSB. "
                                       "Ganze Folge: %1")
                            .arg(befehle.join(QStringLiteral(" | ")))));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestSeitenbandFolgtDemBand tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_seitenband_folgt_dem_band.moc"
