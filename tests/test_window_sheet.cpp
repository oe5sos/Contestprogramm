// Kein Test -- ein Werkzeug, gleicher Aufbau und gleiche Begruendung wie
// tests/test_map_mockup.cpp: es rendert das ECHTE MainWindow mit einer
// frischen Datenbank und ein paar frei erfundenen QSOs als Blatt in
// wirklicher Groesse, damit ueber Gestaltung an einem Bild entschieden
// wird und nicht an einer Beschreibung. Anders als das Kartenblatt
// zeigt es das ganze Fenster: Panelaufteilung, Kopfzeilen, Leerzustaende,
// Statuszeile.
//
// Zwei Blaetter: das Fenster, wie es startet, und dasselbe Fenster mit
// runder statt an den Rahmen angepasster Karte -- eine Entscheidung, die
// man nur am Bild trifft.
//
// Ziel ist SHEET_DIR, sonst das Temp-Verzeichnis (wie beim Kartenblatt),
// damit dieses Werkzeug im normalen Durchlauf einfach mitlaeuft.
// Die Rufzeichen sind erfunden und als Platzhalter erkennbar, nie mit
// echten geloggten Kontakten zu verwechseln.
#include <QtTest>

#include <QApplication>
#include <QDir>
#include <QImage>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "core/ColorTheme.h"
#include "ui/MainWindow.h"
#include "ui/MapWidget.h"
#include "ui/StyleKit.h"

using namespace Contestprogramm;

class TestWindowSheet : public QObject {
    Q_OBJECT
private slots:
    void render();
};

void TestWindowSheet::render()
{
    QString outDir = qEnvironmentVariable("SHEET_DIR");
    if (outDir.isEmpty()) {
        outDir = QDir::tempPath();
    }
    QVERIFY(QDir().mkpath(outDir));

    QTemporaryDir data;
    QVERIFY(data.isValid());

    AppController controller;
    QString error;
    QVERIFY2(controller.openDatabase(data.filePath(QStringLiteral("cp.sqlite")), &error),
             qPrintable(error));

    ContestSettings settings = controller.settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    if (settings.activeContestId.isEmpty()) {
        settings.activeContestId = QStringLiteral("IARU_R1_VHF_UHF");
    }
    controller.setSettings(settings);

    struct Seed { const char* call; const char* band; const char* grid; int km; int serial; };
    static const Seed seeds[] = {
        {"OE1AAA", "144", "JN88DD", 233, 1}, {"DL2BBB", "144", "JN68QQ", 189, 2},
        {"HA3CCC", "144", "JN97KK", 412, 3}, {"OK1DDD", "144", "JO70FF", 301, 4},
        {"S5/EEE", "144", "JN76TA", 198, 5}, {"9A1FFF", "432", "JN85GG", 356, 1},
        {"I3GGG",  "432", "JN65XX", 274, 2}, {"OE3HHH", "144", "JN78SE", 145, 6},
        {"DK5III", "432", "JN58LL", 233, 3}, {"SP9JJJ", "144", "JO90AA", 521, 7},
    };
    int minute = 0;
    for (const Seed& s : seeds) {
        QsoRecord r;
        r.callsign = QString::fromLatin1(s.call);
        r.band = QString::fromLatin1(s.band);
        r.mode = QStringLiteral("SSB");
        r.timestampUtc = QDateTime::currentDateTimeUtc().addSecs(-60 * (60 - minute))
                             .toString(Qt::ISODate);
        r.gridSquare = QString::fromLatin1(s.grid);
        r.distanceKm = s.km;
        r.serialSent = s.serial;
        r.serialRcvd = s.serial + 10;
        r.rstSent = QStringLiteral("59");
        r.rstRcvd = QStringLiteral("59");
        r.exchangeSent = QStringLiteral("59 %1 JN67UT").arg(s.serial, 3, 10, QLatin1Char('0'));
        r.exchangeRcvd = QStringLiteral("59 %1 %2").arg(s.serial + 10, 3, 10, QLatin1Char('0'))
                             .arg(QString::fromLatin1(s.grid));
        r.contestId = controller.settings().activeContestId;
        controller.database().insertQso(r);
        minute += 5;
    }

    Style::setActiveTheme(controller.settings().colorTheme);
    qApp->setStyleSheet(Style::appStyleSheet());

    MainWindow window(controller);
    window.resize(1680, 1000);
    window.show();
    for (int i = 0; i < 40; ++i) {
        QCoreApplication::processEvents();
        QTest::qWait(20);
    }
    const auto save = [&](const QString& name) {
        for (int i = 0; i < 20; ++i) { QCoreApplication::processEvents(); QTest::qWait(15); }
        const QPixmap shot = window.grab();
        const QString path = outDir + QLatin1Char('/') + name + QStringLiteral(".png");
        QVERIFY(shot.save(path));
        qInfo() << "geschrieben:" << path << shot.size();
    };
    save(QStringLiteral("contestprogramm-hauptfenster"));

    if (auto* map = window.findChild<MapWidget*>()) {
        map->setFitToWindowEnabled(false);
        save(QStringLiteral("contestprogramm-hauptfenster-runde-karte"));
    } else {
        qWarning("MapWidget nicht gefunden");
    }
}

QTEST_MAIN(TestWindowSheet)
#include "test_window_sheet.moc"
