// Grenzfälle gegen Wertung, Dupe, Locator, laufende Nummer, EDI und
// Baken -- aus der Durchleuchtung vom 2026-10-08. Jeder Fall hier hat
// einmal etwas gezeigt, das vorher niemand gemessen hatte.

#include "core/CallsignPrefix.h"
#include "core/Maidenhead.h"
#include "data/ContestDatabase.h"
#include "data/ContestScoring.h"
#include "data/DupeChecker.h"
#include "data/EdiExporter.h"
#include "data/ContestDefinition.h"
#include "data/QsoRecord.h"
#include "core/SpotParser.h"
#include "core/SpotCandidate.h"
#include "app/ContestSettings.h"

#include <QTemporaryDir>
#include <QtTest>

using namespace Contestprogramm;

namespace {

QsoRecord qso(const QString& call, const QString& band, const QString& grid,
              std::optional<int> serial = 1, const QString& ts = QStringLiteral("2026-09-05T14:00:00"))
{
    QsoRecord r;
    r.callsign = call;
    r.band = band;
    r.mode = QStringLiteral("SSB");
    r.gridSquare = grid;
    r.serialRcvd = serial;
    r.serialSent = serial;
    r.rstSent = QStringLiteral("59");
    r.rstRcvd = QStringLiteral("59");
    r.timestampUtc = ts;
    r.contestId = QStringLiteral("T");
    r.exchangeRcvd = QStringLiteral("59 001 ") + grid;
    return r;
}

} // namespace

class TestGrenzfaelle : public QObject
{
    Q_OBJECT

private slots:
    // ── A) Locator-Grenzfälle ──────────────────────────────────────
    void locatorGrenzen()
    {
        QVERIFY(isFullLocator(QStringLiteral("JN67VV")));
        QVERIFY(isFullLocator(QStringLiteral("jn67vv")));          // Kleinschreibung
        QVERIFY(isFullLocator(QStringLiteral(" JN67VV ")));        // Leerzeichen
        QVERIFY(!isFullLocator(QStringLiteral("JN67")));
        QVERIFY(!isFullLocator(QStringLiteral("JN67V")));
        QVERIFY(!isFullLocator(QStringLiteral("JN67VV12")));       // 8-stellig: kein R1-Austausch
        QVERIFY(!isValidGridSquare(QStringLiteral("SS99XX")));     // S gibt es nicht (A..R)
        QVERIFY(!isValidGridSquare(QStringLiteral("JN6AVV")));     // Ziffernstelle als Buchstabe
        QVERIFY(isValidGridSquare(QStringLiteral("AA00AA")));
        QVERIFY(isValidGridSquare(QStringLiteral("RR99XX")));
        // Antipoden-Nähe darf nicht negativ/NaN werden
        const double weit = iaruQrbKm(QStringLiteral("AA00AA"), QStringLiteral("RR99XX"));
        qInfo() << "AA00AA..RR99XX =" << weit << "km";
        QVERIFY(weit > 0.0 && weit < 20100.0);
        QCOMPARE(qRound(iaruQrbKm(QStringLiteral("JN67VV"), QStringLiteral("JN67VV"))), 0);
    }

    // ── B) Wertung: distance_km ohne Nummer im Austausch ───────────
    void wertungOhneNummernfeld()
    {
        // Eigener Contest im Regeleditor: scoring=distance_km, aber im
        // Austausch nur RST+Locator (kein serial-Feld). Dann ist
        // serialRcvd bei JEDEM QSO leer -> 0 Punkte, ohne Hinweis.
        QsoRecord r = qso(QStringLiteral("DL1ABC"), QStringLiteral("144"),
                          QStringLiteral("JO60AA"), std::nullopt);
        // So steht es in der Regel (IARU R1 1.9.1) -- 0 Punkte ist hier
        // richtig. Falsch war, dass der Regeleditor einen Contest mit
        // Entfernungswertung OHNE Nummernfeld annahm: dann traf das
        // jedes einzelne QSO, ohne dass irgendwo etwas davon stand.
        // Seit 2026-10-08 lehnt validateRules die Kombination ab, siehe
        // regelnOhneNummerErlaubt() weiter unten.
        QCOMPARE(qsoPoints(r, QStringLiteral("JN67VV"), QStringLiteral("distance_km")), 0);
        QVERIFY(qsoDistanceKm(r, QStringLiteral("JN67VV")) > 0);
    }

    void wertungGrenzen()
    {
        const QString own = QStringLiteral("JN67VV");
        // 4-stelliger Locator der Gegenstation -> 0 Punkte (1.9.1)
        QCOMPARE(qsoPoints(qso("DL1ABC", "144", "JO60"), own, "distance_km"), 0);
        // gleiches Feld = 1 Punkt
        QCOMPARE(qsoPoints(qso("DL1ABC", "144", own), own, "distance_km"), 1);
        // Dupe = 0
        QsoRecord d = qso("DL1ABC", "144", "JO60AA");
        d.isDupe = true;
        QCOMPARE(qsoPoints(d, own, "distance_km"), 0);
        // eigener Locator nur 4-stellig: keine Entfernung berechenbar
        QCOMPARE(qsoPoints(qso("DL1ABC", "144", "JO60AA"), QStringLiteral("JN67"), "distance_km"), 0);
        // distanceKm aus der DB schlägt die Rechnung (recomputeDistances)
        QsoRecord g = qso("DL1ABC", "144", "JO60AA");
        g.distanceKm = 187.4;
        QCOMPARE(qsoPoints(g, own, "distance_km"), 188);
        // negative Entfernung in der DB (Datenfehler) darf nicht Punkte geben
        QsoRecord n = qso("DL1ABC", "144", "JO60AA");
        n.distanceKm = -5.0;
        qInfo() << "distanceKm=-5 ->" << qsoPoints(n, own, "distance_km") << "Punkte";
        QCOMPARE(qsoPoints(n, own, "distance_km"), 0);
    }

    void odxUndGrossfelder()
    {
        const QString own = QStringLiteral("JN67VV");
        QVector<QsoRecord> log;
        log << qso("A", "144", "JO60AA");      // weit
        log << qso("B", "144", own);           // 0 km, gleiches Feld
        log << qso("C", "432", "JN68AA");      // anderes Band
        QsoRecord ungueltig = qso("D", "1296", "JO70AA");
        ungueltig.isInvalid = true;            // darf kein Band aufmachen
        log << ungueltig;
        const ContestScore s = computeContestScore(log, own, {"144", "432"}, "distance_km");
        QCOMPARE(s.validQsos, 3);
        QVERIFY(s.band("1296") == nullptr);
        QCOMPARE(s.odxCall, QStringLiteral("A"));
        const BandScore* b144 = s.band("144");
        QVERIFY(b144);
        QCOMPARE(b144->largeSquares, 2);       // JO60 + JN67
        qInfo() << "Punkte gesamt" << s.points << "ODX" << s.odxCall << s.odxKm << s.odxBand;
    }

    // ── C) Dupe-Prüfung ────────────────────────────────────────────
    void dupeGrenzen()
    {
        QTemporaryDir dir;
        ContestDatabase db;
        QVERIFY(db.open(dir.filePath(QStringLiteral("t.sqlite")), QStringLiteral("probe_dupe")));
        QsoRecord r = qso(QStringLiteral("S50AAA"), QStringLiteral("144"), QStringLiteral("JN76AA"));
        QVERIFY(db.insertQso(r));
        DupeChecker dupe(db);
        const QStringList scope{QStringLiteral("callsign"), QStringLiteral("band")};
        QVERIFY(dupe.isDupe("S50AAA", "144", "SSB", "T", scope));
        QVERIFY(dupe.isDupe("s50aaa", "144", "SSB", "T", scope));         // Kleinschreibung
        QVERIFY(dupe.isDupe(" S50AAA ", "144", "SSB", "T", scope));       // Leerzeichen
        QVERIFY(dupe.isDupe("S50AAA/P", "144", "SSB", "T", scope));       // /P (GC 2023 1.2)
        QVERIFY(dupe.isDupe("DL/S50AAA", "144", "SSB", "T", scope));      // Präfix
        QVERIFY(!dupe.isDupe("S50AAA", "432", "SSB", "T", scope));        // anderes Band
        QVERIFY(!dupe.isDupe("S50AAB", "144", "SSB", "T", scope));
        // SQL-LIKE-Metazeichen im getippten Rufzeichen (Tippfehler)
        QVERIFY(!dupe.isDupe("S50%", "144", "SSB", "T", scope));
        QVERIFY(!dupe.isDupe("S50AA_", "144", "SSB", "T", scope));
        QVERIFY(dupe.lastError().isEmpty());
        // ungültig markiert -> kein Dupe mehr
        QVERIFY(db.setQsoInvalid(r.id, true));
        QVERIFY(!dupe.isDupe("S50AAA", "144", "SSB", "T", scope));
        db.close();
    }

    // ── D) Laufende Nummer je Band ─────────────────────────────────
    void laufendeNummerNachLoeschen()
    {
        QTemporaryDir dir;
        ContestDatabase db;
        QVERIFY(db.open(dir.filePath(QStringLiteral("t.sqlite")), QStringLiteral("probe_serial")));
        QCOMPARE(db.nextSerialForContest("T", "144"), 1);
        QsoRecord a = qso("DL1ABC", "144", "JO60AA", 1);
        QVERIFY(db.insertQso(a));
        QsoRecord b = qso("DL2ABC", "144", "JO60AB", 2);
        QVERIFY(db.insertQso(b));
        QCOMPARE(db.nextSerialForContest("T", "144"), 3);
        QCOMPARE(db.nextSerialForContest("T", "432"), 1);   // je Band ab 001
        // letztes QSO gelöscht: die Nummer ist vergeben und bleibt vergeben
        QVERIFY(db.deleteQso(b.id));
        const int nach = db.nextSerialForContest("T", "144");
        qInfo() << "nächste Nummer nach Löschen des letzten QSO:" << nach;
        QCOMPARE(nach, 2);   // bewusst so: kein Loch durch ein Enter zuviel
        // ... aber jetzt die Nummer neu vergeben UND das gelöschte
        // zurückholen (Strg+Z): dann ist Nr. 2 zweimal gesendet.
        QsoRecord c = qso("DL3ABC", "144", "JO60AC", nach);
        QVERIFY(db.insertQso(c));
        QVERIFY(db.undeleteQso(b.id));
        const auto log = db.qsosForContest(QStringLiteral("T"));
        int zweien = 0;
        for (const QsoRecord& r : log) {
            if (r.serialSent && *r.serialSent == 2) { ++zweien; }
        }
        // Zwei QSOs mit derselben gesendeten Nummer: die Datenbank
        // lässt das zu (sie muss -- die Nummer ging ja wirklich zweimal
        // raus), aber der Operator muss es erfahren. MainWindow sagt es
        // seit 2026-10-08 beim Zurückholen in der Statuszeile, und die
        // Log-Prüfung meldet es als Fehler ("Nummer zweimal gesendet").
        qInfo() << "QSOs mit gesendeter Nr. 2 nach Strg+Z:" << zweien;
        QCOMPARE(zweien, 2);
        // ungültig markiertes QSO behält seine Nummer ebenfalls
        QVERIFY(db.setQsoInvalid(a.id, true));
        qInfo() << "nächste Nummer nach 'ungültig':" << db.nextSerialForContest("T", "144");
        db.close();
    }

    // ── E) EDI-Export ──────────────────────────────────────────────
    void ediGrenzen()
    {
        QTemporaryDir dir;
        ContestDatabase db;
        QVERIFY(db.open(dir.filePath(QStringLiteral("t.sqlite")), QStringLiteral("probe_edi")));
        db.setSettingValue(QStringLiteral("own_callsign"), QStringLiteral("OE5SOS"));
        db.setSettingValue(QStringLiteral("own_locator"), QStringLiteral("JN67VV"));
        // QSOs absichtlich in falscher Zeitfolge eingetragen
        QsoRecord spaet = qso("DL2ABC", "144", "JO60AB", 2, QStringLiteral("2026-09-05T15:30:00"));
        QVERIFY(db.insertQso(spaet));
        QsoRecord frueh = qso("DL1ABC", "144", "JO60AA", 1, QStringLiteral("2026-09-05T14:05:00"));
        QVERIFY(db.insertQso(frueh));
        // Feld mit Semikolon: darf die Spalten nicht verschieben
        QsoRecord semi = qso("DL3ABC", "144", "JO60AC", 3, QStringLiteral("2026-09-05T16:00:00"));
        semi.notes = QStringLiteral("Antenne; 4x9el");
        semi.exchangeRcvd = QStringLiteral("59 003 JO60AC Grusz; Hans");
        QVERIFY(db.insertQso(semi));

        QString fehler;
        const ContestDefinition def = ContestDefinition::loadFromFile(
            QStringLiteral(CP_SOURCE_DIR "/resources/contest_definitions/iaru_r1_vhf_uhf.json"), &fehler);
        QVERIFY2(def.isValid(), qPrintable(fehler));
        EdiStationInfo info;
        info.section = QStringLiteral("SINGLE");
        info.email = QStringLiteral("a@b.at");
        info.powerWatts = 100;
        info.antenna = QStringLiteral("9el");
        info.name = QStringLiteral("Martin");
        info.street = QStringLiteral("Weg 1; Stiege 2");
        ContestSettings settings;
        EdiExporter exporter(db);
        const QString edi = exporter.exportBand(QStringLiteral("T"), QStringLiteral("144"), def, settings, info);
        QVERIFY(!edi.isEmpty());
        qInfo().noquote() << edi.left(1400);
        // Kein Rohzeilenumbruch, jede Zeile mit CRLF
        QVERIFY(!edi.contains(QLatin1String("\n\n")));
        // QSO-Zeilen zeitlich sortiert
        const int iFrueh = edi.indexOf(QStringLiteral("DL1ABC"));
        const int iSpaet = edi.indexOf(QStringLiteral("DL2ABC"));
        QVERIFY(iFrueh > 0 && iSpaet > 0);
        QVERIFY2(iFrueh < iSpaet, "QSO-Zeilen nicht nach Zeit sortiert");
        // Semikolon in Kopfzeilen darf nicht als Trenner durchkommen
        const int radr = edi.indexOf(QStringLiteral("RAdr1="));
        const QString radrZeile = edi.mid(radr, edi.indexOf(QStringLiteral("\r\n"), radr) - radr);
        qInfo().noquote() << "RAdr1-Zeile:" << radrZeile;
        QCOMPARE(radrZeile.count(QLatin1Char(';')), 0);
        // Und in der QSO-Zeile auch nicht: die Spaltenzahl muss stimmen
        const int iSemi = edi.indexOf(QStringLiteral("DL3ABC"));
        const int zeilenAnfang = edi.lastIndexOf(QStringLiteral("\r\n"), iSemi) + 2;
        const QString qsoZeile = edi.mid(zeilenAnfang, edi.indexOf(QStringLiteral("\r\n"), iSemi) - zeilenAnfang);
        qInfo().noquote() << "QSO-Zeile mit Semikolon im Austausch:" << qsoZeile;
        const int spalten = qsoZeile.count(QLatin1Char(';'));
        qInfo() << "Spaltentrenner in dieser Zeile:" << spalten;
        // REG1TEST-QSO-Zeile hat 15 Felder = 14 Trenner
        QCOMPARE(spalten, 14);
        db.close();
    }

    // ── F) Grundrufzeichen ─────────────────────────────────────────
    void grundrufzeichen()
    {
        QCOMPARE(baseCallsign(QStringLiteral("S50AAA/P")), QStringLiteral("S50AAA"));
        QCOMPARE(baseCallsign(QStringLiteral("DL/S50AAA")), QStringLiteral("S50AAA"));
        QCOMPARE(baseCallsign(QStringLiteral("DL/S50AAA/P")), QStringLiteral("S50AAA"));
        QCOMPARE(baseCallsign(QStringLiteral("oe5sos")), QStringLiteral("OE5SOS"));
        QCOMPARE(baseCallsign(QStringLiteral("OE5SOS/9")), QStringLiteral("OE5SOS"));
        qInfo() << "OE5SOS/MM ->" << baseCallsign(QStringLiteral("OE5SOS/MM"));
        qInfo() << "leer ->" << baseCallsign(QString());
        qInfo() << "nur Schrägstrich ->" << baseCallsign(QStringLiteral("/"));
    }

    // ── G) Regeleditor: km-Wertung ohne Nummernfeld ────────────────
    void regelnOhneNummerErlaubt()
    {
        ContestDefinition::Rules rules;
        rules.name = QStringLiteral("UKW-Aktivitaetsabend");
        rules.bands = QStringList{QStringLiteral("144")};
        rules.dupeScope = QStringList{QStringLiteral("callsign"), QStringLiteral("band")};
        ContestDefinition::ExchangeField rst;
        rst.key = QStringLiteral("rst");
        rst.label = QStringLiteral("RST");
        rst.type = QStringLiteral("rst");
        ContestDefinition::ExchangeField grid;
        grid.key = QStringLiteral("grid");
        grid.label = QStringLiteral("Locator");
        grid.type = QStringLiteral("grid6");
        rules.exchangeFields = {rst, grid};          // KEINE Nummer
        rules.scoring = QStringLiteral("distance_km");
        rules.serialScope = QStringLiteral("band");
        rules.multiplierField = QStringLiteral("none");
        QString fehler;
        const bool ok = ContestDefinition::validateRules(rules, &fehler);
        qInfo().noquote() << "km-Wertung ohne Nummernfeld:" << (ok ? QStringLiteral("angenommen") : fehler);
        QVERIFY2(!ok, "Entfernungswertung ohne Nummernfeld muss abgelehnt werden");
        QVERIFY(fehler.contains(QStringLiteral("Nummern-Feld")));
        // Mit Nummernfeld geht es durch.
        ContestDefinition::ExchangeField nummer;
        nummer.key = QStringLiteral("serial");
        nummer.label = QStringLiteral("Nr.");
        nummer.type = QStringLiteral("int");
        nummer.autoIncrement = true;
        rules.exchangeFields = {rst, nummer, grid};
        QVERIFY2(ContestDefinition::validateRules(rules, &fehler), qPrintable(fehler));
    }

    // ── H) Baken-Spots ─────────────────────────────────────────────
    void bakenSindErkennbar()
    {
        SpotCandidate k;
        const QString zeile = QStringLiteral(
            "DL|1788000000|14:00|OE5SOS|144412.0|OE5XBM/B|Bake JN68|JN67VV|JN68PC|");
        QVERIFY(SpotParser::parseDxSpotLine(zeile, k));
        // Der Spot bleibt stehen -- eine Bake gehört in die Bandmap, sie
        // zeigt, wohin das Band offen ist (so macht es DXLog auch).
        QCOMPARE(k.callsign, QStringLiteral("OE5XBM/B"));
        // Aber sie ist erkennbar, und der Betriebsassistent nimmt sie
        // seit 2026-10-08 nicht mehr als nächstes Ziel (MainWindow::
        // refreshSuggestionPanel). Vorher entwarf er einen Anruf an sie.
        QVERIFY(isBeaconCallsign(k.callsign));
        QVERIFY(isBeaconCallsign(QStringLiteral("DB0FAI/b")));
        QVERIFY(isBeaconCallsign(QStringLiteral("OE3XAC/BCN")));
        QVERIFY(!isBeaconCallsign(QStringLiteral("OE5SOS")));
        QVERIFY(!isBeaconCallsign(QStringLiteral("S50AAA/P")));
        QVERIFY(!isBeaconCallsign(QStringLiteral("DL/S50AAA")));
    }
};

QTEST_MAIN(TestGrenzfaelle)
#include "test_grenzfaelle.moc"
