// Am 2026-10-08 lagen nach einem Prüflauf zwei QSOs im Papierkorb, und
// niemand konnte sagen, wodurch: die Datenbank merkt sich den Zustand,
// nicht den Weg dorthin. Das Journal (QsoJournal) hält zwar jedes
// geloggte QSO fest -- aber es sagt nichts darüber, was danach damit
// geschah.
//
// Seitdem schreibt das Programm jede Löschung und jedes Zurückholen in
// eine zweite Datei neben dem Journal. Und weil die Nummer eines
// gelöschten QSO sofort wieder vergeben wird, prüft das Zurückholen,
// ob sie inzwischen ein zweites Mal rausging -- im Contest wäre das
// ein Fehler, den der Auswerter findet und der Operator nicht.
//
// Beides wird hier an der echten Bedienung gemessen.

#include <QtTest>

#include "Tastatureingabe.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QFile>
#include <QLineEdit>
#include <QStatusBar>
#include <QTableView>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"
#include "data/QsoJournal.h"
#include "data/QsoRecord.h"
#include "ui/MainWindow.h"
#include "ui/UnifiedLogWidget.h"

using namespace Contestprogramm;

namespace {

QList<QLineEdit*> entryFields(MainWindow& window)
{
    auto* row = window.findChild<QWidget*>(QLatin1String(UnifiedLogWidget::kEntryRowObjectName));
    return row ? row->findChildren<QLineEdit*>() : QList<QLineEdit*>{};
}

int rowForCallsign(QAbstractItemModel* model, const QString& callsign)
{
    for (int row = 0; row < model->rowCount(); ++row) {
        if (model->index(row, UnifiedLogWidget::ColumnCall).data().toString() == callsign) {
            return row;
        }
    }
    return -1;
}

void logge(MainWindow& window, const QString& call, const QString& grid)
{
    const QList<QLineEdit*> fields = entryFields(window);
    QVERIFY(!fields.isEmpty());
    fields.at(0)->setFocus();
    QTest::keyClicks(fields.at(0), call);
    QTest::keyClick(fields.at(0), Qt::Key_Tab);
    Contestprogramm::Pruefstand::tippeInFokus(&window, QStringLiteral("001"));
    Contestprogramm::Pruefstand::tasteInFokus(&window, Qt::Key_Tab);
    Contestprogramm::Pruefstand::tippeInFokus(&window, grid);
    Contestprogramm::Pruefstand::tasteInFokus(&window, Qt::Key_Return);
    QCoreApplication::processEvents();
}

} // namespace

class TestVorgangsprotokoll : public QObject
{
    Q_OBJECT

private slots:
    void loeschenUndZurueckholenStehenImProtokoll();
};

void TestVorgangsprotokoll::loeschenUndZurueckholenStehenImProtokoll()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppController controller;
    QVERIFY(controller.openDatabase(dir.filePath(QStringLiteral("vorgang.sqlite"))));
    ContestSettings settings = controller.settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.activeContestId = QStringLiteral("IARU_R1_VHF_UHF");
    settings.rigctldHost.clear();
    settings.rotor1Enabled = false;
    settings.rotor2Enabled = false;
    settings.esmEnabled = false;
    controller.setSettings(settings);

    MainWindow window(controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.activateWindow();
    (void)QTest::qWaitForWindowActive(&window);

    // QTRY_, nicht QCOMPARE: zwischen dem Enter und dem Satz in der
    // Datenbank liegen Ereignisse, und ein einzelnes processEvents()
    // holt nicht immer alle ein. Gemessen am 2026-10-20: dieser
    // Prüfstand fiel ohne Fremdlast 2 von 6 Mal durch, unter Volllast
    // 1 von 6 -- immer an einer Zusicherung direkt nach einer Eingabe.
    logge(window, QStringLiteral("DL1ABC"), QStringLiteral("JO60AA"));
    logge(window, QStringLiteral("OK2XYZ"), QStringLiteral("JN89AB"));
    QTRY_COMPARE(controller.database().qsoCountForContest(settings.activeContestId), 2);

    auto* table = window.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(table);
    QAbstractItemModel* model = table->model();
    const int zeile = rowForCallsign(model, QStringLiteral("OK2XYZ"));
    QVERIFY(zeile >= 0);
    table->setCurrentIndex(model->index(zeile, UnifiedLogWidget::ColumnCall));
    table->setFocus();
    QTest::keyClick(table, Qt::Key_Backspace);
    QTRY_COMPARE(controller.database().qsoCountForContest(settings.activeContestId), 1);

    // Die Nummer 2 ist jetzt wieder frei und geht an die nächste
    // Station -- so soll es sein, ein Enter zuviel darf kein Loch
    // hinterlassen.
    logge(window, QStringLiteral("HA5QRP"), QStringLiteral("JN97AA"));
    // Die Abfrage gehört IN die Wartebedingung: QTRY_ wertet seinen
    // Ausdruck wiederholt aus, eine vorher gezogene Kopie bliebe alt.
    const auto nummerFuer = [&](const QString& call) -> int {
        for (const QsoRecord& q : controller.database().qsosForContest(settings.activeContestId)) {
            if (q.callsign == call && q.serialSent) {
                return *q.serialSent;
            }
        }
        return -1;   // noch nicht da
    };
    QTRY_COMPARE(controller.database().qsosForContest(settings.activeContestId).size(), 2);
    // Die Nummer des gelöschten QSO muss wieder vergeben werden.
    QTRY_COMPARE(nummerFuer(QStringLiteral("HA5QRP")), 2);

    // Und jetzt kommt das gelöschte QSO zurück: zwei Stationen mit
    // derselben gesendeten Nummer. Das muss der Operator erfahren.
    // Über die Taste, nicht über den Slot: Strg+Z (auf dem Mac Cmd+Z,
    // QKeySequence::Undo löst das selbst auf) ist der Weg, den Martins
    // Finger nehmen.
    const QKeySequence undo = QKeySequence(QKeySequence::Undo);
    QVERIFY(undo.count() > 0);
    const QKeyCombination combo = undo[0];
    QTest::keyClick(&window, combo.key(), combo.keyboardModifiers());
    QCoreApplication::processEvents();
    const QString meldung = window.statusBar()->currentMessage();
    qInfo().noquote() << "Statuszeile:" << meldung;
    QVERIFY2(meldung.contains(QStringLiteral("Nr. 2")) && meldung.contains(QStringLiteral("HA5QRP")),
             qPrintable(QStringLiteral("Keine Warnung über die doppelte Nummer: %1").arg(meldung)));

    // Beide Vorgänge stehen mit Zeitstempel in der Protokolldatei.
    QsoJournal* journal = controller.qsoJournal();
    QVERIFY(journal);
    QFile protokoll(journal->vorgangsPfad());
    QVERIFY2(protokoll.exists(), qPrintable(journal->vorgangsPfad()));
    QVERIFY(protokoll.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString inhalt = QString::fromUtf8(protokoll.readAll());
    qInfo().noquote() << "Protokoll:\n" << inhalt;
    QVERIFY(inhalt.contains(QStringLiteral("GELOESCHT")));
    QVERIFY(inhalt.contains(QStringLiteral("ZURUECKGEHOLT")));
    QVERIFY(inhalt.contains(QStringLiteral("OK2XYZ")));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestVorgangsprotokoll tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_vorgangsprotokoll.moc"
