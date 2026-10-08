// Martin, 2026-10-08: "die 3 anzeigen vereint in einem window, rechts
// übereinander die rotoren, links so groß wie möglich die karte" --
// und dazu "1:1 die gleichen design" und "keine neues design vom
// ziffernblatt".
//
// Karte und Kompasse teilen sich seitdem ein Panel. Der Prüfstand
// misst, wo die drei Anzeigen wirklich landen: die Karte links und
// breiter als alles andere, die Rotoren rechts übereinander, jeder in
// der Breite, für die sein Zifferblatt gezeichnet ist.

#include <QtTest>

#include <QApplication>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "ui/MainWindow.h"
#include "ui/MapWidget.h"
#include "ui/RotorWidget.h"

#include <memory>

using namespace Contestprogramm;

namespace {

QVector<RotorWidget*> kompasse(MainWindow& window)
{
    return window.findChildren<RotorWidget*>().toVector();
}

} // namespace

class TestRotorAnordnung : public QObject
{
    Q_OBJECT

private slots:
    void karteLinksRotorenRechtsUebereinander();
    void einEinzelnerRotorLaesstDerKarteMehrPlatz();

private:
    std::unique_ptr<AppController> controllerFor(QTemporaryDir& dir, bool zweiterRotor);
};

std::unique_ptr<AppController> TestRotorAnordnung::controllerFor(QTemporaryDir& dir, bool zweiterRotor)
{
    auto controller = std::make_unique<AppController>();
    if (!controller->openDatabase(dir.filePath(QStringLiteral("rotor.sqlite")))) {
        return nullptr;
    }
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.rigctldHost.clear();
    settings.rotor1Enabled = true;
    settings.rotor1Label = QStringLiteral("2m");
    settings.rotor1Host.clear();
    settings.rotor2Enabled = zweiterRotor;
    settings.rotor2Label = QStringLiteral("70cm");
    settings.rotor2Host.clear();
    settings.esmEnabled = false;
    controller->setSettings(settings);
    return controller;
}

void TestRotorAnordnung::karteLinksRotorenRechtsUebereinander()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = controllerFor(dir, true);
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 982);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* karte = window.findChild<MapWidget*>();
    QVERIFY(karte);
    const QVector<RotorWidget*> rotoren = kompasse(window);
    QCOMPARE(rotoren.size(), 2);

    const QRect karteRect(karte->mapTo(&window, QPoint(0, 0)), karte->size());
    const QRect obenRect(rotoren.at(0)->mapTo(&window, QPoint(0, 0)), rotoren.at(0)->size());
    const QRect untenRect(rotoren.at(1)->mapTo(&window, QPoint(0, 0)), rotoren.at(1)->size());
    qInfo() << "Karte" << karteRect << "Rotor oben" << obenRect << "Rotor unten" << untenRect;

    // Die Rotoren stehen übereinander, nicht nebeneinander.
    QCOMPARE(obenRect.left(), untenRect.left());
    QVERIFY2(untenRect.top() >= obenRect.bottom(), "Der zweite Kompass steht nicht unter dem ersten");
    // Beide rechts von der Karte.
    QVERIFY2(obenRect.left() >= karteRect.right(), "Die Kompasse stehen nicht rechts neben der Karte");
    // Und die Karte bekommt, was übrig bleibt -- deutlich mehr als die
    // Spalte: 1440 Fensterbreite minus 300 für die Kompasse.
    QVERIFY2(karteRect.width() > obenRect.width() * 2,
             "Die Karte ist nicht deutlich größer als die Rotorspalte");
    // Das Zifferblatt behält die Breite, für die es gezeichnet ist.
    QCOMPARE(obenRect.width(), 300);
}

void TestRotorAnordnung::einEinzelnerRotorLaesstDerKarteMehrPlatz()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = controllerFor(dir, false);
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 982);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    const QVector<RotorWidget*> rotoren = kompasse(window);
    QCOMPARE(rotoren.size(), 1);
    // Ein Kompass allein nimmt die ganze Höhe der Spalte -- und bleibt
    // sichtbar, statt mit einer leeren zweiten Zelle zu teilen.
    auto* karte = window.findChild<MapWidget*>();
    QVERIFY(karte);
    const QRect rotorRect(rotoren.at(0)->mapTo(&window, QPoint(0, 0)), rotoren.at(0)->size());
    const QRect karteRect(karte->mapTo(&window, QPoint(0, 0)), karte->size());
    qInfo() << "ein Rotor:" << rotorRect << "Karte:" << karteRect;
    QVERIFY(rotoren.at(0)->isVisible());
    QVERIFY(rotorRect.left() >= karteRect.right());
    QVERIFY(rotorRect.height() > 200);
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestRotorAnordnung tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_rotor_anordnung.moc"
