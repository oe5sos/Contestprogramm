// Martin, 2026-09-28: "ich möchte wie bei longpath eine leiste haben, wo
// mehrere fenster untergebracht sind, welches ich mit klicken öffne" und
// "mehrere widget in einem fenster".
//
// Dieselbe Bedienung wie Longpaths SideAreaWindow (Zweig
// feature/seitenbereich, dort aus drei Entwürfen Variante 2 gewählt):
// eine schmale Leiste, Klick auf ein inaktives Symbol zeigt dessen
// Panel, Klick auf das aktive klappt zu -- dann bleibt nur die Leiste.
//
// Der Prüfstand fährt genau diese Gesten und sieht dabei nach, dass der
// Inhalt heil bleibt: was in der Eingabezeile steht, steht danach noch
// da.

#include <QtTest>

#include <QApplication>
#include <QLineEdit>
#include <QMenu>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QToolButton>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "ui/MainWindow.h"
#include "ui/PanelContainerWidget.h"
#include "ui/PanelLayoutManager.h"
#include "ui/SideAreaWidget.h"
#include "ui/StyleKit.h"
#include "ui/UnifiedLogWidget.h"

#include <memory>

using namespace Contestprogramm;

class TestSeitenbereich : public QObject
{
    Q_OBJECT

private slots:
    void severalPanelsShareOneWindowAndSwitchOnAClick();
    void clickingTheActiveSymbolCollapsesAndExpandsAgain();
    void takingAPanelOutPutsItBackWhereItCameFrom();
    void theMenuOffersEveryPanel();
    void aPanelInTheSideAreaKeepsWhatWasTypedInIt();
    void clickingTheRailButtonItselfSwitchesThePage();
    void clickingTheRailWithRealPanelsInIt();
    void aPanelPutIntoAHiddenSideAreaDoesNotVanish();
    void pressingTheButtonThroughAccessibilityAlsoSwitches();
    void draggingAPanelOntoTheSideAreaPutsItIn();
    void theSideAreaSurvivesARestart();
    void theActiveRailButtonLooksActive();
    void everyRailButtonCarriesAnIconAndItsName();
    void draggingAPanelOutOfTheRailPutsItBackOnTheCanvas();

private:
    std::unique_ptr<AppController> makeController(QTemporaryDir& dir, const QString& file);
};

std::unique_ptr<AppController> TestSeitenbereich::makeController(QTemporaryDir& dir, const QString& file)
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

void TestSeitenbereich::severalPanelsShareOneWindowAndSwitchOnAClick()
{
    SideAreaWidget bereich;
    bereich.resize(340, 500);
    bereich.show();
    QVERIFY(QTest::qWaitForWindowExposed(&bereich));

    auto* erstes = new QWidget;
    erstes->setObjectName(QStringLiteral("inhalt_a"));
    auto* zweites = new QWidget;
    zweites->setObjectName(QStringLiteral("inhalt_b"));
    auto* drittes = new QWidget;
    drittes->setObjectName(QStringLiteral("inhalt_c"));

    bereich.addPage(QStringLiteral("chat"), QStringLiteral("Chat"), erstes);
    bereich.addPage(QStringLiteral("skeds"), QStringLiteral("Skeds"), zweites);
    bereich.addPage(QStringLiteral("bandmap"), QStringLiteral("Bandmap"), drittes);
    QCoreApplication::processEvents();

    qInfo().noquote() << "in der Leiste:" << bereich.pageIds().join(QStringLiteral(", "))
                      << "| aktiv:" << bereich.activeId();
    QCOMPARE(bereich.pageIds().size(), 3);
    // Das zuletzt Hineingelegte ist das, was man sehen will.
    QCOMPARE(bereich.activeId(), QStringLiteral("bandmap"));

    auto* stack = bereich.findChild<QStackedWidget*>(QLatin1String(SideAreaWidget::kStackObjectName));
    QVERIFY(stack);
    QCOMPARE(stack->currentWidget(), drittes);

    // Ein Klick auf ein anderes Symbol zeigt dessen Panel -- genau ein
    // Widget ist sichtbar, alle liegen in EINEM Fenster.
    auto* chatKnopf = bereich.findChild<QToolButton*>(QStringLiteral("sideRail_chat"));
    QVERIFY2(chatKnopf, "In der Leiste fehlt der Knopf für den Chat");
    chatKnopf->click();
    QCoreApplication::processEvents();
    qInfo().noquote() << "nach dem Klick aktiv:" << bereich.activeId();
    QCOMPARE(bereich.activeId(), QStringLiteral("chat"));
    QCOMPARE(stack->currentWidget(), erstes);
    QVERIFY2(!bereich.isCollapsed(), "Ein Klick auf ein anderes Symbol darf nicht zuklappen");
}

void TestSeitenbereich::clickingTheActiveSymbolCollapsesAndExpandsAgain()
{
    SideAreaWidget bereich;
    bereich.resize(340, 500);
    bereich.show();
    QVERIFY(QTest::qWaitForWindowExposed(&bereich));
    bereich.addPage(QStringLiteral("chat"), QStringLiteral("Chat"), new QWidget);
    bereich.addPage(QStringLiteral("skeds"), QStringLiteral("Skeds"), new QWidget);
    QCoreApplication::processEvents();
    const int breitAufgeklappt = bereich.width();

    // Nochmal auf das AKTIVE Symbol: zuklappen. Longpaths Variante 2.
    QCOMPARE(bereich.activeId(), QStringLiteral("skeds"));
    bereich.railClicked(QStringLiteral("skeds"));
    QCoreApplication::processEvents();
    qInfo() << "zugeklappt:" << bereich.isCollapsed() << "Breite" << bereich.width();
    QVERIFY2(bereich.isCollapsed(), "Der Klick auf das aktive Symbol hat nicht zugeklappt");
    QCOMPARE(bereich.width(), SideAreaWidget::kRailWidth);

    auto* stack = bereich.findChild<QStackedWidget*>(QLatin1String(SideAreaWidget::kStackObjectName));
    QVERIFY(stack);
    QVERIFY2(stack->isHidden(), "Zugeklappt darf der Inhalt nicht mehr dastehen");

    // Und wieder auf: ein Klick auf irgendein Symbol.
    bereich.railClicked(QStringLiteral("chat"));
    QCoreApplication::processEvents();
    qInfo() << "wieder auf:" << !bereich.isCollapsed() << "Breite" << bereich.width()
            << "aktiv" << bereich.activeId();
    QVERIFY2(!bereich.isCollapsed(), "Der Bereich ist nicht wieder aufgegangen");
    QCOMPARE(bereich.activeId(), QStringLiteral("chat"));
    QVERIFY2(!stack->isHidden(), "Der Inhalt fehlt nach dem Aufklappen");
    QVERIFY2(bereich.width() >= breitAufgeklappt - 40,
             qPrintable(QStringLiteral("Nach dem Aufklappen nur %1 breit, vorher %2")
                            .arg(bereich.width())
                            .arg(breitAufgeklappt)));
}

void TestSeitenbereich::takingAPanelOutPutsItBackWhereItCameFrom()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("seite.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* manager = window.findChild<PanelLayoutManager*>();
    QVERIFY(manager);
    auto* bereich = window.findChild<SideAreaWidget*>();
    QVERIFY2(bereich, "Es gibt keinen Seitenbereich im Fenster");

    PanelContainerWidget* bandmap = manager->panel(QStringLiteral("bandmap"));
    QVERIFY(bandmap);
    const QRect zuhause = bandmap->geometry();
    QWidget* flaeche = bandmap->parentWidget();

    QMetaObject::invokeMethod(&window, "putPanelIntoSideArea", Q_ARG(QString, QStringLiteral("bandmap")),
                               Q_ARG(QString, QStringLiteral("Bandmap")));
    QCoreApplication::processEvents();
    qInfo().noquote() << "im Seitenbereich:" << bereich->pageIds().join(QStringLiteral(", "));
    QVERIFY2(bereich->hasPage(QStringLiteral("bandmap")), "Die Bandmap liegt nicht im Seitenbereich");
    QVERIFY2(bandmap->parentWidget() != flaeche, "Die Bandmap hängt noch in der Fläche");

    QMetaObject::invokeMethod(&window, "takePanelOutOfSideArea", Q_ARG(QString, QStringLiteral("bandmap")));
    QCoreApplication::processEvents();
    qInfo().noquote() << "wieder draußen:" << bandmap->geometry() << "war" << zuhause;
    QVERIFY2(!bereich->hasPage(QStringLiteral("bandmap")), "Die Bandmap liegt noch im Seitenbereich");
    QCOMPARE(bandmap->parentWidget(), flaeche);
    QCOMPARE(bandmap->geometry(), zuhause);
}

void TestSeitenbereich::theMenuOffersEveryPanel()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("menue.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* menu = window.findChild<QMenu*>(QStringLiteral("sideAreaMenu"));
    QVERIFY2(menu, "Es gibt kein Menü „In den Seitenbereich“");
    QStringList kennungen;
    QStringList beschriftungen;
    for (QAction* a : menu->actions()) {
        beschriftungen << a->text();
        kennungen << a->objectName();
    }
    qInfo().noquote() << "Im Menü steht:" << beschriftungen.join(QStringLiteral(", "));
    for (const QString& id : {QStringLiteral("chat"), QStringLiteral("skeds"), QStringLiteral("bandmap"),
                               QStringLiteral("checkpartial"), QStringLiteral("ratemeter")}) {
        QVERIFY2(kennungen.contains(QStringLiteral("side_%1").arg(id)),
                 qPrintable(QStringLiteral("Im Menü fehlt: %1").arg(id)));
    }
}

void TestSeitenbereich::aPanelInTheSideAreaKeepsWhatWasTypedInIt()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("inhalt.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* row = window.findChild<QWidget*>(QLatin1String(UnifiedLogWidget::kEntryRowObjectName));
    QVERIFY(row);
    const QList<QLineEdit*> felder = row->findChildren<QLineEdit*>();
    QVERIFY(!felder.isEmpty());
    felder.first()->setFocus();
    QTest::keyClicks(felder.first(), QStringLiteral("OK2XYZ"));
    QCOMPARE(felder.first()->text(), QStringLiteral("OK2XYZ"));

    // Das Log-Panel in den Seitenbereich und wieder heraus -- wer mitten
    // im QSO umräumt, darf seine Eingabe nicht verlieren.
    QMetaObject::invokeMethod(&window, "putPanelIntoSideArea", Q_ARG(QString, QStringLiteral("unifiedlog")),
                               Q_ARG(QString, QStringLiteral("Log")));
    QCoreApplication::processEvents();
    auto* manager = window.findChild<PanelLayoutManager*>();
    QVERIFY(manager);
    PanelContainerWidget* log = manager->panel(QStringLiteral("unifiedlog"));
    QVERIFY(log);
    auto* rowDrin = log->findChild<QWidget*>(QLatin1String(UnifiedLogWidget::kEntryRowObjectName));
    QVERIFY(rowDrin);
    const QList<QLineEdit*> felderDrin = rowDrin->findChildren<QLineEdit*>();
    QVERIFY(!felderDrin.isEmpty());
    qInfo().noquote() << "im Seitenbereich steht im Rufzeichenfeld:" << felderDrin.first()->text();
    QCOMPARE(felderDrin.first()->text(), QStringLiteral("OK2XYZ"));

    QMetaObject::invokeMethod(&window, "takePanelOutOfSideArea", Q_ARG(QString, QStringLiteral("unifiedlog")));
    QCoreApplication::processEvents();
    auto* rowDraussen = window.findChild<QWidget*>(QLatin1String(UnifiedLogWidget::kEntryRowObjectName));
    QVERIFY(rowDraussen);
    const QList<QLineEdit*> felderDraussen = rowDraussen->findChildren<QLineEdit*>();
    QVERIFY(!felderDraussen.isEmpty());
    qInfo().noquote() << "wieder draußen:" << felderDraussen.first()->text();
    QCOMPARE(felderDraussen.first()->text(), QStringLiteral("OK2XYZ"));
}

// Der Weg, den der Bediener wirklich nimmt: ein Klick auf den KNOPF,
// nicht ein Aufruf von railClicked(). Der Unterschied ist nicht
// theoretisch -- live hob ein Klick auf "BA" den Knopf hervor, zeigte
// aber weiter die Skeds. Ursache: die Leiste wurde im Klick-Handler neu
// gebaut, wobei der gerade geklickte Knopf gelöscht wurde. Derselbe
// Fehler wie am selben Tag beim Bandmenü.
//
// Deshalb hier mehrfach hin und her klicken, jedes Mal über den Knopf.
void TestSeitenbereich::clickingTheRailButtonItselfSwitchesThePage()
{
    SideAreaWidget bereich;
    bereich.resize(340, 500);
    bereich.show();
    QVERIFY(QTest::qWaitForWindowExposed(&bereich));

    auto* a = new QWidget;
    auto* b = new QWidget;
    auto* c = new QWidget;
    bereich.addPage(QStringLiteral("bandmap"), QStringLiteral("Bandmap"), a);
    bereich.addPage(QStringLiteral("skeds"), QStringLiteral("Skeds"), b);
    bereich.addPage(QStringLiteral("chat"), QStringLiteral("Chat"), c);
    QCoreApplication::processEvents();

    auto* stack = bereich.findChild<QStackedWidget*>(QLatin1String(SideAreaWidget::kStackObjectName));
    QVERIFY(stack);

    struct Schritt {
        const char* id;
        QWidget* erwartet;
    };
    const QVector<Schritt> schritte{{"bandmap", a}, {"skeds", b}, {"bandmap", a},
                                     {"chat", c},    {"skeds", b}};
    for (const Schritt& schritt : schritte) {
        const QString id = QString::fromLatin1(schritt.id);
        auto* knopf = bereich.findChild<QToolButton*>(QStringLiteral("sideRail_%1").arg(id));
        QVERIFY2(knopf, qPrintable(QStringLiteral("Kein Knopf für %1").arg(id)));
        knopf->click();
        QCoreApplication::processEvents();
        qInfo().noquote() << "Klick auf" << id << "-> aktiv:" << bereich.activeId()
                          << "| gezeigt:" << (stack->currentWidget() == schritt.erwartet ? "richtig"
                                                                                          : "FALSCH");
        QVERIFY2(bereich.activeId() == id,
                 qPrintable(QStringLiteral("Nach dem Klick auf %1 ist %2 aktiv").arg(id, bereich.activeId())));
        QVERIFY2(stack->currentWidget() == schritt.erwartet,
                 qPrintable(QStringLiteral("Nach dem Klick auf %1 steht die falsche Seite da").arg(id)));
        QVERIFY2(!bereich.isCollapsed(),
                 qPrintable(QStringLiteral("Der Klick auf %1 hat zugeklappt").arg(id)));
        // Und der Knopf sieht auch gedrückt aus -- live war er es, ohne
        // dass die Seite wechselte.
        QVERIFY2(knopf->isChecked(), qPrintable(QStringLiteral("Der Knopf %1 sieht nicht gedrückt aus").arg(id)));
        // Und die anderen sehen NICHT gedrückt aus -- sonst sieht man
        // der Leiste nicht an, welche Seite gerade vorne ist.
        for (QToolButton* anderer : bereich.findChildren<QToolButton*>()) {
            if (anderer == knopf) {
                continue;
            }
            QVERIFY2(!anderer->isChecked(),
                     qPrintable(QStringLiteral("Nach dem Klick auf %1 sieht auch %2 gedrückt aus")
                                    .arg(id, anderer->objectName())));
        }
    }

    // Zum Schluss: nochmal auf das aktive, per Knopf -- das klappt zu.
    auto* aktiv = bereich.findChild<QToolButton*>(QStringLiteral("sideRail_skeds"));
    QVERIFY(aktiv);
    aktiv->click();
    QCoreApplication::processEvents();
    qInfo() << "Klick auf das aktive Symbol -> zugeklappt:" << bereich.isCollapsed();
    QVERIFY2(bereich.isCollapsed(), "Der Klick auf das aktive Symbol hat nicht zugeklappt");
}

// Und derselbe Klick mit ECHTEN Panels im Bereich, nicht mit nackten
// QWidgets. Der Unterschied ist nicht theoretisch: live blieb nach dem
// Klick auf "BA" die Skeds-Seite stehen, obwohl der Prüfstand mit
// QWidgets grün war.
void TestSeitenbereich::clickingTheRailWithRealPanelsInIt()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("echt.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* bereich = window.findChild<SideAreaWidget*>();
    QVERIFY(bereich);
    auto* manager = window.findChild<PanelLayoutManager*>();
    QVERIFY(manager);

    QMetaObject::invokeMethod(&window, "putPanelIntoSideArea", Q_ARG(QString, QStringLiteral("bandmap")),
                               Q_ARG(QString, QStringLiteral("Bandmap")));
    QMetaObject::invokeMethod(&window, "putPanelIntoSideArea", Q_ARG(QString, QStringLiteral("skeds")),
                               Q_ARG(QString, QStringLiteral("Skeds")));
    QCoreApplication::processEvents();
    qInfo().noquote() << "im Bereich:" << bereich->pageIds().join(QStringLiteral(", "))
                      << "| aktiv:" << bereich->activeId();
    QCOMPARE(bereich->pageIds().size(), 2);

    auto* stack = bereich->findChild<QStackedWidget*>(QLatin1String(SideAreaWidget::kStackObjectName));
    QVERIFY(stack);
    PanelContainerWidget* bandmap = manager->panel(QStringLiteral("bandmap"));
    PanelContainerWidget* skeds = manager->panel(QStringLiteral("skeds"));
    QVERIFY(bandmap);
    QVERIFY(skeds);

    // Jetzt der Klick auf den Knopf, wie der Bediener ihn macht.
    auto* baKnopf = bereich->findChild<QToolButton*>(QStringLiteral("sideRail_bandmap"));
    QVERIFY2(baKnopf, "Kein Knopf für die Bandmap in der Leiste");
    baKnopf->click();
    QCoreApplication::processEvents();

    qInfo().noquote() << "nach dem Klick aktiv:" << bereich->activeId()
                      << "| im Stapel vorne:"
                      << (stack->currentWidget() == bandmap  ? QStringLiteral("Bandmap")
                          : stack->currentWidget() == skeds ? QStringLiteral("Skeds")
                                                             : QStringLiteral("etwas anderes"))
                      << "| Bandmap sichtbar:" << !bandmap->isHidden()
                      << "| Skeds sichtbar:" << !skeds->isHidden();

    QCOMPARE(bereich->activeId(), QStringLiteral("bandmap"));
    QVERIFY2(stack->currentWidget() == bandmap, "Im Stapel steht nicht die Bandmap vorne");
    // Und das ist das, was man live sieht: das eine Panel steht da, das
    // andere nicht.
    QVERIFY2(!bandmap->isHidden(), "Die Bandmap ist versteckt, obwohl sie aktiv ist");
    QVERIFY2(skeds->isHidden(), "Die Skeds stehen noch da, obwohl die Bandmap aktiv ist");

    // Und jetzt die Zutat, die im Prüfstand fehlte und live immer da
    // ist: eine Größenänderung der Fläche. Sie lässt den Klemmer über
    // alle registrierten Panels laufen -- der fasste dabei auch die im
    // Seitenbereich an und schob das falsche wieder nach vorn.
    window.resize(1300, 820);
    QCoreApplication::processEvents();
    window.resize(1440, 900);
    QCoreApplication::processEvents();

    qInfo().noquote() << "nach zwei Größenänderungen -- aktiv:" << bereich->activeId()
                      << "| Bandmap sichtbar:" << !bandmap->isHidden()
                      << "| Skeds sichtbar:" << !skeds->isHidden()
                      << "| Bandmap-Lage:" << bandmap->geometry();
    QCOMPARE(bereich->activeId(), QStringLiteral("bandmap"));
    QVERIFY2(!bandmap->isHidden(), "Nach der Größenänderung ist die Bandmap verschwunden");
    QVERIFY2(skeds->isHidden(), "Nach der Größenänderung stehen die Skeds wieder obenauf");
    QVERIFY2(stack->currentWidget() == bandmap, "Im Stapel steht nicht mehr die Bandmap vorne");
}

// Live gefunden: Panels, die in den Seitenbereich gelegt wurden, waren
// spurlos weg. Der Bereich hat auf dem 13"-Layout keine Vorgabe, ist
// also anfangs versteckt -- und blieb es, während die Panels
// hineinwanderten. Man sah weder den Bereich noch die Panels.
void TestSeitenbereich::aPanelPutIntoAHiddenSideAreaDoesNotVanish()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("versteckt.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* manager = window.findChild<PanelLayoutManager*>();
    QVERIFY(manager);
    PanelContainerWidget* bereichPanel = manager->panel(QStringLiteral("sidearea"));
    QVERIFY(bereichPanel);

    // Den Ausgangszustand herstellen: der Bereich ist versteckt, wie auf
    // dem 13"-Layout.
    bereichPanel->setVisible(false);
    QCoreApplication::processEvents();
    QVERIFY(bereichPanel->isHidden());

    PanelContainerWidget* bandmap = manager->panel(QStringLiteral("bandmap"));
    QVERIFY(bandmap);
    QMetaObject::invokeMethod(&window, "putPanelIntoSideArea", Q_ARG(QString, QStringLiteral("bandmap")),
                               Q_ARG(QString, QStringLiteral("Bandmap")));
    QCoreApplication::processEvents();

    qInfo() << "Bereich sichtbar:" << !bereichPanel->isHidden()
            << "| Bandmap sichtbar:" << !bandmap->isHidden()
            << "| Bereich-Lage:" << bereichPanel->geometry();

    QVERIFY2(!bereichPanel->isHidden(),
             "Der Seitenbereich ist versteckt geblieben -- das hineingelegte Panel ist damit weg");
    QVERIFY2(!bandmap->isHidden(), "Die Bandmap ist im Seitenbereich verschwunden");
    // Und er steht auf der Fläche, nicht irgendwo daneben.
    QWidget* flaeche = bereichPanel->parentWidget();
    QVERIFY(flaeche);
    QVERIFY2(bereichPanel->geometry().intersects(QRect(QPoint(0, 0), flaeche->size())),
             qPrintable(QStringLiteral("Der Bereich liegt bei %1, die Fläche ist %2 groß")
                            .arg(QString::number(bereichPanel->x()))
                            .arg(QString::number(flaeche->width()))));
}

// Der Weg, den die Bedienungshilfen nehmen -- VoiceOver und jede
// Automatisierung drücken einen ankreuzbaren Knopf, indem sie seinen
// Zustand setzen. Das ergibt toggled, aber KEIN clicked.
//
// Live gefunden 2026-09-28: der Knopf wurde hervorgehoben, die Seite
// wechselte nicht, und im Mitschrieb der laufenden App stand keine
// einzige Zeile -- das Signal kam nie an. Mein Prüfstand mit
// button->click() war grün, weil ein Mausklick BEIDES auslöst.
void TestSeitenbereich::pressingTheButtonThroughAccessibilityAlsoSwitches()
{
    SideAreaWidget bereich;
    bereich.resize(340, 500);
    bereich.show();
    QVERIFY(QTest::qWaitForWindowExposed(&bereich));

    auto* a = new QWidget;
    auto* b = new QWidget;
    bereich.addPage(QStringLiteral("bandmap"), QStringLiteral("Bandmap"), a);
    bereich.addPage(QStringLiteral("skeds"), QStringLiteral("Skeds"), b);
    QCoreApplication::processEvents();
    QCOMPARE(bereich.activeId(), QStringLiteral("skeds"));

    auto* stack = bereich.findChild<QStackedWidget*>(QLatin1String(SideAreaWidget::kStackObjectName));
    QVERIFY(stack);
    auto* baKnopf = bereich.findChild<QToolButton*>(QStringLiteral("sideRail_bandmap"));
    QVERIFY(baKnopf);

    // GENAU das, was die Bedienungshilfen tun: den Zustand setzen.
    // Kein click(), kein Mausereignis.
    baKnopf->setChecked(true);
    QCoreApplication::processEvents();

    qInfo().noquote() << "nach setChecked(true) -- aktiv:" << bereich.activeId()
                      << "| vorne:" << (stack->currentWidget() == a ? "Bandmap" : "Skeds");
    QVERIFY2(bereich.activeId() == QStringLiteral("bandmap"),
             "Über die Bedienungshilfen gedrückt, und nichts ist passiert");
    QCOMPARE(stack->currentWidget(), a);
}

// Martin, 2026-09-28: "karte verbindungen kann ich aber nicht
// reinziehen." Über das Menü ging es schon -- aber ziehen ist der Weg,
// den man erwartet, und den Longpath auch anbietet. Ein Panel, das über
// dem Seitenbereich losgelassen wird, fällt hinein; eines, das woanders
// landet, bleibt wo es ist.
void TestSeitenbereich::draggingAPanelOntoTheSideAreaPutsItIn()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("ziehen.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* manager = window.findChild<PanelLayoutManager*>();
    QVERIFY(manager);
    auto* bereich = window.findChild<SideAreaWidget*>();
    QVERIFY(bereich);
    PanelContainerWidget* bereichPanel = manager->panel(QStringLiteral("sidearea"));
    QVERIFY(bereichPanel);
    // Der Bereich muss dastehen, sonst fängt er nichts auf.
    manager->revealPanel(QStringLiteral("sidearea"));
    QCoreApplication::processEvents();

    PanelContainerWidget* karte = manager->panel(QStringLiteral("map"));
    QVERIFY(karte);
    QVERIFY(!bereich->hasPage(QStringLiteral("map")));

    // Daneben losgelassen: nichts passiert.
    const QPoint daneben = window.mapToGlobal(QPoint(10, 400));
    QMetaObject::invokeMethod(&window, "dropPanelIfOverSideArea", Q_ARG(QString, QStringLiteral("map")),
                               Q_ARG(QPoint, daneben));
    QCoreApplication::processEvents();
    qInfo().noquote() << "daneben losgelassen -- im Bereich:"
                      << (bereich->hasPage(QStringLiteral("map")) ? "ja" : "nein");
    QVERIFY2(!bereich->hasPage(QStringLiteral("map")),
             "Ein Panel, das NEBEN dem Bereich landet, darf nicht hineinfallen");

    // Mitten auf dem Bereich losgelassen: hinein.
    const QPoint mittendrin =
        bereichPanel->mapToGlobal(QPoint(bereichPanel->width() / 2, bereichPanel->height() / 2));
    QMetaObject::invokeMethod(&window, "dropPanelIfOverSideArea", Q_ARG(QString, QStringLiteral("map")),
                               Q_ARG(QPoint, mittendrin));
    QCoreApplication::processEvents();
    qInfo().noquote() << "auf dem Bereich losgelassen -- im Bereich:"
                      << bereich->pageIds().join(QStringLiteral(", "))
                      << "| aktiv:" << bereich->activeId();
    QVERIFY2(bereich->hasPage(QStringLiteral("map")), "Die Karte ist nicht in den Seitenbereich gefallen");
    QCOMPARE(bereich->activeId(), QStringLiteral("map"));

    // Und in der Leiste steht ihr Kürzel, nicht die interne Kennung.
    auto* knopf = bereich->findChild<QToolButton*>(QStringLiteral("sideRail_map"));
    QVERIFY(knopf);
    qInfo().noquote() << "Kürzel in der Leiste:" << knopf->text() << "| Tooltip:" << knopf->toolTip();
    QCOMPARE(knopf->toolTip(), QStringLiteral("Karte / Verbindungen"));
}

// Beim Neustart lag der Bereich wieder leer da -- man hätte Chat,
// Skeds und Karte jedes Mal von Hand hineinlegen müssen, also genau
// die Handgriffe, die er abnehmen soll. Aufgefallen beim Live-Test am
// 2026-09-28: frische Instanz, leerer Bereich.
void TestSeitenbereich::theSideAreaSurvivesARestart()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QStringList seitenVorher;
    {
        auto controller = makeController(dir, QStringLiteral("neustart.sqlite"));
        QVERIFY(controller);
        MainWindow window(*controller);
        window.resize(1440, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        auto* bereich = window.findChild<SideAreaWidget*>();
        QVERIFY(bereich);
        QMetaObject::invokeMethod(&window, "putPanelIntoSideArea", Q_ARG(QString, QStringLiteral("chat")),
                                   Q_ARG(QString, QStringLiteral("Chat")));
        QMetaObject::invokeMethod(&window, "putPanelIntoSideArea", Q_ARG(QString, QStringLiteral("skeds")),
                                   Q_ARG(QString, QStringLiteral("Skeds")));
        bereich->setActive(QStringLiteral("chat"));
        seitenVorher = bereich->pageIds();
        qInfo().noquote() << "vor dem Neustart:" << seitenVorher.join(QStringLiteral(", "))
                          << "| aktiv:" << bereich->activeId();
        QCoreApplication::processEvents();
    }

    // Zweiter Start auf derselben Datenbank -- wie nach Beenden und
    // wieder Aufsperren.
    {
        auto controller = makeController(dir, QStringLiteral("neustart.sqlite"));
        QVERIFY(controller);
        MainWindow window(*controller);
        window.resize(1440, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        auto* bereich = window.findChild<SideAreaWidget*>();
        QVERIFY(bereich);
        qInfo().noquote() << "nach dem Neustart:" << bereich->pageIds().join(QStringLiteral(", "))
                          << "| aktiv:" << bereich->activeId();
        QCOMPARE(bereich->pageIds(), seitenVorher);
        QCOMPARE(bereich->activeId(), QStringLiteral("chat"));
        // Und der Bereich selbst steht da, sonst läge alles im Verborgenen.
        auto* manager = window.findChild<PanelLayoutManager*>();
        QVERIFY(manager);
        PanelContainerWidget* bereichPanel = manager->panel(QStringLiteral("sidearea"));
        QVERIFY(bereichPanel);
        QVERIFY2(!bereichPanel->isHidden(), "Der Seitenbereich ist nach dem Neustart versteckt");
    }
}

// Martin, 2026-09-28: "wird nicht übernommen" -- zwei Bilder, auf
// denen verschiedene Seiten vorne lagen und in der Leiste trotzdem
// immer dasselbe Kürzel hell wirkte. Der Zustand stimmte (isChecked),
// nur SAH man ihm nichts an: für QToolButton gab es keine Stilregel,
// ein flacher Knopf im dunklen Thema sieht gedrückt aus wie nicht
// gedrückt. Dieser Prüfstand hält beides fest -- den Zustand und, mit
// CP_LEISTE_BILD=<pfad>, ein Bild der Leiste zum Ansehen.
void TestSeitenbereich::theActiveRailButtonLooksActive()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("leiste.sqlite"));
    QVERIFY(controller);

    // Auf derselben Bühne wie das laufende Programm: main.cpp setzt
    // dieses Stylesheet, und genau darin fehlte die Regel. Ohne diese
    // Zeile prüfte man den nackten Standardstil, der den gedrückten
    // Knopf von sich aus zeichnet -- der Prüfstand wäre grün und das
    // Programm trotzdem falsch.
    qApp->setStyleSheet(Style::appStyleSheet());

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* bereich = window.findChild<SideAreaWidget*>();
    QVERIFY(bereich);
    for (const auto& paar : {std::pair<QString, QString>{QStringLiteral("ratemeter"), QStringLiteral("Rate")},
                              {QStringLiteral("chat"), QStringLiteral("Chat")},
                              {QStringLiteral("skeds"), QStringLiteral("Skeds")},
                              {QStringLiteral("map"), QStringLiteral("Karte / Verbindungen")}}) {
        QMetaObject::invokeMethod(&window, "putPanelIntoSideArea", Q_ARG(QString, paar.first),
                                   Q_ARG(QString, paar.second));
    }
    bereich->setActive(QStringLiteral("map"));
    QCoreApplication::processEvents();

    auto* rail = bereich->findChild<QWidget*>(QLatin1String(SideAreaWidget::kRailObjectName));
    QVERIFY(rail);

    // Der Zustand: genau einer ist gedrückt, und zwar der aktive.
    QStringList gedrueckt;
    for (QToolButton* knopf : rail->findChildren<QToolButton*>()) {
        if (knopf->isChecked()) {
            gedrueckt << knopf->objectName();
        }
    }
    qInfo().noquote() << "gedrückt:" << gedrueckt.join(QStringLiteral(", "));
    QCOMPARE(gedrueckt, QStringList{QStringLiteral("sideRail_map")});

    // Und das Aussehen: der gedrückte Knopf muss sich vom Nachbarn
    // unterscheiden, sonst sieht man die aktive Seite nicht.
    auto* aktiv = rail->findChild<QToolButton*>(QStringLiteral("sideRail_map"));
    auto* still = rail->findChild<QToolButton*>(QStringLiteral("sideRail_chat"));
    QVERIFY(aktiv && still);
    const QImage bildAktiv = aktiv->grab().toImage();
    const QImage bildStill = still->grab().toImage();
    QVERIFY(!bildAktiv.isNull() && !bildStill.isNull());
    QVERIFY2(bildAktiv.size() == bildStill.size(), "gleich große Knöpfe erwartet");
    // Gemessen wird nicht "irgendwie anders" -- ein Pixelvergleich ist
    // schon durch das Kürzel selbst erfüllt und war in der Gegenprobe
    // auch ohne Regel bei 99 %. Verlangt wird die Akzentfarbe: der
    // Balken am linken Rand des aktiven Knopfes.
    const QColor akzent(Style::kBlueBg());
    auto akzentAnteilAmRand = [&akzent](const QImage& bild) {
        int treffer = 0;
        int gezaehlt = 0;
        for (int y = 0; y < bild.height(); ++y) {
            for (int x = 0; x < std::min(3, bild.width()); ++x) {
                const QColor farbe(bild.pixel(x, y));
                ++gezaehlt;
                if (std::abs(farbe.red() - akzent.red()) < 40 && std::abs(farbe.green() - akzent.green()) < 40
                    && std::abs(farbe.blue() - akzent.blue()) < 40) {
                    ++treffer;
                }
            }
        }
        return gezaehlt > 0 ? double(treffer) / double(gezaehlt) : 0.0;
    };
    const double amAktiven = akzentAnteilAmRand(bildAktiv);
    const double amStillen = akzentAnteilAmRand(bildStill);
    qInfo().noquote() << "Akzent am linken Rand -- aktiv:" << QString::number(amAktiven * 100.0, 'f', 0)
                      << "% still:" << QString::number(amStillen * 100.0, 'f', 0) << "%";
    QVERIFY2(amAktiven > 0.5, "Der aktive Knopf trägt keinen Akzentbalken -- die aktive Seite ist nicht erkennbar");
    QVERIFY2(amStillen < 0.1, "Auch der stille Knopf trägt den Balken");

    const QByteArray ziel = qgetenv("CP_LEISTE_BILD");
    if (!ziel.isEmpty()) {
        QVERIFY(bereich->grab().save(QString::fromLocal8Bit(ziel)));
        qInfo().noquote() << "Bild abgelegt:" << QString::fromLocal8Bit(ziel);
    }
}

// Martin, 2026-09-28: "schön wäre, wenn wir vielleicht icons dazu
// hätten" -- aus drei Blättern hat er C gewählt: Symbol UND Name
// nebeneinander, Leiste 150 px. Beides muss ankommen; ein Knopf ohne
// Symbol fiele in der Reihe sofort auf, einer ohne Namen wäre die
// Fassung, die er nicht wollte.
void TestSeitenbereich::everyRailButtonCarriesAnIconAndItsName()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("symbole.sqlite"));
    QVERIFY(controller);

    qApp->setStyleSheet(Style::appStyleSheet());
    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* bereich = window.findChild<SideAreaWidget*>();
    QVERIFY(bereich);
    const QList<std::pair<QString, QString>> seiten = {
        {QStringLiteral("unifiedlog"), QStringLiteral("Log")},
        {QStringLiteral("rotorrow"), QStringLiteral("Rotoren")},
        {QStringLiteral("map"), QStringLiteral("Karte / Verbindungen")},
        {QStringLiteral("suggestion"), QStringLiteral("Nächstes Ziel")},
        {QStringLiteral("ratemeter"), QStringLiteral("Rate")},
        {QStringLiteral("checkpartial"), QStringLiteral("Check")},
        {QStringLiteral("bandmap"), QStringLiteral("Bandmap")},
        {QStringLiteral("skeds"), QStringLiteral("Skeds")},
        {QStringLiteral("chat"), QStringLiteral("Chat")},
    };
    for (const auto& seite : seiten) {
        QMetaObject::invokeMethod(&window, "putPanelIntoSideArea", Q_ARG(QString, seite.first),
                                   Q_ARG(QString, seite.second));
    }
    QCoreApplication::processEvents();

    for (const auto& seite : seiten) {
        auto* knopf = bereich->findChild<QToolButton*>(QStringLiteral("sideRail_%1").arg(seite.first));
        QVERIFY2(knopf, qPrintable(QStringLiteral("kein Leistenknopf für %1").arg(seite.first)));
        QVERIFY2(!knopf->icon().isNull(), qPrintable(QStringLiteral("%1 hat kein Symbol").arg(seite.first)));
        // Das Symbol darf nicht leer gezeichnet sein -- ein QIcon mit
        // einer durchsichtigen Fläche ist nicht null und sähe im
        // Prüfstand richtig aus.
        const QImage bild = knopf->icon().pixmap(17, 17, QIcon::Normal, QIcon::Off).toImage();
        int gesetzt = 0;
        for (int y = 0; y < bild.height(); ++y) {
            for (int x = 0; x < bild.width(); ++x) {
                if (qAlpha(bild.pixel(x, y)) > 30) {
                    ++gesetzt;
                }
            }
        }
        QVERIFY2(gesetzt > 10, qPrintable(QStringLiteral("%1: Symbol ist leer").arg(seite.first)));
        // Und der Name -- gekürzt, aber erkennbar: der Anfang steht da.
        const QString text = knopf->text();
        QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("%1 hat keinen Namen").arg(seite.first)));
        QVERIFY2(seite.second.startsWith(text.left(4)),
                 qPrintable(QStringLiteral("%1: Name '%2' passt nicht zu '%3'")
                                .arg(seite.first, text, seite.second)));
        QCOMPARE(knopf->toolTip(), seite.second);
    }
    qInfo().noquote() << "neun Knöpfe mit Symbol und Namen, Leiste"
                      << SideAreaWidget::kRailWidth << "px";
}

// Martin, 2026-09-28: "die widgets sollte man aber auch wieder per
// drag and drop rausziehen können, in dem fall nach rechts." Hinein
// ging es längst durch Ziehen, hinaus nur per Rechtsklick -- und den
// findet man nicht von selbst.
void TestSeitenbereich::draggingAPanelOutOfTheRailPutsItBackOnTheCanvas()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("rausziehen.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* manager = window.findChild<PanelLayoutManager*>();
    QVERIFY(manager);
    auto* bereich = window.findChild<SideAreaWidget*>();
    QVERIFY(bereich);
    // Gesperrt hineinlegen -- so fährt Martin sein Layout, und genau
    // daran ist das Herausziehen live gescheitert: trySetGeometry()
    // weist ein gesperrtes Panel ab, es landete an seinem alten Platz
    // statt dort, wo losgelassen wurde.
    if (PanelContainerWidget* vorher = manager->panel(QStringLiteral("map"))) {
        vorher->setLocked(true);
    }
    QMetaObject::invokeMethod(&window, "putPanelIntoSideArea", Q_ARG(QString, QStringLiteral("map")),
                               Q_ARG(QString, QStringLiteral("Karte / Verbindungen")));
    QCoreApplication::processEvents();
    QVERIFY(bereich->hasPage(QStringLiteral("map")));

    PanelContainerWidget* bereichPanel = manager->panel(QStringLiteral("sidearea"));
    QVERIFY(bereichPanel);

    // Innerhalb des Bereichs losgelassen: das bleibt drin. Ein
    // Rutscher beim Umschalten darf das Panel nicht herausreißen.
    const QPoint drinnen =
        bereichPanel->mapToGlobal(QPoint(bereichPanel->width() / 2, bereichPanel->height() / 2));
    QMetaObject::invokeMethod(&window, "dragPanelOutOfSideArea", Q_ARG(QString, QStringLiteral("map")),
                               Q_ARG(QPoint, drinnen));
    QCoreApplication::processEvents();
    qInfo().noquote() << "im Bereich losgelassen -- noch drin:"
                      << (bereich->hasPage(QStringLiteral("map")) ? "ja" : "nein");
    QVERIFY2(bereich->hasPage(QStringLiteral("map")), "Ein Rutscher im Bereich hat das Panel herausgerissen");

    // Nach rechts herausgezogen: liegt wieder auf der Fläche, und zwar
    // dort, wo losgelassen wurde.
    QWidget* flaeche = manager->canvas();
    QVERIFY(flaeche);
    const QPoint zielAufDerFlaeche(900, 300);
    const QPoint zielGlobal = flaeche->mapToGlobal(zielAufDerFlaeche);
    QMetaObject::invokeMethod(&window, "dragPanelOutOfSideArea", Q_ARG(QString, QStringLiteral("map")),
                               Q_ARG(QPoint, zielGlobal));
    QCoreApplication::processEvents();

    QVERIFY2(!bereich->hasPage(QStringLiteral("map")), "Die Karte ist nicht aus dem Bereich herausgekommen");
    PanelContainerWidget* karte = manager->panel(QStringLiteral("map"));
    QVERIFY(karte);
    QCOMPARE(karte->parentWidget(), flaeche);
    QVERIFY2(!karte->isHidden(), "Die Karte ist unsichtbar wieder aufgetaucht");
    qInfo().noquote() << "herausgezogen nach" << karte->geometry() << "-- Ziel war" << zielAufDerFlaeche;
    // Der Griff sitzt links oben am Kopf, also ein paar Pixel neben dem
    // Zeiger; genau darauf prüfen wäre spröde, in der Nähe genügt.
    // Und: ein breites Panel ganz rechts abgelegt wird auf die Fläche
    // zurückgeschoben, sonst hinge die Hälfte draußen -- das ist
    // richtig so und gehört in die Erwartung.
    const int passtNochX = std::max(0, flaeche->width() - karte->width());
    const int erwartetX = std::min(zielAufDerFlaeche.x() - 20, passtNochX);
    QVERIFY2(std::abs(karte->x() - erwartetX) <= 40,
             qPrintable(QStringLiteral("Die Karte liegt bei x=%1, erwartet war %2")
                            .arg(karte->x()).arg(erwartetX)));
    QVERIFY2(std::abs(karte->y() - (zielAufDerFlaeche.y() - 10)) <= 40,
             "Die Karte liegt nicht dort, wo losgelassen wurde");
    // Und das Schloss ist danach wieder zu: der eine Handgriff ging
    // durch, die Sperre bleibt.
    QVERIFY2(karte->isLocked(), "Das Panel ist nach dem Herausziehen nicht mehr gesperrt");

    // Dasselbe mit der Rotorreihe: sie hat ein eigenes Layoutgesetz
    // (reflowRotorRowForCanvasWidth stellt sie in schmalen Fenstern
    // mittig) -- live sah es aus, als rutsche sie nach dem
    // Herausziehen wieder nach links.
    QMetaObject::invokeMethod(&window, "putPanelIntoSideArea", Q_ARG(QString, QStringLiteral("rotorrow")),
                               Q_ARG(QString, QStringLiteral("Rotoren")));
    QCoreApplication::processEvents();
    QVERIFY(bereich->hasPage(QStringLiteral("rotorrow")));
    const QPoint zielRotoren = flaeche->mapToGlobal(QPoint(700, 420));
    QMetaObject::invokeMethod(&window, "dragPanelOutOfSideArea", Q_ARG(QString, QStringLiteral("rotorrow")),
                               Q_ARG(QPoint, zielRotoren));
    QCoreApplication::processEvents();
    PanelContainerWidget* rotoren = manager->panel(QStringLiteral("rotorrow"));
    QVERIFY(rotoren);
    qInfo().noquote() << "Rotoren herausgezogen nach" << rotoren->geometry()
                      << "-- Fläche" << flaeche->size();
    const int passtRotoren = std::max(0, flaeche->width() - rotoren->width());
    QVERIFY2(std::abs(rotoren->x() - std::min(700 - 20, passtRotoren)) <= 40,
             qPrintable(QStringLiteral("Rotoren liegen bei x=%1").arg(rotoren->x())));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestSeitenbereich tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_seitenbereich.moc"
