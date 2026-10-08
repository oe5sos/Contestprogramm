// Martin, 2026-09-28: "schön wäre es, ein windows zu haben, wo ich alle
// windows auch aber auch rausziehen kann um platz zu sparen. das haben
// wir bei longpath auch erledigt." Dazu das Bild von Longpath mit dem
// Menü "Containers".
//
// Also dasselbe hier: jedes Panel lässt sich als eigenes Fenster
// ablösen, auf einen zweiten Schirm schieben und wieder andocken. Der
// Rückweg geht auch über das ✕ des Fensters -- ein Panel, das sich
// wegklicken lässt und dann nirgends mehr steht, wäre eine Falle.
//
// Dieser Prüfstand fährt beides und sieht nach, dass der Inhalt dabei
// heil bleibt: was in der Eingabezeile steht, steht danach noch da.

#include <QtTest>

#include <QApplication>
#include <QLineEdit>
#include <QMenu>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"
#include "ui/MainWindow.h"
#include "ui/PanelContainerWidget.h"
#include "ui/PanelLayoutManager.h"
#include "ui/UnifiedLogWidget.h"

#include <memory>

using namespace Contestprogramm;

class TestPanelAbloesen : public QObject
{
    Q_OBJECT

private slots:
    void aPanelBecomesItsOwnWindowAndComesBack();
    void closingTheWindowDocksItInsteadOfLosingIt();
    void theContentSurvivesTheTrip();
    void theMenuOffersEveryPanel();
    void aDetachedPanelComesBackDetachedAfterARestart();

private:
    std::unique_ptr<AppController> makeController(QTemporaryDir& dir, const QString& file);
};

std::unique_ptr<AppController> TestPanelAbloesen::makeController(QTemporaryDir& dir, const QString& file)
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
    controller->setSettings(settings);
    return controller;
}

void TestPanelAbloesen::aPanelBecomesItsOwnWindowAndComesBack()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("abloesen.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* manager = window.findChild<PanelLayoutManager*>();
    QVERIFY(manager);
    PanelContainerWidget* bandmap = manager->panel(QStringLiteral("bandmap"));
    QVERIFY2(bandmap, "Das Bandmap-Panel gibt es nicht");

    // Angedockt: es steckt in der Fläche und ist kein eigenes Fenster.
    QVERIFY(!bandmap->isFloating());
    QVERIFY2(bandmap->parentWidget() != nullptr, "Ein angedocktes Panel hat keinen Elternteil");
    const QRect inDerFlaeche = bandmap->geometry();

    // Ablösen.
    manager->setPanelFloating(QStringLiteral("bandmap"), true);
    QCoreApplication::processEvents();
    QVERIFY2(bandmap->isFloating(), "Das Panel wurde nicht abgelöst");
    QVERIFY2(bandmap->isWindow(), "Das abgelöste Panel ist kein eigenes Fenster");
    QVERIFY2(bandmap->parentWidget() == nullptr, "Das abgelöste Panel hängt noch in der Fläche");
    qInfo().noquote() << "abgelöst:" << bandmap->windowTitle() << bandmap->geometry();
    // Klartext, nicht die interne Kennung -- live sofort aufgefallen:
    // im Fenstertitel stand "bandmap".
    QCOMPARE(bandmap->windowTitle(), QStringLiteral("Bandmap"));

    // Auf einen anderen Platz schieben -- das ist der Sinn der Sache.
    bandmap->move(200, 150);
    QCoreApplication::processEvents();

    // Und wieder andocken: zurück an seinen Platz in der Fläche.
    manager->setPanelFloating(QStringLiteral("bandmap"), false);
    QCoreApplication::processEvents();
    QVERIFY2(!bandmap->isFloating(), "Das Panel ist noch abgelöst");
    QVERIFY2(bandmap->parentWidget() != nullptr, "Das angedockte Panel hat keinen Elternteil");
    qInfo().noquote() << "angedockt:" << bandmap->geometry() << "war" << inDerFlaeche;
    QCOMPARE(bandmap->geometry(), inDerFlaeche);
}

void TestPanelAbloesen::closingTheWindowDocksItInsteadOfLosingIt()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("zumachen.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* manager = window.findChild<PanelLayoutManager*>();
    QVERIFY(manager);
    PanelContainerWidget* skeds = manager->panel(QStringLiteral("skeds"));
    QVERIFY(skeds);

    manager->setPanelFloating(QStringLiteral("skeds"), true);
    QCoreApplication::processEvents();
    QVERIFY(skeds->isFloating());

    // Das ✕ des Fensters.
    skeds->close();
    QCoreApplication::processEvents();

    qInfo() << "nach dem Zumachen -- abgelöst:" << skeds->isFloating()
            << "sichtbar:" << !skeds->isHidden() << "Elternteil:" << (skeds->parentWidget() != nullptr);
    QVERIFY2(!skeds->isFloating(), "Das Fenster wurde geschlossen statt angedockt");
    QVERIFY2(skeds->parentWidget() != nullptr, "Das Panel hängt nirgends mehr");
    QVERIFY2(!skeds->isHidden(), "Das Panel ist verschwunden -- man findet es nicht wieder");
}

void TestPanelAbloesen::theContentSurvivesTheTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("inhalt.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* manager = window.findChild<PanelLayoutManager*>();
    QVERIFY(manager);

    // In die Eingabezeile des Logs tippen -- und dann das Log-Panel
    // ablösen. Wer mitten im QSO ein Panel verschiebt, darf seine
    // Eingabe nicht verlieren.
    auto* row = window.findChild<QWidget*>(QLatin1String(UnifiedLogWidget::kEntryRowObjectName));
    QVERIFY(row);
    const QList<QLineEdit*> felder = row->findChildren<QLineEdit*>();
    QVERIFY(!felder.isEmpty());
    felder.first()->setFocus();
    QTest::keyClicks(felder.first(), QStringLiteral("DL1ABC"));
    QCOMPARE(felder.first()->text(), QStringLiteral("DL1ABC"));

    manager->setPanelFloating(QStringLiteral("unifiedlog"), true);
    QCoreApplication::processEvents();

    // Die Felder neu suchen -- das Panel hat den Elternteil gewechselt.
    auto* rowDanach = window.findChild<QWidget*>(QLatin1String(UnifiedLogWidget::kEntryRowObjectName));
    if (!rowDanach) {
        // Nach dem Ablösen hängt das Panel nicht mehr unter dem Fenster:
        // dann über das Panel selbst suchen.
        PanelContainerWidget* log = manager->panel(QStringLiteral("unifiedlog"));
        QVERIFY(log);
        rowDanach = log->findChild<QWidget*>(QLatin1String(UnifiedLogWidget::kEntryRowObjectName));
    }
    QVERIFY(rowDanach);
    const QList<QLineEdit*> felderDanach = rowDanach->findChildren<QLineEdit*>();
    QVERIFY(!felderDanach.isEmpty());
    qInfo().noquote() << "nach dem Ablösen steht im Rufzeichenfeld:" << felderDanach.first()->text();
    QVERIFY2(felderDanach.first()->text() == QStringLiteral("DL1ABC"),
             "Die begonnene Eingabe ist beim Ablösen verloren gegangen");

    manager->setPanelFloating(QStringLiteral("unifiedlog"), false);
    QCoreApplication::processEvents();
    auto* rowZurueck = window.findChild<QWidget*>(QLatin1String(UnifiedLogWidget::kEntryRowObjectName));
    QVERIFY(rowZurueck);
    const QList<QLineEdit*> felderZurueck = rowZurueck->findChildren<QLineEdit*>();
    QVERIFY(!felderZurueck.isEmpty());
    qInfo().noquote() << "nach dem Andocken:" << felderZurueck.first()->text();
    QVERIFY2(felderZurueck.first()->text() == QStringLiteral("DL1ABC"),
             "Die begonnene Eingabe ist beim Andocken verloren gegangen");
}

void TestPanelAbloesen::theMenuOffersEveryPanel()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("menue.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* menu = window.findChild<QMenu*>(QStringLiteral("floatPanelsMenu"));
    QVERIFY2(menu, "Es gibt kein Menü „Als eigenes Fenster“");

    QStringList kennungen;
    QStringList beschriftungen;
    for (QAction* a : menu->actions()) {
        if (a->isSeparator()) {
            continue;
        }
        beschriftungen << a->text();
        if (!a->objectName().isEmpty()) {
            kennungen << a->objectName();
        }
    }
    qInfo().noquote() << "Im Menü steht:" << beschriftungen.join(QStringLiteral(", "));

    for (const QString& id : {QStringLiteral("unifiedlog"), QStringLiteral("rotorrow"), QStringLiteral("map"),
                               QStringLiteral("suggestion"), QStringLiteral("ratemeter"),
                               QStringLiteral("checkpartial"), QStringLiteral("bandmap"),
                               QStringLiteral("skeds"), QStringLiteral("chat")}) {
        QVERIFY2(kennungen.contains(QStringLiteral("float_%1").arg(id)),
                 qPrintable(QStringLiteral("Im Menü fehlt: %1").arg(id)));
    }
    QVERIFY(kennungen.contains(QStringLiteral("floatAllPanels")));
    QVERIFY(kennungen.contains(QStringLiteral("dockAllPanels")));
}

// Wer ein Panel auf den zweiten Bildschirm legt, will es dort beim
// nächsten Start wiederfinden.
void TestPanelAbloesen::aDetachedPanelComesBackDetachedAfterARestart()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("neustart.sqlite"));
    QVERIFY(controller);

    QRect fensterLage;
    {
        MainWindow window(*controller);
        window.resize(1440, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* manager = window.findChild<PanelLayoutManager*>();
        QVERIFY(manager);

        manager->setPanelFloating(QStringLiteral("bandmap"), true);
        QCoreApplication::processEvents();
        PanelContainerWidget* bandmap = manager->panel(QStringLiteral("bandmap"));
        QVERIFY(bandmap);
        QVERIFY(bandmap->isFloating());
        bandmap->setGeometry(QRect(300, 220, 400, 320));
        QCoreApplication::processEvents();
        fensterLage = bandmap->geometry();
        qInfo().noquote() << "abgelegt bei" << fensterLage;
        // Das Ablegen selbst merkt sich der Verwalter erst beim
        // nächsten Speichern -- hier von Hand anstoßen, wie es ein
        // Verschieben mit der Maus täte.
        manager->saveLayout(QStringLiteral("bandmap"));
    }

    // Neues Fenster, dieselbe Datenbank.
    {
        MainWindow window(*controller);
        window.resize(1440, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* manager = window.findChild<PanelLayoutManager*>();
        QVERIFY(manager);
        PanelContainerWidget* bandmap = manager->panel(QStringLiteral("bandmap"));
        QVERIFY(bandmap);
        qInfo().noquote() << "nach dem Neustart abgelöst:" << bandmap->isFloating() << bandmap->geometry();
        QVERIFY2(bandmap->isFloating(), "Das abgelöste Panel ist wieder angedockt");
        QCOMPARE(bandmap->geometry(), fensterLage);
    }
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestPanelAbloesen tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_panel_abloesen.moc"
