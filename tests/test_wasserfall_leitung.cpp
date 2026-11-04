// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/test_wasserfall_leitung.cpp  (Contestprogramm)
// =================================================================
//
// test_wasserfall.cpp prüft den Rahmenleser für sich: eine Funktion,
// ein QByteArray, ein Ergebnis. Das ist das halbe Haus. Die andere
// Hälfte ist die LEITUNG -- dass ein Rahmen, der wirklich über einen
// WebSocket kommt, am anderen Ende als Spektrum im Panel landet, und
// dass der Client von selbst wieder anklopft, wenn das Gegenüber weg
// ist.
//
// Gedächtnisnotiz vom 2026-10-30, „Binden statt verbinden": ein
// Prüfstand, der nur die Funktion aufruft, belegt nicht, dass die
// Verbindung steht. Hier bindet ein Doppel (tests/mocks/
// MockSpectrumServer) einen flüchtigen Port auf der Rückschleife, und
// der echte SpectrumClient verbindet sich dorthin.
//
// Was er nicht prüft: das echte Longpath. Das tut der Betreiber mit
// einem Gerät daran; hier steht nur, dass unsere Seite stimmt.
//
// Modification history (Contestprogramm):
//   2026-11-01 — Original von Martin Fischer (OE5SOS), KI-gestützt
//                 über Anthropic Claude.
// =================================================================

#include <QtTest>

#include <QApplication>
#include <QSignalSpy>
#include <QVector>

#include "core/SpectrumClient.h"
#include "mocks/MockSpectrumServer.h"
#include "ui/WaterfallWidget.h"

using namespace Contestprogramm;

class TestWasserfallLeitung : public QObject
{
    Q_OBJECT

private slots:
    void esMeldetSichUndBestelltNurEinBild();
    void einBildKommtBisInsPanel();
    void einVerdorbenerRahmenWirdGezaehltUndFaelltWeg();
    void einAbbruchFuehrtZumNeuenAnklopfen();
    void ausgeschaltetWirdNichtsGehalten();
};

// ── Es meldet sich und bestellt nur ein Bild ────────────────────────
//
// Der wichtigste Satz dieses Prüfstands. Das Log hängt an einem
// Funkgerät, das im Contest QSOs macht; diese Verbindung darf es
// unter keinen Umständen bedienen. Geprüft wird daher nicht nur, DASS
// spectrum_start kommt, sondern dass NICHTS ANDERES kommt.
void TestWasserfallLeitung::esMeldetSichUndBestelltNurEinBild()
{
    MockSpectrumServer server;
    QVERIFY(server.startListening());

    SpectrumClient client;
    client.setEndpunkt(QStringLiteral("127.0.0.1"), server.port());
    QSignalSpy verbindung(&client, &SpectrumClient::verbindungGeaendert);
    client.setGeometrie(512, 10);
    client.starten();

    QTRY_VERIFY_WITH_TIMEOUT(client.istVerbunden(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!server.empfangeneBefehle().isEmpty(), 5000);

    QCOMPARE(verbindung.count(), 1);
    QCOMPARE(verbindung.last().at(0).toBool(), true);

    const QStringList befehle = server.empfangeneBefehle();
    QCOMPARE(befehle.size(), 1);
    QCOMPARE(befehle.first(), QStringLiteral("spectrum_start:0,512,10;"));

    // Eine neue Breite ersetzt die Bestellung, sie stellt keine zweite
    // daneben -- Longpath nimmt den jüngsten spectrum_start.
    client.setGeometrie(734, 10);
    QTRY_COMPARE_WITH_TIMEOUT(server.empfangeneBefehle().size(), 2, 5000);
    QCOMPARE(server.empfangeneBefehle().last(), QStringLiteral("spectrum_start:0,734,10;"));

    // Beenden sagt es ab, statt die Leitung einfach fallen zu lassen.
    client.beenden();
    QTRY_VERIFY_WITH_TIMEOUT(server.empfangeneBefehle().size() == 3, 5000);
    QCOMPARE(server.empfangeneBefehle().last(), QStringLiteral("spectrum_stop:0;"));

    // Und in der ganzen Zeit kein einziger Befehl, der ein Gerät
    // anfasst. Diese Prüfung ist der Grund für den Prüfstand.
    for (const QString& befehl : server.empfangeneBefehle()) {
        QVERIFY2(befehl.startsWith(QStringLiteral("spectrum_")),
                 qPrintable(QStringLiteral("das Log hat gesendet: %1").arg(befehl)));
    }
}

// ── Ein Bild kommt bis ins Panel ────────────────────────────────────
void TestWasserfallLeitung::einBildKommtBisInsPanel()
{
    MockSpectrumServer server;
    QVERIFY(server.startListening());

    SpectrumClient client;
    client.setEndpunkt(QStringLiteral("127.0.0.1"), server.port());
    client.setGeometrie(256, 10);
    client.starten();
    // Auf den Stand des CLIENTS warten, nicht auf den des Servers.
    //
    // Hier stand zuerst `server.hasClient()` -- und auf Windows ist das
    // zu frueh: der Server hat die Verbindung schon angenommen, waehrend
    // der WebSocket-Handschlag auf unserer Seite noch laeuft. Der
    // Nachzug unten las dann `false` und der Lauf fiel rot aus (CI
    // 2026-11-01, einBildKommtBisInsPanel). Auf macOS und Linux ging es
    // durch, weil der Handschlag dort schneller fertig war als die
    // naechste Runde der Ereignisschleife -- ein Zeitrennen, kein
    // Plattformunterschied.
    QTRY_VERIFY_WITH_TIMEOUT(client.istVerbunden(), 5000);
    QVERIFY(server.hasClient());

    WaterfallWidget panel;
    panel.resize(256, 300);
    panel.show();
    QObject::connect(&client, &SpectrumClient::spektrumDa, &panel,
                     [&panel](const QVector<quint8>& werte, int) { panel.zeigeSpektrum(werte); });
    QObject::connect(&client, &SpectrumClient::verbindungGeaendert, &panel,
                     [&panel](bool an) { panel.setVerbunden(an); });
    // Wie im Fenster: den Stand gleich nachziehen. Genau darum geht es
    // in diesem Prüfpunkt -- die Verbindung steht oben schon, die
    // Meldung darüber ist vorbei, und ohne diesen Nachzug wartete das
    // Panel auf etwas, das nicht mehr kommt. Im Fenster ist das kein
    // erfundener Fall: AppController::openDatabase schaltet das
    // Spektrum ein, bevor MainWindow überhaupt existiert.
    panel.setVerbunden(client.istVerbunden());
    QVERIFY(panel.istVerbunden());

    QSignalSpy bilder(&client, &SpectrumClient::spektrumDa);
    const int zeilenVorher = panel.zeilenFuerTest();

    // -50 dBm: ein deutliches Signal.
    server.sendeSpektrum(256, quint8(-50 + SpectrumClient::kDbmVersatz));
    QTRY_COMPARE_WITH_TIMEOUT(bilder.count(), 1, 5000);
    QCOMPARE(panel.zeilenFuerTest(), zeilenVorher + 1);

    // Drei weitere Bilder: der Wasserfall wächst Zeile für Zeile.
    for (int i = 0; i < 3; ++i) {
        server.sendeSpektrum(256, quint8(-90 + SpectrumClient::kDbmVersatz));
    }
    QTRY_COMPARE_WITH_TIMEOUT(bilder.count(), 4, 5000);
    QCOMPARE(panel.zeilenFuerTest(), zeilenVorher + 4);
    QCOMPARE(client.verworfeneRahmen(), 0);
}

// ── Ein verdorbener Rahmen wird gezählt und fällt weg ───────────────
//
// Derselbe Rahmen wie in der Gegenprobe von test_wasserfall, nur
// diesmal über eine echte Leitung: 8192 versprochene Punkte, 10
// mitgelieferte. Er darf nicht ankommen, nicht abstürzen und nicht
// unbemerkt bleiben.
void TestWasserfallLeitung::einVerdorbenerRahmenWirdGezaehltUndFaelltWeg()
{
    MockSpectrumServer server;
    QVERIFY(server.startListening());

    SpectrumClient client;
    client.setEndpunkt(QStringLiteral("127.0.0.1"), server.port());
    client.starten();
    QTRY_VERIFY_WITH_TIMEOUT(server.hasClient(), 5000);

    QSignalSpy bilder(&client, &SpectrumClient::spektrumDa);

    server.sendeVerdorbenenRahmen(8192, 10);
    QTRY_COMPARE_WITH_TIMEOUT(client.verworfeneRahmen(), 1, 5000);
    QCOMPARE(bilder.count(), 0);

    // Ein fremder Strom auf derselben Leitung: auch verworfen, auch
    // gezählt -- und danach geht es weiter.
    server.sendeFremdenStrom();
    QTRY_COMPARE_WITH_TIMEOUT(client.verworfeneRahmen(), 2, 5000);
    QCOMPARE(bilder.count(), 0);

    server.sendeSpektrum(256, quint8(150));
    QTRY_COMPARE_WITH_TIMEOUT(bilder.count(), 1, 5000);
    QCOMPARE(client.verworfeneRahmen(), 2);
}

// ── Ein Abbruch führt zum neuen Anklopfen ───────────────────────────
//
// Longpath wird im Contest neu gestartet, der Rechner kommt aus dem
// Schlaf, das WLAN hat eine Lücke. Das Log darf dabei nichts merken
// außer dem fehlenden Bild -- und wenn Longpath wieder da ist, soll
// der Wasserfall von selbst weiterlaufen.
void TestWasserfallLeitung::einAbbruchFuehrtZumNeuenAnklopfen()
{
    MockSpectrumServer server;
    QVERIFY(server.startListening());

    SpectrumClient client;
    client.setEndpunkt(QStringLiteral("127.0.0.1"), server.port());
    client.starten();
    QTRY_VERIFY_WITH_TIMEOUT(client.istVerbunden(), 5000);
    // Erst die erste Bestellung abwarten, nicht nur die Verbindung. Auf
    // dem Client steht die Leitung schon, bevor sein spectrum_start beim
    // Server angekommen ist; wer dann trennt, hat eine leere Liste, und
    // `.last()` darauf ist kein Fehlschlag, sondern ein Absturz (CI
    // Windows, 2026-11-03: Zugriffsfehler in QString::startsWith).
    QTRY_COMPARE_WITH_TIMEOUT(server.empfangeneBefehle().size(), 1, 5000);

    QSignalSpy verbindung(&client, &SpectrumClient::verbindungGeaendert);
    server.trenne();
    QTRY_VERIFY_WITH_TIMEOUT(!client.istVerbunden(), 5000);
    QVERIFY(!verbindung.isEmpty());
    QCOMPARE(verbindung.last().at(0).toBool(), false);

    // Die erste Wartestufe ist 1 s; der Client klopft von selbst
    // wieder an, ohne dass jemand etwas tun muss.
    QTRY_VERIFY_WITH_TIMEOUT(client.istVerbunden(), 8000);
    QTRY_VERIFY_WITH_TIMEOUT(server.hasClient(), 5000);
    // Und bestellt sein Bild neu -- sonst wäre die Leitung wieder da
    // und blieb doch leer. Gezählt, nicht nur `.last()` angesehen: der
    // Server behält die Bestellung der ersten Verbindung, und die allein
    // hätte diese Prüfung bestanden, ohne dass je neu bestellt wurde.
    QTRY_COMPARE_WITH_TIMEOUT(server.empfangeneBefehle().size(), 2, 5000);
    QVERIFY(server.empfangeneBefehle().last().startsWith(QStringLiteral("spectrum_start:")));

    client.beenden();
}

// ── Ausgeschaltet wird nichts gehalten ──────────────────────────────
void TestWasserfallLeitung::ausgeschaltetWirdNichtsGehalten()
{
    MockSpectrumServer server;
    QVERIFY(server.startListening());

    SpectrumClient client;
    client.setEndpunkt(QStringLiteral("127.0.0.1"), server.port());

    // Ohne starten() passiert nichts. Das ist der Werkszustand: das
    // Spektrum ist in den Einstellungen aus, und ein ausgeschalteter
    // Wasserfall darf keine Verbindung offenhalten.
    QTest::qWait(300);
    QVERIFY(!client.istVerbunden());
    QVERIFY(!server.hasClient());

    client.starten();
    QTRY_VERIFY_WITH_TIMEOUT(client.istVerbunden(), 5000);
    client.beenden();
    QTRY_VERIFY_WITH_TIMEOUT(!client.istVerbunden(), 5000);

    // Und nach dem Abschalten klopft er nicht weiter an. Ein Logbuch,
    // das alle Sekunde an eine Tür klopft, die niemand öffnen soll,
    // wäre genau die Art Dauerlast, die im Contest auffällt.
    QTest::qWait(2500);
    QVERIFY(!client.istVerbunden());
}

QTEST_MAIN(TestWasserfallLeitung)
#include "test_wasserfall_leitung.moc"
