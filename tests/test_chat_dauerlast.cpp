// =================================================================
// tests/test_chat_dauerlast.cpp  (Contestprogramm)
// =================================================================
//
// Was 24 Stunden Chat und Cluster mit dem Chatmodell machen.
//
// ChatFeedModel::addCandidate() baut nach JEDER ankommenden Zeile die
// ganze Sichtliste neu auf (beginResetModel + einmal durch alle
// bisherigen Einträge; die Sichtbarkeitsregel ist relativ zu allen
// übrigen Bewertungen, deshalb geht es nicht einzeln). Der Kommentar
// dort rechnet mit "tens per minute, not thousands" -- das stimmt für
// die RATE, sagt aber nichts über die SUMME: ein 24-Stunden-Contest
// mit 30 Zeilen je Minute sind 43 200 Einträge, und der Aufwand je
// Zeile wächst mit dem, was schon dasteht.
//
// Gemessen wird, was der Bediener merkt: wie lange der EINE Anschlag
// dauert, der gerade ankommt, wenn schon viel dasteht.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-10-16 — New. AI-assisted via Anthropic Claude Code, operator
//                 Ralph Martin Fischer.
// =================================================================

#include <QtTest>

#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QTemporaryDir>

#include "core/GeoFilter.h"
#include "core/SpotCandidate.h"
#include "data/ContestDatabase.h"
#include "data/DupeChecker.h"
#include "models/ChatFeedModel.h"

using namespace Contestprogramm;

class TestChatDauerlast : public QObject
{
    Q_OBJECT

private slots:
    void eineZeileBleibtSchnellAuchNachStunden();
};

void TestChatDauerlast::eineZeileBleibtSchnellAuchNachStunden()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("dauerlast.sqlite")), QStringLiteral("chat_dauerlast")));

    GeoFilter geoFilter;
    geoFilter.setOwnGrid(QStringLiteral("JN67UT"));
    geoFilter.setRadiusKm(1000.0);
    DupeChecker dupeChecker(db);

    ChatFeedModel model(geoFilter, dupeChecker, nullptr);
    model.setActiveContest(QStringLiteral("IARU_R1_VHF_UHF"));
    model.setContestBands({QStringLiteral("144"), QStringLiteral("432")});

    const QDateTime start = QDateTime::currentDateTimeUtc();
    const auto zeile = [&](int i) {
        SpotCandidate c;
        // Rund 250 verschiedene Stationen -- so sieht ein Chat- und
        // Clusterstrom wirklich aus: dieselbe Station steht vielfach
        // darin, aber nicht nur zehn.
        c.callsign = QStringLiteral("DL%1ABC").arg(i % 250);
        c.grid = QStringLiteral("JN58SD");
        c.freqHz = 144300000LL + (i % 50) * 1000;
        c.timestampUtc = start.addSecs(i * 2);
        c.source = (i % 2) ? QStringLiteral("on4kst") : QStringLiteral("cluster");
        return c;
    };

    // Die Messpunkte: nach 1 000, 6 000 und 12 000 Zeilen jeweils die
    // Dauer EINER weiteren Zeile -- das Achtfache des Deckels, mehr
    // braucht es nicht, um zu zeigen, dass der Aufwand stehen bleibt.
    // 24 Stunden zu 30 Zeilen je Minute wären 43 200; so weit kam die
    // erste Messung gar nicht, sie lief ohne Deckel bei 20 000 schon
    // in die 300-Sekunden-Grenze des Prüfstands.
    QVector<int> marken = {1000, 6000, 12000};
    int geschrieben = 0;
    QVector<double> dauern;
    for (int marke : marken) {
        while (geschrieben < marke) {
            model.addCandidate(zeile(geschrieben++));
        }
        QElapsedTimer t;
        t.start();
        model.addCandidate(zeile(geschrieben++));
        const double ms = t.nsecsElapsed() / 1000000.0;
        dauern << ms;
        qInfo().noquote() << QStringLiteral("nach %1 Zeilen: eine weitere dauert %2 ms")
                                 .arg(marke)
                                 .arg(ms, 0, 'f', 2);
    }

    // Eine ankommende Zeile darf den Bediener nicht aufhalten: der
    // Anschlag im Rufzeichenfeld soll unter 10 ms bleiben (so ist auch
    // die Dupe-Prüfung und Check Partial gemessen worden), und das
    // Chatmodell läuft im selben Faden.
    QVERIFY2(dauern.last() < 10.0,
             qPrintable(QStringLiteral("Nach 12 000 Zeilen dauert eine weitere %1 ms")
                            .arg(dauern.last(), 0, 'f', 2)));
    // Und der Aufwand darf nicht weiter mitwachsen: die späte Zeile
    // darf nicht ein Vielfaches der frühen kosten.
    QVERIFY2(dauern.last() < dauern.first() * 4.0 + 1.0,
             qPrintable(QStringLiteral("Eine Zeile kostete nach 1 000 Zeilen %1 ms, nach 12 000 aber %2 ms")
                            .arg(dauern.first(), 0, 'f', 2)
                            .arg(dauern.last(), 0, 'f', 2)));

    // Das Zweite, was mit der Liste mitwächst: nach JEDEM geloggten
    // QSO läuft refreshWorkedAndScores() einmal über alles und fragt
    // je Eintrag die Datenbank nach einem Dupe. Das hält den Bediener
    // unmittelbar nach dem Return auf.
    QElapsedTimer t;
    t.start();
    model.refreshWorkedAndScores();
    const double auffrischung = t.nsecsElapsed() / 1000000.0;
    qInfo().noquote() << QStringLiteral("Auffrischung nach einem geloggten QSO: %1 ms")
                             .arg(auffrischung, 0, 'f', 2);
    QVERIFY2(auffrischung < 50.0,
             qPrintable(QStringLiteral("Nach dem Loggen eines QSOs rechnet das Chatmodell %1 ms")
                            .arg(auffrischung, 0, 'f', 2)));
}

QTEST_MAIN(TestChatDauerlast)
#include "test_chat_dauerlast.moc"
