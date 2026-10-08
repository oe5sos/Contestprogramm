// Martin, 2026-10-08: "vielleicht kann man als option 1-4 rotoren
// rechts neben dem hauptrotor einblenden. sprich übereinander. wenn nur
// 2 angelegt und aktiv sind, dann natürlich nur 2".
//
// Also zwei Anordnungen im Rotoren-Panel: die bisherige Reihe (alle
// gleich groß nebeneinander) und "Hauptrotor groß, weitere rechts
// übereinander". Welche gilt, steht in ContestSettings; umgeschaltet
// wird im Zahnrad rechts oben im Panelkopf.
//
// Der Prüfstand misst, wo die Kompasse wirklich landen -- nicht, welche
// Einstellung gesetzt ist. Mit CP_SHEET_DIR im Environment legt er
// außerdem ein Bild je Anordnung ab (wirkliche Größe), so wie
// test_window_sheet es fürs ganze Fenster macht.

#include <QtTest>

#include <QApplication>
#include <QDir>
#include <QPixmap>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "ui/MainWindow.h"
#include "ui/RotorWidget.h"

using namespace Contestprogramm;

namespace {

QVector<RotorWidget*> kompasse(MainWindow& window)
{
    return window.findChildren<RotorWidget*>().toVector();
}

void blattAblegen(QWidget* widget, const QString& name)
{
    const QByteArray dir = qgetenv("CP_SHEET_DIR");
    if (dir.isEmpty() || !widget) {
        return;
    }
    QDir().mkpath(QString::fromLocal8Bit(dir));
    const QPixmap bild = widget->grab();
    const QString pfad = QString::fromLocal8Bit(dir) + QLatin1Char('/') + name + QStringLiteral(".png");
    bild.save(pfad);
    qInfo().noquote() << "Blatt:" << pfad << bild.size();
}

} // namespace

class TestRotorAnordnung : public QObject
{
    Q_OBJECT

private slots:
    void reiheStelltNebeneinander();
    void hauptrotorStapeltDieUebrigenRechts();
    void einEinzelnerRotorBleibtInDerReihe();

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

void TestRotorAnordnung::reiheStelltNebeneinander()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = controllerFor(dir, true);
    QVERIFY(controller);
    ContestSettings settings = controller->settings();
    settings.rotorPanelLayout = ContestSettings::RotorPanelLayout::Row;
    controller->setSettings(settings);

    MainWindow window(*controller);
    window.resize(1440, 982);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    const QVector<RotorWidget*> rotoren = kompasse(window);
    QCOMPARE(rotoren.size(), 2);
    // Nebeneinander: gleiche Oberkante, verschiedene linke Kanten.
    const QPoint a = rotoren.at(0)->mapTo(&window, QPoint(0, 0));
    const QPoint b = rotoren.at(1)->mapTo(&window, QPoint(0, 0));
    qInfo() << "Reihe: Rotor1" << a << "Rotor2" << b;
    QCOMPARE(a.y(), b.y());
    QVERIFY(a.x() != b.x());
    blattAblegen(rotoren.at(0)->parentWidget(), QStringLiteral("rotoren-reihe"));
}

void TestRotorAnordnung::hauptrotorStapeltDieUebrigenRechts()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = controllerFor(dir, true);
    QVERIFY(controller);
    ContestSettings settings = controller->settings();
    settings.rotorPanelLayout = ContestSettings::RotorPanelLayout::MainPlusColumn;
    settings.mainRotorSlot = 1;
    controller->setSettings(settings);

    MainWindow window(*controller);
    window.resize(1440, 982);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    const QVector<RotorWidget*> rotoren = kompasse(window);
    QCOMPARE(rotoren.size(), 2);
    RotorWidget* haupt = nullptr;
    RotorWidget* neben = nullptr;
    for (RotorWidget* r : rotoren) {
        (r->bandLabel() == QStringLiteral("2m") ? haupt : neben) = r;
    }
    QVERIFY(haupt && neben);
    const QRect hauptRect(haupt->mapTo(&window, QPoint(0, 0)), haupt->size());
    const QRect nebenRect(neben->mapTo(&window, QPoint(0, 0)), neben->size());
    qInfo() << "Hauptrotor" << hauptRect << "daneben" << nebenRect;
    // Rechts daneben, nicht darunter oder darüber.
    QVERIFY2(nebenRect.left() >= hauptRect.right(), "Der zweite Rotor steht nicht rechts vom Hauptrotor");
    // Und der Hauptrotor ist der größere.
    QVERIFY2(hauptRect.width() >= nebenRect.width(), "Der Hauptrotor ist nicht breiter");
    blattAblegen(haupt->parentWidget(), QStringLiteral("rotoren-hauptrotor"));
}

void TestRotorAnordnung::einEinzelnerRotorBleibtInDerReihe()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = controllerFor(dir, false);
    QVERIFY(controller);
    ContestSettings settings = controller->settings();
    settings.rotorPanelLayout = ContestSettings::RotorPanelLayout::MainPlusColumn;
    controller->setSettings(settings);

    MainWindow window(*controller);
    window.resize(1440, 982);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    // "wenn nur 2 angelegt und aktiv sind, dann natürlich nur 2" -- und
    // bei einem einzigen gibt es nichts zu stapeln: er nimmt die ganze
    // Fläche, keine leere Spalte daneben.
    const QVector<RotorWidget*> rotoren = kompasse(window);
    QCOMPARE(rotoren.size(), 1);
    qInfo() << "ein Rotor:" << rotoren.at(0)->size();
    QVERIFY(rotoren.at(0)->isVisible());
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestRotorAnordnung tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_rotor_anordnung.moc"
