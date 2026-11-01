// Martin, 2026-09-27: "bekannte rufzeichen aus ukw contest logs und
// auch qrz, aber insbesondere ehemalige logs bei ukw sind primär die
// benchmark."
//
// Zwei Dinge hängen daran. Erstens: die eigenen früheren Logs müssen
// in der Vorschlagsliste stehen -- bis dahin kannte sie nur den
// laufenden Contest, die importierte Locator-Liste und die SCP-Datei.
// Zweitens: wenn eine bekannte Station mit einem anderen Locator
// eingetippt wird, gehört das gesagt (Tucnaks "cross control couple
// callsign - locator") -- auf UKW ist der Locator der Austausch, und
// ein Tippfehler darin kostet das QSO bei der Auswertung.

#include <QtTest>

#include <QApplication>
#include <QLabel>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"
#include "data/QsoRecord.h"
#include "ui/CheckPartialWidget.h"
#include "ui/MainWindow.h"
#include "ui/UnifiedLogWidget.h"

#include <memory>

using namespace Contestprogramm;

class TestBekannteRufzeichen : public QObject
{
    Q_OBJECT

private slots:
    void earlierContestsAreKnownCallsigns();
    void checkPanelOffersCallsignsFromEarlierLogs();
    void aDifferentLocatorForAKnownStationIsFlagged();
    void aDeletedQsoStopsTeachingItsLocator();
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

void insertQso(ContestDatabase& db, const QString& contestId, const QString& call, const QString& grid,
               const QString& time)
{
    QsoRecord r;
    r.callsign = call;
    r.band = QStringLiteral("144");
    r.mode = QStringLiteral("SSB");
    r.timestampUtc = time;
    r.gridSquare = grid;
    r.serialSent = 1;
    r.contestId = contestId;
    QVERIFY(db.insertQso(r));
}

} // namespace

void TestBekannteRufzeichen::earlierContestsAreKnownCallsigns()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("bekannt.sqlite")), QStringLiteral("bekannt")));

    // Ein archiviertes Log von letztem Jahr ...
    insertQso(db, QStringLiteral("IARU_R1_VHF_UHF@2025-10-05"), QStringLiteral("DL1ABC"),
              QStringLiteral("JN58SD"), QStringLiteral("2025-10-05T12:00:00Z"));
    // ... und dieselbe Station, die inzwischen umgezogen ist.
    insertQso(db, QStringLiteral("IARU_R1_VHF_UHF@2026-03-07"), QStringLiteral("DL1ABC"),
              QStringLiteral("JN59AA"), QStringLiteral("2026-03-07T12:00:00Z"));
    insertQso(db, QStringLiteral("IARU_R1_VHF_UHF"), QStringLiteral("OE3XYZ"),
              QStringLiteral("JN78SE"), QStringLiteral("2026-10-03T12:00:00Z"));

    const QHash<QString, QString> known = db.allWorkedCallsigns();
    QCOMPARE(known.size(), 2);
    // Der zuletzt gehörte Locator gewinnt.
    QCOMPARE(known.value(QStringLiteral("DL1ABC")), QStringLiteral("JN59AA"));
    QCOMPARE(known.value(QStringLiteral("OE3XYZ")), QStringLiteral("JN78SE"));

    // Ein gelöschtes QSO zählt nicht mehr dazu.
    const QVector<QsoRecord> current = db.qsosForContest(QStringLiteral("IARU_R1_VHF_UHF"));
    QCOMPARE(current.size(), 1);
    QVERIFY(db.deleteQso(current.first().id));
    QVERIFY(!db.allWorkedCallsigns().contains(QStringLiteral("OE3XYZ")));
}

void TestBekannteRufzeichen::checkPanelOffersCallsignsFromEarlierLogs()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("check.sqlite"));
    QVERIFY(controller);
    // Eine Station, die nur im Log von letztem Jahr steht.
    insertQso(controller->database(), QStringLiteral("IARU_R1_VHF_UHF@2025-10-05"),
              QStringLiteral("DK5III"), QStringLiteral("JN58LL"), QStringLiteral("2025-10-05T12:00:00Z"));

    MainWindow window(*controller);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);
    auto* matches = window.findChild<QLabel*>(QStringLiteral("checkMatches"));
    QVERIFY(matches);

    log->setCallsign(QStringLiteral("DK5"));
    QVERIFY2(matches->text().contains(QStringLiteral("DK5III")), qPrintable(matches->text()));
}

void TestBekannteRufzeichen::aDifferentLocatorForAKnownStationIsFlagged()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("kreuz.sqlite"));
    QVERIFY(controller);
    insertQso(controller->database(), QStringLiteral("IARU_R1_VHF_UHF@2025-10-05"),
              QStringLiteral("DL1ABC"), QStringLiteral("JN58SD"), QStringLiteral("2025-10-05T12:00:00Z"));

    MainWindow window(*controller);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);

    log->setCallsign(QStringLiteral("DL1ABC"));
    // Derselbe Locator: kein Wort.
    log->setExchangeFieldValue(QStringLiteral("grid"), QStringLiteral("JN58SD"));
    QVERIFY2(log->entryWarning().isEmpty(), qPrintable(log->entryWarning()));

    // Kürzer getippt ist kein Widerspruch.
    log->setExchangeFieldValue(QStringLiteral("grid"), QStringLiteral("JN58"));
    QVERIFY2(log->entryWarning().isEmpty(), qPrintable(log->entryWarning()));

    // Ein anderer Locator dagegen schon.
    log->setExchangeFieldValue(QStringLiteral("grid"), QStringLiteral("JN68QQ"));
    QVERIFY2(log->entryWarning().contains(QStringLiteral("JN58SD")), qPrintable(log->entryWarning()));
    QVERIFY2(log->entryWarning().contains(QStringLiteral("JN68QQ")), qPrintable(log->entryWarning()));

    // Gewarnt, nicht gesperrt: das QSO lässt sich trotzdem loggen --
    // die Station kann umgezogen sein.
    log->setExchangeFieldValue(QStringLiteral("serial"), QStringLiteral("001"));
    emit log->logRequested();
    const QVector<QsoRecord> qsos = controller->database().qsosForContest(QStringLiteral("IARU_R1_VHF_UHF"));
    QCOMPARE(qsos.size(), 1);
    QCOMPARE(qsos.first().gridSquare, QStringLiteral("JN68QQ"));
}

// Ein gelöschtes QSO ist gelöscht, weil es ein Fehler war -- oft
// genau deshalb, weil der Locator nicht stimmte. Es darf danach weder
// den Locator vorschlagen noch gegen den richtigen warnen.
void TestBekannteRufzeichen::aDeletedQsoStopsTeachingItsLocator()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("vergessen.sqlite")), QStringLiteral("vergessen")));

    insertQso(db, QStringLiteral("IARU_R1_VHF_UHF@2025-10-05"), QStringLiteral("DL1ABC"),
              QStringLiteral("JN58SD"), QStringLiteral("2025-10-05T12:00:00Z"));
    const QVector<QsoRecord> stored = db.qsosForContest(QStringLiteral("IARU_R1_VHF_UHF@2025-10-05"));
    QCOMPARE(stored.size(), 1);

    // Solange es im Log steht, ist es das Gedächtnis.
    QCOMPARE(db.lastKnownGridForCallsign(QStringLiteral("DL1ABC")).value_or(QString()), QStringLiteral("JN58SD"));
    QVERIFY(db.allWorkedCallsigns().contains(QStringLiteral("DL1ABC")));

    // Gelöscht zählt es nicht mehr -- beide Wege sehen das gleich.
    QVERIFY(db.deleteQso(stored.first().id));
    QVERIFY(!db.lastKnownGridForCallsign(QStringLiteral("DL1ABC")).has_value());
    QVERIFY(!db.allWorkedCallsigns().contains(QStringLiteral("DL1ABC")));

    // Zurückgeholt zählt es wieder.
    QVERIFY(db.undeleteQso(stored.first().id));
    QCOMPARE(db.lastKnownGridForCallsign(QStringLiteral("DL1ABC")).value_or(QString()), QStringLiteral("JN58SD"));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestBekannteRufzeichen tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_bekannte_rufzeichen.moc"
