// Die mitgelieferten Contest-Regeln gegen die wirklichen.
//
// tests/test_contest_schedule.cpp prüft, dass "erstes volles Wochenende"
// richtig GERECHNET wird -- mit selbst gebauten Zeitplänen. Was es nicht
// prüft: ob in den ausgelieferten resources/contest_definitions/*.json
// auch die richtigen Angaben stehen. Ein "weekend": 2 oder ein falscher
// Monat bliebe dort unbemerkt, bis der Countdown am Contesttag auf den
// falschen Termin zeigt und jedes geloggte QSO als "außerhalb des
// Contestzeitraums" gilt.
//
// Die Daten unten sind die veröffentlichten Termine, nicht noch einmal
// dieselbe Rechnung: IARU R1 145 MHz am 5./6. September 2026, UHF/Mikro
// am 3./4. Oktober, Marconi Memorial am 7./8. November, jeweils 14:00
// bis 14:00 UTC (iaru-r1.org, WA7BNM-Kalender und der kroatische
// UKW-Kalender, abgerufen 2026-10-16).
//
// Dazu die Wertungsregeln aus GC 2023, die für dieses Programm zählen:
// Entfernung in km als Punkte, eine Station einmal je BAND (nicht je
// Betriebsart), laufende Nummer je Band, Austausch RST + Nr. + Locator.

#include <QtTest>
#include <QDir>
#include <QSet>

#include <QFile>
#include <QTimeZone>

#include "data/ContestDefinition.h"
#include "data/ContestSchedule.h"

using namespace Contestprogramm;

namespace {

ContestDefinition laden(const QString& dateiname)
{
    QString fehler;
    const ContestDefinition d = ContestDefinition::loadFromFile(
        QStringLiteral(CONTESTPROGRAMM_SOURCE_DIR "/resources/contest_definitions/") + dateiname, &fehler);
    if (!d.isValid()) {
        qWarning().noquote() << dateiname << fehler;
    }
    return d;
}

QDateTime utc(int j, int m, int t, int h)
{
    return QDateTime(QDate(j, m, t), QTime(h, 0), QTimeZone::UTC);
}

} // namespace

class TestAusgelieferteRegeln : public QObject
{
    Q_OBJECT

private slots:
    void dieTermineSindDieVeroeffentlichten();
    void dieWertungsregelnSindDieDerIaru();
    void dieKurzwellenTermineSindDieVeroeffentlichten();
    void jedeAusgelieferteRegelLaesstSichLesen();
};

void TestAusgelieferteRegeln::dieTermineSindDieVeroeffentlichten()
{
    struct Erwartet {
        const char* datei;
        int monat;
        int tag2026;   // Samstag, an dem er 2026 beginnt
    };
    // 14:00 UTC Samstag bis 14:00 UTC Sonntag, 24 Stunden.
    const Erwartet kTermine[] = {
        {"iaru_r1_vhf.json", 9, 5},
        {"iaru_r1_uhf.json", 10, 3},
        {"iaru_r1_marconi.json", 11, 7},
    };

    for (const Erwartet& e : kTermine) {
        const ContestDefinition d = laden(QString::fromLatin1(e.datei));
        QVERIFY2(d.isValid(), e.datei);
        const ContestSchedule plan = d.schedule();
        QVERIFY2(plan.isValid(), e.datei);
        QCOMPARE(plan.month, e.monat);
        QCOMPARE(plan.hours, 24);
        QCOMPARE(plan.startTimeUtc, QTime(14, 0));

        const ContestWindow w2026 = plan.windowIn(2026);
        QVERIFY2(w2026.startUtc == utc(2026, e.monat, e.tag2026, 14),
                 qPrintable(QStringLiteral("%1: 2026 beginnt laut Programm %2, veröffentlicht ist %3")
                                .arg(QLatin1String(e.datei),
                                     w2026.startUtc.toString(Qt::ISODate),
                                     utc(2026, e.monat, e.tag2026, 14).toString(Qt::ISODate))));
        // Und er endet 24 Stunden später, am Sonntag.
        QCOMPARE(w2026.endUtc, w2026.startUtc.addSecs(24 * 3600));
        QCOMPARE(w2026.startUtc.date().dayOfWeek(), int(Qt::Saturday));
        QCOMPARE(w2026.endUtc.date().dayOfWeek(), int(Qt::Sunday));
    }
}

void TestAusgelieferteRegeln::dieWertungsregelnSindDieDerIaru()
{
    for (const char* datei : {"iaru_r1_vhf.json", "iaru_r1_uhf.json", "iaru_r1_marconi.json",
                              "iaru_r1_vhf_uhf.json", "oe_vhf_uhf.json"}) {
        const ContestDefinition d = laden(QString::fromLatin1(datei));
        QVERIFY2(d.isValid(), datei);

        // Punkte sind die Entfernung (GC 2023: km abgerundet, plus 1).
        QCOMPARE(d.scoring(), QStringLiteral("distance_km"));
        // Laufende Nummer je Band, nicht je Contest.
        QCOMPARE(d.serialScope(), QStringLiteral("band"));
        // Eine Station zählt einmal je BAND -- die Betriebsart gehört
        // NICHT in die Dupe-Regel, sonst wäre dieselbe Station in CW
        // und SSB zweimal wertbar.
        QStringList scope = d.dupeScope();
        scope.sort();
        QCOMPARE(scope, QStringList({QStringLiteral("band"), QStringLiteral("callsign")}));

        // Austausch: RST, laufende Nummer, Locator -- genau diese drei
        // Rollen, gleich wie die Schlüssel heißen.
        bool rst = false, nummer = false, locator = false;
        for (const ContestDefinition::ExchangeField& f : d.exchangeFields()) {
            rst = rst || f.type == QStringLiteral("rst");
            nummer = nummer || f.autoIncrement;
            locator = locator || f.type == QStringLiteral("grid6");
        }
        QVERIFY2(rst, datei);
        QVERIFY2(nummer, datei);
        QVERIFY2(locator, datei);

        // Und die Bänder sind die des jeweiligen Contests.
        QVERIFY2(!d.bands().isEmpty(), datei);
    }

    // Der Marconi Memorial ist ein CW-Contest -- das muss dastehen,
    // sonst loggt das Programm ohne CAT alles als SSB mit 59.
    const ContestDefinition marconi = laden(QStringLiteral("iaru_r1_marconi.json"));
    QCOMPARE(marconi.modes(), QStringList{QStringLiteral("CW")});
    QCOMPARE(marconi.bands(), QStringList{QStringLiteral("144")});

    // Der September-Contest ist 2 m, der Oktober-Contest 70 cm und höher.
    QCOMPARE(laden(QStringLiteral("iaru_r1_vhf.json")).bands(), QStringList{QStringLiteral("144")});
    const QStringList uhf = laden(QStringLiteral("iaru_r1_uhf.json")).bands();
    QVERIFY2(uhf.first() == QStringLiteral("432"), qPrintable(uhf.join(QLatin1Char(','))));
    QVERIFY2(uhf.contains(QStringLiteral("1296")), qPrintable(uhf.join(QLatin1Char(','))));
    QVERIFY2(!uhf.contains(QStringLiteral("144")), qPrintable(uhf.join(QLatin1Char(','))));
}

QTEST_APPLESS_MAIN(TestAusgelieferteRegeln)
// ── Die Kurzwellenconteste, gegen die Daten der Veranstalter ─────────
//
// Betreiber am 2026-10-23: „hier sollten nur auch alle
// kurzwellenconteste eingetragen werden."
//
// Dieselbe Art Pruefung wie oben, und aus demselben Grund: ein Termin,
// den das Programm selbst ausrechnet, ist erst dann etwas wert, wenn
// er mit dem uebereinstimmt, was die Veranstalter veroeffentlichen.
// Die Zahlen hier stammen von cqww.com, cqwpx.com, arrl.org und
// iaru.org (abgerufen 2026-10-23).
//
// Der Fallstrick ist das Wort VOLL: CQ WW SSB 2026 lief am 24./25.
// Oktober, obwohl der 31. ein Samstag ist -- dessen Sonntag liegt
// schon im November, das Wochenende ist also nicht voll.
void TestAusgelieferteRegeln::dieKurzwellenTermineSindDieVeroeffentlichten()
{
    struct Erwartet {
        const char* datei;
        int jahr;
        int monat;
        int tag;          // Samstag, an dem er beginnt
        int stundeUtc;
        int dauer;
    };
    const Erwartet kTermine[] = {
        {"cq_ww_ssb.json",  2026, 10, 24, 0, 48},
        {"cq_ww_ssb.json",  2027, 10, 30, 0, 48},
        {"cq_ww_cw.json",   2027, 11, 27, 0, 48},
        {"cq_wpx_ssb.json", 2025,  3, 29, 0, 48},
        {"cq_wpx_cw.json",  2025,  5, 24, 0, 48},
        {"iaru_hf.json",    2026,  7, 11, 12, 24},
    };

    for (const Erwartet& e : kTermine) {
        const ContestDefinition d = laden(QString::fromLatin1(e.datei));
        QVERIFY2(d.isValid(), e.datei);
        const ContestSchedule plan = d.schedule();
        QVERIFY2(plan.isValid(), e.datei);
        QCOMPARE(plan.month, e.monat);
        QCOMPARE(plan.hours, e.dauer);

        const ContestWindow w = plan.windowIn(e.jahr);
        QVERIFY2(w.startUtc == utc(e.jahr, e.monat, e.tag, e.stundeUtc),
                 qPrintable(QStringLiteral("%1 %2: Programm sagt %3, veröffentlicht ist %4")
                                .arg(QLatin1String(e.datei))
                                .arg(e.jahr)
                                .arg(w.startUtc.toString(Qt::ISODate),
                                     utc(e.jahr, e.monat, e.tag, e.stundeUtc)
                                         .toString(Qt::ISODate))));
        QCOMPARE(w.startUtc.date().dayOfWeek(), int(Qt::Saturday));
        QCOMPARE(w.endUtc, w.startUtc.addSecs(qint64(e.dauer) * 3600));
    }

    // WPX zaehlt Praefixe -- das ist der ganze Sinn des Contests, und
    // das Modell kann es.
    for (const char* datei : {"cq_wpx_ssb.json", "cq_wpx_cw.json"}) {
        QCOMPARE(laden(QString::fromLatin1(datei)).multiplierField(),
                 QStringLiteral("prefix"));
    }
}

// ── Jede ausgelieferte Regel laesst sich lesen ───────────────────────
//
// Die Laeufe oben pruefen eine NAMENTLICH aufgezaehlte Liste. Eine
// neue Datei, die niemand dort eintraegt, waere also ungeprueft --
// und ein Tippfehler darin faellt erst dem Betreiber auf, wenn die
// Auswahl beim Start einen Contest weniger zeigt.
//
// Darum einmal ueber den ganzen Ordner.
void TestAusgelieferteRegeln::jedeAusgelieferteRegelLaesstSichLesen()
{
    QDir ordner(QStringLiteral(CONTESTPROGRAMM_SOURCE_DIR
                               "/resources/contest_definitions"));
    const QStringList dateien = ordner.entryList({QStringLiteral("*.json")}, QDir::Files);
    QVERIFY2(dateien.size() >= 10,
             qPrintable(QStringLiteral("nur %1 Regeldateien gefunden")
                            .arg(dateien.size())));

    QSet<QString> ids;
    for (const QString& datei : dateien) {
        QString fehler;
        const ContestDefinition d = ContestDefinition::loadFromFile(
            ordner.filePath(datei), &fehler);
        QVERIFY2(d.isValid(),
                 qPrintable(QStringLiteral("%1: %2").arg(datei, fehler)));
        QVERIFY2(!d.id().isEmpty(), qPrintable(datei));
        QVERIFY2(!d.name().isEmpty(), qPrintable(datei));
        QVERIFY2(!d.bands().isEmpty(), qPrintable(datei));
        // Zwei Regeln mit derselben Kennung waeren in der Auswahl
        // nicht zu unterscheiden.
        QVERIFY2(!ids.contains(d.id()),
                 qPrintable(QStringLiteral("%1: Kennung \"%2\" gibt es zweimal")
                                .arg(datei, d.id())));
        ids.insert(d.id());
    }
}

#include "test_ausgelieferte_regeln.moc"
