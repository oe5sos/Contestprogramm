// Ein UKW-Contest über 24 Stunden bringt schnell vierstellige
// QSO-Zahlen. Alles, was beim Tippen passiert -- Dupe-Prüfung,
// Vorschlagsliste, Wertung --, muss auch dann noch sofort antworten,
// sonst steht der Bediener mitten im Pile-up vor einem hängenden
// Fenster.
//
// Dieser Prüfstand füllt das Log mit 2000 QSOs und misst die Wege, die
// an einem Tastendruck hängen. Die Grenzen sind großzügig gesetzt --
// er soll einen Einbruch um Größenordnungen finden, nicht die
// Tagesform des Rechners bewerten.

#include <QtTest>

#include <QApplication>
#include <QElapsedTimer>
#include <QLabel>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"
#include "data/QsoRecord.h"
#include "ui/MainWindow.h"
#include "ui/UnifiedLogWidget.h"

#include <memory>

using namespace Contestprogramm;

class TestGrossesLog : public QObject
{
    Q_OBJECT

private slots:
    void twoThousandQsosStayResponsive();
};

void TestGrossesLog::twoThousandQsosStayResponsive()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = std::make_unique<AppController>();
    QVERIFY(controller->openDatabase(dir.filePath(QStringLiteral("gross.sqlite"))));
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.activeContestId = QStringLiteral("IARU_R1_VHF_UHF");
    settings.rigctldHost.clear();
    settings.rotor1Enabled = false;
    settings.rotor2Enabled = false;
    controller->setSettings(settings);

    const QStringList grids{QStringLiteral("JN58SD"), QStringLiteral("JO70FF"), QStringLiteral("JN88TC"),
                            QStringLiteral("JN65XX"), QStringLiteral("JO90AA")};
    const QDateTime start = QDateTime::currentDateTimeUtc().addSecs(-24 * 3600);
    for (int i = 0; i < 2000; ++i) {
        QsoRecord r;
        r.callsign = QStringLiteral("DL%1%2%3").arg(i % 10).arg(QChar(QLatin1Char('A' + (i / 10) % 26)))
                         .arg(QChar(QLatin1Char('A' + (i / 260) % 26)));
        r.band = (i % 3 == 0) ? QStringLiteral("432") : QStringLiteral("144");
        r.mode = QStringLiteral("SSB");
        r.timestampUtc = start.addSecs(i * 40).toString(Qt::ISODate);
        r.gridSquare = grids.at(i % grids.size());
        r.distanceKm = 100.0 + (i % 400);
        r.serialSent = i + 1;
        r.serialRcvd = i + 1;
        r.contestId = settings.activeContestId;
        QVERIFY(controller->database().insertQso(r));
    }
    QCOMPARE(controller->database().qsosForContest(settings.activeContestId).size(), 2000);

    QElapsedTimer timer;
    timer.start();
    MainWindow window(*controller);
    window.resize(1680, 1000);
    window.show();
    for (int i = 0; i < 20; ++i) {
        QCoreApplication::processEvents();
        QTest::qWait(10);
    }
    const qint64 build = timer.elapsed();
    qInfo() << "Fenster mit 2000 QSOs gebaut in" << build << "ms";
    QVERIFY2(build < 20000, qPrintable(QStringLiteral("Fenster brauchte %1 ms für 2000 QSOs").arg(build)));

    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);
    auto* lastQso = window.findChild<QLabel*>(QLatin1String(UnifiedLogWidget::kLastQsoLabelObjectName));
    QVERIFY(lastQso);

    // Der Weg, der an jedem Tastendruck hängt: Rufzeichen setzen ->
    // Dupe-Prüfung, Vorschlagsliste, Info-Zeile. Zwanzig Mal, damit
    // ein einzelner Ausreißer nicht das Bild bestimmt.
    timer.restart();
    for (int i = 0; i < 20; ++i) {
        log->setCallsign(QStringLiteral("DL%1AB").arg(i % 10));
    }
    const qint64 perKeystroke = timer.elapsed() / 20;
    qInfo() << "ein gesetztes Rufzeichen (Dupe + Vorschlaege + Info-Zeile):" << perKeystroke << "ms";
    QVERIFY2(perKeystroke < 250,
              qPrintable(QStringLiteral("Ein Rufzeichen kostete %1 ms bei 2000 QSOs").arg(perKeystroke)));

    // Und ein echter Dupe wird auch im großen Log gefunden.
    log->setCallsign(QStringLiteral("DL0AA"));
    QVERIFY2(log->dupeIndicatorActive() || !lastQso->text().isEmpty(), qPrintable(lastQso->text()));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestGrossesLog tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_grosses_log.moc"
