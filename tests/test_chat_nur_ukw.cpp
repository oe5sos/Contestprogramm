// Chat immer UKW -- Betreiber am 2026-11-04: "chat ist fuer mich nur
// ukw wichtig", auf die Rueckfrage "immer UKW".
//
// Der Befund, der dahinter steckt: in seinen Einstellungen stand
// active_contest_id = CQ_WW_SSB und active_bands = ["14"]. Der
// Chatfilter folgte diesen Baendern, liess also 20 m durch und blendete
// UKW aus. Das Programm zeigte korrekt, was eingestellt war, und
// trotzdem das Falsche -- zwei Quellen speisen den Strom, ON4KST
// (reiner UKW-Chat) und der DX-Cluster ve7cc.net (fast nur Kurzwelle).
//
// Das Tor sitzt im Sichtbarkeitsfilter, NICHT am Eingang: gefiltert
// heisst verborgen, nicht weggeworfen. test_chat_panel verlangt
// ausdruecklich, dass ein ausgefilterter Kurzwellen-Spot in der Ansicht
// "alle Zeilen zeigen" noch da ist -- ein erster Versuch, ihn in
// addCandidate() abzuweisen, hat genau daran zu Recht scheitern.

#include <QtTest>

#include <QCoreApplication>
#include <QDateTime>
#include <QTemporaryDir>

#include "core/BandUtils.h"
#include "core/GeoFilter.h"
#include "core/SpotCandidate.h"
#include "data/ContestDatabase.h"
#include "data/DupeChecker.h"
#include "models/ChatFeedModel.h"

using namespace Contestprogramm;

namespace {
SpotCandidate spot(const QString& call, qint64 hz)
{
    SpotCandidate c;
    c.callsign = call;
    c.freqHz = hz;
    c.timestampUtc = QDateTime::currentDateTimeUtc();
    return c;
}

// Alle Rufzeichen, die der Filter gerade sichtbar laesst.
QStringList sichtbar(ChatFeedModel& model)
{
    QStringList raus;
    for (int r = 0; r < model.rowCount(); ++r) {
        raus << model.data(model.index(r, 0)).toString();
    }
    return raus;
}
} // namespace

class TestChatNurUkw : public QObject
{
    Q_OBJECT

private slots:
    void theUkwEdgeSitsAtFiftyMegahertz();
    void unknownFrequenciesAreNotThrownAway();
    void anHfContestDoesNotDragShortwaveIntoTheChat();
    void aVhfContestStillNarrowsToItsOwnBands();
    void filteredLinesAreOnlyHiddenNotDiscarded();
};

void TestChatNurUkw::theUkwEdgeSitsAtFiftyMegahertz()
{
    QVERIFY(!isUkwOrUnknownHz(1810000LL));      // 160 m
    QVERIFY(!isUkwOrUnknownHz(3650000LL));      // 80 m
    QVERIFY(!isUkwOrUnknownHz(7050000LL));      // 40 m
    QVERIFY(!isUkwOrUnknownHz(10120000LL));     // 30 m
    QVERIFY(!isUkwOrUnknownHz(14080000LL));     // 20 m
    QVERIFY(!isUkwOrUnknownHz(18100000LL));     // 17 m
    QVERIFY(!isUkwOrUnknownHz(21200000LL));     // 15 m
    QVERIFY(!isUkwOrUnknownHz(24930000LL));     // 12 m
    QVERIFY(!isUkwOrUnknownHz(28400000LL));     // 10 m
    QVERIFY(!isUkwOrUnknownHz(29700000LL));     // die Oberkante von 10 m

    QVERIFY(isUkwOrUnknownHz(kUkwUntergrenzeHz)); // genau die Kante
    QVERIFY(isUkwOrUnknownHz(50150000LL));      // 6 m zaehlt mit
    QVERIFY(isUkwOrUnknownHz(70200000LL));      // 4 m
    QVERIFY(isUkwOrUnknownHz(144300000LL));     // 2 m
    QVERIFY(isUkwOrUnknownHz(432200000LL));     // 70 cm
    QVERIFY(isUkwOrUnknownHz(1296200000LL));    // 23 cm
    QVERIFY(isUkwOrUnknownHz(10368000000LL));   // 3 cm
}

void TestChatNurUkw::unknownFrequenciesAreNotThrownAway()
{
    QVERIFY(isUkwOrUnknownHz(0));
    QVERIFY(isUkwOrUnknownHz(-1));
    // 40 MHz liegt in keinem Amateurband -- unbekannt, nicht
    // "Kurzwelle". Lieber eine Zeile zu viel als eine zu wenig.
    QVERIFY(isUkwOrUnknownHz(40000000LL));
}

void TestChatNurUkw::anHfContestDoesNotDragShortwaveIntoTheChat()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("ukw1.sqlite")), QStringLiteral("chat_ukw_hf")));
    GeoFilter geo;
    geo.setOwnGrid(QStringLiteral("JN67VV"));
    geo.setRadiusKm(20000.0);
    DupeChecker dupes(db);
    ChatFeedModel model(geo, dupes, nullptr);

    // Genau Martins Lage am 2026-11-04.
    model.setContestBands(QStringList{QStringLiteral("14")});

    model.addCandidate(spot(QStringLiteral("XE1AOH"), 7164000LL));     // 40 m
    model.addCandidate(spot(QStringLiteral("K1ABC"), 14080000LL));     // 20 m: das Contestband!
    model.addCandidate(spot(QStringLiteral("OE1ABC"), 144300000LL));   // 2 m
    model.addCandidate(spot(QStringLiteral("DL1DEF"), 432200000LL));   // 70 cm
    model.addCandidate(spot(QStringLiteral("HB9JKL"), 0));             // freie Zeile

    const QStringList zuSehen = sichtbar(model);
    QVERIFY2(!zuSehen.contains(QStringLiteral("XE1AOH")), qPrintable(zuSehen.join(QLatin1Char(','))));
    // Das eigene Contestband hilft nichts, wenn es Kurzwelle ist.
    QVERIFY2(!zuSehen.contains(QStringLiteral("K1ABC")), qPrintable(zuSehen.join(QLatin1Char(','))));
    QVERIFY2(zuSehen.contains(QStringLiteral("OE1ABC")), qPrintable(zuSehen.join(QLatin1Char(','))));
    QVERIFY2(zuSehen.contains(QStringLiteral("DL1DEF")), qPrintable(zuSehen.join(QLatin1Char(','))));
    QVERIFY2(zuSehen.contains(QStringLiteral("HB9JKL")), qPrintable(zuSehen.join(QLatin1Char(','))));
}

void TestChatNurUkw::aVhfContestStillNarrowsToItsOwnBands()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("ukw2.sqlite")), QStringLiteral("chat_ukw_vhf")));
    GeoFilter geo;
    geo.setOwnGrid(QStringLiteral("JN67VV"));
    geo.setRadiusKm(20000.0);
    DupeChecker dupes(db);
    ChatFeedModel model(geo, dupes, nullptr);

    // Ein reiner 2-m-Contest: die Verengung innerhalb UKW bleibt, sonst
    // waere die Contest-Disziplin weg.
    model.setContestBands(QStringList{QStringLiteral("144")});

    model.addCandidate(spot(QStringLiteral("OE1ABC"), 144300000LL));   // sein Band
    model.addCandidate(spot(QStringLiteral("DL1DEF"), 432200000LL));   // UKW, aber nicht sein Band
    model.addCandidate(spot(QStringLiteral("XE1AOH"), 7164000LL));     // Kurzwelle

    const QStringList zuSehen = sichtbar(model);
    QVERIFY2(zuSehen.contains(QStringLiteral("OE1ABC")), qPrintable(zuSehen.join(QLatin1Char(','))));
    QVERIFY2(!zuSehen.contains(QStringLiteral("DL1DEF")), qPrintable(zuSehen.join(QLatin1Char(','))));
    QVERIFY2(!zuSehen.contains(QStringLiteral("XE1AOH")), qPrintable(zuSehen.join(QLatin1Char(','))));
}

void TestChatNurUkw::filteredLinesAreOnlyHiddenNotDiscarded()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("ukw3.sqlite")), QStringLiteral("chat_ukw_roh")));
    GeoFilter geo;
    geo.setOwnGrid(QStringLiteral("JN67VV"));
    geo.setRadiusKm(20000.0);
    DupeChecker dupes(db);
    ChatFeedModel model(geo, dupes, nullptr);
    model.setContestBands(QStringList{QStringLiteral("144")});

    model.addCandidate(spot(QStringLiteral("XE1AOH"), 7164000LL));
    model.addCandidate(spot(QStringLiteral("OE1ABC"), 144300000LL));

    QVERIFY(!sichtbar(model).contains(QStringLiteral("XE1AOH")));
    // Beide Zeilen liegen weiter im Strom ...
    QCOMPARE(model.lineCount(), 2);
    // ... und "alle Zeilen zeigen" holt die Kurzwelle wieder hervor.
    model.setShowRawFeed(true);
    QVERIFY2(sichtbar(model).contains(QStringLiteral("XE1AOH")),
             "der Kurzwellen-Spot ist ganz verschwunden");
}

QTEST_MAIN(TestChatNurUkw)
#include "test_chat_nur_ukw.moc"
