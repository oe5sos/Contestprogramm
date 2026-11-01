// Wertung nach den IARU R1 VHF+-Regeln (GC 2023), Pruefbericht vom
// 2026-09-25 -- ein Test je Fund:
//   1.2    S50AAA/P und DL/S50AAA sind dieselbe Station wie S50AAA
//          (Live-Dupe, Nachberechnung, Log-Pruefung)
//   1.9.1  unvollstaendiges QSO (keine Nummer, 4-stelliger Locator) = 0 Punkte;
//          eigener Locator muss sechsstellig sein (Startcheck)
//   1.10.1 Entfernung mit 111,2 km je Grad, Mittelpunkt der Felder
//   1.5    der UHF-Contest hat 432 MHz bis 10 GHz
//   REG1TEST TDate = Contesttage, nicht QSO-Tage; CODXC = Entfernung
//   Neuer eigener Locator -> Entfernungen des laufenden Logs neu
//   Zeitfenster auf die Minute (14:00:30 am Sonntag gilt noch)

#include "app/ContestSettings.h"
#include "core/CallsignPrefix.h"
#include "core/Maidenhead.h"
#include "data/ContestDatabase.h"
#include "data/ContestDefinition.h"
#include "data/ContestSchedule.h"
#include "data/ContestScoring.h"
#include "data/DupeChecker.h"
#include "data/DupeRescore.h"
#include "data/EdiExporter.h"
#include "data/LogCheck.h"
#include "data/QsoRecord.h"
#include "data/ReadinessCheck.h"

#include <QTemporaryDir>
#include <QtTest>

#include <cmath>

using namespace Contestprogramm;

namespace {

const QString kOwn = QStringLiteral("JN67UT");
const QString kContest = QStringLiteral("IARU_R1_UHF");
const QStringList kCallBand{QStringLiteral("callsign"), QStringLiteral("band")};

QsoRecord qso(const QString& call, const QString& band, const QString& grid,
              const QString& time = QStringLiteral("2026-10-03T14:10:00Z"))
{
    QsoRecord r;
    r.callsign = call;
    r.band = band;
    r.mode = QStringLiteral("SSB");
    r.timestampUtc = time;
    r.gridSquare = grid;
    r.serialSent = 1;
    r.serialRcvd = 7;
    r.rstSent = QStringLiteral("59");
    r.rstRcvd = QStringLiteral("59");
    r.contestId = kContest;
    return r;
}

ContestDefinition shippedUhf()
{
    QString error;
    const ContestDefinition def = ContestDefinition::loadFromFile(
        QString::fromUtf8(CP_SOURCE_DIR "/resources/contest_definitions/iaru_r1_uhf.json"), &error);
    if (!def.isValid()) { qWarning() << error; }
    return def;
}

} // namespace

class TestWertungIaruGc2023 : public QObject
{
    Q_OBJECT

private slots:
    // ── 1.2: Zusatz vor/hinter dem Schraegstrich ist dieselbe Station ──
    void baseCallsignDropsPrefixAndSuffix()
    {
        QCOMPARE(baseCallsign(QStringLiteral("S50AAA/P")), QStringLiteral("S50AAA"));
        QCOMPARE(baseCallsign(QStringLiteral("s50aaa/p")), QStringLiteral("S50AAA"));
        QCOMPARE(baseCallsign(QStringLiteral("DL/S50AAA")), QStringLiteral("S50AAA"));
        QCOMPARE(baseCallsign(QStringLiteral("9A/OE5SOS/P")), QStringLiteral("OE5SOS"));
        QCOMPARE(baseCallsign(QStringLiteral("OE5SOS/M")), QStringLiteral("OE5SOS"));
        QCOMPARE(baseCallsign(QStringLiteral("N8BJQ/9")), QStringLiteral("N8BJQ"));
        QCOMPARE(baseCallsign(QStringLiteral("OE5SOS")), QStringLiteral("OE5SOS"));
    }

    void liveDupeCheckSeesThroughPortableSuffix()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        ContestDatabase db;
        QVERIFY(db.open(dir.filePath(QStringLiteral("dupe.sqlite")), QStringLiteral("gc2023_dupe")));
        QsoRecord first = qso(QStringLiteral("S50AAA"), QStringLiteral("432"), QStringLiteral("JN75DS"));
        QVERIFY(db.insertQso(first));

        const DupeChecker checker(db);
        QVERIFY(checker.isDupe(QStringLiteral("S50AAA/P"), QStringLiteral("432"), QStringLiteral("CW"), kContest, kCallBand));
        QVERIFY(checker.isDupe(QStringLiteral("DL/S50AAA"), QStringLiteral("432"), QStringLiteral("SSB"), kContest, kCallBand));
        // Anderes Band: kein Dupe. Anderes Rufzeichen, das S50AAA nur
        // enthaelt: auch keins (der SQL-Vorfilter entscheidet nicht allein).
        QVERIFY(!checker.isDupe(QStringLiteral("S50AAA/P"), QStringLiteral("1296"), QStringLiteral("SSB"), kContest, kCallBand));
        QVERIFY(!checker.isDupe(QStringLiteral("S50AAAB"), QStringLiteral("432"), QStringLiteral("SSB"), kContest, kCallBand));
    }

    void rescoreMarksThePortableSecondContactAsDupe()
    {
        QVector<QsoRecord> log{qso(QStringLiteral("S50AAA"), QStringLiteral("432"), QStringLiteral("JN75DS")),
                               qso(QStringLiteral("S50AAA/P"), QStringLiteral("432"), QStringLiteral("JN75DS"),
                                   QStringLiteral("2026-10-03T15:00:00Z"))};
        log[0].id = 1;
        log[1].id = 2;
        const QVector<DupeFlagChange> changes = recomputeDupeFlags(log, kCallBand);
        QCOMPARE(changes.size(), 1);
        QCOMPARE(changes.first().qsoId, 2);
        QVERIFY(changes.first().isDupe);
    }

    void logCheckWarnsAboutAnUnmarkedPortableDupe()
    {
        QVector<QsoRecord> log{qso(QStringLiteral("S50AAA"), QStringLiteral("432"), QStringLiteral("JN75DS")),
                               qso(QStringLiteral("S50AAA/P"), QStringLiteral("432"), QStringLiteral("JN75DS"),
                                   QStringLiteral("2026-10-03T15:00:00Z"))};
        log[0].id = 1;
        log[1].id = 2;
        LogCheckContext context;
        context.ownCallsign = QStringLiteral("OE5SOS");
        context.ownGrid = kOwn;
        context.bands = {QStringLiteral("432")};
        context.dupeScope = kCallBand;
        const LogCheckResult result = checkLog(log, context);
        bool warned = false;
        for (const auto& issue : result.issues) {
            if (issue.code == QStringLiteral("unmarked_dupe")) { warned = true; }
        }
        QVERIFY2(warned, "S50AAA/P nach S50AAA auf 432 muss als Dupe auffallen");
    }

    // ── 1.10.1: 111,2 km je Grad ────────────────────────────────────────
    void qrbUsesTheRuleFactor()
    {
        const double rule = iaruQrbKm(kOwn, QStringLiteral("JN58SD"));
        // Dieselben Mittelpunkte, derselbe Winkel -- nur der Faktor:
        // 111,2 km je Grad statt Erdradius 6371 km.
        const double map = calculateDistanceKm(kOwn, QStringLiteral("JN58SD"));
        const double ratio = 111.2 / (6371.0 * M_PI / 180.0);
        QVERIFY(std::abs(rule - map * ratio) < 1e-6);
        QVERIFY(rule > map);   // 6371,29 > 6371
        QCOMPARE(iaruQrbKm(kOwn, kOwn), 0.0);
        QCOMPARE(iaruQrbKm(kOwn, QStringLiteral("XX99")), -1.0);
    }

    // ── 1.9.1: unvollstaendig = 0 Punkte ──────────────────────────────
    void incompleteQsosScoreNothing()
    {
        QsoRecord full = qso(QStringLiteral("DL1ABC"), QStringLiteral("432"), QStringLiteral("JN58SD"));
        const int points = qsoDistancePoints(full, kOwn);
        QCOMPARE(points, static_cast<int>(std::floor(iaruQrbKm(kOwn, QStringLiteral("JN58SD")))) + 1);

        QsoRecord noSerial = full;
        noSerial.serialRcvd.reset();
        QCOMPARE(qsoDistancePoints(noSerial, kOwn), 0);

        QsoRecord fourDigits = full;
        fourDigits.gridSquare = QStringLiteral("JN58");
        QCOMPARE(qsoDistancePoints(fourDigits, kOwn), 0);

        QsoRecord sameSquare = full;
        sameSquare.gridSquare = kOwn;
        QCOMPARE(qsoDistancePoints(sameSquare, kOwn), 1);
    }

    void readinessRejectsAFourDigitOwnLocator()
    {
        ReadinessContext context;
        context.ownCallsign = QStringLiteral("OE5SOS");
        context.ownGrid = QStringLiteral("JN67");
        const ReadinessResult result = checkReadiness(context);
        bool error = false;
        for (const ReadinessItem& item : result.items) {
            if (item.code == QStringLiteral("own_grid") && item.level == ReadinessItem::Level::Error) {
                error = true;
            }
        }
        QVERIFY2(error, "ein vierstelliger eigener Locator muss im Startcheck ein Fehler sein");
    }

    // ── Neuer eigener Locator: das laufende Log neu vermessen ─────────
    void recomputeDistancesFollowsTheNewOwnLocator()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        ContestDatabase db;
        QVERIFY(db.open(dir.filePath(QStringLiteral("recompute.sqlite")), QStringLiteral("gc2023_recompute")));
        QsoRecord r = qso(QStringLiteral("DL1ABC"), QStringLiteral("432"), QStringLiteral("JN58SD"));
        r.distanceKm = iaruQrbKm(QStringLiteral("JN78AA"), QStringLiteral("JN58SD"));   // mit dem Heim-Locator geloggt
        QVERIFY(db.insertQso(r));
        QsoRecord other = qso(QStringLiteral("OE3XYZ"), QStringLiteral("432"), QStringLiteral("JN88TC"));
        other.contestId = QStringLiteral("ANDERER_CONTEST");
        other.distanceKm = 999.0;
        QVERIFY(db.insertQso(other));

        QCOMPARE(db.recomputeDistances(kContest, kOwn), 1);
        const QVector<QsoRecord> after = db.qsosForContest(kContest);
        QCOMPARE(after.size(), 1);
        QVERIFY(after.first().distanceKm);
        QVERIFY(std::abs(*after.first().distanceKm - iaruQrbKm(kOwn, QStringLiteral("JN58SD"))) < 1e-9);
        // Ein anderes Log (anderer Standort) bleibt unberuehrt.
        QCOMPARE(*db.qsosForContest(QStringLiteral("ANDERER_CONTEST")).first().distanceKm, 999.0);
    }

    // ── 1.5: die mitgelieferte UHF-Definition ────────────────────────
    void shippedUhfDefinitionHasTheMicrowaveBands()
    {
        const ContestDefinition def = shippedUhf();
        QVERIFY(def.isValid());
        QCOMPARE(def.bands(), (QStringList{QStringLiteral("432"), QStringLiteral("1296"), QStringLiteral("2320"),
                                           QStringLiteral("3400"), QStringLiteral("5760"), QStringLiteral("10368")}));
        const ContestWindow window = def.schedule().windowIn(2026);
        QCOMPARE(window.startUtc, QDateTime(QDate(2026, 10, 3), QTime(14, 0), QTimeZone::UTC));
        QCOMPARE(window.endUtc, QDateTime(QDate(2026, 10, 4), QTime(14, 0), QTimeZone::UTC));
    }

    // ── Zeitfenster auf die Minute ───────────────────────────────────
    void windowEdgesHaveMinuteResolution()
    {
        const ContestWindow window = shippedUhf().schedule().windowIn(2026);
        QVERIFY(window.contains(QDateTime(QDate(2026, 10, 4), QTime(14, 0, 30), QTimeZone::UTC)));
        QVERIFY(!window.contains(QDateTime(QDate(2026, 10, 4), QTime(14, 1, 0), QTimeZone::UTC)));
        QVERIFY(!window.contains(QDateTime(QDate(2026, 10, 3), QTime(13, 59, 59), QTimeZone::UTC)));
        QVERIFY(window.contains(QDateTime(QDate(2026, 10, 3), QTime(14, 0, 0), QTimeZone::UTC)));
    }

    // ── REG1TEST: TDate und CODXC ────────────────────────────────────
    void ediTakesTheContestDaysAndTheOdxDistance()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        ContestDatabase db;
        QVERIFY(db.open(dir.filePath(QStringLiteral("edi.sqlite")), QStringLiteral("gc2023_edi")));
        // Nur am Samstag gefunkt.
        QsoRecord r = qso(QStringLiteral("DL1ABC"), QStringLiteral("432"), QStringLiteral("JN58SD"));
        r.distanceKm = iaruQrbKm(kOwn, QStringLiteral("JN58SD"));
        QVERIFY(db.insertQso(r));

        ContestSettings settings;
        settings.ownCallsign = QStringLiteral("OE5SOS");
        settings.ownGrid = kOwn;
        settings.activeContestId = kContest;
        EdiStationInfo station;
        const EdiExporter exporter(db);
        const QString edi = exporter.exportBand(kContest, QStringLiteral("432"), shippedUhf(), settings, station);
        QVERIFY2(edi.contains(QStringLiteral("TDate=20261003;20261004\r\n")), qPrintable(edi.left(200)));
        const int km = static_cast<int>(std::floor(*r.distanceKm));
        QVERIFY2(edi.contains(QStringLiteral("CODXC=DL1ABC;JN58SD;%1\r\n").arg(km)), qPrintable(edi));
    }
};

QTEST_MAIN(TestWertungIaruGc2023)
#include "test_wertung_iaru_gc2023.moc"
