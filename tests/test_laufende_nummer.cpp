// Martin, 2026-09-28: "es ist keine chronologische nmer vorhanden."
//
// Die Spalte "QSO#" gab es, aber sie war in der kompakten Ansicht
// ausgeblendet und zeigte die GESENDETE Contest-Nummer. Das war zweimal
// falsch: dieselbe Zahl steht schon im gesendeten Austausch daneben, und
// auf UKW faengt die gesendete Nummer je Band wieder bei 001 an (IARU
// R1) -- nach einem Bandwechsel stuende also wieder eine 1 in der Liste.
//
// Jetzt steht dort, das wievielte QSO des Logs es ist. Dieser Pruefstand
// haelt beides fest: dass die Zahlen 1, 2, 3 ... lauten, auch ueber
// einen Bandwechsel hinweg, und dass sie nach einem geloeschten QSO
// wieder lueckenlos sind.

#include <QtTest>

#include <QAbstractItemModel>
#include <QApplication>
#include <QTableView>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"
#include "data/QsoRecord.h"
#include "ui/MainWindow.h"
#include "ui/UnifiedLogWidget.h"

#include <memory>

using namespace Contestprogramm;

class TestLaufendeNummer : public QObject
{
    Q_OBJECT

private slots:
    void theNumbersRunOneTwoThreeAcrossABandChange();
    void theNumbersCloseUpAfterADeletion();
    void theOperatorCanSwitchTheColumnOffLikeInDxLog();

private:
    std::unique_ptr<AppController> makeController(QTemporaryDir& dir, const QString& file);
    // Die Spalte QSO# aller Verlaufszeilen, von oben nach unten.
    QStringList nummern(QAbstractItemModel* model);
};

std::unique_ptr<AppController> TestLaufendeNummer::makeController(QTemporaryDir& dir, const QString& file)
{
    auto controller = std::make_unique<AppController>();
    if (!controller->openDatabase(dir.filePath(file))) {
        return nullptr;
    }
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.activeContestId = QStringLiteral("IARU_R1_VHF_UHF");
    settings.rigctldHost.clear();
    settings.rotor1Enabled = false;
    settings.rotor2Enabled = false;
    settings.esmEnabled = false;
    controller->setSettings(settings);

    // Drei auf 144, zwei auf 432 -- auf 432 faengt die GESENDETE Nummer
    // wieder bei 1 an, die laufende darf das nicht.
    struct Eintrag {
        QString call;
        QString band;
        int nrGesendet;
    };
    const QVector<Eintrag> eintraege{{QStringLiteral("DL1ABC"), QStringLiteral("144"), 1},
                                     {QStringLiteral("OK2XYZ"), QStringLiteral("144"), 2},
                                     {QStringLiteral("HA5QRP"), QStringLiteral("144"), 3},
                                     {QStringLiteral("S51DX"), QStringLiteral("432"), 1},
                                     {QStringLiteral("OE3ABC"), QStringLiteral("432"), 2}};
    int i = 0;
    for (const Eintrag& e : eintraege) {
        QsoRecord r;
        r.callsign = e.call;
        r.band = e.band;
        r.mode = QStringLiteral("SSB");
        // Aufsteigend in der Zeit, aelteste zuerst.
        r.timestampUtc = QDateTime::currentDateTimeUtc().addSecs(-600 * (10 - i)).toString(Qt::ISODate);
        r.rstSent = QStringLiteral("59");
        r.rstRcvd = QStringLiteral("59");
        r.serialSent = e.nrGesendet;
        r.serialRcvd = 7;
        r.gridSquare = QStringLiteral("JN58SD");
        r.distanceKm = 165.0;
        r.contestId = settings.activeContestId;
        if (!controller->database().insertQso(r)) {
            return nullptr;
        }
        ++i;
    }
    return controller;
}

QStringList TestLaufendeNummer::nummern(QAbstractItemModel* model)
{
    QStringList werte;
    for (int row = 0; row < model->rowCount(); ++row) {
        // Nur Verlaufszeilen tragen ein Rufzeichen; Trenner und
        // Kandidaten bleiben aussen vor.
        const QString call = model->index(row, UnifiedLogWidget::ColumnCall).data().toString();
        if (call.isEmpty()) {
            continue;
        }
        const QString nr = model->index(row, UnifiedLogWidget::ColumnSerial).data().toString();
        if (!nr.isEmpty()) {
            werte << nr;
        }
    }
    return werte;
}

void TestLaufendeNummer::theNumbersRunOneTwoThreeAcrossABandChange()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("nummer.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* table = window.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(table);

    // Die Spalte muss ueberhaupt zu sehen sein -- sie war in der
    // kompakten Ansicht ausgeblendet.
    QVERIFY2(!table->isColumnHidden(UnifiedLogWidget::ColumnSerial),
             "Die Spalte mit der laufenden Nummer ist ausgeblendet");

    const QStringList werte = nummern(table->model());
    qInfo().noquote() << "laufende Nummern:" << werte.join(QStringLiteral(", "));
    QCOMPARE(werte.size(), 5);
    QCOMPARE(werte, QStringList({QStringLiteral("001"), QStringLiteral("002"), QStringLiteral("003"),
                                  QStringLiteral("004"), QStringLiteral("005")}));
}

void TestLaufendeNummer::theNumbersCloseUpAfterADeletion()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("loesch.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.activateWindow();
    (void)QTest::qWaitForWindowActive(&window);

    auto* table = window.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(table);
    QAbstractItemModel* model = table->model();
    QCOMPARE(nummern(model).size(), 5);

    // Das zweite QSO weg -- mit der Taste, wie Martin es tut.
    int zeile = -1;
    for (int row = 0; row < model->rowCount(); ++row) {
        if (model->index(row, UnifiedLogWidget::ColumnCall).data().toString() == QStringLiteral("OK2XYZ")) {
            zeile = row;
            break;
        }
    }
    QVERIFY(zeile >= 0);
    table->setFocus();
    table->setCurrentIndex(model->index(zeile, UnifiedLogWidget::ColumnCall));
    QTest::keyClick(table, Qt::Key_Backspace);
    QCoreApplication::processEvents();

    const QStringList werte = nummern(model);
    qInfo().noquote() << "nach dem Löschen:" << werte.join(QStringLiteral(", "));
    QCOMPARE(werte.size(), 4);
    QVERIFY2(werte == QStringList({QStringLiteral("001"), QStringLiteral("002"), QStringLiteral("003"),
                                    QStringLiteral("004")}),
             qPrintable(QStringLiteral("Lücke in der Zählung: %1").arg(werte.join(QStringLiteral(", ")))));
}

// Abschaltbar, wie DXLog.net es loest: "Hides the QSO numbers on the
// left, useful for serial number contests so wrong serials don't get
// sent" (dxlog.net/docs, Main Window). Die Begruendung trifft Martins
// Conteste: auf UKW faengt die GESENDETE Nummer je Band wieder bei 001
// an, die laufende nicht -- wer die falsche Spalte abliest, sendet die
// falsche Nummer. Der Schalter sitzt im ⚙ des Log-Kopfes, wo Martin
// Optionen haben will.
void TestLaufendeNummer::theOperatorCanSwitchTheColumnOffLikeInDxLog()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("schalter.sqlite"));
    QVERIFY(controller);

    {
        MainWindow window(*controller);
        window.resize(1440, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        auto* table = window.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
        QVERIFY(table);
        auto* log = window.findChild<UnifiedLogWidget*>();
        QVERIFY(log);

        // Standard: an.
        QVERIFY(log->runningNumberVisible());
        QVERIFY(!table->isColumnHidden(UnifiedLogWidget::ColumnSerial));

        // Den Weg gehen, den der Menuepunkt im ⚙ geht: das Widget
        // umstellen UND die Einstellung mitschreiben. Nur das Widget
        // umzustellen genuegt nicht und soll es auch nicht -- MainWindow
        // zieht die Anzeige regelmaessig an den Einstellungen nach (wie
        // bei Ansicht und Eingabezeile auch), und haette die Wahl sonst
        // beim naechsten CAT-Takt wieder ueberschrieben.
        const auto umschalten = [&](bool an) {
            log->setRunningNumberVisible(an);
            ContestSettings settings = controller->settings();
            settings.logShowRunningNumber = an;
            controller->setSettings(settings);
            QCoreApplication::processEvents();
        };

        umschalten(false);
        qInfo() << "Schalter steht auf" << log->runningNumberVisible()
                << "-- Spalte versteckt:" << table->isColumnHidden(UnifiedLogWidget::ColumnSerial);
        QVERIFY2(table->isColumnHidden(UnifiedLogWidget::ColumnSerial),
                 "Die Spalte bleibt stehen, obwohl sie abgeschaltet wurde");

        // Und wieder an.
        umschalten(true);
        QVERIFY(!table->isColumnHidden(UnifiedLogWidget::ColumnSerial));

        // Abgeschaltet lassen -- fuer den Neustart unten.
        umschalten(false);
    }

    // Neues Fenster, dieselbe Datenbank: die Wahl haelt.
    {
        MainWindow window(*controller);
        window.resize(1440, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* table = window.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
        QVERIFY(table);
        qInfo() << "nach dem Neustart ausgeblendet:"
                << table->isColumnHidden(UnifiedLogWidget::ColumnSerial);
        QVERIFY2(table->isColumnHidden(UnifiedLogWidget::ColumnSerial),
                 "Die abgeschaltete Spalte ist nach dem Neustart wieder da");
    }
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestLaufendeNummer tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_laufende_nummer.moc"
