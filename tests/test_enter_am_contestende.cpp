// =================================================================
// tests/test_enter_am_contestende.cpp  (Contestprogramm)
// =================================================================
//
// Was der Return am ENDE eines 24-Stunden-Contests kostet.
//
// tests/test_grosses_log.cpp misst den Weg am Tastendruck (Dupe,
// Vorschläge, Info-Zeile) bei 2 000 QSOs. Nicht gemessen war bisher
// der andere, längere Weg: das LOGGEN selbst. Nach jedem Return läuft
// MainWindow::refreshAfterLogChange() -- Logtabelle, eigene nächste
// Nummer, Dupe-Nachwertung, Multiplikatoren, Karte, Vorschlagspanel,
// Check-Partial-Quellen, Rate-Panel, Statuszeile -- und jede dieser
// Stationen rechnet über das ganze Log.
//
// Gemessen wird deshalb in der Lage, in der es am meisten gibt: Stunde
// 20 eines Contests. 1 200 QSOs im Log, dazu die Chat- und
// Cluster-Zeilen, die sich bis dahin angesammelt haben.
//
// Die Grenze ist großzügig: dieser Prüfstand soll einen Einbruch um
// Größenordnungen finden, nicht die Tagesform des Rechners bewerten.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-10-16 — New. AI-assisted via Anthropic Claude Code, operator
//                 Ralph Martin Fischer.
// =================================================================

#include <QtTest>

#include <QApplication>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "core/SpotCandidate.h"
#include "data/ContestDatabase.h"
#include "data/QsoRecord.h"
#include "ui/MainWindow.h"
#include "ui/UnifiedLogWidget.h"

#include <algorithm>
#include <memory>

using namespace Contestprogramm;

class TestEnterAmContestende : public QObject
{
    Q_OBJECT

private slots:
    void derReturnBleibtSchnellAuchInStunde20();
};

void TestEnterAmContestende::derReturnBleibtSchnellAuchInStunde20()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = std::make_unique<AppController>();
    QVERIFY(controller->openDatabase(dir.filePath(QStringLiteral("stunde20.sqlite"))));
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.activeContestId = QStringLiteral("IARU_R1_VHF_UHF");
    settings.rigctldHost.clear();
    settings.rotor1Enabled = false;
    settings.rotor2Enabled = false;
    settings.esmEnabled = false;
    controller->setSettings(settings);

    // 1 200 QSOs -- ein gutes 24-Stunden-Ergebnis vom Feuerkogel.
    const QStringList grids{QStringLiteral("JN58SD"), QStringLiteral("JO70FF"), QStringLiteral("JN88TC"),
                            QStringLiteral("JN65XX"), QStringLiteral("JO90AA"), QStringLiteral("JN47PH")};
    const QDateTime start = QDateTime::currentDateTimeUtc().addSecs(-20 * 3600);
    for (int i = 0; i < 1200; ++i) {
        QsoRecord r;
        r.callsign = QStringLiteral("DL%1%2%3").arg(i % 10)
                         .arg(QChar(QLatin1Char('A' + (i / 10) % 26)))
                         .arg(QChar(QLatin1Char('A' + (i / 260) % 26)));
        r.band = (i % 3 == 0) ? QStringLiteral("432") : QStringLiteral("144");
        r.mode = QStringLiteral("SSB");
        r.timestampUtc = start.addSecs(i * 60).toString(Qt::ISODate);
        r.gridSquare = grids.at(i % grids.size());
        r.distanceKm = 100.0 + (i % 400);
        r.serialSent = i + 1;
        r.serialRcvd = i + 1;
        r.rstSent = QStringLiteral("59");
        r.rstRcvd = QStringLiteral("59");
        r.contestId = settings.activeContestId;
        QVERIFY(controller->database().insertQso(r));
    }

    MainWindow window(*controller);
    window.resize(1680, 1000);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCoreApplication::processEvents();

    // Die Chat- und Clusterzeilen aus zwanzig Stunden. Beide Quellen,
    // wie im Betrieb.
    for (int i = 0; i < 2000; ++i) {
        SpotCandidate c;
        c.callsign = QStringLiteral("OK%1%2").arg(i % 10).arg(QChar(QLatin1Char('A' + (i / 10) % 26)));
        c.grid = grids.at(i % grids.size());
        c.freqHz = 144300000LL + (i % 60) * 1000;
        c.timestampUtc = start.addSecs(i * 30);
        c.source = (i % 2) ? QStringLiteral("on4kst") : QStringLiteral("cluster");
        if (i % 2) {
            controller->on4kstFeedModel().addCandidate(c);
        } else {
            controller->clusterFeedModel().addCandidate(c);
        }
    }
    QCoreApplication::processEvents();

    auto* row = window.findChild<QWidget*>(QLatin1String(UnifiedLogWidget::kEntryRowObjectName));
    QVERIFY(row);
    const QList<QLineEdit*> felder = row->findChildren<QLineEdit*>();
    QVERIFY2(felder.size() >= 4, "Die Eingabezeile hat nicht Rufzeichen + RST + Nr. + Locator");

    const int vorher = controller->database().qsosForContest(settings.activeContestId).size();

    // Ein QSO über die Tastatur, wie seine Finger es machen -- und die
    // Stoppuhr NUR um den Return.
    // Fünf QSOs hintereinander, wie im Pile-up -- und der mittlere
    // Wert zählt, nicht der eine Ausreißer.
    QVector<double> dauern;
    const QStringList rufe{QStringLiteral("S51ABC"), QStringLiteral("S52DEF"), QStringLiteral("S53GHI"),
                           QStringLiteral("S54JKL"), QStringLiteral("S55MNO")};
    for (int i = 0; i < rufe.size(); ++i) {
        felder.at(0)->setFocus();
        QTest::keyClicks(felder.at(0), rufe.at(i));
        felder.at(2)->setFocus();
        QTest::keyClicks(felder.at(2), QStringLiteral("%1").arg(42 + i, 3, 10, QLatin1Char('0')));
        felder.at(3)->setFocus();
        QTest::keyClicks(felder.at(3), QStringLiteral("JN76TA"));

        QElapsedTimer t;
        t.start();
        QTest::keyClick(felder.at(3), Qt::Key_Return);
        dauern << t.nsecsElapsed() / 1000000.0;
        QCoreApplication::processEvents();
    }
    std::sort(dauern.begin(), dauern.end());
    const double returnMs = dauern.at(dauern.size() / 2);

    const int nachher = controller->database().qsosForContest(settings.activeContestId).size();
    QStringList alle;
    for (double d : dauern) { alle << QStringLiteral("%1").arg(d, 0, 'f', 1); }
    qInfo().noquote() << QStringLiteral("Return bei %1 QSOs und 2 000 Chatzeilen: %2 ms (alle: %3)")
                             .arg(vorher)
                             .arg(returnMs, 0, 'f', 1)
                             .arg(alle.join(QStringLiteral(" · ")));
    QCOMPARE(nachher, vorher + rufe.size());

    // 250 ms sind schon viel zu viel zum Weitertippen; darüber hat das
    // Programm ein Problem, nicht der Rechner einen schlechten Tag.
    QVERIFY2(returnMs < 250.0,
             qPrintable(QStringLiteral("Der Return dauerte %1 ms bei %2 QSOs")
                            .arg(returnMs, 0, 'f', 1)
                            .arg(vorher)));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestEnterAmContestende tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_enter_am_contestende.moc"
