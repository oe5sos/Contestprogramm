// Martin, 2026-09-27: "alles muss auch ohne verbindung zum funkgerät
// passieren", und die UKW-Conteste sind der Hauptfall. Band und
// Betriebsart hatten seit dem Umbau auf CAT keine eigene Bedienung
// mehr -- ohne Gerät stand das Log für immer auf dem ersten Band des
// Contests, und die Betriebsart auf SSB.
//
// Vorbild ist Tucnak: ein Bandmenü auf Alt+B und je Band eine Taste.

#include <QtTest>

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "core/RigctldClient.h"
#include "data/ContestDatabase.h"
#include "data/QsoRecord.h"
#include "ui/MainWindow.h"
#include "ui/UnifiedLogWidget.h"

#include <memory>

using namespace Contestprogramm;

class TestBandModeOhneGeraet : public QObject
{
    Q_OBJECT

private slots:
    void bandMenuOffersTheContestBandsAndSwitching();
    void modeMenuSwitchesTheModeAndTheReport();
    void loggingFollowsTheChosenBand();
    void rigctldModeCommandIsWellFormed();

private:
    std::unique_ptr<AppController> makeController(QTemporaryDir& dir, const QString& file);
};

std::unique_ptr<AppController> TestBandModeOhneGeraet::makeController(QTemporaryDir& dir, const QString& file)
{
    auto controller = std::make_unique<AppController>();
    if (!controller->openDatabase(dir.filePath(file))) {
        return nullptr;
    }
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.activeContestId = QStringLiteral("IARU_R1_VHF_UHF"); // 144 + 432
    settings.rigctldHost.clear();
    settings.rotor1Enabled = false;
    settings.rotor2Enabled = false;
    settings.esmEnabled = false;
    controller->setSettings(settings);
    return controller;
}

void TestBandModeOhneGeraet::bandMenuOffersTheContestBandsAndSwitching()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("band.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    auto* bandButton = window.findChild<QPushButton*>(QStringLiteral("bandButton"));
    QVERIFY(bandButton);
    // Ohne Funkgerät steht das Log auf dem ersten Band des Contests --
    // und der Knopf sagt es.
    QCOMPARE(window.currentBand(), QStringLiteral("144"));
    QCOMPARE(bandButton->text(), QStringLiteral("144"));

    auto* menu = window.findChild<QMenu*>(QStringLiteral("bandMenu"));
    QVERIFY(menu);
    QStringList offered;
    for (QAction* action : menu->actions()) {
        offered << action->text();
    }
    QCOMPARE(offered, QStringList({QStringLiteral("144"), QStringLiteral("432")}));

    // Umschalten ohne jedes Gerät.
    auto* toUhf = window.findChild<QAction*>(QStringLiteral("bandAction_432"));
    QVERIFY(toUhf);
    toUhf->trigger();
    QCOMPARE(window.currentBand(), QStringLiteral("432"));
    QCOMPARE(bandButton->text(), QStringLiteral("432"));
    // Und das Menü zeigt, wo man steht.
    QVERIFY(toUhf->isChecked());
    QVERIFY(!window.findChild<QAction*>(QStringLiteral("bandAction_144"))->isChecked());

    // Die Tasten: Strg+1 ist das erste Band des Contests.
    QVERIFY2(window.findChild<QAction*>(QStringLiteral("bandAction_144"))->shortcut()
                 == QKeySequence(Qt::CTRL | Qt::Key_1),
             "Strg+1 fehlt am ersten Band");
    QVERIFY2(toUhf->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_2), "Strg+2 fehlt am zweiten Band");
}

void TestBandModeOhneGeraet::modeMenuSwitchesTheModeAndTheReport()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("mode.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    auto* modeButton = window.findChild<QPushButton*>(QStringLiteral("modeButton"));
    QVERIFY(modeButton);
    QCOMPARE(modeButton->text(), QStringLiteral("SSB"));

    // Der vorbelegte Rapport hängt an der Betriebsart: 59 in SSB,
    // 599 in CW. Die Vorschau der gesendeten Zeile zeigt es.
    auto* preview = window.findChild<QLabel*>(QStringLiteral("sentExchangePreview"));
    QVERIFY(preview);
    QVERIFY2(preview->text().contains(QStringLiteral("59")), qPrintable(preview->text()));
    QVERIFY2(!preview->text().contains(QStringLiteral("599")), qPrintable(preview->text()));

    auto* toCw = window.findChild<QAction*>(QStringLiteral("modeAction_CW"));
    QVERIFY(toCw);
    toCw->trigger();
    QCOMPARE(modeButton->text(), QStringLiteral("CW"));
    // ... ohne dass ein Gerät beteiligt war.
    QVERIFY2(preview->text().contains(QStringLiteral("599")), qPrintable(preview->text()));
}

void TestBandModeOhneGeraet::loggingFollowsTheChosenBand()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("log_band.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);

    window.findChild<QAction*>(QStringLiteral("bandAction_432"))->trigger();
    log->setCallsign(QStringLiteral("DL1ABC"));
    log->setExchangeFieldValue(QStringLiteral("serial"), QStringLiteral("001"));
    log->setExchangeFieldValue(QStringLiteral("grid"), QStringLiteral("JN58SD"));
    emit log->logRequested();

    const QVector<QsoRecord> qsos = controller->database().qsosForContest(QStringLiteral("IARU_R1_VHF_UHF"));
    QCOMPARE(qsos.size(), 1);
    QCOMPARE(qsos.first().band, QStringLiteral("432"));
}

// "M USB 0" -- Betriebsart und Durchlass, 0 = Vorgabe des Geräts.
void TestBandModeOhneGeraet::rigctldModeCommandIsWellFormed()
{
    QCOMPARE(RigctldClient::setModeCommand(QStringLiteral("USB")), QByteArray("M USB 0\n"));
    QCOMPARE(RigctldClient::setModeCommand(QStringLiteral("cw")), QByteArray("M CW 0\n"));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestBandModeOhneGeraet tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_band_mode_ohne_geraet.moc"
