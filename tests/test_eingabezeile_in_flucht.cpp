// Martin, 2026-09-28, mit Bild: "sollte alles in einer reihe sein."
//
// Die Eingabezeile stand um eine Spalte nach links versetzt gegen die
// Tabelle darüber: ihre Zelle für die laufende Nummer wurde nur in der
// Ansicht DXLog-Vollspalten angelegt, die Spalte selbst steht aber seit
// heute in beiden Ansichten. Eine Zelle zu wenig, und alles dahinter
// rutscht.
//
// Gemessen wird deshalb nicht, wie viele Zellen es gibt, sondern wo ihre
// KANTEN liegen: jede Zelle der Eingabezeile muss an derselben x-Stelle
// anfangen wie ihre Spalte in der Tabelle. Sein Wort dafür, vom
// 2026-09-27: "es soll wie die darüber aussehen, nichts extra. quasi wie
// bei excel."

#include <QtTest>

#include <QApplication>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QTableView>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"
#include "data/QsoRecord.h"
#include "ui/MainWindow.h"
#include "ui/StyleKit.h"
#include "ui/UnifiedLogWidget.h"

#include <memory>

using namespace Contestprogramm;

class TestEingabezeileInFlucht : public QObject
{
    Q_OBJECT

private slots:
    void theEntryRowLinesUpWithTheTableInBothViews();
    void itStaysInLineWhenTheRunningNumberIsSwitchedOff();
    void itStaysInLineWhenTheRunningNumberComesBack();
    void theStatusColumnIsOnlyAsWideAsItsPill();

private:
    std::unique_ptr<AppController> makeController(QTemporaryDir& dir, const QString& file);
    static int zellenAnfang(MainWindow& window, QWidget* zelle);
    static int spaltenAnfang(QTableView* table, int spalte);
};

std::unique_ptr<AppController> TestEingabezeileInFlucht::makeController(QTemporaryDir& dir, const QString& file)
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

    // Zwei QSOs, wie auf seinem Bild.
    for (int i = 0; i < 2; ++i) {
        QsoRecord r;
        r.callsign = i == 0 ? QStringLiteral("OE5A00") : QStringLiteral("OE5AA0");
        r.band = QStringLiteral("144");
        r.mode = QStringLiteral("SSB");
        r.timestampUtc = QDateTime::currentDateTimeUtc().addSecs(-60 * (2 - i)).toString(Qt::ISODate);
        r.rstSent = QStringLiteral("59");
        r.rstRcvd = QStringLiteral("59");
        r.serialSent = i + 1;
        r.serialRcvd = i + 1;
        r.gridSquare = QStringLiteral("JN78CG");
        r.distanceKm = 51.9;
        r.bearingDeg = 36;
        r.contestId = settings.activeContestId;
        if (!controller->database().insertQso(r)) {
            return nullptr;
        }
    }
    return controller;
}


// Die x-Stelle, an der eine Zelle der Eingabezeile anfaengt, gemessen
// im selben Bezugsrahmen wie die Tabellenspalten: relativ zum linken
// Rand der Zeile.
int TestEingabezeileInFlucht::zellenAnfang(MainWindow& window, QWidget* zelle)
{
    auto* row = window.findChild<QWidget*>(QLatin1String(UnifiedLogWidget::kEntryRowObjectName));
    if (!row || !zelle) {
        return -1;
    }
    return zelle->mapTo(row, QPoint(0, 0)).x();
}

// Die x-Stelle einer Spalte, im selben Bezugsrahmen.
int TestEingabezeileInFlucht::spaltenAnfang(QTableView* table, int spalte)
{
    return table->horizontalHeader()->sectionPosition(spalte);
}

void TestEingabezeileInFlucht::theEntryRowLinesUpWithTheTableInBothViews()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("flucht.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCoreApplication::processEvents();

    auto* table = window.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(table);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);
    auto* row = window.findChild<QWidget*>(QLatin1String(UnifiedLogWidget::kEntryRowObjectName));
    QVERIFY(row);

    for (const auto ansicht : {ContestSettings::LogViewMode::Compact,
                                ContestSettings::LogViewMode::DxLogFullColumns}) {
        log->setViewMode(ansicht);
        QCoreApplication::processEvents();
        const QString name = ansicht == ContestSettings::LogViewMode::Compact
                                 ? QStringLiteral("Kompakt")
                                 : QStringLiteral("DXLog-Vollspalten");

        // Das Rufzeichenfeld ist die Zelle, an der man eine verrutschte
        // Zeile am deutlichsten sieht -- und die, in die getippt wird.
        const QList<QLineEdit*> felder = row->findChildren<QLineEdit*>();
        QVERIFY2(!felder.isEmpty(), "Die Eingabezeile hat keine Felder");
        QLineEdit* rufzeichen = felder.first();
        const int zelleX = zellenAnfang(window, rufzeichen);
        const int spalteX = spaltenAnfang(table, UnifiedLogWidget::ColumnCall);
        // Die Einzelbreiten vor dem Rufzeichen -- dort muss ein Versatz
        // entstehen, wenn einer entsteht.
        QStringList breiten;
        for (const auto paar : {std::pair<const char*, int>{"QSO#", UnifiedLogWidget::ColumnSerial},
                                 {"Band", UnifiedLogWidget::ColumnBand},
                                 {"Zeit", UnifiedLogWidget::ColumnTime}}) {
            if (!table->isColumnHidden(paar.second)) {
                breiten << QStringLiteral("%1=%2").arg(QLatin1String(paar.first)).arg(table->columnWidth(paar.second));
            }
        }
        qInfo().noquote() << name << "-- Rufzeichenfeld bei" << zelleX << ", Call-Spalte bei" << spalteX
                          << "| Breite" << rufzeichen->width() << "gegen"
                          << table->columnWidth(UnifiedLogWidget::ColumnCall)
                          << "| davor:" << breiten.join(QLatin1Char(' '));

        QVERIFY2(std::abs(zelleX - spalteX) <= 1,
                 qPrintable(QStringLiteral("%1: das Rufzeichenfeld fängt bei %2 an, seine Spalte bei %3 -- "
                                            "die Zeile ist verrutscht")
                                .arg(name)
                                .arg(zelleX)
                                .arg(spalteX)));
        QCOMPARE(rufzeichen->width(), table->columnWidth(UnifiedLogWidget::ColumnCall));

        // Und die Bandzelle genauso, sofern sie dasteht.
        auto* band = window.findChild<QLabel*>(QLatin1String(UnifiedLogWidget::kEntryBandLabelObjectName));
        if (band && !band->isHidden()) {
            const int bandX = zellenAnfang(window, band);
            const int bandSpalteX = spaltenAnfang(table, UnifiedLogWidget::ColumnBand);
            QVERIFY2(std::abs(bandX - bandSpalteX) <= 1,
                     qPrintable(QStringLiteral("%1: die Bandzelle fängt bei %2 an, ihre Spalte bei %3")
                                    .arg(name)
                                    .arg(bandX)
                                    .arg(bandSpalteX)));
        }
    }
}

// Und wenn die laufende Nummer abgeschaltet wird, muss die leere Zelle
// mit ihr verschwinden -- sonst rutscht die Zeile in die andere
// Richtung.
void TestEingabezeileInFlucht::itStaysInLineWhenTheRunningNumberIsSwitchedOff()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("flucht2.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCoreApplication::processEvents();

    auto* table = window.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(table);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);
    auto* row = window.findChild<QWidget*>(QLatin1String(UnifiedLogWidget::kEntryRowObjectName));
    QVERIFY(row);

    log->setRunningNumberVisible(false);
    ContestSettings settings = controller->settings();
    settings.logShowRunningNumber = false;
    controller->setSettings(settings);
    QCoreApplication::processEvents();

    QVERIFY(table->isColumnHidden(UnifiedLogWidget::ColumnSerial));
    QLineEdit* rufzeichen = row->findChildren<QLineEdit*>().first();
    const int zelleX = zellenAnfang(window, rufzeichen);
    const int spalteX = spaltenAnfang(table, UnifiedLogWidget::ColumnCall);
    qInfo().noquote() << "ohne laufende Nummer -- Rufzeichenfeld bei" << zelleX << ", Call-Spalte bei" << spalteX;
    QVERIFY2(std::abs(zelleX - spalteX) <= 1,
             qPrintable(QStringLiteral("Das Rufzeichenfeld fängt bei %1 an, seine Spalte bei %2")
                            .arg(zelleX)
                            .arg(spalteX)));
}

// Der Rückweg: aus und wieder AN. Die Windows-CI hat ihn am
// 2026-09-28 im Rüttel-Prüfstand gefunden -- "nach nummer(an):
// Rufzeichenfeld bei 70, Spalte bei 185". Auf macOS fiel es nicht auf,
// weil dort ein Layout-Durchlauf die Zeile nachträglich einfing.
void TestEingabezeileInFlucht::itStaysInLineWhenTheRunningNumberComesBack()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("flucht3.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCoreApplication::processEvents();

    auto* table = window.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(table);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);
    auto* row = window.findChild<QWidget*>(QLatin1String(UnifiedLogWidget::kEntryRowObjectName));
    QVERIFY(row);

    log->setRunningNumberVisible(false);
    log->setRunningNumberVisible(true);
    // Ein Layout-Durchlauf gehört dazu: die endgültige Spaltenbreite
    // steht erst danach fest, und genau dann muss die Zeile folgen.
    QCoreApplication::processEvents();
    QVERIFY(!table->isColumnHidden(UnifiedLogWidget::ColumnSerial));
    for (QLineEdit* feld : row->findChildren<QLineEdit*>()) {
        qInfo().noquote() << "Feld" << feld->objectName() << feld->placeholderText() << "bei"
                          << zellenAnfang(window, feld) << "Breite" << feld->width();
    }
    for (int spalte = 0; spalte < 6; ++spalte) {
        qInfo().noquote() << "Spalte" << spalte << "versteckt" << table->isColumnHidden(spalte) << "bei"
                          << spaltenAnfang(table, spalte);
    }
    QLineEdit* rufzeichen = row->findChildren<QLineEdit*>().first();
    const int zelleX = zellenAnfang(window, rufzeichen);
    const int spalteX = spaltenAnfang(table, UnifiedLogWidget::ColumnCall);
    qInfo().noquote() << "laufende Nummer wieder an -- Rufzeichenfeld bei" << zelleX << ", Call-Spalte bei"
                      << spalteX;
    QVERIFY2(std::abs(zelleX - spalteX) <= 1,
             qPrintable(QStringLiteral("Das Rufzeichenfeld fängt bei %1 an, seine Spalte bei %2")
                            .arg(zelleX)
                            .arg(spalteX)));
}

// Martin, 2026-09-29: "bitte viel schmäler machen, ich benötige
// platz!!!" -- über die letzte Spalte im Log. Sie wuchs bis 110 px in
// jede freie Lücke und stand auf fast jeder Zeile leer da. Jetzt ist
// sie so breit wie ihr breitestes Schild und keinen Deut breiter.
void TestEingabezeileInFlucht::theStatusColumnIsOnlyAsWideAsItsPill()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("statusbreite.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCoreApplication::processEvents();

    auto* table = window.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(table);

    // Was das breiteste Schild in seiner eigenen Schrift braucht.
    QFont pillenSchrift = table->font();
    pillenSchrift.setPixelSize(Style::kFontCaption);
    pillenSchrift.setWeight(QFont::DemiBold);
    const QFontMetrics masse(pillenSchrift);
    const int noetig = masse.horizontalAdvance(QStringLiteral("UNGÜLTIG")) + 16 + 12;

    const int breite = table->columnWidth(UnifiedLogWidget::ColumnStatus);
    qInfo().noquote() << "Statusspalte:" << breite << "px, das Schild braucht" << noetig << "px";
    QVERIFY2(breite >= noetig - 2,
             qPrintable(QStringLiteral("Die Statusspalte ist %1 px breit, UNGÜLTIG braucht %2 px")
                            .arg(breite).arg(noetig)));
    QVERIFY2(breite <= noetig + 2,
             qPrintable(QStringLiteral("Die Statusspalte ist mit %1 px breiter als nötig (%2 px)")
                            .arg(breite).arg(noetig)));
    // Und deutlich schmäler als der alte Deckel von 110 px.
    QVERIFY2(breite < 100, "Die Statusspalte ist nicht schmäler geworden");
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestEingabezeileInFlucht tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_eingabezeile_in_flucht.moc"
