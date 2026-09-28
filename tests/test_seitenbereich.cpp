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

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestSeitenbereich tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_seitenbereich.moc"
