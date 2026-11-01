// Martin, 2026-09-27: "automatische speicherung immer wieder". Sie
// lief auch vorher -- jedes QSO sofort in die Datenbank, dazu jede
// Minute eine vollständige Kopie --, aber zu sehen war sie nur, wenn
// sie scheiterte. Zwischen zwei Kopien wusste niemand, ob überhaupt
// noch eine kommt.
//
// Jetzt steht der Zeitpunkt der letzten Kopie fest in der Fußzeile.
// Fest, nicht als Meldung: eine Meldung ist nach fünf Sekunden weg und
// streitet sich obendrein mit der Dupe-Meldung um dieselbe Zeile.

#include <QtTest>

#include <QApplication>
#include <QLabel>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"
#include "data/LogBackup.h"
#include "data/QsoRecord.h"
#include "ui/MainWindow.h"

#include <memory>

using namespace Contestprogramm;

class TestSicherungSichtbar : public QObject
{
    Q_OBJECT

private slots:
    void theFooterShowsWhenTheLogWasLastCopied();
};

void TestSicherungSichtbar::theFooterShowsWhenTheLogWasLastCopied()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = std::make_unique<AppController>();
    QVERIFY(controller->openDatabase(dir.filePath(QStringLiteral("sicherung.sqlite"))));
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.activeContestId = QStringLiteral("IARU_R1_VHF_UHF");
    settings.rigctldHost.clear();
    settings.rotor1Enabled = false;
    settings.rotor2Enabled = false;
    controller->setSettings(settings);

    MainWindow window(*controller);
    auto* badge = window.findChild<QLabel*>(QStringLiteral("backupStatus"));
    QVERIFY(badge);
    // Vor der ersten Kopie ein Strich, keine erfundene Zeit.
    QVERIFY2(badge->text().contains(QStringLiteral("Gesichert")), qPrintable(badge->text()));
    QVERIFY2(!badge->text().contains(QLatin1Char(':')) || badge->text().contains(QStringLiteral("—")),
              qPrintable(badge->text()));

    LogBackup* backup = controller->logBackup();
    QVERIFY(backup);
    // Ein QSO, damit es überhaupt etwas zu sichern gibt.
    QsoRecord r;
    r.callsign = QStringLiteral("DL1ABC");
    r.band = QStringLiteral("144");
    r.mode = QStringLiteral("SSB");
    r.timestampUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    r.contestId = settings.activeContestId;
    QVERIFY(controller->database().insertQso(r));

    QString error;
    const QString path = backup->backupNow(true, &error);
    QVERIFY2(!path.isEmpty(), qPrintable(error));
    QCoreApplication::processEvents();

    // Jetzt steht eine Uhrzeit da.
    const QString shown = badge->text();
    QVERIFY2(shown.contains(QStringLiteral("Gesichert")), qPrintable(shown));
    const QString now = QDateTime::currentDateTimeUtc().time().toString(QStringLiteral("HH:mm"));
    QVERIFY2(shown.contains(now), qPrintable(shown + QStringLiteral(" | erwartet ") + now));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestSicherungSichtbar tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_sicherung_sichtbar.moc"
