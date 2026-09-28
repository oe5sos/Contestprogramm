// Martin, 2026-09-28: "beim start des contest soll ich dies ggf.
// zusätzlich anführen, sprich ich muss gefragt werden. standard nicht."
//
// Gemeint sind die Bänder. Der IARU-R1-Contest kennt sieben, gefahren
// wird meist eines -- und eine Bandspalte, in der überall dasselbe
// steht, sagt nichts. Also wird beim Beginn eines neuen Logs gefragt,
// vorbelegt mit genau einem Band: dem, auf dem gerade gearbeitet wird.
//
// Die Frage sitzt IM Dialog "Neues Log beginnen?", nicht in einem
// zweiten danach: ein zweiter modaler Dialog im selben Ablauf bliebe in
// jedem Prüfstand stehen, der den ersten wegklickt, und hinge die CI
// auf. Dieser Prüfstand fährt beide Wege -- ankreuzen und abbrechen.

#include <QtTest>

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QGroupBox>
#include <QMessageBox>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"
#include "data/QsoRecord.h"
#include "ui/MainWindow.h"
#include "ui/UnifiedLogWidget.h"

#include <memory>

using namespace Contestprogramm;

namespace {

// Was der Prüfstand im Dialog vorgefunden hat.
struct DialogBefund {
    bool gefunden = false;
    bool gruppeDa = false;
    QStringList angekreuzt;
    QStringList alleBaender;
};

// Der Dialog ist modal: trigger() kehrt erst zurück, wenn er zu ist.
// Also vorher einen Wecker stellen, der ihn sucht, die Kästchen setzt
// und den Knopf drückt.
void bedieneDialog(DialogBefund& befund, const QStringList& ankreuzen, const QString& knopf)
{
    QTimer::singleShot(0, [&befund, ankreuzen, knopf]() {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (!box) {
            return;
        }
        befund.gefunden = true;
        befund.gruppeDa = box->findChild<QGroupBox*>(QStringLiteral("newLogBandGroup")) != nullptr;
        for (QCheckBox* kasten : box->findChildren<QCheckBox*>()) {
            const QString band = kasten->objectName().mid(QStringLiteral("newLogBand_").size());
            befund.alleBaender << band;
            if (kasten->isChecked()) {
                befund.angekreuzt << band;
            }
            // Erst nachdem der Ist-Zustand festgehalten ist, umstellen.
            if (!ankreuzen.isEmpty()) {
                kasten->setChecked(ankreuzen.contains(band));
            }
        }
        for (QAbstractButton* button : box->buttons()) {
            if (button->text() == knopf) {
                QTimer::singleShot(0, [button]() { button->click(); });
            }
        }
    });
}

} // namespace

class TestBaenderBeimContestbeginn : public QObject
{
    Q_OBJECT

private slots:
    void theDialogAsksAndPreselectsExactlyTheCurrentBand();
    void choosingTwoBandsBringsTheBandColumnBack();

private:
    std::unique_ptr<AppController> makeController(QTemporaryDir& dir, const QString& file);
};

std::unique_ptr<AppController> TestBaenderBeimContestbeginn::makeController(QTemporaryDir& dir,
                                                                            const QString& file)
{
    auto controller = std::make_unique<AppController>();
    if (!controller->openDatabase(dir.filePath(file))) {
        return nullptr;
    }
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67VV");
    settings.activeContestId = QStringLiteral("IARU_R1_VHF_UHF");
    settings.rigctldHost.clear();
    settings.rotor1Enabled = false;
    settings.rotor2Enabled = false;
    settings.esmEnabled = false;
    settings.activeBands.clear();
    controller->setSettings(settings);

    // Ein QSO, sonst gibt es nichts zu archivieren und der Dialog kommt
    // gar nicht erst.
    QsoRecord r;
    r.callsign = QStringLiteral("DL1ABC");
    r.band = QStringLiteral("144");
    r.mode = QStringLiteral("SSB");
    r.timestampUtc = QDateTime::currentDateTimeUtc().addSecs(-600).toString(Qt::ISODate);
    r.rstSent = QStringLiteral("59");
    r.rstRcvd = QStringLiteral("59");
    r.serialSent = 1;
    r.serialRcvd = 1;
    r.gridSquare = QStringLiteral("JN58SD");
    r.distanceKm = 165.0;
    r.contestId = settings.activeContestId;
    if (!controller->database().insertQso(r)) {
        return nullptr;
    }
    return controller;
}

void TestBaenderBeimContestbeginn::theDialogAsksAndPreselectsExactlyTheCurrentBand()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("baender.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* action = window.findChild<QAction*>(QStringLiteral("newLogAction"));
    QVERIFY2(action, "Der Menüpunkt „Neues Log beginnen“ fehlt");

    DialogBefund befund;
    bedieneDialog(befund, {}, QStringLiteral("Neues Log beginnen"));
    action->trigger();
    QCoreApplication::processEvents();

    QVERIFY2(befund.gefunden, "Der Dialog kam gar nicht");
    QVERIFY2(befund.gruppeDa, "Es wird nicht nach den Bändern gefragt");
    qInfo().noquote() << "angeboten:" << befund.alleBaender.join(QLatin1Char(' '))
                      << "| vorbelegt:" << befund.angekreuzt.join(QLatin1Char(' '));

    QVERIFY2(befund.alleBaender.size() >= 2,
             "Der Contest bietet weniger als zwei Bänder an -- dann gäbe es nichts zu fragen");
    // "standard nicht": vorbelegt ist genau eines, das laufende.
    QCOMPARE(befund.angekreuzt, QStringList{QStringLiteral("144")});
}

void TestBaenderBeimContestbeginn::choosingTwoBandsBringsTheBandColumnBack()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("zweibaender.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* table = window.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(table);
    auto* action = window.findChild<QAction*>(QStringLiteral("newLogAction"));
    QVERIFY(action);

    // Erst nur 144 wählen: die Bandspalte hat nichts zu zeigen.
    DialogBefund einBand;
    bedieneDialog(einBand, {QStringLiteral("144")}, QStringLiteral("Neues Log beginnen"));
    action->trigger();
    QCoreApplication::processEvents();
    QVERIFY(einBand.gefunden);
    QCOMPARE(controller->settings().activeBands, QStringList{QStringLiteral("144")});
    qInfo() << "nur 144 -- Bandspalte versteckt:" << table->isColumnHidden(UnifiedLogWidget::ColumnBand);
    QVERIFY2(table->isColumnHidden(UnifiedLogWidget::ColumnBand),
             "Bei einem einzigen Band steht die Bandspalte trotzdem da");

    // Jetzt 144 und 432: die Spalte wird gebraucht.
    QsoRecord r;
    r.callsign = QStringLiteral("OK2XYZ");
    r.band = QStringLiteral("144");
    r.mode = QStringLiteral("SSB");
    r.timestampUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    r.rstSent = QStringLiteral("59");
    r.rstRcvd = QStringLiteral("59");
    r.serialSent = 1;
    r.serialRcvd = 1;
    r.gridSquare = QStringLiteral("JN99AA");
    r.distanceKm = 310.0;
    r.contestId = QStringLiteral("IARU_R1_VHF_UHF");
    QVERIFY(controller->database().insertQso(r));

    DialogBefund zweiBaender;
    bedieneDialog(zweiBaender, {QStringLiteral("144"), QStringLiteral("432")},
                   QStringLiteral("Neues Log beginnen"));
    action->trigger();
    QCoreApplication::processEvents();
    QVERIFY(zweiBaender.gefunden);
    qInfo().noquote() << "gewählt:" << controller->settings().activeBands.join(QLatin1Char(' '))
                      << "| Bandspalte versteckt:" << table->isColumnHidden(UnifiedLogWidget::ColumnBand);
    QCOMPARE(controller->settings().activeBands.size(), 2);
    QVERIFY2(!table->isColumnHidden(UnifiedLogWidget::ColumnBand),
             "Bei zwei Bändern fehlt die Bandspalte");
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestBaenderBeimContestbeginn tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_baender_beim_contestbeginn.moc"
