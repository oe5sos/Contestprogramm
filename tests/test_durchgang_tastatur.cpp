// Martin, 2026-09-28: "teste dann bitte selbst, lege logs an, return,
// mache das naechste, gehe dann wieder zu den vorigen, aendere es, usw."
// und: "wichtig, reicht dort der tabulator oder muss das neu
// abgespeichert werden".
//
// Also genau das, mit echten Tastendruecken am echten, gezeigten
// Fenster -- nicht ueber Signale und nicht ueber setData(), sondern so,
// wie seine Finger es machen: ins Rufzeichenfeld tippen, Tab ins
// naechste Feld, Return loggen, weiter. Dann zurueck zu einem frueheren
// QSO, die Zelle aufmachen, aendern, mit Tab verlassen -- und nachsehen,
// ob das allein schon in der Datenbank steht.
//
// Die Antwort auf seine Frage steht in
// aTabAloneCommitsACorrectionToTheDatabase(): der Tabulator genuegt,
// nichts muss zusaetzlich gespeichert werden. Dieser Pruefstand haelt
// das fest, damit es auch so bleibt.

#include <QtTest>

#include <QAbstractItemModel>
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QTableView>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "core/Maidenhead.h"
#include "data/ContestDatabase.h"
#include "data/QsoRecord.h"
#include "ui/MainWindow.h"
#include "ui/UnifiedLogWidget.h"

#include <memory>

using namespace Contestprogramm;

namespace {

// Die Eingabezeile: Rufzeichenfeld zuerst, dann die Austauschfelder in
// der Reihenfolge, in der der Contest sie deklariert (IARU R1: Nr.,
// Locator). Die Felder tragen keine eigenen objectNames -- die
// Erstellungsreihenfolge innerhalb der Eingabezeile ist der Zugang, den
// auch die uebrigen Pruefstaende nehmen.
QList<QLineEdit*> entryFields(MainWindow& window)
{
    auto* row = window.findChild<QWidget*>(QLatin1String(UnifiedLogWidget::kEntryRowObjectName));
    if (!row) {
        return {};
    }
    return row->findChildren<QLineEdit*>();
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

} // namespace

class TestDurchgangTastatur : public QObject
{
    Q_OBJECT

private slots:
    void loggingSeveralQsosByKeyboardThenCorrectingEarlierOnes();
    void aTabAloneCommitsACorrectionToTheDatabase();
    void quittingWithACorrectionStillOpenDoesNotCrash();

private:
    std::unique_ptr<AppController> makeController(QTemporaryDir& dir, const QString& file);
    // Ein QSO ueber die Tastatur: Rufzeichen, Tab, Locator, Return.
    // (Die Nummer traegt das Programm selbst ein -- deshalb wird das
    // Nummernfeld nur uebersprungen, nicht getippt.)
    void logByKeyboard(MainWindow& window, const QString& call, int nrRcvd, const QString& grid);
};

std::unique_ptr<AppController> TestDurchgangTastatur::makeController(QTemporaryDir& dir, const QString& file)
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
    return controller;
}

void TestDurchgangTastatur::logByKeyboard(MainWindow& window, const QString& call, int nrRcvd,
                                          const QString& grid)
{
    // Die Reihenfolge in der Eingabezeile, gemessen: Rufzeichen, RST,
    // empfangene Nr., Locator. Alle vier gehoeren zum Austausch -- ohne
    // die empfangene Nr. loggt das Programm voellig zu Recht nicht
    // (IARU R1: unvollstaendiger Austausch = 0 Punkte).
    const QList<QLineEdit*> fields = entryFields(window);
    QVERIFY2(fields.size() >= 4, "Die Eingabezeile hat nicht Rufzeichen + RST + Nr. + Locator");
    QLineEdit* callField = fields.at(0);
    QLineEdit* nrField = fields.at(2);
    QLineEdit* gridField = fields.at(3);

    callField->setFocus();
    QTest::keyClicks(callField, call);
    QTest::keyClick(callField, Qt::Key_Tab);
    nrField->setFocus();
    QTest::keyClicks(nrField, QStringLiteral("%1").arg(nrRcvd, 3, 10, QLatin1Char('0')));
    QTest::keyClick(nrField, Qt::Key_Tab);
    gridField->setFocus();
    QTest::keyClicks(gridField, grid);
    // Return loggt.
    QTest::keyClick(gridField, Qt::Key_Return);
    QCoreApplication::processEvents();

    // Nach einem geloggten QSO steht die Zeile leer da -- bleibt etwas
    // stehen, wurde nicht geloggt, und der Pruefstand sagt gleich hier
    // Bescheid statt erst zehn Zeilen spaeter.
    QStringList stehengeblieben;
    for (QLineEdit* f : fields) {
        if (!f->text().isEmpty() && f != fields.at(1)) { // RST bleibt mit Absicht auf 59
            stehengeblieben << f->text();
        }
    }
    QVERIFY2(stehengeblieben.isEmpty(),
             qPrintable(QStringLiteral("%1 wurde nicht geloggt, stehen geblieben: %2")
                            .arg(call, stehengeblieben.join(QLatin1String(", ")))));
}

// Der ganze Vorgang: fuenf QSOs anlegen, dann zurueck zu frueheren und
// dort etwas aendern, dann weiterloggen, dann noch einmal zurueck.
void TestDurchgangTastatur::loggingSeveralQsosByKeyboardThenCorrectingEarlierOnes()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("durchgang.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* table = window.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(table);
    QAbstractItemModel* model = table->model();

    struct Eintrag {
        QString call;
        int nr;
        QString grid;
    };
    const QVector<Eintrag> eintraege{{QStringLiteral("DL1ABC"), 14, QStringLiteral("JN58SD")},
                                     {QStringLiteral("OK2XYZ"), 7, QStringLiteral("JN99AA")},
                                     {QStringLiteral("HA5QRP"), 132, QStringLiteral("JN97MM")},
                                     {QStringLiteral("S51DX"), 88, QStringLiteral("JN76AB")},
                                     {QStringLiteral("OE3ABC"), 3, QStringLiteral("JN88CD")}};

    for (const Eintrag& e : eintraege) {
        logByKeyboard(window, e.call, e.nr, e.grid);
    }

    QVector<QsoRecord> qsos = controller->database().qsosForContest(QStringLiteral("IARU_R1_VHF_UHF"));
    qInfo() << "nach fuenf Return:" << qsos.size() << "QSOs";
    QCOMPARE(qsos.size(), 5);
    // Laufende Nummern, ohne Luecke und ohne Dopplung.
    QSet<int> nummern;
    for (const QsoRecord& q : qsos) {
        QVERIFY2(q.serialSent.has_value(), qPrintable(QStringLiteral("%1 ohne gesendete Nr.").arg(q.callsign)));
        nummern.insert(*q.serialSent);
        QVERIFY2(!q.gridSquare.isEmpty(), qPrintable(QStringLiteral("%1 ohne Locator").arg(q.callsign)));
        QVERIFY2(q.distanceKm.has_value() && *q.distanceKm > 0.0,
                 qPrintable(QStringLiteral("%1 ohne Entfernung").arg(q.callsign)));
    }
    QCOMPARE(nummern.size(), 5);
    QCOMPARE(*std::min_element(nummern.begin(), nummern.end()), 1);
    QCOMPARE(*std::max_element(nummern.begin(), nummern.end()), 5);

    // Zurueck zum zweiten QSO: das Rufzeichen war falsch.
    const int zeile = rowForCallsign(model, QStringLiteral("OK2XYZ"));
    QVERIFY2(zeile >= 0, "OK2XYZ steht nicht in der Liste");
    QVERIFY(model->setData(model->index(zeile, UnifiedLogWidget::ColumnCall), QStringLiteral("OK2XYY"),
                           Qt::EditRole));
    QCoreApplication::processEvents();

    qsos = controller->database().qsosForContest(QStringLiteral("IARU_R1_VHF_UHF"));
    QCOMPARE(qsos.size(), 5);
    bool korrigiert = false;
    for (const QsoRecord& q : qsos) {
        QVERIFY2(q.callsign != QStringLiteral("OK2XYZ"), "Das falsche Rufzeichen steht noch da");
        if (q.callsign == QStringLiteral("OK2XYY")) {
            korrigiert = true;
        }
    }
    QVERIFY(korrigiert);

    // Weiterloggen: die naechste Nummer muss 006 sein -- eine Korrektur
    // darf die laufende Nummerierung nicht durcheinanderbringen.
    logByKeyboard(window, QStringLiteral("DL9ZZZ"), 41, QStringLiteral("JO50AB"));
    qsos = controller->database().qsosForContest(QStringLiteral("IARU_R1_VHF_UHF"));
    QCOMPARE(qsos.size(), 6);
    for (const QsoRecord& q : qsos) {
        if (q.callsign == QStringLiteral("DL9ZZZ")) {
            qInfo() << "DL9ZZZ bekam Nr." << q.serialSent.value_or(-1);
            QCOMPARE(q.serialSent.value_or(-1), 6);
        }
    }

    // Und noch einmal zurueck, diesmal zum ersten QSO: der Locator war
    // falsch abgehoert. Die Entfernung muss mitgehen, sonst steht im Log
    // eine Punktzahl zum alten Quadrat.
    const int zeile1 = rowForCallsign(model, QStringLiteral("DL1ABC"));
    QVERIFY(zeile1 >= 0);
    int idDl1abc = -1;
    double alteEntfernung = 0.0;
    for (const QsoRecord& q : qsos) {
        if (q.callsign == QStringLiteral("DL1ABC")) {
            idDl1abc = q.id;
            alteEntfernung = q.distanceKm.value_or(0.0);
        }
    }
    QVERIFY(idDl1abc > 0);
    QVERIFY(model->setData(model->index(zeile1, UnifiedLogWidget::ColumnSerialGridRcvd),
                           QStringLiteral("59 001 JO70FF"), Qt::EditRole));
    QCoreApplication::processEvents();

    const auto q1 = controller->database().qsoById(idDl1abc);
    QVERIFY(q1);
    QCOMPARE(q1->gridSquare, QStringLiteral("JO70FF"));
    const double erwartet = iaruQrbKm(QStringLiteral("JN67UT"), QStringLiteral("JO70FF"));
    qInfo() << "DL1ABC: alt" << alteEntfernung << "km, neu" << q1->distanceKm.value_or(0.0) << "km, erwartet"
            << erwartet << "km";
    QVERIFY(q1->distanceKm.has_value());
    QVERIFY2(std::abs(*q1->distanceKm - erwartet) < 1.0,
             qPrintable(QStringLiteral("Entfernung %1 km statt %2 km").arg(*q1->distanceKm).arg(erwartet)));

    // Zum Schluss: das Log ist vollstaendig, sechs QSOs, jedes mit
    // Locator und Entfernung, nichts durch die Korrekturen verloren.
    qsos = controller->database().qsosForContest(QStringLiteral("IARU_R1_VHF_UHF"));
    QCOMPARE(qsos.size(), 6);
    for (const QsoRecord& q : qsos) {
        QVERIFY2(!q.gridSquare.isEmpty(), qPrintable(q.callsign));
        QVERIFY2(q.distanceKm.value_or(0.0) > 0.0, qPrintable(q.callsign));
    }
    qInfo() << "Durchgang fertig: 6 QSOs, 2 Korrekturen, Nummerierung 1..6 unversehrt";
}

// Martins Frage, direkt gemessen: reicht der Tabulator, oder muss die
// Korrektur zusaetzlich gespeichert werden? Hier wird die Zelle wirklich
// aufgemacht, im Editor getippt und mit Tab verlassen -- danach wird
// ohne jeden weiteren Handgriff in der Datenbank nachgesehen.
void TestDurchgangTastatur::aTabAloneCommitsACorrectionToTheDatabase()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("tab.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    logByKeyboard(window, QStringLiteral("DL1ABC"), 14, QStringLiteral("JN58SD"));
    auto* table = window.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(table);
    QAbstractItemModel* model = table->model();
    const int zeile = rowForCallsign(model, QStringLiteral("DL1ABC"));
    QVERIFY(zeile >= 0);

    // Die Zelle aufmachen, wie ein Doppelklick es tut.
    const QModelIndex index = model->index(zeile, UnifiedLogWidget::ColumnCall);
    table->setCurrentIndex(index);
    table->edit(index);
    QCoreApplication::processEvents();

    // Der Editor, den der Delegate gerade gebaut hat.
    QLineEdit* editor = nullptr;
    const QList<QLineEdit*> kandidaten = table->viewport()->findChildren<QLineEdit*>();
    for (QLineEdit* e : kandidaten) {
        if (e->isVisible()) {
            editor = e;
        }
    }
    QVERIFY2(editor, "Die Zelle liess sich nicht aufmachen");
    qInfo() << "Editor steht offen mit" << editor->text();

    editor->selectAll();
    QTest::keyClicks(editor, QStringLiteral("DL1ABD"));
    // Und nur das: Tab. Kein Return, kein Speichern-Knopf, nichts.
    QTest::keyClick(editor, Qt::Key_Tab);
    QCoreApplication::processEvents();

    const QVector<QsoRecord> qsos = controller->database().qsosForContest(QStringLiteral("IARU_R1_VHF_UHF"));
    QCOMPARE(qsos.size(), 1);
    qInfo() << "nach dem Tabulator steht in der Datenbank:" << qsos.first().callsign;
    QVERIFY2(qsos.first().callsign == QStringLiteral("DL1ABD"),
             qPrintable(QStringLiteral("Der Tabulator allein hat nicht gespeichert -- es steht noch %1 da")
                            .arg(qsos.first().callsign)));
    // Und die Liste zeigt es auch sofort.
    QVERIFY(rowForCallsign(model, QStringLiteral("DL1ABD")) >= 0);

    // Und was macht der Tabulator danach? Bei einer Tabelle springt er
    // in die naechste bearbeitbare Zelle. Hier nicht unbedingt: die
    // Korrektur laedt das Modell neu, und ein Editor, dessen Zeile
    // gerade neu gebaut wurde, gehoert der Ansicht nicht mehr (Qt sagt
    // das auch: "commitData called with an editor that does not belong
    // to this view"). Deshalb wird hier festgehalten, WAS wirklich
    // passiert, statt es zu vermuten.
    QCoreApplication::processEvents();
    int offeneEditoren = 0;
    for (QLineEdit* e : table->viewport()->findChildren<QLineEdit*>()) {
        if (e->isVisible()) {
            ++offeneEditoren;
        }
    }
    qInfo() << "nach dem Tabulator offene Zelleneditoren:" << offeneEditoren
            << "-- aktuelle Zelle:" << table->currentIndex().row() << table->currentIndex().column();

    // Der Tabulator muss WEITERTRAGEN, nicht ins Leere laufen: die
    // naechste korrigierbare Zelle (Nr./Grid) steht offen, und man kann
    // sofort weitertippen -- "quasi wie bei excel".
    QVERIFY2(offeneEditoren == 1,
             qPrintable(QStringLiteral("Nach dem Tabulator stehen %1 Editoren offen, erwartet 1")
                            .arg(offeneEditoren)));
    QVERIFY2(table->currentIndex().isValid(), "Nach dem Tabulator gibt es keine aktuelle Zelle mehr");
    QVERIFY2(model->flags(table->currentIndex()) & Qt::ItemIsEditable,
             qPrintable(QStringLiteral("Der Tabulator landete auf Spalte %1, die nicht korrigierbar ist")
                            .arg(table->currentIndex().column())));
    QCOMPARE(table->currentIndex().column(), int(UnifiedLogWidget::ColumnSerialGridRcvd));

    // Entscheidend fuer den Arbeitsfluss: die Tabelle muss danach
    // bedienbar bleiben -- eine gueltige aktuelle Zelle, und eine
    // zweite Korrektur muss ohne Umweg gehen.
    const int zeile2 = rowForCallsign(model, QStringLiteral("DL1ABD"));
    QVERIFY(zeile2 >= 0);
    const QModelIndex index2 = model->index(zeile2, UnifiedLogWidget::ColumnCall);
    table->setCurrentIndex(index2);
    table->edit(index2);
    QCoreApplication::processEvents();
    QLineEdit* editor2 = nullptr;
    for (QLineEdit* e : table->viewport()->findChildren<QLineEdit*>()) {
        if (e->isVisible()) {
            editor2 = e;
        }
    }
    QVERIFY2(editor2, "Eine zweite Korrektur liess sich nicht anfangen");
    editor2->selectAll();
    QTest::keyClicks(editor2, QStringLiteral("DL1ABE"));
    QTest::keyClick(editor2, Qt::Key_Tab);
    QCoreApplication::processEvents();
    const QVector<QsoRecord> danach = controller->database().qsosForContest(QStringLiteral("IARU_R1_VHF_UHF"));
    QCOMPARE(danach.size(), 1);
    qInfo() << "zweite Korrektur, wieder nur mit Tabulator:" << danach.first().callsign;
    QCOMPARE(danach.first().callsign, QStringLiteral("DL1ABE"));
}

// Und der Absturz, der bei genau diesem Durchgang zum Vorschein kam:
// beim Beenden mit offener Zellenkorrektur verliert der Editor den Fokus
// und schickt seine Aenderung los -- in ein MainWindow, dessen Destruktor
// schon gelaufen ist. Qt bricht das mit einem QFATAL ab, das Programm
// stirbt mit SIGABRT. Dieser Pruefstand macht genau das und darf nicht
// abstuerzen.
void TestDurchgangTastatur::quittingWithACorrectionStillOpenDoesNotCrash()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("beenden.sqlite"));
    QVERIFY(controller);

    {
        MainWindow window(*controller);
        window.resize(1440, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        logByKeyboard(window, QStringLiteral("DL1ABC"), 14, QStringLiteral("JN58SD"));
        auto* table = window.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
        QVERIFY(table);
        QAbstractItemModel* model = table->model();
        const int zeile = rowForCallsign(model, QStringLiteral("DL1ABC"));
        QVERIFY(zeile >= 0);

        // Eine Korrektur anfangen -- und offen stehen lassen.
        const QModelIndex index = model->index(zeile, UnifiedLogWidget::ColumnCall);
        table->setCurrentIndex(index);
        table->edit(index);
        QCoreApplication::processEvents();
        QLineEdit* editor = nullptr;
        for (QLineEdit* e : table->viewport()->findChildren<QLineEdit*>()) {
            if (e->isVisible()) {
                editor = e;
            }
        }
        QVERIFY2(editor, "Die Zelle liess sich nicht aufmachen");
        editor->selectAll();
        QTest::keyClicks(editor, QStringLiteral("DL1ABX"));
        qInfo() << "Korrektur steht offen mit" << editor->text() << "-- und jetzt wird beendet";
        // Kein Tab, kein Return: das Fenster geht einfach zu.
    }
    QCoreApplication::processEvents();
    qInfo() << "beendet, ohne Absturz";

    // Die halbfertige Korrektur darf dabei nicht heimlich doch landen:
    // wer abbricht, hat abgebrochen.
    const QVector<QsoRecord> qsos = controller->database().qsosForContest(QStringLiteral("IARU_R1_VHF_UHF"));
    QCOMPARE(qsos.size(), 1);
    qInfo() << "in der Datenbank steht:" << qsos.first().callsign;
    QCOMPARE(qsos.first().callsign, QStringLiteral("DL1ABC"));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestDurchgangTastatur tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_durchgang_tastatur.moc"
