// Aus Martins Arbeitsanweisung fuer die UKW-Conteste: "fehler sollen
// einfach und schnell geaendert und geloescht werden", und dazu vom
// 2026-09-27: "bei mac gibt es keine entf taste" / "mit cursor hinfahren
// und delete taste".
//
// Geloescht wird also mit Rueckschritt (und mit Entf, wo es die Taste
// gibt), solange die Log-Liste den Fokus hat. Genau daran haengt ein
// Randfall, der wehtaete: der Kurzbefehl ist mit
// Qt::WidgetWithChildrenShortcut an die Tabelle gebunden -- und ein
// offener Zelleneditor IST ein Kind der Tabelle. Loescht ein Rueckschritt
// mitten in einer Korrektur also das ganze QSO? Und loescht er eines,
// waehrend in der Eingabezeile getippt wird? Beides wird hier gemessen,
// nicht angenommen.

#include <QtTest>

#include <QAbstractItemModel>
#include <QApplication>
#include <QLineEdit>
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

QLineEdit* sichtbarerZelleneditor(QTableView* table)
{
    QLineEdit* editor = nullptr;
    for (QLineEdit* e : table->viewport()->findChildren<QLineEdit*>()) {
        if (e->isVisible()) {
            editor = e;
        }
    }
    return editor;
}

} // namespace

class TestLoeschenTaste : public QObject
{
    Q_OBJECT

private slots:
    void backspaceOnASelectedRowDeletesThatQso();
    void deleteKeyDoesTheSame();
    void backspaceInsideACellEditorDeletesACharacterNotTheQso();
    void backspaceInTheEntryRowDeletesACharacterNotTheQso();
    void backspaceInEmptyEntryFieldMustNotDeleteAQso();

private:
    std::unique_ptr<AppController> makeController(QTemporaryDir& dir, const QString& file, int anzahl);
};

std::unique_ptr<AppController> TestLoeschenTaste::makeController(QTemporaryDir& dir, const QString& file, int anzahl)
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

    const QStringList rufe{QStringLiteral("DL1ABC"), QStringLiteral("OK2XYZ"), QStringLiteral("HA5QRP")};
    for (int i = 0; i < anzahl && i < rufe.size(); ++i) {
        QsoRecord r;
        r.callsign = rufe.at(i);
        r.band = QStringLiteral("144");
        r.mode = QStringLiteral("SSB");
        r.timestampUtc = QDateTime::currentDateTimeUtc().addSecs(-600 * (i + 1)).toString(Qt::ISODate);
        r.rstSent = QStringLiteral("59");
        r.rstRcvd = QStringLiteral("59");
        r.serialSent = i + 1;
        r.serialRcvd = i + 1;
        r.gridSquare = QStringLiteral("JN58SD");
        r.distanceKm = 165.0;
        r.contestId = settings.activeContestId;
        if (!controller->database().insertQso(r)) {
            return nullptr;
        }
    }
    return controller;
}

void TestLoeschenTaste::backspaceOnASelectedRowDeletesThatQso()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("back.sqlite"), 3);
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    // Aktiv, nicht nur sichtbar -- die Loeschtaste haengt an einem
    // QShortcut, und Qt liefert Kurzbefehle nur an ein aktives Fenster
    // aus. Ohne das flattert so ein Pruefstand auf einem CI-Laeufer
    // (erlebt am 2026-09-28 mit test_qso_loeschen auf dem CI-Mac).
    window.activateWindow();
    (void)QTest::qWaitForWindowActive(&window);

    auto* table = window.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(table);
    QAbstractItemModel* model = table->model();
    const int zeile = rowForCallsign(model, QStringLiteral("OK2XYZ"));
    QVERIFY(zeile >= 0);

    // Mit dem Cursor hinfahren und die Taste drücken -- genau so, wie er
    // es beschrieben hat.
    table->setFocus();
    table->setCurrentIndex(model->index(zeile, UnifiedLogWidget::ColumnCall));
    QTest::keyClick(table, Qt::Key_Backspace);
    QCoreApplication::processEvents();

    const QVector<QsoRecord> qsos = controller->database().qsosForContest(QStringLiteral("IARU_R1_VHF_UHF"));
    qInfo() << "nach dem Rückschritt noch" << qsos.size() << "QSOs";
    QCOMPARE(qsos.size(), 2);
    for (const QsoRecord& q : qsos) {
        QVERIFY2(q.callsign != QStringLiteral("OK2XYZ"), "Das QSO steht noch da");
    }
    // Und die anderen beiden sind unversehrt.
    QVERIFY(rowForCallsign(model, QStringLiteral("DL1ABC")) >= 0);
    QVERIFY(rowForCallsign(model, QStringLiteral("HA5QRP")) >= 0);
}

void TestLoeschenTaste::deleteKeyDoesTheSame()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("del.sqlite"), 3);
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    // Aktiv, nicht nur sichtbar -- die Loeschtaste haengt an einem
    // QShortcut, und Qt liefert Kurzbefehle nur an ein aktives Fenster
    // aus. Ohne das flattert so ein Pruefstand auf einem CI-Laeufer
    // (erlebt am 2026-09-28 mit test_qso_loeschen auf dem CI-Mac).
    window.activateWindow();
    (void)QTest::qWaitForWindowActive(&window);

    auto* table = window.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(table);
    QAbstractItemModel* model = table->model();
    const int zeile = rowForCallsign(model, QStringLiteral("HA5QRP"));
    QVERIFY(zeile >= 0);
    table->setFocus();
    table->setCurrentIndex(model->index(zeile, UnifiedLogWidget::ColumnCall));
    QTest::keyClick(table, Qt::Key_Delete);
    QCoreApplication::processEvents();

    const QVector<QsoRecord> qsos = controller->database().qsosForContest(QStringLiteral("IARU_R1_VHF_UHF"));
    qInfo() << "nach Entf noch" << qsos.size() << "QSOs";
    QCOMPARE(qsos.size(), 2);
    for (const QsoRecord& q : qsos) {
        QVERIFY2(q.callsign != QStringLiteral("HA5QRP"), "Das QSO steht noch da");
    }
}

// Der Randfall, um den es hier eigentlich geht.
void TestLoeschenTaste::backspaceInsideACellEditorDeletesACharacterNotTheQso()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("editor.sqlite"), 1);
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    // Aktiv, nicht nur sichtbar -- die Loeschtaste haengt an einem
    // QShortcut, und Qt liefert Kurzbefehle nur an ein aktives Fenster
    // aus. Ohne das flattert so ein Pruefstand auf einem CI-Laeufer
    // (erlebt am 2026-09-28 mit test_qso_loeschen auf dem CI-Mac).
    window.activateWindow();
    (void)QTest::qWaitForWindowActive(&window);

    auto* table = window.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(table);
    QAbstractItemModel* model = table->model();
    const int zeile = rowForCallsign(model, QStringLiteral("DL1ABC"));
    QVERIFY(zeile >= 0);

    const QModelIndex index = model->index(zeile, UnifiedLogWidget::ColumnCall);
    table->setCurrentIndex(index);
    table->edit(index);
    QCoreApplication::processEvents();
    QLineEdit* editor = sichtbarerZelleneditor(table);
    QVERIFY2(editor, "Die Zelle liess sich nicht aufmachen");
    QCOMPARE(editor->text(), QStringLiteral("DL1ABC"));

    // Cursor ans Ende, dann ein Zeichen löschen.
    editor->setCursorPosition(editor->text().size());
    QTest::keyClick(editor, Qt::Key_Backspace);
    QCoreApplication::processEvents();

    qInfo().noquote() << "im Editor steht jetzt:" << editor->text();
    QVERIFY2(editor->text() == QStringLiteral("DL1AB"),
             qPrintable(QStringLiteral("Der Rückschritt hat kein Zeichen gelöscht, im Editor steht: %1")
                            .arg(editor->text())));

    const QVector<QsoRecord> qsos = controller->database().qsosForContest(QStringLiteral("IARU_R1_VHF_UHF"));
    QVERIFY2(qsos.size() == 1,
             "Der Rückschritt im Zelleneditor hat das ganze QSO gelöscht -- das darf er nicht");
    QCOMPARE(qsos.first().callsign, QStringLiteral("DL1ABC"));
}

void TestLoeschenTaste::backspaceInTheEntryRowDeletesACharacterNotTheQso()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("eingabe.sqlite"), 1);
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    // Aktiv, nicht nur sichtbar -- die Loeschtaste haengt an einem
    // QShortcut, und Qt liefert Kurzbefehle nur an ein aktives Fenster
    // aus. Ohne das flattert so ein Pruefstand auf einem CI-Laeufer
    // (erlebt am 2026-09-28 mit test_qso_loeschen auf dem CI-Mac).
    window.activateWindow();
    (void)QTest::qWaitForWindowActive(&window);

    auto* table = window.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(table);
    QAbstractItemModel* model = table->model();
    // Eine Zeile ist markiert -- so steht es nach einem Dupe-Hinweis
    // oder einem Klick ins Log wirklich da.
    table->setCurrentIndex(model->index(rowForCallsign(model, QStringLiteral("DL1ABC")),
                                        UnifiedLogWidget::ColumnCall));

    const QList<QLineEdit*> fields = entryFields(window);
    QVERIFY(fields.size() >= 4);
    QLineEdit* callField = fields.at(0);
    callField->setFocus();
    QTest::keyClicks(callField, QStringLiteral("OK2XYZ"));
    QTest::keyClick(callField, Qt::Key_Backspace);
    QCoreApplication::processEvents();

    qInfo().noquote() << "in der Eingabezeile steht jetzt:" << callField->text();
    QCOMPARE(callField->text(), QStringLiteral("OK2XY"));
    const QVector<QsoRecord> qsos = controller->database().qsosForContest(QStringLiteral("IARU_R1_VHF_UHF"));
    QVERIFY2(qsos.size() == 1,
             "Der Rückschritt beim Tippen hat ein geloggtes QSO gelöscht -- das darf er nicht");
}

// Der Fall, den der Test darueber nicht abdeckt: das Feld ist LEER.
// Ein QLineEdit ohne Inhalt nimmt den Rueckschritt nicht an, also
// reicht Qt ihn weiter -- und dann steht nur noch der Kurzbefehl der
// Tabelle im Weg. Im Contest passiert das staendig: Rufzeichen
// weggeputzt, Finger bleibt auf der Taste.
void TestLoeschenTaste::backspaceInEmptyEntryFieldMustNotDeleteAQso()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("leer.sqlite"), 3);
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
    table->setCurrentIndex(model->index(rowForCallsign(model, QStringLiteral("DL1ABC")),
                                        UnifiedLogWidget::ColumnCall));

    const QList<QLineEdit*> fields = entryFields(window);
    QVERIFY(fields.size() >= 4);
    QLineEdit* callField = fields.at(0);
    callField->clear();
    callField->setFocus();
    QCoreApplication::processEvents();
    for (int i = 0; i < 3; ++i) {
        QTest::keyClick(callField, Qt::Key_Backspace);
        QCoreApplication::processEvents();
    }

    const QVector<QsoRecord> qsos = controller->database().qsosForContest(QStringLiteral("IARU_R1_VHF_UHF"));
    qInfo() << "QSOs nach drei Rueckschritten im leeren Feld:" << qsos.size();
    QVERIFY2(qsos.size() == 3,
             "Rueckschritt im leeren Eingabefeld hat ein geloggtes QSO geloescht");
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestLoeschenTaste tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_loeschen_taste.moc"
