// Eine Zahl im Rufzeichenfeld ist eine Frequenz, keine Station: "14045"
// und Enter geht auf 14,045 MHz und stellt das Band um -- so wie N1MM
// und DXLog es machen. Bis 2026-09-23 gab es ohne CAT überhaupt keinen
// Weg, das Band zu wechseln; auf Kurzwelle war das ein Riegel.

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

class TestFrequencyEntry : public QObject
{
    Q_OBJECT

private slots:
    void aNumberInTheCallsignFieldChangesTheBand();
    void aBandTheContestDoesNotHaveSaysSoAndChangesNothing();
    void arealCallsignStillLogs();

private:
    std::unique_ptr<AppController> makeController(QTemporaryDir& dir, const QString& fileName);
};

std::unique_ptr<AppController> TestFrequencyEntry::makeController(QTemporaryDir& dir, const QString& fileName)
{
    auto controller = std::make_unique<AppController>();
    if (!controller->openDatabase(dir.filePath(fileName))) {
        return nullptr;
    }
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.activeContestId = QStringLiteral("KW_UEBUNG"); // 1,8 bis 28 MHz
    settings.rigctldHost.clear();
    settings.rotor1Enabled = false;
    settings.rotor2Enabled = false;
    settings.esmEnabled = false;
    controller->setSettings(settings);
    return controller;
}

void TestFrequencyEntry::aNumberInTheCallsignFieldChangesTheBand()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("freq_entry.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);

    const auto bandAfterEntry = [&](const QString& text) {
        log->setCallsign(text);
        emit log->logRequested();
        // Das Feld ist wieder leer -- die Zahl war ein Befehl, kein
        // Rufzeichen, und darf nicht stehenbleiben.
        return qMakePair(log->callsign(), window.currentBand());
    };

    // Kilohertz ohne Trennzeichen -- die Lesart, die N1MM vorgibt.
    auto result = bandAfterEntry(QStringLiteral("14045"));
    QVERIFY(result.first.isEmpty());
    QCOMPARE(result.second, QStringLiteral("14"));

    // Megahertz mit Punkt und mit Komma.
    result = bandAfterEntry(QStringLiteral("7.120"));
    QCOMPARE(result.second, QStringLiteral("7"));
    result = bandAfterEntry(QStringLiteral("3,520"));
    QCOMPARE(result.second, QStringLiteral("3.5"));

    // Und ohne Trennzeichen, wo Kilohertz auf kein Band führt: "1.8"
    // heißt in Kilohertz gelesen 1 800 kHz und trifft damit 160 m.
    result = bandAfterEntry(QStringLiteral("1830"));
    QCOMPARE(result.second, QStringLiteral("1.8"));

    // Nichts davon ist im Log gelandet.
    QCOMPARE(controller->database().qsosForContest(QStringLiteral("KW_UEBUNG")).size(), 0);
}

void TestFrequencyEntry::aBandTheContestDoesNotHaveSaysSoAndChangesNothing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("freq_entry_other.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);

    log->setCallsign(QStringLiteral("14045"));
    emit log->logRequested();
    QCOMPARE(window.currentBand(), QStringLiteral("14"));

    // 144,300 MHz -- ein Band, das dieser Contest nicht führt. Das Band
    // bleibt, wo es war, und die Fußzeile sagt warum.
    log->setCallsign(QStringLiteral("144300"));
    emit log->logRequested();
    QCOMPARE(window.currentBand(), QStringLiteral("14"));
}

void TestFrequencyEntry::arealCallsignStillLogs()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("freq_entry_call.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);

    // Ein Rufzeichen mit Ziffern darin bleibt ein Rufzeichen.
    log->setCallsign(QStringLiteral("DL1ABC"));
    log->setExchangeFieldValue(QStringLiteral("serial"), QStringLiteral("001"));
    emit log->logRequested();
    const QVector<QsoRecord> qsos = controller->database().qsosForContest(QStringLiteral("KW_UEBUNG"));
    QCOMPARE(qsos.size(), 1);
    QCOMPARE(qsos.first().callsign, QStringLiteral("DL1ABC"));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestFrequencyEntry tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_frequency_entry.moc"
