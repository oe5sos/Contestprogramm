// Martin, 2026-09-27: "ziel sollte sein, dass nach eingabe des
// rufzeichen schon automatsich der locator im locator steht um einerseits
// weniger fehler zu machen und auch schneller zu sein. dieser kann aber
// ggf. abweichen, wenn locator geändert worden ist." Und zur Quelle:
// "wichtig jedoch die einreichung der ergebnsise, diese sind
// treffsicherer" -- QRZ nur ergaenzend.
//
// Gebaut war das (vier Stufen in MainWindow::
// handleCallsignLookupRequested), aber nie mit echten Tastendruecken am
// echten Fenster gefahren. Genau das macht dieser Pruefstand: Rufzeichen
// tippen, warten, nachsehen, ob der Locator von selbst dasteht -- und
// nachsehen, dass eine Station, die umgezogen ist, sich weiter von Hand
// eintragen laesst.

#include <QtTest>

#include <QApplication>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QTextStream>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "core/CallsignLocatorLookup.h"
#include "data/ContestDatabase.h"
#include "data/QsoRecord.h"
#include "ui/MainWindow.h"
#include "ui/UnifiedLogWidget.h"

#include <memory>

using namespace Contestprogramm;

namespace {

QList<QLineEdit*> entryFields(MainWindow& window)
{
    auto* row = window.findChild<QWidget*>(QLatin1String(UnifiedLogWidget::kEntryRowObjectName));
    return row ? row->findChildren<QLineEdit*>() : QList<QLineEdit*>{};
}

// Eine Ergebnisliste in der Form, die der ÖVSV-Auswerteserver ausgibt:
// Kopfzeile mit benannten Spalten, darunter je Station eine Zeile.
bool schreibeErgebnisliste(const QString& pfad)
{
    QFile f(pfad);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream out(&f);
    out << "Platz;Call;WWL;Punkte;QSOs\n";
    out << "1;OE3ABC;JN88CD;12345;210\n";
    out << "2;DL1XYZ;JO50AB;11000;190\n";
    out << "3;OK2QRP;JN99MM;9000;150\n";
    return true;
}

} // namespace

class TestLocatorKommtVonSelbst : public QObject
{
    Q_OBJECT

private slots:
    void typingAKnownCallsignFillsTheLocatorByItself();
    void aMovedStationCanStillBeTypedOverByHand();
    void theOwnLogBeatsTheImportedList();
    void theRealResultsListsHeaderFormIsUnderstood();

private:
    std::unique_ptr<AppController> makeController(QTemporaryDir& dir, const QString& file);
};

std::unique_ptr<AppController> TestLocatorKommtVonSelbst::makeController(QTemporaryDir& dir, const QString& file)
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
    // Kein QRZ: Stufe 3 darf hier gar nicht mitspielen, damit gemessen
    // wird, was die EINREICHUNGEN leisten.
    settings.callbookProvider = ContestSettings::CallbookProvider::None;
    controller->setSettings(settings);
    return controller;
}

void TestLocatorKommtVonSelbst::typingAKnownCallsignFillsTheLocatorByItself()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("auto.sqlite"));
    QVERIFY(controller);

    const QString liste = dir.filePath(QStringLiteral("ergebnisse.csv"));
    QVERIFY(schreibeErgebnisliste(liste));
    QString fehler;
    const auto summe = controller->callsignLocatorLookup().importCsvFile(liste, &fehler);
    qInfo() << "eingelesen:" << summe.imported << "Zeilen, uebersprungen:" << summe.skipped << fehler;
    QCOMPARE(summe.imported, 3);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    const QList<QLineEdit*> fields = entryFields(window);
    QVERIFY(fields.size() >= 4);
    QLineEdit* callField = fields.at(0);
    QLineEdit* gridField = fields.at(3);
    QVERIFY(gridField->text().isEmpty());

    callField->setFocus();
    QTest::keyClicks(callField, QStringLiteral("DL1XYZ"));
    // Die Abfrage feuert erst, wenn das Tippen zur Ruhe kommt -- also
    // warten wie ein Mensch, der das Rufzeichen fertig gesagt hat.
    QTRY_COMPARE_WITH_TIMEOUT(gridField->text(), QStringLiteral("JO50AB"), 3000);
    qInfo().noquote() << "nach dem Tippen von DL1XYZ steht im Locatorfeld:" << gridField->text();

    // Und ein Rufzeichen, das nirgends steht, fuellt nichts aus --
    // erfinden darf das Programm nichts.
    callField->clear();
    gridField->clear();
    QTest::keyClicks(callField, QStringLiteral("S59NIX"));
    QTest::qWait(1200);
    qInfo().noquote() << "nach S59NIX steht im Locatorfeld:"
                      << (gridField->text().isEmpty() ? QStringLiteral("(leer)") : gridField->text());
    QCOMPARE(gridField->text(), QString());
}

void TestLocatorKommtVonSelbst::aMovedStationCanStillBeTypedOverByHand()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("umgezogen.sqlite"));
    QVERIFY(controller);

    const QString liste = dir.filePath(QStringLiteral("ergebnisse.csv"));
    QVERIFY(schreibeErgebnisliste(liste));
    QCOMPARE(controller->callsignLocatorLookup().importCsvFile(liste).imported, 3);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    const QList<QLineEdit*> fields = entryFields(window);
    QVERIFY(fields.size() >= 4);
    QLineEdit* callField = fields.at(0);
    QLineEdit* nrField = fields.at(2);
    QLineEdit* gridField = fields.at(3);

    callField->setFocus();
    QTest::keyClicks(callField, QStringLiteral("OE3ABC"));
    QTRY_COMPARE_WITH_TIMEOUT(gridField->text(), QStringLiteral("JN88CD"), 3000);

    // Er fährt dieses Jahr von einem anderen Berg -- also von Hand
    // darüber, und das muss halten.
    gridField->setFocus();
    gridField->selectAll();
    QTest::keyClicks(gridField, QStringLiteral("JN77QT"));
    QCoreApplication::processEvents();
    QCOMPARE(gridField->text(), QStringLiteral("JN77QT"));

    // Und so wird es auch geloggt, nicht mit dem Wert aus der Liste.
    nrField->setFocus();
    QTest::keyClicks(nrField, QStringLiteral("042"));
    QTest::keyClick(nrField, Qt::Key_Return);
    QCoreApplication::processEvents();

    const QVector<QsoRecord> qsos = controller->database().qsosForContest(QStringLiteral("IARU_R1_VHF_UHF"));
    QCOMPARE(qsos.size(), 1);
    qInfo().noquote() << "geloggt wurde:" << qsos.first().callsign << qsos.first().gridSquare;
    QCOMPARE(qsos.first().gridSquare, QStringLiteral("JN77QT"));
}

// Die Rangfolge, die Martin vorgegeben hat: das eigene Log zaehlt mehr
// als jede Liste -- wer heute schon von dort gefahren ist, faehrt heute
// von dort.
void TestLocatorKommtVonSelbst::theOwnLogBeatsTheImportedList()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("rang.sqlite"));
    QVERIFY(controller);

    const QString liste = dir.filePath(QStringLiteral("ergebnisse.csv"));
    QVERIFY(schreibeErgebnisliste(liste));
    QCOMPARE(controller->callsignLocatorLookup().importCsvFile(liste).imported, 3);

    // OK2QRP steht in der Liste mit JN99MM -- heute aber schon mit
    // JN89AA im Log, auf dem anderen Band.
    QsoRecord r;
    r.callsign = QStringLiteral("OK2QRP");
    r.band = QStringLiteral("432");
    r.mode = QStringLiteral("SSB");
    r.timestampUtc = QDateTime::currentDateTimeUtc().addSecs(-1800).toString(Qt::ISODate);
    r.rstSent = QStringLiteral("59");
    r.rstRcvd = QStringLiteral("59");
    r.serialSent = 1;
    r.serialRcvd = 5;
    r.gridSquare = QStringLiteral("JN89AA");
    r.distanceKm = 200.0;
    r.contestId = QStringLiteral("IARU_R1_VHF_UHF");
    QVERIFY(controller->database().insertQso(r));

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    const QList<QLineEdit*> fields = entryFields(window);
    QVERIFY(fields.size() >= 4);
    fields.at(0)->setFocus();
    QTest::keyClicks(fields.at(0), QStringLiteral("OK2QRP"));
    QTRY_COMPARE_WITH_TIMEOUT(fields.at(3)->text(), QStringLiteral("JN89AA"), 3000);
    qInfo().noquote() << "eigenes Log gewinnt gegen die Liste:" << fields.at(3)->text();
}

// Die Form, die Martins wirkliche Liste hat (~/Longpath/
// ukw-ergebnislisten/ukw-ergebnisse-2020-2026.csv, aus den
// ÖVSV-Ergebnislisten und -Einreichungen 2020 bis 2026): Kopfzeile
// "Call,WWL,Quelle", mit KOMMA getrennt, und eine dritte Spalte, die
// nicht der Name der Station ist, sondern der Contest, aus dem die Zeile
// stammt. Die Pruefstaende oben nehmen Semikolon -- beides muss gehen,
// sonst laeuft er in einen leeren Import.
void TestLocatorKommtVonSelbst::theRealResultsListsHeaderFormIsUnderstood()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("echteform.sqlite"));
    QVERIFY(controller);

    const QString pfad = dir.filePath(QStringLiteral("echt.csv"));
    {
        QFile f(pfad);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&f);
        out << "Call,WWL,Quelle\n";
        out << "OE7/HB9WAM/P,JN57GA,2026 IARU R1 50 MHz\n";
        out << "9A/E75UA,JN75RP,2025 2. Subregionaler\n";
        out << "OE5SOS,JN67UT,2024 UKW-Contest\n";
    }

    QString fehler;
    const auto summe = controller->callsignLocatorLookup().importCsvFile(pfad, &fehler);
    qInfo() << "echte Form -- eingelesen:" << summe.imported << "uebersprungen:" << summe.skipped << fehler;
    QCOMPARE(summe.imported, 3);
    QCOMPARE(summe.skipped, 0);

    // Auch die Rufzeichen mit Schraegstrich muessen wiederzufinden sein
    // -- auf UKW faehrt halb Europa portabel.
    const auto portabel = controller->callsignLocatorLookup().lookupLocal(QStringLiteral("OE7/HB9WAM/P"));
    QVERIFY2(portabel.has_value(), "Ein portables Rufzeichen ist nicht wiederzufinden");
    QCOMPARE(*portabel, QStringLiteral("JN57GA"));
    const auto gast = controller->callsignLocatorLookup().lookupLocal(QStringLiteral("9A/E75UA"));
    QVERIFY2(gast.has_value(), "Ein Gastrufzeichen ist nicht wiederzufinden");
    QCOMPARE(*gast, QStringLiteral("JN75RP"));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestLocatorKommtVonSelbst tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_locator_kommt_von_selbst.moc"
