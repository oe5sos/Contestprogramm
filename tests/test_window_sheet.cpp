// Kein Test -- ein Werkzeug, gleicher Aufbau und gleiche Begruendung wie
// tests/test_map_mockup.cpp: es rendert das ECHTE MainWindow mit einer
// frischen Datenbank und ein paar frei erfundenen QSOs als Blatt in
// wirklicher Groesse, damit ueber Gestaltung an einem Bild entschieden
// wird und nicht an einer Beschreibung. Anders als das Kartenblatt
// zeigt es das ganze Fenster: Panelaufteilung, Kopfzeilen, Leerzustaende,
// Statuszeile.
//
// Drei Blaetter, jedes in wirklicher Groesse (Hausregel): das Fenster
// wie es startet, dasselbe mit "Flaeche fuellen" statt runder Scheibe
// (die Einstellung gibt es weiter im Karten-Zahnrad, also gehoert sie
// aufs Blatt), und das Kurzwellen-Log mit seinen sechs Baendern.
//
// Die drei Gestaltungsfragen vom 2026-09-23 -- runde Karte,
// bernsteinfarbenes Profil-Abzeichen, Bandfarbe im Log -- sind
// entschieden und stehen jetzt im Programm; die Varianten, die dieses
// Werkzeug dafuer kurzzeitig selbst gebaut hat, sind wieder raus.
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
    void shortwaveLog();
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

    // Die runde Scheibe ist die Vorgabe; "Flaeche fuellen" bleibt im
    // Karten-Zahnrad erreichbar und gehoert darum weiter aufs Blatt.
    if (auto* map = window.findChild<MapWidget*>()) {
        map->setFitToWindowEnabled(true);
        save(QStringLiteral("contestprogramm-hauptfenster-flaeche-fuellen"));
    } else {
        qWarning("MapWidget nicht gefunden");
    }
}


// Das Kurzwellen-Uebungslog: sechs Baender untereinander, also die
// Ansicht, in der Bandspalte und Bandfarbe ueberhaupt etwas sagen. Das
// VHF-Fenster oben zeigt das nicht.
void TestWindowSheet::shortwaveLog()
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
    QVERIFY2(controller.openDatabase(data.filePath(QStringLiteral("kw.sqlite")), &error), qPrintable(error));

    ContestSettings settings = controller.settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.activeContestId = QStringLiteral("KW_UEBUNG");
    controller.setSettings(settings);

    struct Seed { const char* call; const char* band; const char* mode; int serial; };
    static const Seed seeds[] = {
        {"W1AAA", "14", "CW", 1},   {"JA2BBB", "14", "CW", 2},  {"PY3CCC", "21", "SSB", 3},
        {"VK4DDD", "21", "SSB", 4}, {"ZS5EEE", "28", "SSB", 5}, {"UA6FFF", "7", "CW", 6},
        {"LU7GGG", "7", "CW", 7},   {"K8HHH", "3.5", "CW", 8},  {"OH9III", "1.8", "CW", 9},
        {"VE2JJJ", "14", "SSB", 10},
    };
    int minute = 0;
    for (const Seed& seed : seeds) {
        QsoRecord r;
        r.callsign = QString::fromLatin1(seed.call);
        r.band = QString::fromLatin1(seed.band);
        r.mode = QString::fromLatin1(seed.mode);
        r.timestampUtc = QDateTime::currentDateTimeUtc().addSecs(-60 * (60 - minute)).toString(Qt::ISODate);
        r.serialSent = seed.serial;
        r.serialRcvd = seed.serial + 30;
        r.rstSent = r.mode == QStringLiteral("CW") ? QStringLiteral("599") : QStringLiteral("59");
        r.rstRcvd = r.rstSent;
        r.exchangeSent = QStringLiteral("%1 %2").arg(r.rstSent).arg(seed.serial, 3, 10, QLatin1Char('0'));
        r.exchangeRcvd = QStringLiteral("%1 %2").arg(r.rstRcvd).arg(seed.serial + 30, 3, 10, QLatin1Char('0'));
        r.contestId = settings.activeContestId;
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
    save(QStringLiteral("contestprogramm-kurzwelle-log"));
}

QTEST_MAIN(TestWindowSheet)
#include "test_window_sheet.moc"
