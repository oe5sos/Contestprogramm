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
#include <QLabel>
#include <QTableView>

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
    void cursorOnTheRowAndTheDeleteKey();
    void escapeBringsTheCursorBackToTheEntryRow();
    void deletingTheLastQsoGivesItsNumberBack();
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

// Martin, 2026-09-27: "mit cursor hinfahren und delete taste" -- und
// dazu: "bei mac gibt es keine entf taste". Auf der MacBook-Tastatur
// ist die Taste mit dem Pfeil Rückschritt; Entf gibt es nur als
// fn+Rückschritt. Gebunden sind darum beide, und beide wirken auf die
// Zeile, auf der der Cursor steht.
void TestQsoLoeschen::cursorOnTheRowAndTheDeleteKey()
{
    for (const Qt::Key key : {Qt::Key_Backspace, Qt::Key_Delete}) {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto controller = makeController(dir, QStringLiteral("taste.sqlite"));
        QVERIFY(controller);
        QVERIFY(insertQso(*controller, QStringLiteral("DL1ABC"), 1) > 0);
        QVERIFY(insertQso(*controller, QStringLiteral("OE3XYZ"), 2) > 0);

        MainWindow window(*controller);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        // Das Fenster muss AKTIV sein, nicht nur sichtbar: die Löschtaste
        // hängt an einem QShortcut mit Qt::WidgetWithChildrenShortcut, und
        // Qt liefert Kurzbefehle nur an ein aktives Fenster aus. Auf dem
        // CI-Mac fiel dieser Prüfstand am 2026-09-28 einmal durch (2 QSOs
        // statt 1 übrig, also Taste ohne Wirkung), lokal 25 von 25 grün --
        // genau das Bild einer Fensteraktivierung, die manchmal noch nicht
        // durch ist. activateWindow() ohne QVERIFY: auf einem Bildschirm
        // ohne Fenstermanager sagt qWaitForWindowActive nichts Sicheres,
        // aber wo es etwas sagt, wartet es richtig.
        window.activateWindow();
        (void)QTest::qWaitForWindowActive(&window);
        auto* table = window.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
        QVERIFY(table);

        // Hinfahren: auf die erste geloggte Zeile.
        table->setFocus();
        table->setCurrentIndex(table->model()->index(0, UnifiedLogWidget::ColumnCall));
        QCOMPARE(table->model()->index(0, UnifiedLogWidget::ColumnCall).data().toString(),
                  QStringLiteral("DL1ABC"));

        // Und drücken.
        QTest::keyClick(table, key);
        const QVector<QsoRecord> left = controller->database().qsosForContest(QStringLiteral("IARU_R1_VHF_UHF"));
        QCOMPARE(left.size(), 1);
        QCOMPARE(left.first().callsign, QStringLiteral("OE3XYZ"));
    }
}

// Wer zum Löschen in die Liste fährt, muss auch wieder heraus. Tucnak:
// "ESC: Always brings you back to the QSO input line". Ohne das hätte
// das Löschen von heute ein Loch in den Arbeitsfluss gerissen.
void TestQsoLoeschen::escapeBringsTheCursorBackToTheEntryRow()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("zurueck.sqlite"));
    QVERIFY(controller);
    QVERIFY(insertQso(*controller, QStringLiteral("DL1ABC"), 1) > 0);

    MainWindow window(*controller);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);
    auto* table = window.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(table);

    table->setFocus();
    QVERIFY(table->hasFocus());
    QVERIFY(log->returnToEntryRow());

    // Der Fokus steht wieder im Rufzeichenfeld: tippen landet dort.
    QTest::keyClicks(QApplication::focusWidget(), QStringLiteral("OE1XYZ"));
    QCOMPARE(log->callsign(), QStringLiteral("OE1XYZ"));
}

// Was das Löschen an der eigenen laufenden Nummer ändert. Sie ist
// MAX(serial_sent) + 1 über das laufende Log: wer das letzte QSO
// löscht, bekommt dessen Nummer wieder. Das ist so gewollt -- ein
// versehentliches Enter soll keine Lücke hinterlassen --, und wer
// mittendrin löscht, ändert an der Zählung nichts.
//
// Wichtig ist, dass die Eingabezeile es auch zeigt: sie zeigte weiter
// die alte, schon vergebene Nummer, bis refreshAfterLogChange() die
// Vorschau mit neu berechnet hat.
void TestQsoLoeschen::deletingTheLastQsoGivesItsNumberBack()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("nummer.sqlite"));
    QVERIFY(controller);
    const int first = insertQso(*controller, QStringLiteral("DL1ABC"), 1);
    const int second = insertQso(*controller, QStringLiteral("OE3XYZ"), 2);
    const int third = insertQso(*controller, QStringLiteral("HB9QQQ"), 3);
    QVERIFY(first > 0 && second > 0 && third > 0);

    MainWindow window(*controller);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);
    auto* preview = window.findChild<QLabel*>(QStringLiteral("sentExchangePreview"));
    QVERIFY(preview);
    QVERIFY2(preview->text().contains(QStringLiteral("004")), qPrintable(preview->text()));

    // Das letzte weg: seine Nummer wird wieder vergeben, und die
    // Eingabezeile sagt es sofort.
    emit log->historyDeleteRequested(third);
    QCOMPARE(controller->database().nextSerialForContest(QStringLiteral("IARU_R1_VHF_UHF"), QString()), 3);
    QVERIFY2(preview->text().contains(QStringLiteral("003")), qPrintable(preview->text()));

    // Eines mittendrin weg: an der Zählung ändert das nichts.
    emit log->historyDeleteRequested(first);
    QCOMPARE(controller->database().nextSerialForContest(QStringLiteral("IARU_R1_VHF_UHF"), QString()), 3);
    QVERIFY2(preview->text().contains(QStringLiteral("003")), qPrintable(preview->text()));

    // Und zurückgeholt zählt es wieder mit. Über den Slot-Namen, weil
    // das Rückgängig an Strg+Z hängt und keine öffentliche Methode ist.
    QVERIFY(QMetaObject::invokeMethod(&window, "undoLastDelete"));
    QCOMPARE(controller->database().qsosForContest(QStringLiteral("IARU_R1_VHF_UHF")).size(), 2);
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestQsoLoeschen tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_qso_loeschen.moc"
