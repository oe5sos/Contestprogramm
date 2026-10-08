// Martin, 2026-10-08: "kannst du hier einen dritten Rotor dazu
// designen. diese antenne wird aber als fixantenne gesehen, hat also
// keinen rotor. sollte nur das gleiche design haben. benenne sie als
// Antenne Fix".
//
// Dasselbe Instrument, aber nichts dreht sich: die Nadel steht auf der
// eingestellten Richtung, es gibt kein Ziel, ein Klick ins Zifferblatt
// verstellt nichts, und in der Fußzeile steht die feste Richtung statt
// "verbunden/getrennt" -- eine Aussage über eine Steuerleitung, die es
// hier gar nicht gibt.
//
// Mit CP_SHEET_DIR im Environment legt der Prüfstand ein Blatt ab.

#include <QtTest>

#include <QApplication>
#include <QDir>
#include <QPainter>
#include <QPixmap>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "ui/MainWindow.h"
#include "ui/RotorWidget.h"

#include <memory>

using namespace Contestprogramm;

namespace {

RotorWidget* kompassMit(MainWindow& window, const QString& label)
{
    for (RotorWidget* w : window.findChildren<RotorWidget*>()) {
        if (w->bandLabel() == label) {
            return w;
        }
    }
    return nullptr;
}

std::unique_ptr<AppController> controllerFor(QTemporaryDir& dir, bool festeAntenne)
{
    auto controller = std::make_unique<AppController>();
    if (!controller->openDatabase(dir.filePath(QStringLiteral("fix.sqlite")))) {
        return nullptr;
    }
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.rigctldHost.clear();
    settings.rotor1Enabled = true;
    settings.rotor1Label = QStringLiteral("2m");
    settings.rotor1Host.clear();
    settings.rotor2Enabled = true;
    settings.rotor2Label = QStringLiteral("70cm");
    settings.rotor2Host.clear();
    settings.fixedAntennaEnabled = festeAntenne;
    settings.fixedAntennaLabel = QStringLiteral("Antenne Fix");
    settings.fixedAntennaBearingDeg = 315.0;
    settings.esmEnabled = false;
    controller->setSettings(settings);
    return controller;
}

} // namespace

class TestAntenneFix : public QObject
{
    Q_OBJECT

private slots:
    void stehtAlsDrittesInstrumentNebenDenRotoren();
    void drehtNichtUndZeigtDieFesteRichtung();
    void ausgeschaltetIstSieNichtDa();
};

void TestAntenneFix::stehtAlsDrittesInstrumentNebenDenRotoren()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = controllerFor(dir, true);
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 982);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    const QList<RotorWidget*> kompasse = window.findChildren<RotorWidget*>();
    QCOMPARE(kompasse.size(), 3);
    RotorWidget* fix = kompassMit(window, QStringLiteral("Antenne Fix"));
    QVERIFY2(fix, "Die feste Antenne fehlt");
    RotorWidget* zwei = kompassMit(window, QStringLiteral("70cm"));
    QVERIFY(zwei);

    const QRect fixRect(fix->mapTo(&window, QPoint(0, 0)), fix->size());
    const QRect zweiRect(zwei->mapTo(&window, QPoint(0, 0)), zwei->size());
    qInfo() << "70cm" << zweiRect << "Antenne Fix" << fixRect;
    // Rechts neben den drehbaren, in derselben Reihe und gleich groß --
    // "sollte nur das gleiche design haben".
    QVERIFY2(fixRect.left() >= zweiRect.right(), "Die feste Antenne steht nicht rechts daneben");
    QCOMPARE(fixRect.top(), zweiRect.top());
    // Gleich groß -- bis auf das Pixel, das beim Aufteilen einer
    // ungeraden Breite auf drei Instrumente übrig bleibt.
    QVERIFY(qAbs(fixRect.width() - zweiRect.width()) <= 2);
    QCOMPARE(fixRect.height(), zweiRect.height());

    const QByteArray sheetDir = qgetenv("CP_SHEET_DIR");
    if (!sheetDir.isEmpty()) {
        QDir().mkpath(QString::fromLocal8Bit(sheetDir));
        QWidget* reihe = fix->parentWidget();
        QVERIFY(reihe);
        const QString pfad = QString::fromLocal8Bit(sheetDir) + QStringLiteral("/antenne-fix.png");
        QVERIFY(reihe->grab().save(pfad));
        qInfo().noquote() << "Blatt:" << pfad;

        // Und ein zweites Blatt in der Hoehe, die das Panel im
        // Seitenbereich wirklich hat -- dort steht die Fusszeile mit
        // Namen und fester Richtung.
        QPixmap gross(660, 620);
        gross.fill(QColor(16, 20, 26));
        {
            QPainter maler(&gross);
            RotorWidget rotor(QStringLiteral("2m"));
            rotor.setConnected(true);
            rotor.setAzimuthDeg(14.0);
            rotor.resize(320, 600);
            QPixmap links(320, 600);
            links.fill(Qt::transparent);
            rotor.render(&links, QPoint(), QRegion(), QWidget::DrawChildren);
            maler.drawPixmap(6, 10, links);

            RotorWidget feste(QStringLiteral("Antenne Fix"));
            feste.setFixedAntenna(true);
            feste.setAzimuthDeg(315.0);
            feste.resize(320, 600);
            QPixmap rechts(320, 600);
            rechts.fill(Qt::transparent);
            feste.render(&rechts, QPoint(), QRegion(), QWidget::DrawChildren);
            maler.drawPixmap(334, 10, rechts);
        }
        const QString pfad2 = QString::fromLocal8Bit(sheetDir) + QStringLiteral("/antenne-fix-gross.png");
        QVERIFY(gross.save(pfad2));
        qInfo().noquote() << "Blatt:" << pfad2;
    }
}

void TestAntenneFix::drehtNichtUndZeigtDieFesteRichtung()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = controllerFor(dir, true);
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 982);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    RotorWidget* fix = kompassMit(window, QStringLiteral("Antenne Fix"));
    QVERIFY(fix);
    QVERIFY(fix->isFixedAntenna());
    QCOMPARE(qRound(fix->azimuthDeg()), 315);

    // Ein Doppelklick mitten ins Zifferblatt: bei einem Rotor wäre das
    // der Befehl "dreh dorthin", hier darf er nichts tun.
    QSignalSpy drehen(fix, &RotorWidget::rotateRequested);
    const QPoint imRing(fix->width() / 2, fix->height() / 4);
    QTest::mouseDClick(fix, Qt::LeftButton, Qt::NoModifier, imRing);
    QCoreApplication::processEvents();
    qInfo() << "rotateRequested nach Doppelklick:" << drehen.count()
            << "| Richtung danach:" << fix->azimuthDeg();
    QCOMPARE(drehen.count(), 0);
    QCOMPARE(qRound(fix->azimuthDeg()), 315);

    // Und die Richtung folgt den Einstellungen.
    ContestSettings geaendert = controller->settings();
    geaendert.fixedAntennaBearingDeg = 120.0;
    controller->setSettings(geaendert);
    QMetaObject::invokeMethod(&window, "applyRotorWidgetSettings");
    QCoreApplication::processEvents();
    QCOMPARE(qRound(kompassMit(window, QStringLiteral("Antenne Fix"))->azimuthDeg()), 120);
}

void TestAntenneFix::ausgeschaltetIstSieNichtDa()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = controllerFor(dir, false);
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 982);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QCOMPARE(window.findChildren<RotorWidget*>().size(), 2);
    QVERIFY(!kompassMit(window, QStringLiteral("Antenne Fix")));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestAntenneFix tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_antenne_fix.moc"
