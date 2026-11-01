// core/Transverter.h: IF ↔ RF conversion, default offsets, persistence,
// the dialog's suggestion, and the band a rig frequency names in the
// real MainWindow with the transverter switched on and off.

#include <QtTest>

#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QStatusBar>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "core/BandUtils.h"
#include "core/Transverter.h"
#include "data/ContestDatabase.h"
#include "data/QsoRecord.h"
#include "ui/MainWindow.h"
#include "ui/TransverterDialog.h"
#include "ui/UnifiedLogWidget.h"

#include <memory>

using namespace Contestprogramm;

class TestTransverter : public QObject
{
    Q_OBJECT

private slots:
    void bandsCarryRangesAndBases();
    void defaultOffsetIsTheDifferenceOfBandBases();
    void convertsOnlyWhenActiveAndInsideTheBand();
    void roundTripsThroughTheSettingsTable();
    void dialogSuggestsTheOffsetAndKeepsAnEditedOne();
    void rigFrequencyNamesTheBandThroughTheTransverter();
};

void TestTransverter::bandsCarryRangesAndBases()
{
    qint64 low = 0;
    qint64 high = 0;
    QVERIFY(bandRangeHz(QStringLiteral("144"), low, high));
    QCOMPARE(low, 144000000LL);
    QCOMPARE(high, 148000000LL);
    QVERIFY(bandRangeHz(QStringLiteral("1296"), low, high));
    QCOMPARE(low, 1240000000LL);
    QVERIFY(!bandRangeHz(QStringLiteral("23cm"), low, high));
    QCOMPARE(bandBaseHz(QStringLiteral("432")), 432000000LL);
    QCOMPARE(bandBaseHz(QStringLiteral("10368")), 10368000000LL);
    QCOMPARE(bandBaseHz(QStringLiteral("x")), 0LL);
    QCOMPARE(bandLabelForFrequencyHz(28500000), QStringLiteral("28"));
    QCOMPARE(bandLabelForFrequencyHz(1296300000), QStringLiteral("1296"));
    QCOMPARE(bandLabelForFrequencyHz(10368100000LL), QStringLiteral("10368"));
    QCOMPARE(bandLabelForFrequencyHz(100000000), QString());
    QVERIFY(knownBands().contains(QStringLiteral("2320")));
}

void TestTransverter::defaultOffsetIsTheDifferenceOfBandBases()
{
    QCOMPARE(TransverterSetup::defaultOffsetHz(QStringLiteral("144"), QStringLiteral("1296")), 1152000000LL);
    QCOMPARE(TransverterSetup::defaultOffsetHz(QStringLiteral("432"), QStringLiteral("1296")), 864000000LL);
    QCOMPARE(TransverterSetup::defaultOffsetHz(QStringLiteral("28"), QStringLiteral("1296")), 1268000000LL);
    QCOMPARE(TransverterSetup::defaultOffsetHz(QStringLiteral("144"), QStringLiteral("10368")), 10224000000LL);
    QCOMPARE(TransverterSetup::defaultOffsetHz(QStringLiteral("144"), QStringLiteral("144")), 0LL);
    QCOMPARE(TransverterSetup::defaultOffsetHz(QStringLiteral("144"), QString()), 0LL);
}

void TestTransverter::convertsOnlyWhenActiveAndInsideTheBand()
{
    TransverterSetup t;
    t.ifBand = QStringLiteral("144");
    t.rfBand = QStringLiteral("1296");
    t.offsetHz = 1152000000LL;
    QVERIFY(t.configured());
    QVERIFY(!t.active()); // the switch is off
    QCOMPARE(t.rfFrequencyHz(144300000), 144300000LL);
    QCOMPARE(t.rigFrequencyHz(1296300000), 1296300000LL);

    t.enabled = true;
    QVERIFY(t.active());
    QCOMPARE(t.rfFrequencyHz(144300000), 1296300000LL);
    QCOMPARE(t.rigFrequencyHz(1296300000), 144300000LL);
    // The rig on another band: untouched -- the transverter is not in
    // that path.
    QCOMPARE(t.rfFrequencyHz(432300000), 432300000LL);
    QCOMPARE(t.rigFrequencyHz(432300000), 432300000LL);
    QCOMPARE(t.describe(), QStringLiteral("144 → 1296 (+1152 MHz)"));

    TransverterSetup odd;
    odd.enabled = true;
    odd.ifBand = QStringLiteral("28");
    odd.rfBand = QStringLiteral("1296");
    odd.offsetHz = 1268000000LL + 1500;
    QCOMPARE(odd.describe(), QStringLiteral("28 → 1296 (+1268.002 MHz)"));
    QCOMPARE(odd.rfFrequencyHz(28200000), 1296201500LL);

    TransverterSetup none;
    QVERIFY(!none.configured());
    QCOMPARE(none.rfFrequencyHz(144300000), 144300000LL);
    QVERIFY(none.describe().isEmpty());
}

void TestTransverter::roundTripsThroughTheSettingsTable()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("trv.sqlite")), QStringLiteral("trv")));
    QVERIFY(!TransverterSetup::load(db).configured());
    TransverterSetup t;
    t.enabled = true;
    t.ifBand = QStringLiteral("432");
    t.rfBand = QStringLiteral("1296");
    t.offsetHz = 864000000LL;
    t.save(db);
    const TransverterSetup back = TransverterSetup::load(db);
    QVERIFY(back.active());
    QCOMPARE(back.ifBand, QStringLiteral("432"));
    QCOMPARE(back.rfBand, QStringLiteral("1296"));
    QCOMPARE(back.offsetHz, 864000000LL);
}

void TestTransverter::dialogSuggestsTheOffsetAndKeepsAnEditedOne()
{
    TransverterDialog dialog{TransverterSetup()};
    // A fresh dialog proposes the everyday 23 cm case.
    QCOMPARE(dialog.setup().ifBand, QStringLiteral("144"));
    QCOMPARE(dialog.setup().rfBand, QStringLiteral("1296"));
    QCOMPARE(dialog.offsetHz(), 1152000000LL);
    QVERIFY(!dialog.setup().enabled);

    dialog.selectBands(QStringLiteral("432"), QStringLiteral("1296"));
    QCOMPARE(dialog.offsetHz(), 864000000LL);
    dialog.selectBands(QStringLiteral("144"), QStringLiteral("2320"));
    QCOMPARE(dialog.offsetHz(), 2176000000LL);

    // An edited offset survives; the pair given at construction too.
    TransverterSetup custom;
    custom.enabled = true;
    custom.ifBand = QStringLiteral("144");
    custom.rfBand = QStringLiteral("1296");
    custom.offsetHz = 1152001000LL;
    TransverterDialog edited(custom);
    QCOMPARE(edited.offsetHz(), 1152001000LL);
    QVERIFY(edited.setup().enabled);
    QCOMPARE(edited.setup().offsetHz, 1152001000LL);

    // The same band twice is no transverter.
    edited.selectBands(QStringLiteral("144"), QStringLiteral("144"));
    QVERIFY(!edited.setup().configured());
}

void TestTransverter::rigFrequencyNamesTheBandThroughTheTransverter()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = std::make_unique<AppController>();
    QVERIFY(controller->openDatabase(dir.filePath(QStringLiteral("trvwin.sqlite"))));
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.activeContestId = QStringLiteral("IARU_R1_UHF"); // 432 + 1296
    settings.rigctldHost.clear();
    settings.rotor1Enabled = false;
    settings.rotor2Enabled = false;
    settings.esmEnabled = false;
    controller->setSettings(settings);
    TransverterSetup t;
    t.ifBand = QStringLiteral("144");
    t.rfBand = QStringLiteral("1296");
    t.offsetHz = 1152000000LL;
    t.enabled = false;
    t.save(controller->database());

    MainWindow window(*controller);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);
    // The window hands the setup to the controller (the <RadioInfo>
    // broadcast side) too.
    QVERIFY(controller->transverter().configured());
    QVERIFY(!controller->transverter().active());
    // Seit 2026-09-23 ein Eintrag im ⚙-Menü der obersten Zeile statt
    // eines Kästchens in einer eigenen Zeile -- Martins Regel, Optionen
    // gehören unter das Zahnrad rechts oben.
    auto* toggle = window.findChild<QAction*>(QStringLiteral("transverterCheck"));
    QVERIFY(toggle);
    QVERIFY(toggle->isVisible()); // configured: the switch is offered
    QVERIFY(!toggle->isChecked());
    QVERIFY(toggle->text().contains(QStringLiteral("144 → 1296")));

    const auto logQso = [&](const QString& call, const QString& serial) {
        log->setCallsign(call);
        log->setExchangeFieldValue(QStringLiteral("serial"), serial);
        log->setExchangeFieldValue(QStringLiteral("grid"), QStringLiteral("JN58SD"));
        emit log->logRequested();
        const QVector<QsoRecord> qsos = controller->database().qsosForContest(settings.activeContestId);
        return qsos.isEmpty() ? QString() : qsos.last().band;
    };

    // Switch off, rig on 144.300: not a contest band, the log stays on
    // the contest's first band (432).
    emit controller->rigctldClient().frequencyChanged(144300000);
    QCOMPARE(logQso(QStringLiteral("DL1ABC"), QStringLiteral("1")), QStringLiteral("432"));

    // Switch on: the same rig frequency is 1296.300 on the air.
    toggle->setChecked(true);
    QVERIFY(TransverterSetup::load(controller->database()).enabled); // persisted
    QVERIFY(controller->transverter().active()); // and the broadcast side knows
    emit controller->rigctldClient().frequencyChanged(144300000);
    QCOMPARE(logQso(QStringLiteral("DL2ABC"), QStringLiteral("1")), QStringLiteral("1296"));

    // Rig on 432.200 with the switch on: 432 is not the IF band, so it
    // is plain 432.
    emit controller->rigctldClient().frequencyChanged(432200000);
    QCOMPARE(logQso(QStringLiteral("DL3ABC"), QStringLiteral("2")), QStringLiteral("432"));

    // Switch off again while the rig still shows 144.300: back to the
    // rule for bands outside the contest -- the band stays where it is.
    toggle->setChecked(false);
    QVERIFY(!controller->transverter().active());
    emit controller->rigctldClient().frequencyChanged(144300000);
    QCOMPARE(logQso(QStringLiteral("DL4ABC"), QStringLiteral("3")), QStringLiteral("432"));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestTransverter tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_transverter.moc"
