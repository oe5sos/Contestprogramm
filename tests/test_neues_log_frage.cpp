// Martin, 2026-09-23: "wenn ich auf datei neu gehe werde ich gefragt,
// ob ich das alte löschen mag. ich sagte nein, aber die logs bleiben
// bestehen." Die Frage lautete "Jetzt archivieren?" mit Ja/Nein, und
// "archivieren" las sich wie "wegwerfen" -- er wollte ein leeres Log
// und die alten QSOs behalten, also genau das, was Ja getan hätte.
//
// Dieser Prüfstand klickt den echten Dialog an: einmal Abbrechen
// (nichts verschoben), einmal "Neues Log beginnen" (alles im Archiv,
// nichts gelöscht). Und er liest den Text nach, weil genau der den
// Fehler verursacht hat.

#include <QtTest>

#include <QAbstractButton>
#include <QApplication>
#include <QMessageBox>
#include <QTemporaryDir>
#include <QTimer>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"
#include "data/QsoRecord.h"
#include "ui/MainWindow.h"

#include <memory>

using namespace Contestprogramm;

namespace {

// Der Dialog ist modal: trigger() kehrt erst zurück, wenn er zu ist.
// Also vorher einen Wecker stellen, der ihn sucht, prüft und drückt.
struct DialogProbe {
    QString title;
    QString informative;
    QStringList buttonTexts;
    bool found = false;
};

void clickDialogButton(DialogProbe& probe, const QString& buttonText)
{
    QTimer::singleShot(0, [&probe, buttonText]() {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (!box) {
            return; // nicht gefunden -- der Prüfstand fällt unten durch
        }
        probe.found = true;
        probe.title = box->text();
        probe.informative = box->informativeText();
        for (QAbstractButton* button : box->buttons()) {
            probe.buttonTexts << button->text();
            if (button->text() == buttonText) {
                QTimer::singleShot(0, [button]() { button->click(); });
            }
        }
    });
}

} // namespace

class TestNeuesLogFrage : public QObject
{
    Q_OBJECT

private slots:
    void cancellingLeavesTheLogAlone();
    void startingMovesTheQsosToTheArchiveAndDeletesNothing();

private:
    std::unique_ptr<AppController> makeController(QTemporaryDir& dir, const QString& file);
};

std::unique_ptr<AppController> TestNeuesLogFrage::makeController(QTemporaryDir& dir, const QString& file)
{
    auto controller = std::make_unique<AppController>();
    if (!controller->openDatabase(dir.filePath(file))) {
        return nullptr;
    }
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.activeContestId = QStringLiteral("KW_UEBUNG");
    settings.rigctldHost.clear();
    settings.rotor1Enabled = false;
    settings.rotor2Enabled = false;
    controller->setSettings(settings);

    for (int i = 1; i <= 3; ++i) {
        QsoRecord r;
        r.callsign = QStringLiteral("DL%1ABC").arg(i);
        r.band = QStringLiteral("14");
        r.mode = QStringLiteral("CW");
        r.timestampUtc = QDateTime::currentDateTimeUtc().addSecs(-60 * i).toString(Qt::ISODate);
        r.serialSent = i;
        r.contestId = settings.activeContestId;
        if (!controller->database().insertQso(r)) {
            return nullptr;
        }
    }
    return controller;
}

void TestNeuesLogFrage::cancellingLeavesTheLogAlone()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("abbruch.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    auto* action = window.findChild<QAction*>(QStringLiteral("newLogAction"));
    QVERIFY(action);

    DialogProbe probe;
    clickDialogButton(probe, QStringLiteral("Abbrechen"));
    action->trigger();
    QVERIFY2(probe.found, "Der Dialog kam nicht");

    // Der Text, der den Fehler verursacht hat: die erste Zeile sagt,
    // was der Menüpunkt verspricht, und dass nichts gelöscht wird,
    // steht vor allem anderen.
    QCOMPARE(probe.title, QStringLiteral("Neues Log beginnen?"));
    QVERIFY2(probe.informative.startsWith(QStringLiteral("Gelöscht wird nichts")), qPrintable(probe.informative));
    // Auf den Knöpfen steht die Handlung, nicht "Ja"/"Nein".
    QVERIFY2(probe.buttonTexts.contains(QStringLiteral("Neues Log beginnen")), qPrintable(probe.buttonTexts.join(QLatin1Char('|'))));
    QVERIFY2(probe.buttonTexts.contains(QStringLiteral("Abbrechen")), qPrintable(probe.buttonTexts.join(QLatin1Char('|'))));
    QVERIFY2(!probe.buttonTexts.contains(QStringLiteral("&Yes")), "keine Ja/Nein-Knöpfe mehr");

    // Abgebrochen: alles liegt, wo es lag.
    QCOMPARE(controller->database().qsosForContest(QStringLiteral("KW_UEBUNG")).size(), 3);
    QCOMPARE(controller->database().contestIdsInLog().size(), 1);
}

void TestNeuesLogFrage::startingMovesTheQsosToTheArchiveAndDeletesNothing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("neu.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    auto* action = window.findChild<QAction*>(QStringLiteral("newLogAction"));
    QVERIFY(action);

    DialogProbe probe;
    clickDialogButton(probe, QStringLiteral("Neues Log beginnen"));
    action->trigger();
    QVERIFY2(probe.found, "Der Dialog kam nicht");

    // Das laufende Log ist leer ...
    QCOMPARE(controller->database().qsosForContest(QStringLiteral("KW_UEBUNG")).size(), 0);
    // ... und die drei QSOs stehen vollzählig unter der Archivkennung.
    const QStringList ids = controller->database().contestIdsInLog();
    QCOMPARE(ids.size(), 1);
    QVERIFY2(ids.first().startsWith(QStringLiteral("KW_UEBUNG@")), qPrintable(ids.first()));
    QCOMPARE(controller->database().qsosForContest(ids.first()).size(), 3);
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestNeuesLogFrage tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_neues_log_frage.moc"
