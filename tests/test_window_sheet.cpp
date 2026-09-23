// Kein Test -- ein Werkzeug, gleicher Aufbau und gleiche Begruendung wie
// tests/test_map_mockup.cpp: es rendert das ECHTE MainWindow mit einer
// frischen Datenbank und ein paar frei erfundenen QSOs als Blatt in
// wirklicher Groesse, damit ueber Gestaltung an einem Bild entschieden
// wird und nicht an einer Beschreibung. Anders als das Kartenblatt
// zeigt es das ganze Fenster: Panelaufteilung, Kopfzeilen, Leerzustaende,
// Statuszeile.
//
// Blaetter paarweise, eines je Variante und in wirklicher Groesse
// (Hausregel): das Fenster wie es startet gegen dasselbe Fenster mit
// runder Karte, das blaue Profil-Abzeichen gegen ein bernsteinfarbenes,
// und das Kurzwellen-Log ohne gegen eines mit Bandfarben. Die Varianten
// werden HIER gebaut, nicht im Programm -- solange nichts entschieden
// ist, soll der Quelltext keine halbfertige zweite Gestaltung tragen.
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

#include <QHeaderView>
#include <QPainter>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QTableView>

#include "app/AppController.h"
#include "core/ColorTheme.h"
#include "ui/MainWindow.h"
#include "ui/MapWidget.h"
#include "ui/ProfileRail.h"
#include "ui/StyleKit.h"
#include "ui/UnifiedLogWidget.h"

using namespace Contestprogramm;

namespace {

// Eine Farbe je Band, gedaempft, damit die Zeile nicht zum Regenbogen
// wird -- die Hausregel erlaubt ~2% Farbflaeche. Nur der Text der
// Bandzelle wird eingefaerbt, kein Hintergrund.
QColor bandTint(const QString& band)
{
    static const QHash<QString, int> kHues{
        {QStringLiteral("1.8"), 20},  {QStringLiteral("3.5"), 40},  {QStringLiteral("7"), 75},
        {QStringLiteral("14"), 145},  {QStringLiteral("21"), 190},  {QStringLiteral("28"), 265},
        {QStringLiteral("50"), 300},  {QStringLiteral("144"), 40},  {QStringLiteral("432"), 190},
        {QStringLiteral("1296"), 265},
    };
    const auto it = kHues.constFind(band.trimmed());
    if (it == kHues.constEnd()) {
        return QColor(Style::kTextPrimary());
    }
    return QColor::fromHsl(it.value(), 110, 165);
}

class BandTintDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override
    {
        // initStyleOption ueberschreiben, nicht nur die lokale Kopie
        // anfassen -- die Delegate-Falle aus dem Log-Feinschliff vom
        // 2026-09-11.
        QStyledItemDelegate::initStyleOption(option, index);
        const QColor tint = bandTint(index.data(Qt::DisplayRole).toString());
        option->palette.setColor(QPalette::Text, tint);
        option->palette.setColor(QPalette::HighlightedText, tint);
    }
};

} // namespace

class TestWindowSheet : public QObject {
    Q_OBJECT
private slots:
    void render();
    void shortwaveLogWithAndWithoutBandColours();
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

    // Frage 2: das Profil-Abzeichen links oben. Heute das einzige
    // kraeftige Blau im ganzen Fenster -- daneben gestellt dasselbe
    // Abzeichen in der Farbe, die das Fenster sonst fuer "aktiv"
    // benutzt. Erst als Paar entscheidbar, darum beide Blaetter.
    if (auto* rail = window.findChild<ProfileRail*>()) {
        save(QStringLiteral("contestprogramm-abzeichen-blau"));
        const QList<QPushButton*> badges = rail->findChildren<QPushButton*>();
        for (QPushButton* badge : badges) {
            if (badge->text() == QStringLiteral("+")) {
                continue;
            }
            badge->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 2px solid %2; border-radius: 16px; color: %3; }")
                    .arg(Style::kAmberBg(), Style::kAmberBorder(), Style::kAmberText()));
        }
        save(QStringLiteral("contestprogramm-abzeichen-bernstein"));
        for (QPushButton* badge : badges) {
            if (badge->text() == QStringLiteral("+")) {
                continue;
            }
            badge->setStyleSheet(
                QStringLiteral("QPushButton { background: %1; border: 2px solid %2; border-radius: 16px; color: %3; }")
                    .arg(Style::kBlueBg(), Style::kBlueBorder(), Style::kBlueText()));
        }
    } else {
        qWarning("ProfileRail nicht gefunden");
    }

    if (auto* map = window.findChild<MapWidget*>()) {
        map->setFitToWindowEnabled(false);
        save(QStringLiteral("contestprogramm-hauptfenster-runde-karte"));
    } else {
        qWarning("MapWidget nicht gefunden");
    }
}


// Frage 3: Bandfarbe im Log. Sinn ergibt sie erst, wo wirklich mehrere
// Baender untereinander stehen -- darum das Kurzwellen-Uebungslog und
// nicht das VHF-Fenster oben. Zwei Blaetter, sonst identisch: einmal
// wie heute, einmal mit eingefaerbter Bandzelle.
void TestWindowSheet::shortwaveLogWithAndWithoutBandColours()
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
    save(QStringLiteral("contestprogramm-kurzwelle-log-ohne-bandfarbe"));

    auto* feedTable = window.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(feedTable);
    feedTable->setItemDelegateForColumn(UnifiedLogWidget::ColumnBand, new BandTintDelegate(feedTable));
    feedTable->viewport()->update();
    save(QStringLiteral("contestprogramm-kurzwelle-log-mit-bandfarbe"));
}

QTEST_MAIN(TestWindowSheet)
#include "test_window_sheet.moc"
