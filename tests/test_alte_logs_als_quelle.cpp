// Martin, 2026-09-27: "bekannte rufzeichen aus ukw contest logs und auch
// qzr, aber insbesondere ehemalige logs bei ukw sind primäre die
// benechmark."
//
// Die eigenen alten Logs sind also die erste Quelle. Datei > Listen laden
// > "Locator aus alten Logs (EDI/ADIF)..." liest sie ein. Dieser
// Pruefstand fuehrt echte Eigenheiten vor, die in einer wirklichen
// ADIF-Datei stehen -- gemessen an ~/Longpath/werkzeug/logbuch-sandbox/
// logbook.adi mit 9271 QSOs: Locatoren in Kleinschreibung ("JN88ee"),
// VIERSTELLIGE Locatoren ("JN17") und QSOs ohne jeden Locator.
//
// Der vierstellige ist der interessante Fall: er ist ein gueltiger
// Locator, aber fuer eine UKW-Einreichung zu grob. Im Eingabefeld sieht
// er fertig aus -- und ein Return darauf loggt ihn.

#include <QtTest>

#include <QApplication>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QTextStream>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/LogFileReader.h"
#include "core/Maidenhead.h"
#include "core/CallsignLocatorLookup.h"
#include "data/ContestDatabase.h"
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

// Eine ADIF-Datei mit genau den Eigenheiten, die in einer wirklichen
// stehen.
bool schreibeAdif(const QString& pfad)
{
    QFile f(pfad);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream out(&f);
    out << "Ein Kopf, den jedes Programm anders schreibt\n<EOH>\n";
    // Kleinschreibung -- so steht es wirklich drin.
    out << "<CALL:6>OE3ABC<GRIDSQUARE:6>JN88ee<BAND:4>2m<QSO_DATE:8>20240601<TIME_ON:4>1200<EOR>\n";
    // Vierstellig.
    out << "<CALL:6>DL1XYZ<GRIDSQUARE:4>JN17<BAND:4>2m<QSO_DATE:8>20240601<TIME_ON:4>1210<EOR>\n";
    // Ganz ohne Locator.
    out << "<CALL:6>OK2QRP<BAND:4>2m<QSO_DATE:8>20240601<TIME_ON:4>1220<EOR>\n";
    // Sauberer Sechssteller, gross geschrieben.
    out << "<CALL:6>HA5ABC<GRIDSQUARE:6>JN97MM<BAND:4>2m<QSO_DATE:8>20240601<TIME_ON:4>1230<EOR>\n";
    return true;
}

// Was MainWindow::importOldLogs() tut, ohne den Dateidialog: lesen,
// gueltige Locatoren uebernehmen.
struct ImportErgebnis {
    int uebernommen = 0;
    int ohneLocator = 0;
};

ImportErgebnis importiere(AppController& controller, const QString& pfad)
{
    ImportErgebnis e;
    QFile file(pfad);
    if (!file.open(QIODevice::ReadOnly)) {
        return e;
    }
    const QVector<ImportedQso> qsos = LogFileReader::parse(file.readAll(), pfad);
    for (const ImportedQso& qso : qsos) {
        if (isValidGridSquare(qso.grid)) {
            controller.database().upsertImportedLocator(qso.callsign, qso.grid);
            ++e.uebernommen;
        } else {
            ++e.ohneLocator;
        }
    }
    return e;
}

} // namespace

class TestAlteLogsAlsQuelle : public QObject
{
    Q_OBJECT

private slots:
    void anAdifFilesQuirksAllSurviveTheImport();
    void aFourCharacterLocatorNeverLandsInTheEntryField();
};

std::unique_ptr<AppController> makeController(QTemporaryDir& dir, const QString& file)
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
    settings.callbookProvider = ContestSettings::CallbookProvider::None;
    controller->setSettings(settings);
    return controller;
}

void TestAlteLogsAlsQuelle::anAdifFilesQuirksAllSurviveTheImport()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("alt.sqlite"));
    QVERIFY(controller);

    const QString pfad = dir.filePath(QStringLiteral("alt.adi"));
    QVERIFY(schreibeAdif(pfad));
    const ImportErgebnis e = importiere(*controller, pfad);
    qInfo() << "uebernommen:" << e.uebernommen << "ohne Locator:" << e.ohneLocator;
    QCOMPARE(e.uebernommen, 3);   // klein, vierstellig, sechsstellig
    QCOMPARE(e.ohneLocator, 1);   // das ohne GRIDSQUARE

    CallsignLocatorLookup& lookup = controller->callsignLocatorLookup();
    // Kleinschreibung muss gross zurueckkommen, sonst findet der
    // Nachschlag sie spaeter nicht.
    const auto klein = lookup.lookupLocal(QStringLiteral("OE3ABC"));
    QVERIFY2(klein.has_value(), "Ein klein geschriebener Locator ging verloren");
    QCOMPARE(*klein, QStringLiteral("JN88EE"));
    const auto gross = lookup.lookupLocal(QStringLiteral("HA5ABC"));
    QVERIFY(gross.has_value());
    QCOMPARE(*gross, QStringLiteral("JN97MM"));
    // Das QSO ohne Locator darf gar nichts hinterlassen.
    QVERIFY2(!lookup.lookupLocal(QStringLiteral("OK2QRP")).has_value(),
             "Ein QSO ohne Locator hat trotzdem etwas hinterlassen");
}

// Der Fall, um den es geht.
void TestAlteLogsAlsQuelle::aFourCharacterLocatorNeverLandsInTheEntryField()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("vier.sqlite"));
    QVERIFY(controller);

    const QString pfad = dir.filePath(QStringLiteral("alt.adi"));
    QVERIFY(schreibeAdif(pfad));
    QCOMPARE(importiere(*controller, pfad).uebernommen, 3);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    const QList<QLineEdit*> fields = entryFields(window);
    QVERIFY(fields.size() >= 4);
    QLineEdit* callField = fields.at(0);
    QLineEdit* gridField = fields.at(3);

    // Erst der saubere Sechssteller: der soll kommen.
    callField->setFocus();
    QTest::keyClicks(callField, QStringLiteral("HA5ABC"));
    QTRY_COMPARE_WITH_TIMEOUT(gridField->text(), QStringLiteral("JN97MM"), 3000);

    // Jetzt der Viersteller. Er darf NICHT im Feld landen: dort sieht er
    // fertig aus, und ein Return loggt ihn -- eine UKW-Einreichung mit
    // vierstelligem Locator ist die Punkte nicht wert (IARU R1 rechnet
    // aus dem Sechssteller, und die Auswertung will ihn auch).
    callField->clear();
    gridField->clear();
    QTest::keyClicks(callField, QStringLiteral("DL1XYZ"));
    QTest::qWait(1500);
    qInfo().noquote() << "nach DL1XYZ (vierstellig bekannt) steht im Locatorfeld:"
                      << (gridField->text().isEmpty() ? QStringLiteral("(leer)") : gridField->text());
    QVERIFY2(gridField->text().size() != 4,
             qPrintable(QStringLiteral("Ein vierstelliger Locator wurde vorgeschlagen: %1").arg(gridField->text())));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestAlteLogsAlsQuelle tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_alte_logs_als_quelle.moc"
