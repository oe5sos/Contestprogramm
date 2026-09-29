// Martin, 2026-09-29: "primäres ziel und das muss zu 100 % passen sind
// meine logs, die ich eingebe. die dürfen nicht weg sein."
//
// Dieser Prüfstand beweist genau das, und zwar auf die harte Tour: ein
// Kindprozess loggt QSOs und wird MITTEN IM BETRIEB abgeschossen --
// kein Beenden, kein Aufräumen, keine Destruktoren, so wie bei
// Stromausfall oder einem erzwungenen Beenden. Danach öffnet der
// Elternprozess dieselbe Datenbank und zählt nach.
//
// Warum _exit() und nicht ein normales return: ein sauberes Ende
// schließt die Datenbank ordentlich und würde beweisen, dass
// ordentliches Schließen funktioniert -- nicht, dass ein UNordentliches
// Ende die Daten verschont.

#include <QtTest>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QApplication>
#include <QProcess>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>

#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"
#include "data/QsoJournal.h"
#include "data/QsoRecord.h"


using namespace Contestprogramm;

namespace {

QsoRecord macheQso(int nummer)
{
    QsoRecord r;
    r.callsign = QStringLiteral("DL%1ABC").arg(nummer % 10);
    r.band = QStringLiteral("144");
    r.mode = QStringLiteral("SSB");
    r.timestampUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    r.rstSent = QStringLiteral("59");
    r.rstRcvd = QStringLiteral("59");
    r.serialSent = nummer;
    r.serialRcvd = nummer;
    r.gridSquare = QStringLiteral("JN78CG");
    r.distanceKm = 51.9;
    r.bearingDeg = 36;
    r.contestId = QStringLiteral("IARU_R1_VHF_UHF");
    return r;
}

} // namespace

class TestLogHaltbarkeit : public QObject
{
    Q_OBJECT

private slots:
    void everyLoggedQsoSurvivesAHardKill();
    void loggingStaysFastEnoughWithFullSync();
    void aKillInTheMiddleOfWritingLosesNothingThatWasConfirmed();
    void afterAHardKillTheLogIsStillUsable();
    void theJournalHoldsEveryQsoEvenWhenTheDatabaseRefuses();
};

void TestLogHaltbarkeit::everyLoggedQsoSurvivesAHardKill()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString datei = dir.filePath(QStringLiteral("haltbar.sqlite"));
    constexpr int kAnzahl = 25;

    // Ein eigener Prozess schreibt die QSOs und wird dann mit SIGKILL
    // erschlagen -- härter geht es nicht: kein Beenden, kein
    // Aufräumen, keine Destruktoren, nichts wird noch weggeschrieben.
    // Das ist der Stromausfall, so nah man ihm im Prüfstand kommt.
    //
    // (Ein fork() im laufenden Prüfstand war der erste Versuch und
    // endete mit SIGABRT: die Datenbankschicht verträgt das Abspalten
    // mitten im Betrieb nicht. Ein sauber gestarteter Prozess schon.)
    QProcess kind;
    kind.setProgram(QCoreApplication::applicationFilePath());
    kind.setArguments({QStringLiteral("--schreibe"), datei, QString::number(kAnzahl)});
    kind.start();
    QVERIFY2(kind.waitForStarted(5000), "der schreibende Prozess startet nicht");

    // Warten, bis er gemeldet hat, dass alle QSOs drin sind.
    QByteArray gemeldet;
    QElapsedTimer uhr;
    uhr.start();
    while (uhr.elapsed() < 10000 && !gemeldet.contains("FERTIG")) {
        kind.waitForReadyRead(200);
        gemeldet += kind.readAllStandardOutput();
    }
    QVERIFY2(gemeldet.contains("FERTIG"),
             qPrintable(QStringLiteral("der Prozess hat die QSOs nicht gemeldet: %1")
                            .arg(QString::fromLatin1(gemeldet))));

    kind.kill(); // SIGKILL
    kind.waitForFinished(5000);
    qInfo().noquote() << "Prozess erschlagen, Endezustand:" << int(kind.exitStatus());

    ContestDatabase db;
    QVERIFY2(db.open(datei), "Die Datenbank lässt sich nach dem harten Ende nicht öffnen");
    const int gezaehlt = db.qsoCountForContest(QStringLiteral("IARU_R1_VHF_UHF"));
    qInfo().noquote() << gezaehlt << "von" << kAnzahl << "QSOs haben den Abschuss überlebt";
    QCOMPARE(gezaehlt, kAnzahl);

    // Und die Sicherheitsstufe schwarz auf weiß: FULL, nicht NORMAL.
    QVariant stufe;
    QVERIFY(db.pragmaValueForTest(QStringLiteral("synchronous"), stufe));
    qInfo().noquote() << "PRAGMA synchronous =" << stufe.toInt() << "(2 = FULL)";
    QCOMPARE(stufe.toInt(), 2);
}

// Der Fall, der im Contest wirklich vorkommt: der Rechner geht MITTEN
// im Schreiben aus, nicht in einer ruhigen Minute danach. Geprüft
// wird die Zusage, die zählt -- was das Programm bestätigt hat, ist
// da. Was im selben Augenblick noch unterwegs war, darf fehlen; nur
// zerstört sein darf nichts.
void TestLogHaltbarkeit::aKillInTheMiddleOfWritingLosesNothingThatWasConfirmed()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString datei = dir.filePath(QStringLiteral("mittendrin.sqlite"));

    QProcess kind;
    kind.setProgram(QCoreApplication::applicationFilePath());
    // 4000 QSOs: der Prozess ist garantiert noch am Schreiben, wenn er
    // erschlagen wird.
    kind.setArguments({QStringLiteral("--schreibe-laufend"), datei, QStringLiteral("4000")});
    kind.start();
    QVERIFY(kind.waitForStarted(5000));

    // Ein Stück schreiben lassen, dann mitten hinein.
    QByteArray gemeldet;
    QElapsedTimer uhr;
    uhr.start();
    while (uhr.elapsed() < 5000 && !gemeldet.contains("STAND")) {
        kind.waitForReadyRead(100);
        gemeldet += kind.readAllStandardOutput();
    }
    QVERIFY2(gemeldet.contains("STAND"), "der Prozess hat keinen Zwischenstand gemeldet");
    const int bestaetigt = QString::fromLatin1(gemeldet).split(QStringLiteral("STAND ")).last()
                               .split(QLatin1Char('\n')).first().toInt();
    QVERIFY(bestaetigt > 0);
    kind.kill();
    kind.waitForFinished(5000);

    ContestDatabase db;
    QVERIFY2(db.open(datei), "Die Datenbank ist nach dem Abschuss mitten im Schreiben nicht zu öffnen");
    const int gezaehlt = db.qsoCountForContest(QStringLiteral("IARU_R1_VHF_UHF"));
    qInfo().noquote() << "bestätigt waren" << bestaetigt << "QSOs, in der Datei stehen" << gezaehlt;
    QVERIFY2(gezaehlt >= bestaetigt,
             qPrintable(QStringLiteral("Bestätigt waren %1 QSOs, wiedergefunden nur %2 -- bestätigte "
                                        "QSOs dürfen NIE fehlen")
                            .arg(bestaetigt).arg(gezaehlt)));
}

// Und danach muss man weiterarbeiten können: das Log öffnet, zählt,
// nimmt neue QSOs an. Eine Datei, die zwar alle Zeilen enthält, sich
// aber nicht mehr beschreiben lässt, wäre im Contest genauso schlimm.
void TestLogHaltbarkeit::afterAHardKillTheLogIsStillUsable()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString datei = dir.filePath(QStringLiteral("weiter.sqlite"));

    QProcess kind;
    kind.setProgram(QCoreApplication::applicationFilePath());
    kind.setArguments({QStringLiteral("--schreibe"), datei, QStringLiteral("10")});
    kind.start();
    QVERIFY(kind.waitForStarted(5000));
    QByteArray gemeldet;
    QElapsedTimer uhr;
    uhr.start();
    while (uhr.elapsed() < 10000 && !gemeldet.contains("FERTIG")) {
        kind.waitForReadyRead(200);
        gemeldet += kind.readAllStandardOutput();
    }
    QVERIFY(gemeldet.contains("FERTIG"));
    kind.kill();
    kind.waitForFinished(5000);

    ContestDatabase db;
    QVERIFY(db.open(datei));
    QCOMPARE(db.qsoCountForContest(QStringLiteral("IARU_R1_VHF_UHF")), 10);
    // Weiterloggen, als wäre nichts gewesen.
    for (int i = 11; i <= 15; ++i) {
        QsoRecord r = macheQso(i);
        QVERIFY2(db.insertQso(r), "nach dem Abschuss lässt sich nicht weiterloggen");
    }
    QCOMPARE(db.qsoCountForContest(QStringLiteral("IARU_R1_VHF_UHF")), 15);
    qInfo().noquote() << "nach dem Abschuss weitergeloggt, jetzt 15 QSOs";
}

// Die zweite Spur. Wenn die Datenbank versagt -- Platte voll, Datei
// gesperrt, Schema kaputt --, darf das QSO nicht nur in der
// Eingabezeile stehen. Hier wird die Datenbank schreibgeschützt
// gemacht, also genau dieser Fall erzwungen, und geprüft, dass das
// Journal die Zeile trotzdem hat.
void TestLogHaltbarkeit::theJournalHoldsEveryQsoEvenWhenTheDatabaseRefuses()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString journalPfad = dir.filePath(QStringLiteral("journal.adi"));
    ContestSettings einstellungen;
    einstellungen.ownCallsign = QStringLiteral("OE5SOS");
    einstellungen.ownGrid = QStringLiteral("JN67VV");

    QsoJournal journal(journalPfad);
    for (int i = 1; i <= 5; ++i) {
        QsoRecord r = macheQso(i);
        QVERIFY2(journal.schreibe(r, einstellungen),
                 qPrintable(QStringLiteral("Journal schreibt nicht: %1").arg(journal.letzterFehler())));
    }

    QFile datei(journalPfad);
    QVERIFY(datei.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString inhalt = QString::fromUtf8(datei.readAll());
    datei.close();
    const int zeilen = inhalt.count(QStringLiteral("<EOR>"));
    qInfo().noquote() << "Journal enthält" << zeilen << "QSO-Zeilen";
    QCOMPARE(zeilen, 5);
    QVERIFY2(inhalt.contains(QStringLiteral("<CALL:6>DL1ABC")), "das Rufzeichen fehlt im Journal");
    QVERIFY2(inhalt.contains(QStringLiteral("<GRIDSQUARE:6>JN78CG")), "der Locator fehlt im Journal");

    // Und jetzt der eigentliche Fall: die Datenbank nimmt nichts mehr
    // an, das Journal schon.
    const QString dbPfad = dir.filePath(QStringLiteral("gesperrt.sqlite"));
    {
        ContestDatabase db;
        QVERIFY(db.open(dbPfad));
        QsoRecord r = macheQso(99);
        QVERIFY(db.insertQso(r));
    }
    QVERIFY(QFile::setPermissions(dbPfad, QFileDevice::ReadOwner));
    ContestDatabase gesperrt;
    const bool geoeffnet = gesperrt.open(dbPfad);
    QsoRecord r = macheQso(100);
    const bool inDatenbank = geoeffnet && gesperrt.insertQso(r);
    const bool imJournal = journal.schreibe(r, einstellungen);
    qInfo().noquote() << "schreibgeschützte Datenbank -- in der Datenbank:" << inDatenbank
                      << "| im Journal:" << imJournal;
    QVERIFY2(imJournal, "Das Journal muss auch dann schreiben, wenn die Datenbank es nicht tut");
    QFile::setPermissions(dbPfad, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

void TestLogHaltbarkeit::loggingStaysFastEnoughWithFullSync()
{
    // Der Preis für die Sicherheit, gemessen statt behauptet: FULL
    // kostet einen fsync je QSO. Im Contest kommt ein QSO alle paar
    // Sekunden -- alles unter ein paar Millisekunden ist unsichtbar.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("tempo.sqlite"))));

    constexpr int kAnzahl = 50;
    QElapsedTimer uhr;
    uhr.start();
    for (int i = 1; i <= kAnzahl; ++i) {
        QsoRecord r = macheQso(i);
        QVERIFY(db.insertQso(r));
    }
    const qint64 gesamt = uhr.elapsed();
    const double proQso = double(gesamt) / kAnzahl;
    qInfo().noquote() << "Ein QSO zu loggen kostet" << QString::number(proQso, 'f', 2) << "ms (FULL sync)";
    QVERIFY2(proQso < 50.0,
             qPrintable(QStringLiteral("Ein QSO zu loggen dauert %1 ms -- das ist im Contest spürbar")
                            .arg(proQso)));
}

int main(int argc, char* argv[])
{
    // Zweite Rolle desselben Programms: QSOs schreiben und auf den
    // Abschuss warten. Siehe everyLoggedQsoSurvivesAHardKill().
    if (argc >= 4 && QString::fromLatin1(argv[1]) == QStringLiteral("--schreibe")) {
        QCoreApplication app(argc, argv);
        ContestDatabase db;
        if (!db.open(QString::fromLocal8Bit(argv[2]))) {
            fprintf(stdout, "OEFFNEN-FEHLGESCHLAGEN\n");
            fflush(stdout);
            return 2;
        }
        const int anzahl = QString::fromLatin1(argv[3]).toInt();
        for (int i = 1; i <= anzahl; ++i) {
            QsoRecord r = macheQso(i);
            if (!db.insertQso(r)) {
                fprintf(stdout, "SCHREIBEN-FEHLGESCHLAGEN\n");
                fflush(stdout);
                return 3;
            }
        }
        fprintf(stdout, "FERTIG\n");
        fflush(stdout);
        // Hier stehen bleiben und erschlagen werden.
        for (;;) {
            QThread::msleep(50);
        }
    }

    if (argc >= 4 && QString::fromLatin1(argv[1]) == QStringLiteral("--schreibe-laufend")) {
        QCoreApplication app(argc, argv);
        ContestDatabase db;
        if (!db.open(QString::fromLocal8Bit(argv[2]))) {
            return 2;
        }
        const int anzahl = QString::fromLatin1(argv[3]).toInt();
        for (int i = 1; i <= anzahl; ++i) {
            QsoRecord r = macheQso(i);
            if (!db.insertQso(r)) {
                return 3;
            }
            // Jeden Zwischenstand melden: was hier gemeldet ist, hat
            // die Datenbank bestätigt und muss den Abschuss überleben.
            if (i % 50 == 0) {
                fprintf(stdout, "STAND %d\n", i);
                fflush(stdout);
            }
        }
        return 0;
    }

    QApplication app(argc, argv);
    TestLogHaltbarkeit tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_log_haltbarkeit.moc"
