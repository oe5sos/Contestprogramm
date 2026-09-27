// Der Absturz vom 2026-09-27 kam so: readings() im Rate-Panel liefert
// fünf Kacheln statt sechs, wenn der Contest keinen Multiplikator
// führt -- gezeichnet wurden aber sechs. Gefunden wurde er nicht von
// einem Prüfstand, sondern beim Starten des Programms.
//
// Dieser hier schließt die Lücke, durch die er gekommen ist: er baut
// das ECHTE Fenster für JEDE mitgelieferte Contest-Definition, in
// mehreren Größen, und zeichnet es. Contest-Definitionen unterscheiden
// sich in Bändern, Betriebsarten, Wertung und Multiplikator -- also
// genau in dem, woran die Panels ihre Anzeige aufhängen.
//
// Geprüft wird zweierlei: es bricht nicht ab (ein QList::at daneben
// beendet den Prozess, der Prüfstand fällt mit ihm), und es steht
// wirklich etwas auf dem Bild statt einer leeren Fläche.

#include <QtTest>

#include <QApplication>
#include <QImage>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"
#include "data/ContestDefinition.h"
#include "data/QsoRecord.h"
#include "ui/MainWindow.h"

#include <memory>

using namespace Contestprogramm;

class TestAlleConttesteZeichnen : public QObject
{
    Q_OBJECT

private slots:
    void everyContestDefinitionPaintsAtEverySize();
};

namespace {

// Wie viele verschiedene Farben das Bild trägt -- ein leeres Fenster
// hat eine Handvoll, ein gezeichnetes Dutzende.
int distinctColours(const QImage& image)
{
    QSet<QRgb> seen;
    for (int y = 0; y < image.height(); y += 3) {
        for (int x = 0; x < image.width(); x += 3) {
            seen.insert(image.pixel(x, y));
            if (seen.size() > 200) {
                return seen.size();
            }
        }
    }
    return seen.size();
}

} // namespace

void TestAlleConttesteZeichnen::everyContestDefinitionPaintsAtEverySize()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    AppController probe;
    QVERIFY(probe.openDatabase(dir.filePath(QStringLiteral("probe.sqlite"))));
    QStringList contestIds;
    for (const ContestDefinition& def : probe.availableContestDefinitions()) {
        contestIds << def.id();
    }
    QVERIFY2(contestIds.size() >= 5, qPrintable(QStringLiteral("nur %1 Definitionen gefunden").arg(contestIds.size())));

    int n = 0;
    for (const QString& contestId : contestIds) {
        auto controller = std::make_unique<AppController>();
        QVERIFY(controller->openDatabase(dir.filePath(QStringLiteral("cp%1.sqlite").arg(n++))));
        ContestSettings settings = controller->settings();
        // Ohne Rufzeichen öffnet das Fenster beim ersten Start einen
        // modalen Dialog und der Prüfstand bliebe stehen.
        settings.ownCallsign = QStringLiteral("OE5SOS");
        settings.ownGrid = QStringLiteral("JN67UT");
        settings.activeContestId = contestId;
        settings.rigctldHost.clear();
        settings.rotor1Enabled = false;
        settings.rotor2Enabled = false;
        controller->setSettings(settings);

        // Ein paar QSOs, damit die Panels auch etwas zu zeigen haben --
        // ein leeres Log nimmt andere Wege durch dieselbe Anzeige.
        const ContestDefinition* def = controller->findContestDefinition(contestId);
        QVERIFY2(def, qPrintable(contestId));
        const QString band = def->bands().isEmpty() ? QStringLiteral("144") : def->bands().first();
        for (int i = 1; i <= 4; ++i) {
            QsoRecord r;
            r.callsign = QStringLiteral("DL%1ABC").arg(i);
            r.band = band;
            r.mode = def->modes().isEmpty() ? QStringLiteral("SSB") : def->modes().first();
            r.timestampUtc = QDateTime::currentDateTimeUtc().addSecs(-600 * i).toString(Qt::ISODate);
            r.gridSquare = QStringLiteral("JN58SD");
            r.distanceKm = 233.0;
            r.serialSent = i;
            r.serialRcvd = i;
            r.contestId = contestId;
            QVERIFY(controller->database().insertQso(r));
        }

        MainWindow window(*controller);
        for (const QSize& size : {QSize(1680, 1000), QSize(1200, 760), QSize(900, 620)}) {
            window.resize(size);
            window.show();
            for (int i = 0; i < 12; ++i) {
                QCoreApplication::processEvents();
                QTest::qWait(10);
            }
            const QImage shot = window.grab().toImage();
            QVERIFY2(!shot.isNull(), qPrintable(contestId));
            QVERIFY2(distinctColours(shot) > 20,
                      qPrintable(QStringLiteral("%1 bei %2x%3 wirkt leer")
                                     .arg(contestId).arg(size.width()).arg(size.height())));
        }
    }
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestAlleConttesteZeichnen tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_alle_conteste_zeichnen.moc"
