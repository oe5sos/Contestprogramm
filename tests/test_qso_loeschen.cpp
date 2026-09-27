// Martin, 2026-09-27: "fehler sollen einfach und schnell geändert und
// gelöscht werden". Bis dahin konnte ein QSO nur als ungültig markiert
// werden -- es blieb im Log stehen. Jetzt gibt es beides: löschen
// (weg aus Liste, Wertung, Dupe-Prüfung, Export) und weiterhin
// ungültig markieren (bleibt sichtbar, zählt nicht).
//
// Gelöscht heißt nicht weggeworfen: das QSO wandert unter die
// Papierkorb-Kennung, wie N1MM es in eine eigene Datei legt. Darum
// geht ein Griff daneben mit Strg+Z zurück.

#include <QtTest>

#include <QApplication>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"
#include "data/QsoRecord.h"
#include "ui/MainWindow.h"
#include "ui/UnifiedLogWidget.h"

#include <memory>

using namespace Contestprogramm;

class TestQsoLoeschen : public QObject
{
    Q_OBJECT

private slots:
    void deletedQsoLeavesTheLogButNotTheDatabase();
    void undeleteBringsItBack();
    void deletingFreesTheCallsignForANewQso();
    void deletedQsoIsOutOfTheExports();
};

namespace {

std::unique_ptr<AppController> makeController(QTemporaryDir& dir, const QString& file)
{
    auto controller = std::make_unique<AppController>();
    if (!controller->openDatabase(dir.filePath(file))) {
        return nullptr;
    }
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.activeContestId = QStringLiteral("IARU_R1_VHF_UHF");
    settings.rigctldHost.clear();
    settings.rotor1Enabled = false;
    settings.rotor2Enabled = false;
    settings.esmEnabled = false;
    controller->setSettings(settings);
    return controller;
}

int insertQso(AppController& controller, const QString& call, int serial)
{
    QsoRecord r;
    r.callsign = call;
    r.band = QStringLiteral("144");
    r.mode = QStringLiteral("SSB");
    r.timestampUtc = QDateTime::currentDateTimeUtc().addSecs(-60 * serial).toString(Qt::ISODate);
    r.gridSquare = QStringLiteral("JN58SD");
    r.serialSent = serial;
    r.serialRcvd = serial;
    r.contestId = controller.settings().activeContestId;
    return controller.database().insertQso(r) ? r.id : -1;
}

} // namespace

void TestQsoLoeschen::deletedQsoLeavesTheLogButNotTheDatabase()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("del.sqlite"));
    QVERIFY(controller);
    const int id = insertQso(*controller, QStringLiteral("DL1ABC"), 1);
    QVERIFY(id > 0);
    QVERIFY(insertQso(*controller, QStringLiteral("OE3XYZ"), 2) > 0);

    ContestDatabase& db = controller->database();
    QCOMPARE(db.qsosForContest(QStringLiteral("IARU_R1_VHF_UHF")).size(), 2);

    QString error;
    QVERIFY2(db.deleteQso(id, &error), qPrintable(error));

    // Weg aus dem Log ...
    const QVector<QsoRecord> left = db.qsosForContest(QStringLiteral("IARU_R1_VHF_UHF"));
    QCOMPARE(left.size(), 1);
    QCOMPARE(left.first().callsign, QStringLiteral("OE3XYZ"));

    // ... aber nicht aus der Datenbank: es liegt im Papierkorb.
    const auto record = db.qsoById(id);
    QVERIFY(record.has_value());
    QCOMPARE(record->contestId,
              QStringLiteral("IARU_R1_VHF_UHF") + ContestDatabase::deletedBinSuffix());
    QCOMPARE(db.qsosForContest(record->contestId).size(), 1);
}

void TestQsoLoeschen::undeleteBringsItBack()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("undel.sqlite"));
    QVERIFY(controller);
    const int id = insertQso(*controller, QStringLiteral("DL1ABC"), 1);
    QVERIFY(id > 0);

    ContestDatabase& db = controller->database();
    QVERIFY(db.deleteQso(id));
    QCOMPARE(db.qsosForContest(QStringLiteral("IARU_R1_VHF_UHF")).size(), 0);
    QVERIFY(db.undeleteQso(id));
    QCOMPARE(db.qsosForContest(QStringLiteral("IARU_R1_VHF_UHF")).size(), 1);
    QCOMPARE(db.qsoById(id)->contestId, QStringLiteral("IARU_R1_VHF_UHF"));

    // Zweimal zurückholen schadet nicht.
    QVERIFY(db.undeleteQso(id));
    QCOMPARE(db.qsosForContest(QStringLiteral("IARU_R1_VHF_UHF")).size(), 1);
}

// Der eigentliche Zweck: ein versehentlich falsch geloggtes QSO
// blockiert die Station nicht mehr als Dupe.
void TestQsoLoeschen::deletingFreesTheCallsignForANewQso()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("del_dupe.sqlite"));
    QVERIFY(controller);
    const int id = insertQso(*controller, QStringLiteral("DL1ABC"), 1);
    QVERIFY(id > 0);

    MainWindow window(*controller);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);

    // Dasselbe Rufzeichen auf demselben Band ist ein Dupe ...
    log->setCallsign(QStringLiteral("DL1ABC"));
    QVERIFY(log->dupeIndicatorActive());

    // ... bis das falsche QSO weg ist.
    emit log->historyDeleteRequested(id);
    QVERIFY(!log->dupeIndicatorActive());
    QCOMPARE(controller->database().qsosForContest(QStringLiteral("IARU_R1_VHF_UHF")).size(), 0);
}

void TestQsoLoeschen::deletedQsoIsOutOfTheExports()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("del_export.sqlite"));
    QVERIFY(controller);
    const int id = insertQso(*controller, QStringLiteral("DL1ABC"), 1);
    QVERIFY(insertQso(*controller, QStringLiteral("OE3XYZ"), 2) > 0);
    QVERIFY(controller->database().deleteQso(id));

    // qsosForContest ist die Quelle jedes Exports (EDI, Cabrillo,
    // ADIF) und jeder Wertung -- ein gelöschtes QSO ist dort nicht
    // mehr, ohne dass jeder Export das einzeln wissen muss.
    const QVector<QsoRecord> records = controller->database().qsosForContest(QStringLiteral("IARU_R1_VHF_UHF"));
    QCOMPARE(records.size(), 1);
    QCOMPARE(records.first().callsign, QStringLiteral("OE3XYZ"));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestQsoLoeschen tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_qso_loeschen.moc"
