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
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>

#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"
#include "data/LogFileReader.h"
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
    void theJournalCanBeReadBackCompletely();
    void aLostDatabaseIsRebuiltFromTheJournal();
    void aJournalThatCannotWriteSaysSoAndDoesNotStopLogging();
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

// Eine Sicherung zählt erst, wenn man sie ZURÜCKLESEN kann -- und
// zwar vollständig. Der ADIF-Leser überging bis 2026-09-29
// Seriennummern und Rapporte; ein damit wiederhergestelltes Log hätte
// keine Punkte gehabt. Dieser Prüfstand schreibt ein Journal und
// liest es wieder ein, Feld für Feld.
void TestLogHaltbarkeit::theJournalCanBeReadBackCompletely()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString pfad = dir.filePath(QStringLiteral("rueck.adi"));
    ContestSettings einstellungen;
    einstellungen.ownCallsign = QStringLiteral("OE5SOS");
    einstellungen.ownGrid = QStringLiteral("JN67VV");

    QsoJournal journal(pfad);
    for (int i = 1; i <= 3; ++i) {
        QsoRecord r = macheQso(i);
        QVERIFY(journal.schreibe(r, einstellungen));
    }

    QFile datei(pfad);
    QVERIFY(datei.open(QIODevice::ReadOnly));
    const QVector<ImportedQso> gelesen = LogFileReader::parse(datei.readAll(), pfad);
    datei.close();

    qInfo().noquote() << "zurückgelesen:" << gelesen.size() << "QSOs";
    QCOMPARE(gelesen.size(), 3);
    const ImportedQso& erstes = gelesen.first();
    qInfo().noquote() << "erstes QSO:" << erstes.callsign << erstes.band << erstes.mode << erstes.grid
                      << "RST" << erstes.rstSent << "/" << erstes.rstRcvd << "Nr." << erstes.serialSent
                      << "/" << erstes.serialRcvd << erstes.contestId;
    QCOMPARE(erstes.callsign, QStringLiteral("DL1ABC"));
    QCOMPARE(erstes.band, QStringLiteral("144"));
    QCOMPARE(erstes.mode, QStringLiteral("SSB"));
    QCOMPARE(erstes.grid, QStringLiteral("JN78CG"));
    QCOMPARE(erstes.rstSent, QStringLiteral("59"));
    QCOMPARE(erstes.rstRcvd, QStringLiteral("59"));
    QCOMPARE(erstes.serialSent, 1);
    QCOMPARE(erstes.serialRcvd, 1);
    QCOMPARE(erstes.contestId, QStringLiteral("IARU_R1_VHF_UHF"));
    QVERIFY2(!erstes.timestampUtc.isEmpty(), "ohne Zeit ist ein Contest-QSO nicht wiederherstellbar");
}

// Der ganze Rettungsweg, von hinten aufgezäumt: die Datenbank ist
// weg, das Journal ist da. Danach muss wieder jedes QSO im Log stehen
// -- mit Nummern, sonst zählt es nichts. Das ist der Fall, für den
// das Journal überhaupt existiert.
void TestLogHaltbarkeit::aLostDatabaseIsRebuiltFromTheJournal()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString journalPfad = dir.filePath(QStringLiteral("verlust.adi"));
    const QString dbPfad = dir.filePath(QStringLiteral("verlust.sqlite"));
    ContestSettings einstellungen;
    einstellungen.ownCallsign = QStringLiteral("OE5SOS");
    einstellungen.ownGrid = QStringLiteral("JN67VV");
    const QString contestId = QStringLiteral("IARU_R1_VHF_UHF");

    // Contest fahren: 12 QSOs, jedes zuerst ins Journal, dann in die
    // Datenbank -- genau die Reihenfolge des Programms.
    QsoJournal journal(journalPfad);
    {
        ContestDatabase db;
        QVERIFY(db.open(dbPfad));
        for (int i = 1; i <= 12; ++i) {
            QsoRecord r = macheQso(i);
            QVERIFY(journal.schreibe(r, einstellungen));
            QVERIFY(db.insertQso(r));
        }
        QCOMPARE(db.qsoCountForContest(contestId), 12);
    }

    // Und jetzt ist die Datenbank hin.
    QVERIFY(QFile::remove(dbPfad));

    // Wiederherstellen: leere Datenbank, Journal einlesen, ergänzen --
    // dieselbe Logik wie MainWindow::restoreFromJournal().
    ContestDatabase neu;
    QVERIFY(neu.open(dbPfad));
    QCOMPARE(neu.qsoCountForContest(contestId), 0);

    QFile datei(journalPfad);
    QVERIFY(datei.open(QIODevice::ReadOnly));
    const QVector<ImportedQso> gelesen = LogFileReader::parse(datei.readAll(), journalPfad);
    datei.close();
    QCOMPARE(gelesen.size(), 12);

    int ergaenzt = 0;
    for (const ImportedQso& q : gelesen) {
        QsoRecord r;
        r.callsign = q.callsign;
        r.band = q.band;
        r.mode = q.mode;
        r.timestampUtc = q.timestampUtc;
        r.gridSquare = q.grid;
        r.rstSent = q.rstSent;
        r.rstRcvd = q.rstRcvd;
        if (q.serialSent > 0) {
            r.serialSent = q.serialSent;
        }
        if (q.serialRcvd > 0) {
            r.serialRcvd = q.serialRcvd;
        }
        r.contestId = contestId;
        if (neu.insertQso(r)) {
            ++ergaenzt;
        }
    }
    qInfo().noquote() << "aus dem Journal wiederhergestellt:" << ergaenzt << "QSOs";
    QCOMPARE(ergaenzt, 12);
    QCOMPARE(neu.qsoCountForContest(contestId), 12);

    // Und die Nummern sind mit zurückgekommen -- ohne sie wäre das Log
    // wertlos.
    const QVector<QsoRecord> wieder = neu.qsosForContest(contestId);
    QCOMPARE(wieder.size(), 12);
    int mitNummer = 0;
    for (const QsoRecord& q : wieder) {
        if (q.serialSent.has_value() && q.serialRcvd.has_value()) {
            ++mitNummer;
        }
    }
    qInfo().noquote() << mitNummer << "von" << wieder.size() << "QSOs haben beide Nummern";
    QCOMPARE(mitNummer, 12);
}

// Und wenn die zweite Spur selbst ausfällt -- Platte voll, Ordner
// nicht beschreibbar, Stick abgezogen? Dann muss zweierlei gelten:
// das Journal sagt es (still scheitern wäre das Schlimmste, man
// verließe sich auf ein Netz, das es nicht gibt), und das Loggen geht
// trotzdem weiter. Die Datenbank ist die erste Spur; ein kaputtes
// Journal darf den Contest nicht anhalten.
void TestLogHaltbarkeit::aJournalThatCannotWriteSaysSoAndDoesNotStopLogging()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
#ifdef Q_OS_WIN
    // Windows kennt keinen Schreibschutz auf Ordnern in dem Sinn, den
    // dieser Prüfstand braucht: setPermissions() greift dort nicht,
    // das Schreiben gelänge trotzdem und der Prüfstand wäre rot, ohne
    // dass etwas falsch wäre. Lieber ehrlich überspringen als eine
    // Zusage prüfen, die hier nicht herstellbar ist.
    QSKIP("Schreibgeschützte Ordner lassen sich unter Windows so nicht herstellen");
#endif
    const QString gesperrterOrdner = dir.filePath(QStringLiteral("nichtbeschreibbar"));
    QVERIFY(QDir().mkpath(gesperrterOrdner));
    // Lesen und betreten erlaubt, schreiben nicht.
    QVERIFY(QFile::setPermissions(gesperrterOrdner,
                                   QFileDevice::ReadOwner | QFileDevice::ExeOwner));

    ContestSettings einstellungen;
    einstellungen.ownCallsign = QStringLiteral("OE5SOS");
    QsoJournal journal(QDir(gesperrterOrdner).filePath(QStringLiteral("geht-nicht.adi")));
    QsoRecord r = macheQso(1);
    const bool geschrieben = journal.schreibe(r, einstellungen);
    qInfo().noquote() << "Journal in gesperrtem Ordner -- geschrieben:" << geschrieben
                      << "| Fehler:" << journal.letzterFehler();
    QVERIFY2(!geschrieben, "Das Journal behauptet, in einen gesperrten Ordner geschrieben zu haben");
    QVERIFY2(!journal.letzterFehler().isEmpty(),
             "Das Journal scheitert STILL -- man verließe sich auf ein Netz, das es nicht gibt");

    // Und die Datenbank nimmt das QSO trotzdem an.
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("weiterhin.sqlite"))));
    QsoRecord r2 = macheQso(2);
    QVERIFY2(db.insertQso(r2), "Ein kaputtes Journal darf das Loggen nicht anhalten");
    QCOMPARE(db.qsoCountForContest(QStringLiteral("IARU_R1_VHF_UHF")), 1);

    QFile::setPermissions(gesperrterOrdner,
                           QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
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
