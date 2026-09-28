// Martin, 2026-09-28: "weiters ist das layout auch wieder anders
// geworden" -- das Rotor-Panel stand plötzlich links unten statt
// rechts, obwohl die gespeicherte Lage weiter rechts sagte.
//
// Verdacht: die Panels werden in die Fläche zurückgeklemmt, sobald
// diese "echt" ist -- und echt heißt bisher schon 300×200. Beim Start
// geht das Fenster durch Zwischengrößen, und ein Panel am rechten Rand
// wandert dabei nach links. Der Klemmer holt es nie zurück, die
// verschobene Lage wird gespeichert, und beim nächsten Start ist sie
// die Wahrheit.
//
// Dieser Prüfstand fährt genau das: weite Fläche, Panel rechts,
// Fläche kurz kleiner, Fläche wieder weit.

#include <QtTest>

#include <QApplication>
#include <QResizeEvent>
#include <QTemporaryDir>
#include <QWidget>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"
#include "ui/MainWindow.h"
#include "ui/PanelContainerWidget.h"
#include "ui/PanelLayoutManager.h"

#include <memory>

using namespace Contestprogramm;

namespace {

QWidget* makeContent()
{
    auto* w = new QWidget;
    w->setMinimumSize(80, 40);
    return w;
}

void resizeCanvas(PanelLayoutManager& manager, const QSize& newSize)
{
    const QSize oldSize = manager.canvas()->size();
    manager.canvas()->resize(newSize);
    QResizeEvent event(newSize, oldSize);
    QCoreApplication::sendEvent(manager.canvas(), &event);
}

} // namespace

class TestLayoutBleibt : public QObject
{
    Q_OBJECT

private slots:
    void aPanelAtTheRightEdgeSurvivesASmallerCanvas();
    void theRealWindowKeepsItsPanelsThroughAResize();
};

void TestLayoutBleibt::aPanelAtTheRightEdgeSurvivesASmallerCanvas()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("layout.sqlite")), QStringLiteral("layout_bleibt")));

    QWidget parent;
    PanelLayoutManager manager(db, &parent);
    PanelContainerWidget* rotor = manager.registerPanel(QStringLiteral("rotorrow"), QStringLiteral("Rotoren"),
                                                        makeContent(), false, QRect(1054, 503, 346, 222));
    QVERIFY(rotor);

    // Das Fenster steht: die Lage aus der Datenbank gilt.
    resizeCanvas(manager, QSize(1400, 760));
    QCOMPARE(rotor->geometry(), QRect(1054, 503, 346, 222));

    // Jetzt eine Zwischengröße, wie sie beim Start vorkommt -- klein,
    // aber nach der bisherigen Regel "echt" (>= 300x200).
    resizeCanvas(manager, QSize(900, 600));

    // Und wieder die volle Fläche.
    resizeCanvas(manager, QSize(1400, 760));

    // Hier muss das Panel wieder stehen, wo der Bediener es hingelegt
    // hat. Ohne Gedächtnis für die gewollte Lage bleibt es dort, wohin
    // die Zwischengröße es geschoben hat.
    QCOMPARE(rotor->geometry(), QRect(1054, 503, 346, 222));
}

// Und derselbe Weg am ECHTEN Fenster: MainWindow mit allem, was daran
// haengt -- Profil, Entwurf, Startreihenfolge --, gezeigt, wirklich in
// der Groesse geaendert. Der Prüfstand oben nagelt die Mechanik fest,
// dieser hier beweist, dass sie in der App auch greift.
void TestLayoutBleibt::theRealWindowKeepsItsPanelsThroughAResize()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppController controller;
    QVERIFY(controller.openDatabase(dir.filePath(QStringLiteral("echt.sqlite"))));
    ContestSettings settings = controller.settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.rigctldHost.clear();
    settings.rotor1Enabled = false;
    settings.rotor2Enabled = false;
    controller.setSettings(settings);

    MainWindow window(controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    // Die Entwurfs-Setzzeit (1500 ms) muss GANZ ablaufen: solange sie
    // laeuft, legt eine frische Installation ihre Panels bei jeder
    // Groessenaenderung absichtlich neu nach dem passenden Entwurf --
    // macOS verkleinert ein zu grosses Fenster erst nach show(). Martins
    // Lage ist eine andere: gespeichertes Layout, Fenster laengst offen.
    QTest::qWait(1800);

    auto* rotor = window.findChild<PanelContainerWidget*>(QStringLiteral("rotorrow"));
    QVERIFY2(rotor, "Das Rotor-Panel gibt es nicht");

    // Der Bediener legt es nach rechts unten -- wie in Martins Layout.
    // Aus der echten Flaeche gerechnet, nicht geraten: bei einem
    // 1440x900-Fenster bleiben nur 1372x793 uebrig (Raender, Menue-,
    // Werkzeug- und Statusleiste), und eine Lage, die darueber
    // hinausragt, wird voellig zu Recht geklemmt.
    QWidget* flaeche = rotor->parentWidget();
    QVERIFY(flaeche);
    qInfo() << "Flaeche" << flaeche->size();
    const QRect gewollt(flaeche->width() - 640, flaeche->height() - 320, 620, 300);
    QVERIFY(rotor->trySetGeometry(gewollt));
    const QRect gesetzt = rotor->geometry();
    QCOMPARE(gesetzt, gewollt);

    // Fenster klein, Fenster wieder gross -- genau der Vorgang, bei dem
    // das Panel bisher nach links oben gewandert ist und dort blieb.
    window.resize(1000, 640);
    QTest::qWait(250);
    window.resize(1440, 900);
    QTest::qWait(250);

    qInfo() << "gewollt" << gesetzt << "-- jetzt" << rotor->geometry();
    QCOMPARE(rotor->geometry(), gesetzt);

    // Und noch eine Runde, deutlich kleiner: das Panel darf geklemmt
    // werden, muss danach aber wieder an seinen Platz zurueck.
    window.resize(820, 560);
    QTest::qWait(250);
    window.resize(1440, 900);
    QTest::qWait(250);
    qInfo() << "nach der zweiten Runde" << rotor->geometry();
    QCOMPARE(rotor->geometry(), gesetzt);
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestLayoutBleibt tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_layout_bleibt.moc"
