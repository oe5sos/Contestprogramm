// =================================================================
// tests/test_fenster_bei_leerem_log.cpp  (Contestprogramm)
// =================================================================
//
// Jedes Fenster und jeden Dialog einmal aufmachen UND zeichnen -- in
// den beiden Lagen, in denen am wenigsten dasteht und am meisten
// schiefgehen kann:
//
//   1. Leeres Log, kein Contest ausgewaehlt, keine Einstellungen.
//      Das ist der Zustand in der ersten Minute nach dem ersten Start
//      -- und der, in dem jede Durchschnittsrechnung durch Null teilt.
//   2. Ein Log mit EINEM unbrauchbaren QSO: kein Locator, keine
//      Nummer, leere Betriebsart, unbekanntes Band, unlesbare
//      Zeitangabe. So etwas kommt ueber einen Import herein (eine
//      fremde ADIF-Datei) oder aus einer halb getippten Zeile.
//
// Gezeichnet wird wirklich (grab()), nicht nur gebaut: eine
// Division durch Null faellt erst beim Malen auf, als "nan" in einem
// QRectF -- und ein QPainter mit nan zeichnet nichts, ohne sich zu
// beschweren. Deshalb wird zusaetzlich jede Beschriftung im Fenster
// nach "nan", "inf" und "-1" durchsucht.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-10-16 — New. AI-assisted via Anthropic Claude Code, operator
//                 Ralph Martin Fischer.
// =================================================================

#include <QtTest>

#include <QApplication>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QRegularExpression>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"
#include "core/ColorTheme.h"
#include "data/ContestStatistics.h"
#include "data/LogBackup.h"
#include "data/LogCheck.h"
#include "data/MultiplierTracker.h"
#include "data/QsoRecord.h"
#include "data/ReadinessCheck.h"
#include "ui/AboutDialog.h"
#include "ui/BackupRestoreDialog.h"
#include "ui/CabrilloExportDialog.h"
#include "ui/ContestPickerDialog.h"
#include "ui/ContestRulesEditor.h"
#include "ui/EdiExportDialog.h"
#include "ui/MainWindow.h"
#include "ui/UnifiedLogWidget.h"
#include "ui/EsmTemplatesDialog.h"
#include "ui/LogCheckWindow.h"
#include "ui/MultiplierWindow.h"
#include "ui/ReadinessWindow.h"
#include "ui/ScoreboardDialog.h"
#include "ui/SettingsDialog.h"
#include "ui/ShortcutsWindow.h"
#include "ui/StatisticsWindow.h"
#include "ui/StyleKit.h"
#include "ui/TransverterDialog.h"
#include "ui/UpdateDialog.h"

#include <memory>

using namespace Contestprogramm;

namespace {

// Was in einer Beschriftung niemals stehen darf. "nan"/"inf" kommen
// aus einer Division durch Null, "-1" aus einem nicht gesetzten
// Zaehler, der als Zahl durchgereicht wurde.
void keineUnsinnsBeschriftung(QWidget* fenster, const QString& name)
{
    for (const QLabel* label : fenster->findChildren<QLabel*>()) {
        const QString text = label->text();
        for (const QString& unsinn : {QStringLiteral("nan"), QStringLiteral("inf")}) {
            // Wortweise, damit "Information" nicht als "inf" gilt.
            for (const QString& wort : text.split(QRegularExpression(QStringLiteral("[\\s:;,()/]+")),
                                                   Qt::SkipEmptyParts)) {
                QVERIFY2(wort.compare(unsinn, Qt::CaseInsensitive) != 0,
                         qPrintable(QStringLiteral("%1: Beschriftung \"%2\"").arg(name, text)));
                QVERIFY2(wort.compare(QStringLiteral("-") + unsinn, Qt::CaseInsensitive) != 0,
                         qPrintable(QStringLiteral("%1: Beschriftung \"%2\"").arg(name, text)));
            }
        }
    }
}

// Aufmachen, zeichnen, durchsehen, zumachen.
void durch(QWidget* fenster, const QString& name)
{
    QVERIFY2(fenster, qPrintable(name));
    fenster->resize(fenster->sizeHint().expandedTo(QSize(600, 400)));
    fenster->show();
    QCoreApplication::processEvents();
    const QPixmap bild = fenster->grab();
    QVERIFY2(!bild.isNull(), qPrintable(QStringLiteral("%1: nichts gezeichnet").arg(name)));
    keineUnsinnsBeschriftung(fenster, name);
    fenster->hide();
    QCoreApplication::processEvents();
}

QsoRecord unbrauchbaresQso()
{
    QsoRecord r;
    r.callsign = QStringLiteral("DL1ABC");
    r.band = QStringLiteral("999");          // kein Band, das das Programm kennt
    r.mode = QString();                       // keine Betriebsart
    r.timestampUtc = QStringLiteral("gestern abend");  // keine lesbare Zeit
    r.gridSquare = QString();                 // kein Locator
    r.contestId = QStringLiteral("IARU_R1_VHF_UHF");
    return r;
}

} // namespace

class TestFensterBeiLeeremLog : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void alleFensterBeiLeeremLog();
    void alleFensterMitEinemUnbrauchbarenQso();
    void hauptfensterMitVerschwundenemContest();

private:
    void alleFenster(bool mitQso, const QString& dateiname);
};

void TestFensterBeiLeeremLog::initTestCase()
{
    // Ohne das Thema sind die Farben nicht gesetzt und die Blätter
    // sehen anders aus als im Programm (hier nur der Vollständigkeit
    // halber -- gezeichnet wird so, wie es der Bediener sieht).
    Style::setActiveTheme(ContestSettings().colorTheme);
    qApp->setStyleSheet(Style::appStyleSheet());
}

void TestFensterBeiLeeremLog::alleFenster(bool mitQso, const QString& dateiname)
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppController controller;
    QVERIFY(controller.openDatabase(dir.filePath(dateiname)));

    ContestSettings settings = controller.settings();
    if (mitQso) {
        settings.ownCallsign = QStringLiteral("OE5SOS");
        settings.ownGrid = QStringLiteral("JN67UT");
        settings.activeContestId = QStringLiteral("IARU_R1_VHF_UHF");
        controller.setSettings(settings);
        QsoRecord q = unbrauchbaresQso();
        QVERIFY(controller.database().insertQso(q));
    }
    // Sonst bleibt alles, wie es nach dem ersten Start dasteht: kein
    // Rufzeichen, kein Locator, kein Contest.

    const QVector<ContestDefinition> contests = controller.availableContestDefinitions();
    const QVector<QsoRecord> qsos = controller.database().qsosForContest(settings.activeContestId);

    {
        AboutDialog::Facts facts;
        AboutDialog dialog(facts);
        durch(&dialog, QStringLiteral("AboutDialog"));
    }
    {
        const QVector<LogBackup::Entry> keineSicherungen;
        BackupRestoreDialog dialog(keineSicherungen);
        durch(&dialog, QStringLiteral("BackupRestoreDialog"));
    }
    {
        const CabrilloCategories kategorien;
        CabrilloExportDialog dialog(kategorien);
        durch(&dialog, QStringLiteral("CabrilloExportDialog"));
    }
    {
        ContestPickerDialog dialog(contests, settings.activeContestId);
        dialog.setOwnGrid(settings.ownGrid);
        durch(&dialog, QStringLiteral("ContestPickerDialog"));
    }
    {
        ContestRulesEditor dialog(contests, settings.activeContestId);
        durch(&dialog, QStringLiteral("ContestRulesEditor"));
    }
    {
        // Der EDI-Dialog braucht eine Regeldefinition; ohne aktiven
        // Contest nimmt er eine leere -- genau die Lage, die zu prüfen
        // ist.
        ContestDefinition definition;
        for (const ContestDefinition& d : contests) {
            if (d.id() == settings.activeContestId) {
                definition = d;
            }
        }
        EdiExportDialog dialog(controller.database(), definition, settings);
        durch(&dialog, QStringLiteral("EdiExportDialog"));
    }
    {
        const EsmTemplates vorlagen;
        EsmTemplatesDialog dialog(vorlagen);
        durch(&dialog, QStringLiteral("EsmTemplatesDialog"));
    }
    {
        LogCheckWindow window;
        LogCheckContext ctx;
        ctx.ownCallsign = settings.ownCallsign;
        ctx.ownGrid = settings.ownGrid;
        window.setResult(checkLog(qsos, ctx), QStringLiteral("Prüflauf"));
        durch(&window, QStringLiteral("LogCheckWindow"));
    }
    {
        MultiplierTracker tracker(controller.database());
        MultiplierWindow window(tracker);
        durch(&window, QStringLiteral("MultiplierWindow"));
    }
    {
        ReadinessWindow window;
        ReadinessContext ctx;
        ctx.nowUtc = QDateTime::currentDateTimeUtc();
        ctx.ownCallsign = settings.ownCallsign;
        ctx.ownGrid = settings.ownGrid;
        window.setResult(checkReadiness(ctx));
        durch(&window, QStringLiteral("ReadinessWindow"));
    }
    {
        ScoreboardDialog dialog(settings);
        durch(&dialog, QStringLiteral("ScoreboardDialog"));
    }
    {
        SettingsDialog dialog(settings, contests);
        durch(&dialog, QStringLiteral("SettingsDialog"));
    }
    {
        ShortcutsWindow window;
        durch(&window, QStringLiteral("ShortcutsWindow"));
    }
    {
        StatisticsWindow window(controller.database());
        window.setContest(settings.activeContestId, settings.ownGrid,
                          QStringList{QStringLiteral("144"), QStringLiteral("432")},
                          QStringLiteral("distance_km"));
        durch(&window, QStringLiteral("StatisticsWindow"));
    }
    {
        const TransverterSetup aufbau;
        TransverterDialog dialog(aufbau);
        durch(&dialog, QStringLiteral("TransverterDialog"));
    }
    {
        UpdateDialog dialog;
        durch(&dialog, QStringLiteral("UpdateDialog"));
    }
}

void TestFensterBeiLeeremLog::alleFensterBeiLeeremLog()
{
    alleFenster(false, QStringLiteral("leer.sqlite"));
}

void TestFensterBeiLeeremLog::alleFensterMitEinemUnbrauchbarenQso()
{
    alleFenster(true, QStringLiteral("unbrauchbar.sqlite"));
}

// Die Kennung in den Einstellungen zeigt auf einen Contest, den es
// nicht mehr gibt. Über den Regeleditor faengt das Programm das ab
// (MainWindow schaltet dann auf den ersten vorhandenen), aber die
// Kennung kann auch anders stehenbleiben: eine eigene Regeldatei, die
// jemand im Dateisystem wegraeumt, oder eine Datenbank von einem
// anderen Rechner. Dann steht das Programm mit einer Kennung da, zu
// der es keine Regeln gibt -- und muss trotzdem bedienbar sein.
void TestFensterBeiLeeremLog::hauptfensterMitVerschwundenemContest()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppController controller;
    QVERIFY(controller.openDatabase(dir.filePath(QStringLiteral("verschwunden.sqlite"))));
    ContestSettings settings = controller.settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.activeContestId = QStringLiteral("GIBTESNICHT");
    settings.rigctldHost.clear();
    settings.rotor1Enabled = false;
    settings.rotor2Enabled = false;
    controller.setSettings(settings);
    QVERIFY(controller.findContestDefinition(QStringLiteral("GIBTESNICHT")) == nullptr);

    MainWindow window(controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCoreApplication::processEvents();

    const QPixmap bild = window.grab();
    QVERIFY(!bild.isNull());
    keineUnsinnsBeschriftung(&window, QStringLiteral("MainWindow ohne Regeln"));

    // Und das Loggen: ohne Austauschfelder kann nichts geloggt werden,
    // aber ein Tastendruck darf nicht abstuerzen.
    auto* row = window.findChild<QWidget*>(QLatin1String(UnifiedLogWidget::kEntryRowObjectName));
    QVERIFY(row);
    const QList<QLineEdit*> felder = row->findChildren<QLineEdit*>();
    qInfo().noquote() << "Eingabefelder ohne Regeln:" << felder.size()
                      << "aktiver Contest:" << controller.settings().activeContestId;
    if (!felder.isEmpty()) {
        felder.at(0)->setFocus();
        QTest::keyClicks(felder.at(0), QStringLiteral("DL1ABC"));
        QTest::keyClick(felder.at(0), Qt::Key_Return);
        QCoreApplication::processEvents();
    }
    qInfo().noquote() << "QSOs danach:"
                      << controller.database().qsosForContest(controller.settings().activeContestId).size();
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestFensterBeiLeeremLog tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_fenster_bei_leerem_log.moc"
