// Martin, 2026-09-28: "bitte teste live die änderung eines calls oder
// locator" -- nachdem er einen Locator nachträglich geändert hatte und
// danach das Log leer aussah.
//
// Die bestehenden Prüfstände lösen eine Korrektur über das SIGNAL aus.
// Dieser hier geht den Weg, den ein Doppelklick wirklich nimmt: er
// schreibt über setData() in das Modell der Tabelle, so wie der
// Zelleneditor beim Verlassen der Zelle. Und er tut es zusätzlich mit
// gesetztem Grid-Filter, denn genau dort lag die Überraschung: der
// Filter wirft Zeilen aus der Liste, die nicht mehr passen -- bis
// 2026-09-28 ohne ein Wort.

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

class TestKorrekturLive : public QObject
{
    Q_OBJECT

private slots:
    void editingTheCallsignThroughTheCellLands();
    void editingTheLocatorThroughTheCellRecomputesTheDistance();
    void aDoubleClickOnTheCallsignCellReallyOpensAnEditor();
    void aGridFilterHidesRowsAndSaysSo();

private:
    std::unique_ptr<AppController> makeController(QTemporaryDir& dir, const QString& file);
};

namespace {

// Die Zeile im Tabellenmodell, die zu dieser QSO-Kennung gehört.
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

std::unique_ptr<AppController> TestKorrekturLive::makeController(QTemporaryDir& dir, const QString& file)
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

    for (int i = 1; i <= 2; ++i) {
        QsoRecord r;
        r.callsign = i == 1 ? QStringLiteral("DL1ABC") : QStringLiteral("OE3XYZ");
        r.band = QStringLiteral("144");
        r.mode = QStringLiteral("SSB");
        r.timestampUtc = QDateTime::currentDateTimeUtc().addSecs(-600 * i).toString(Qt::ISODate);
        r.rstSent = QStringLiteral("59");
        r.rstRcvd = QStringLiteral("59");
        r.serialSent = i;
        r.serialRcvd = i;
        r.gridSquare = QStringLiteral("JN58SD");
        r.distanceKm = 233.0;
        r.contestId = settings.activeContestId;
        if (!controller->database().insertQso(r)) {
            return nullptr;
        }
    }
    return controller;
}

// Rufzeichen korrigieren, über die Zelle.
void TestKorrekturLive::editingTheCallsignThroughTheCellLands()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("call.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    auto* table = window.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(table);
    QAbstractItemModel* model = table->model();

    const int row = rowForCallsign(model, QStringLiteral("DL1ABC"));
    QVERIFY2(row >= 0, "Die Zeile steht nicht in der Liste");

    // Genau das, was der Zelleneditor beim Verlassen tut.
    QVERIFY(model->setData(model->index(row, UnifiedLogWidget::ColumnCall), QStringLiteral("dl1abd"), Qt::EditRole));

    const QVector<QsoRecord> qsos = controller->database().qsosForContest(QStringLiteral("IARU_R1_VHF_UHF"));
    QCOMPARE(qsos.size(), 2);
    bool gefunden = false;
    for (const QsoRecord& q : qsos) {
        if (q.callsign == QStringLiteral("DL1ABD")) {
            gefunden = true;
        }
        QVERIFY2(q.callsign != QStringLiteral("DL1ABC"), "Das alte Rufzeichen steht noch da");
    }
    QVERIFY2(gefunden, "Das korrigierte Rufzeichen fehlt");
    // Und die Liste zeigt es auch.
    QVERIFY(rowForCallsign(model, QStringLiteral("DL1ABD")) >= 0);
}

// Locator korrigieren, über die Zelle -- und die Entfernung muss
// mitgehen, sonst steht im Log eine Punktzahl zum alten Quadrat.
void TestKorrekturLive::editingTheLocatorThroughTheCellRecomputesTheDistance()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("loc.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    auto* table = window.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(table);
    QAbstractItemModel* model = table->model();

    const int row = rowForCallsign(model, QStringLiteral("DL1ABC"));
    QVERIFY(row >= 0);
    const QVector<QsoRecord> vorher = controller->database().qsosForContest(QStringLiteral("IARU_R1_VHF_UHF"));
    const double alteEntfernung = vorher.first().distanceKm.value_or(0.0);
    QVERIFY(alteEntfernung > 0.0);

    // Die Zelle trägt "Nr. + Locator"; der Editor liefert beides zurück.
    QVERIFY(model->setData(model->index(row, UnifiedLogWidget::ColumnSerialGridRcvd),
                            QStringLiteral("59 001 JO70FF"), Qt::EditRole));

    const auto q = controller->database().qsoById(vorher.first().id);
    QVERIFY(q);
    QCOMPARE(q->gridSquare, QStringLiteral("JO70FF"));
    QVERIFY2(q->distanceKm.has_value(), "Keine Entfernung nach der Korrektur");
    // Genau nachgerechnet, nicht über den Daumen: die Entfernung muss
    // die zum NEUEN Quadrat sein, nach derselben Formel wie beim
    // Loggen (IARU R1: 111,2 km je Grad).
    const double erwartet = iaruQrbKm(QStringLiteral("JN67UT"), QStringLiteral("JO70FF"));
    qInfo() << "alt" << alteEntfernung << "km, neu" << *q->distanceKm << "km, erwartet" << erwartet << "km";
    QVERIFY2(std::abs(*q->distanceKm - erwartet) < 1.0,
              qPrintable(QStringLiteral("Entfernung %1 km statt %2 km").arg(*q->distanceKm).arg(erwartet)));
    QVERIFY2(std::abs(*q->distanceKm - alteEntfernung) > 1.0,
              qPrintable(QStringLiteral("Entfernung blieb bei %1 km").arg(alteEntfernung)));
    // Die Zeile bleibt in der Liste stehen.
    QVERIFY(rowForCallsign(model, QStringLiteral("DL1ABC")) >= 0);
}

// Der Grid-Filter: er wirft Zeilen aus der Liste. Das ist gewollt --
// aber es muss dastehen, sonst sieht ein gefiltertes Log aus wie ein
// verlorenes (Martin, 2026-09-28, mit einem "#" im Filter und einer
// Liste ohne ein einziges QSO).
void TestKorrekturLive::aGridFilterHidesRowsAndSaysSo()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("filter.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    auto* table = window.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(table);
    QAbstractItemModel* model = table->model();
    auto* filter = window.findChild<QLineEdit*>(QStringLiteral("gridFilter"));
    QVERIFY2(filter, "Das Filterfeld hat keinen Namen");
    auto* hinweis = window.findChild<QLabel*>(QLatin1String(UnifiedLogWidget::kFilterNoticeObjectName));
    QVERIFY2(hinweis, "Es gibt keinen Hinweis auf den Filter");

    // Ohne Filter: beide QSOs da, kein Hinweis.
    QVERIFY(rowForCallsign(model, QStringLiteral("DL1ABC")) >= 0);
    QVERIFY(hinweis->isHidden());

    // Ein Filter, der zu nichts passt -- genau Martins "#".
    filter->setText(QStringLiteral("#"));
    QCoreApplication::processEvents();
    QCOMPARE(rowForCallsign(model, QStringLiteral("DL1ABC")), -1);
    QVERIFY2(!hinweis->isHidden(), "Der Filter versteckt alles und sagt nichts");
    QVERIFY2(hinweis->text().contains(QStringLiteral("#")), qPrintable(hinweis->text()));
    QVERIFY2(hinweis->text().contains(QStringLiteral("0 von 2")), qPrintable(hinweis->text()));

    // Ein Filter, der passt: beide bleiben, der Hinweis sagt es trotzdem.
    filter->setText(QStringLiteral("JN58"));
    QCoreApplication::processEvents();
    QVERIFY(rowForCallsign(model, QStringLiteral("DL1ABC")) >= 0);
    QVERIFY2(hinweis->text().contains(QStringLiteral("2 von 2")), qPrintable(hinweis->text()));

    // Leer: der Hinweis verschwindet.
    filter->clear();
    QCoreApplication::processEvents();
    QVERIFY(hinweis->isHidden());
}

// Martin, 2026-09-29: "möchte log ändern, funktioniert nicht."
//
// Die Prüfstände darüber rufen setData() auf -- sie beweisen, dass das
// MODELL eine Änderung richtig verarbeitet, aber nicht, dass man
// überhaupt dorthin kommt. Genau dieselbe halbe Strecke wie beim
// Herausziehen aus dem Seitenbereich. Dieser hier klickt doppelt auf
// die Zelle, wie eine Hand es täte, und sieht nach, ob ein Editor
// aufgeht.
void TestKorrekturLive::aDoubleClickOnTheCallsignCellReallyOpensAnEditor()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("dblclick.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    window.activateWindow();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCoreApplication::processEvents();

    auto* table = window.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(table);
    QAbstractItemModel* modell = table->model();
    QVERIFY(modell);

    // Die Zeile des geloggten QSO finden (nicht die Eingabezeile).
    int zeile = -1;
    for (int r = 0; r < modell->rowCount(); ++r) {
        const QModelIndex idx = modell->index(r, UnifiedLogWidget::ColumnCall);
        if (modell->flags(idx) & Qt::ItemIsEditable) {
            zeile = r;
            break;
        }
    }
    QVERIFY2(zeile >= 0, "keine bearbeitbare Rufzeichenzelle im Log");

    QSignalSpy resetSpy(modell, &QAbstractItemModel::modelReset);
    QSignalSpy doppelSpy(table, &QAbstractItemView::doubleClicked);
    QSignalSpy klickSpy(table, &QAbstractItemView::clicked);
    const QModelIndex zelle = modell->index(zeile, UnifiedLogWidget::ColumnCall);
    table->scrollTo(zelle);
    const QRect r = table->visualRect(zelle);
    QVERIFY(r.isValid());
    // Echte Mausereignisse, direkt an das Widget geschickt -- NICHT
    // QTest::mouseDClick: das geht über das Fenstersystem und verpufft,
    // solange das Fenster nicht wirklich auf einem Schirm liegt (im
    // Prüfstandslauf nie). Genau daran ist die erste Fassung dieses
    // Prüfstands gescheitert: clicked feuerte nicht einmal. Mit
    // sendEvent kommt an, was ankommen soll -- dieselbe Technik, die
    // beim Seitenbereich den Zug am Panelkopf nachgewiesen hat.
    const QPoint mitte = r.center();
    const QPoint global = table->viewport()->mapToGlobal(mitte);
    auto schicke = [&](QEvent::Type art, Qt::MouseButton knopf, Qt::MouseButtons gedrueckt) {
        QMouseEvent ereignis(art, QPointF(mitte), QPointF(global), knopf, gedrueckt, Qt::NoModifier);
        QApplication::sendEvent(table->viewport(), &ereignis);
    };
    schicke(QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton);
    schicke(QEvent::MouseButtonRelease, Qt::LeftButton, Qt::NoButton);
    schicke(QEvent::MouseButtonDblClick, Qt::LeftButton, Qt::LeftButton);
    schicke(QEvent::MouseButtonRelease, Qt::LeftButton, Qt::NoButton);
    QCoreApplication::processEvents();

    QList<QLineEdit*> editoren = table->viewport()->findChildren<QLineEdit*>();
    qInfo().noquote() << "Modell-Rücksetzungen:" << resetSpy.count()
                      << "| doubleClicked:" << doppelSpy.count() << "| clicked:" << klickSpy.count();
    qInfo().noquote() << "nach dem Doppelklick offene Editoren:" << editoren.size()
                      << "| editTriggers:" << int(table->editTriggers())
                      << "| Zelle editierbar:" << bool(modell->flags(zelle) & Qt::ItemIsEditable);

    QVERIFY2(!editoren.isEmpty(),
             "Ein Doppelklick auf die Rufzeichenzelle öffnet keinen Editor -- das Log lässt sich nicht ändern");
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestKorrekturLive tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_korrektur_live.moc"
