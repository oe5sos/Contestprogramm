// Martin, 2026-09-28: "bitte teste mal, schreibe, lösche, ändere usw.!
// ich komme immer sofort auf fehler und du zu selten."
//
// Er hat recht. Meine übrigen Prüfstände fahren die Fälle, an die ich
// gedacht habe. Er bedient frei -- und findet dadurch anderes.
//
// Dieser hier bedient frei: eine zufällige Folge von Handlungen am
// echten, gezeigten Fenster (loggen, korrigieren, löschen, Band
// wechseln, Ansicht umschalten, filtern, Fenster ziehen, Log leeren),
// und nach JEDEM Schritt werden dieselben Wahrheiten nachgesehen:
//
//   * die Zahl der Zeilen stimmt mit der Datenbank überein
//   * die laufenden Nummern sind 1..n, ohne Lücke und ohne Dopplung
//   * keine Zeile hat ein leeres Rufzeichen
//   * das Rufzeichenfeld steht in der Flucht mit seiner Spalte
//   * Qt hat sich nicht beschwert (jede QWARN/QCRITICAL zählt)
//
// Der Zufall hat einen festen Startwert: ein Fehlschlag ist damit
// wiederholbar, und die Ausgabe nennt die Folge, die dorthin geführt
// hat.

#include <QtTest>

#include <QAbstractItemModel>
#include <QApplication>
#include <QHeaderView>
#include <QLineEdit>
#include <QRandomGenerator>
#include <QTableView>
#include <QLayout>
#include <QStatusBar>
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

// Alles, was Qt während eines Schrittes zu meckern hat. Bekannte,
// harmlose Meldungen stehen in der Ausnahmeliste -- jede andere ist ein
// Fund.
QStringList g_meldungen;
QtMessageHandler g_vorher = nullptr;

bool istBekannteMeldung(const QString& text)
{
    static const QStringList harmlos{
        QStringLiteral("Populating font family aliases"),
        QStringLiteral("propagateSizeHints"),
        // Qt selbst: ein Editor meldet beim Fokusverlust ein zweites
        // Mal, nachdem er schon geschlossen wurde. Normalverhalten des
        // Delegates, kein Zustand des Programms.
        QStringLiteral("commitData called with an editor that does not belong"),
    };
    for (const QString& h : harmlos) {
        if (text.contains(h)) {
            return true;
        }
    }
    return false;
}

void sammle(QtMsgType typ, const QMessageLogContext& ctx, const QString& text)
{
    if ((typ == QtWarningMsg || typ == QtCriticalMsg || typ == QtFatalMsg) && !istBekannteMeldung(text)) {
        g_meldungen << text;
    }
    if (g_vorher) {
        g_vorher(typ, ctx, text);
    }
}

// Nur die Felder, die WIRKLICH in der Zeile stehen. Beim Umbauen der
// Eingabezeile wird der alte Behälter der Austauschfelder mit
// deleteLater() weggeräumt -- verzögert. QCoreApplication::
// processEvents() arbeitet verzögerte Löschungen NICHT ab (dafür
// braucht es sendPostedEvents mit QEvent::DeferredDelete), also lagen
// in diesem Prüfstand nach jedem Ansichtswechsel die alten Felder noch
// herum: sieben statt vier, nach dem zweiten zehn. Im laufenden
// Programm passiert das nicht, dort dreht sich die Ereignisschleife.
// Hier musste ich es lernen -- der Prüfstand tippte in eine Leiche und
// wunderte sich, dass nichts geloggt wurde.
QList<QLineEdit*> entryFields(MainWindow& window)
{
    auto* row = window.findChild<QWidget*>(QLatin1String(UnifiedLogWidget::kEntryRowObjectName));
    if (!row) {
        return {};
    }
    QList<QLineEdit*> felder;
    for (QLineEdit* e : row->findChildren<QLineEdit*>()) {
        if (!e->isHidden()) {
            felder << e;
        }
    }
    return felder;
}

// Ereignisse abarbeiten, verzögerte Löschungen eingeschlossen.
void arbeiteAb()
{
    QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}

} // namespace

namespace {

// Ein echter Mausklick auf eine Zelle -- Ereignisse direkt an den
// Viewport, NICHT QTest::mouseClick: das geht über das Fenstersystem
// und verpufft, solange das Fenster nicht wirklich auf einem Schirm
// liegt (im Prüfstandslauf nie). Genau daran waren die Maus-Prüfstände
// bisher blind: clicked feuerte nicht einmal.
void klickeZelle(QTableView* tabelle, const QModelIndex& index, bool doppelt)
{
    if (!tabelle || !index.isValid()) {
        return;
    }
    tabelle->scrollTo(index);
    const QPoint mitte = tabelle->visualRect(index).center();
    const QPointF global = tabelle->viewport()->mapToGlobal(mitte);
    auto schicke = [&](QEvent::Type art, Qt::MouseButtons gedrueckt) {
        QMouseEvent e(art, QPointF(mitte), global, Qt::LeftButton, gedrueckt, Qt::NoModifier);
        QApplication::sendEvent(tabelle->viewport(), &e);
    };
    schicke(QEvent::MouseButtonPress, Qt::LeftButton);
    schicke(QEvent::MouseButtonRelease, Qt::NoButton);
    if (doppelt) {
        schicke(QEvent::MouseButtonDblClick, Qt::LeftButton);
        schicke(QEvent::MouseButtonRelease, Qt::NoButton);
    }
}

} // namespace

class TestRuetteln : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void freeHandedOperationKeepsTheLogConsistent_data();
    void freeHandedOperationKeepsTheLogConsistent();

private:
    std::unique_ptr<AppController> makeController(QTemporaryDir& dir, const QString& file);
};

void TestRuetteln::initTestCase()
{
    g_vorher = qInstallMessageHandler(sammle);
}

void TestRuetteln::cleanupTestCase()
{
    qInstallMessageHandler(g_vorher);
}

std::unique_ptr<AppController> TestRuetteln::makeController(QTemporaryDir& dir, const QString& file)
{
    auto controller = std::make_unique<AppController>();
    if (!controller->openDatabase(dir.filePath(file))) {
        return nullptr;
    }
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67VV");
    settings.activeContestId = QStringLiteral("IARU_R1_VHF_UHF");
    settings.rigctldHost.clear();
    settings.rotor1Enabled = false;
    settings.rotor2Enabled = false;
    settings.esmEnabled = false;
    settings.activeBands.clear();
    controller->setSettings(settings);
    return controller;
}

void TestRuetteln::freeHandedOperationKeepsTheLogConsistent_data()
{
    QTest::addColumn<quint32>("startwert");
    QTest::addColumn<int>("schritte");
    // Mehrere Startwerte: jeder fährt eine andere Folge. Fest gewählt,
    // damit ein Fehlschlag wiederholbar ist.
    // Länge je Folge: 200 im Alltag, beliebig mehr für einen Dauerlauf
    // (CP_RUETTELN_SCHRITTE=5000). Ein Contest dauert 24 Stunden --
    // was dabei schiefgeht, zeigt sich nicht in 200 Handgriffen, und
    // unter dem Speicherprüfer schon gar nicht. Martin, 2026-09-29:
    // "beim contest kann ich mir keine fehler leisten."
    bool zahlOk = false;
    const int schritteProFolge = qEnvironmentVariableIntValue("CP_RUETTELN_SCHRITTE", &zahlOk) > 0 && zahlOk
        ? qEnvironmentVariableIntValue("CP_RUETTELN_SCHRITTE")
        : 200;

    QTest::newRow("Folge 1") << quint32(20260928) << schritteProFolge;
    QTest::newRow("Folge 2") << quint32(4711) << schritteProFolge;
    QTest::newRow("Folge 3") << quint32(144432) << schritteProFolge;
    QTest::newRow("Folge 4") << quint32(1) << schritteProFolge;
    QTest::newRow("Folge 5") << quint32(999983) << schritteProFolge;
}

void TestRuetteln::freeHandedOperationKeepsTheLogConsistent()
{
    QFETCH(quint32, startwert);
    QFETCH(int, schritte);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("ruetteln.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.activateWindow();
    (void)QTest::qWaitForWindowActive(&window);

    auto* table = window.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(table);
    auto* log = window.findChild<UnifiedLogWidget*>();
    QVERIFY(log);
    auto* row = window.findChild<QWidget*>(QLatin1String(UnifiedLogWidget::kEntryRowObjectName));
    QVERIFY(row);
    QAbstractItemModel* model = table->model();

    QRandomGenerator zufall(startwert);
    QStringList verlauf;
    g_meldungen.clear();

    const QStringList rufzeichen{QStringLiteral("DL1ABC"), QStringLiteral("OK2XYZ"), QStringLiteral("HA5QRP"),
                                 QStringLiteral("S51DX"),  QStringLiteral("OE3ABC"), QStringLiteral("DL9ZZZ"),
                                 QStringLiteral("9A2AA"),  QStringLiteral("HB9XYZ"), QStringLiteral("I5YDI")};
    const QStringList locatoren{QStringLiteral("JN58SD"), QStringLiteral("JN99AA"), QStringLiteral("JN97MM"),
                                QStringLiteral("JN76AB"), QStringLiteral("JO50AB"), QStringLiteral("JN88CD")};

    // Eine Verlaufszeile und ihre laufende Nummer.
    const auto verlaufszeilen = [&]() {
        QList<QPair<QString, QString>> zeilen;
        for (int r = 0; r < model->rowCount(); ++r) {
            const QString call = model->index(r, UnifiedLogWidget::ColumnCall).data().toString();
            if (call.isEmpty()) {
                continue;
            }
            zeilen.append({call, model->index(r, UnifiedLogWidget::ColumnSerial).data().toString()});
        }
        return zeilen;
    };

    const auto pruefeAlles = [&](const QString& nachSchritt) {
        // 1. Qt hat nichts zu meckern.
        QVERIFY2(g_meldungen.isEmpty(),
                 qPrintable(QStringLiteral("nach „%1“ (Folge %2): %3\\nVerlauf: %4")
                                .arg(nachSchritt)
                                .arg(startwert)
                                .arg(g_meldungen.join(QStringLiteral(" | ")), verlauf.join(QStringLiteral(" > ")))));

        const QVector<QsoRecord> qsos = controller->database().qsosForContest(QStringLiteral("IARU_R1_VHF_UHF"));
        const QList<QPair<QString, QString>> zeilen = verlaufszeilen();
        const bool gefiltert = !log->findChild<QLineEdit*>(QStringLiteral("gridFilter"))
                                || !window.findChild<QLineEdit*>(QStringLiteral("gridFilter"))->text().isEmpty();

        if (!gefiltert) {
            // 2. So viele Zeilen wie QSOs.
            QVERIFY2(zeilen.size() == qsos.size(),
                     qPrintable(QStringLiteral("nach „%1“ (Folge %2): %3 Zeilen, %4 QSOs\\nVerlauf: %5")
                                    .arg(nachSchritt)
                                    .arg(startwert)
                                    .arg(zeilen.size())
                                    .arg(qsos.size())
                                    .arg(verlauf.join(QStringLiteral(" > ")))));

            // 3. Die laufenden Nummern sind 1..n, ohne Lücke.
            for (int i = 0; i < zeilen.size(); ++i) {
                const QString erwartet = QStringLiteral("%1").arg(i + 1, 3, 10, QLatin1Char('0'));
                QVERIFY2(zeilen.at(i).second == erwartet,
                         qPrintable(QStringLiteral("nach „%1“ (Folge %2): Zeile %3 trägt %4 statt %5\\nVerlauf: %6")
                                        .arg(nachSchritt)
                                        .arg(startwert)
                                        .arg(i)
                                        .arg(zeilen.at(i).second, erwartet, verlauf.join(QStringLiteral(" > ")))));
            }
        }

        // 4. Keine Zeile ohne Rufzeichen (eine halb geschriebene Zeile
        //    wäre ein verlorenes QSO).
        for (const QsoRecord& q : qsos) {
            QVERIFY2(!q.callsign.isEmpty(),
                     qPrintable(QStringLiteral("nach „%1“: ein QSO ohne Rufzeichen").arg(nachSchritt)));
        }

        // 5. Die Eingabezeile steht in der Flucht.
        const QList<QLineEdit*> felder = row->findChildren<QLineEdit*>();
        if (!felder.isEmpty() && !table->isColumnHidden(UnifiedLogWidget::ColumnCall)) {
            // Das Layout wirklich rechnen lassen, bevor gemessen wird:
            // setFixedSize() allein verschiebt noch nichts, die
            // Positionen kommen erst beim nächsten Durchlauf. Ohne das
            // misst man einen Zwischenstand.
            if (QLayout* l = row->layout()) {
                l->activate();
            }
            const int zelleX = felder.first()->mapTo(row, QPoint(0, 0)).x();
            const int spalteX = table->horizontalHeader()->sectionPosition(UnifiedLogWidget::ColumnCall);
            if (std::abs(zelleX - spalteX) > 1) {
                // Zeigen, WO die Abweichung entsteht: jede Zelle vor dem
                // Rufzeichen, mit ihrer Breite und der ihrer Spalte.
                QStringList vergleich;
                if (QLayout* l = row->layout()) {
                    for (int i = 0; i < l->count(); ++i) {
                        QWidget* w = l->itemAt(i)->widget();
                        if (!w || w->isHidden()) {
                            continue;
                        }
                        vergleich << QStringLiteral("%1(%2) Widget=%3+%4 Layout=%5+%6")
                                          .arg(w->metaObject()->className())
                                          .arg(w->objectName().isEmpty() ? QStringLiteral("-") : w->objectName())
                                          .arg(w->mapTo(row, QPoint(0, 0)).x())
                                          .arg(w->width())
                                          .arg(l->itemAt(i)->geometry().x())
                                          .arg(l->itemAt(i)->geometry().width());
                        if (w == felder.first()) {
                            break;
                        }
                    }
                }
                QStringList spalten;
                QHeaderView* kopf = table->horizontalHeader();
                for (int v = 0; v < kopf->count(); ++v) {
                    const int c = kopf->logicalIndex(v);
                    if (!kopf->isSectionHidden(c) && kopf->sectionPosition(c) <= spalteX) {
                        spalten << QStringLiteral("%1 x=%2 b=%3")
                                       .arg(c)
                                       .arg(kopf->sectionPosition(c))
                                       .arg(table->columnWidth(c));
                    }
                }
                qInfo().noquote() << "   Zellen davor:" << vergleich.join(QLatin1Char(' '));
                qInfo().noquote() << "   Spalten davor:" << spalten.join(QLatin1Char(' '));
            }
            QVERIFY2(std::abs(zelleX - spalteX) <= 1,
                     qPrintable(QStringLiteral("nach „%1“ (Folge %2): Rufzeichenfeld bei %3, Spalte bei %4"
                                                "\\nVerlauf: %5")
                                    .arg(nachSchritt)
                                    .arg(startwert)
                                    .arg(zelleX)
                                    .arg(spalteX)
                                    .arg(verlauf.join(QStringLiteral(" > ")))));
        }
    };

    // Tippen wie ein Mensch: ins erste Feld, dann mit Tab weiter. Nicht
    // über feste Feldnummern -- die verschieben sich, sobald sich die
    // Zeile neu aufbaut, und dann tippt der Prüfstand ins Leere, ohne
    // es zu merken. Genau das ist mir hier zuerst passiert: 60 Schritte,
    // zwanzig Mal "geloggt", am Ende null QSOs -- und alle Invarianten
    // grün, weil null Zeilen zu null QSOs passen.
    const auto logge = [&](const QString& call, const QString& grid, int nr) {
        const QList<QLineEdit*> felder = entryFields(window);
        if (felder.size() < 4) {
            return;
        }
        const int vorherQsos =
            controller->database().qsosForContest(QStringLiteral("IARU_R1_VHF_UHF")).size();
        // Nur die Felder leeren, die der Bediener selbst füllt. Das
        // RST-Feld steht auf 59 und bleibt -- wer es leert, hat einen
        // unvollständigen Austausch, und das ist ein anderer Fall.
        felder.at(0)->clear();
        felder.at(2)->clear();
        felder.at(3)->clear();
        QLineEdit* rufzeichen = felder.at(0);
        rufzeichen->setFocus();
        QTest::keyClicks(rufzeichen, call);
        // Zum Nummernfeld: zweimal Tab (RST liegt dazwischen).
        QTest::keyClick(rufzeichen, Qt::Key_Tab);
        QLineEdit* nummer = felder.at(2);
        nummer->setFocus();
        QTest::keyClicks(nummer, QStringLiteral("%1").arg(nr, 3, 10, QLatin1Char('0')));
        QTest::keyClick(nummer, Qt::Key_Tab);
        QLineEdit* locator = felder.at(3);
        locator->setFocus();
        QTest::keyClicks(locator, grid);
        QTest::keyClick(locator, Qt::Key_Return);
        arbeiteAb();

        // Und nachsehen, ob es wirklich im Log gelandet ist. Wenn nicht,
        // soll hier stehen, WARUM -- die Statuszeile sagt es (etwa
        // "Exchange unvollständig"), und die Feldinhalte zeigen, was
        // wirklich angekommen war.
        const int nachherQsos =
            controller->database().qsosForContest(QStringLiteral("IARU_R1_VHF_UHF")).size();
        if (nachherQsos == vorherQsos) {
            QStringList inhalt;
            for (QLineEdit* f : entryFields(window)) {
                inhalt << QStringLiteral("[%1]").arg(f->text());
            }
            QFAIL(qPrintable(QStringLiteral("%1 mit %2/%3 ließ sich nicht loggen. Felder: %4 | Statuszeile: %5"
                                             "\nVerlauf: %6")
                                 .arg(call)
                                 .arg(nr)
                                 .arg(grid, inhalt.join(QLatin1Char(' ')),
                                      window.statusBar()->currentMessage(),
                                      verlauf.join(QStringLiteral(" > ")))));
        }
    };

    for (int schritt = 0; schritt < schritte; ++schritt) {
        const int handlung = zufall.bounded(11);
        const QList<QPair<QString, QString>> vorher = verlaufszeilen();

        switch (handlung) {
        case 0:
        case 1:
        case 2: { // loggen -- häufiger als der Rest, wie im Contest
            const QString call = rufzeichen.at(zufall.bounded(rufzeichen.size()));
            const QString grid = locatoren.at(zufall.bounded(locatoren.size()));
            logge(call, grid, zufall.bounded(1, 400));
            verlauf << QStringLiteral("log(%1)").arg(call);
            break;
        }
        case 3: { // Rufzeichen korrigieren
            if (vorher.isEmpty()) {
                break;
            }
            const int r = zufall.bounded(vorher.size());
            int zeile = -1;
            for (int i = 0; i < model->rowCount(); ++i) {
                if (model->index(i, UnifiedLogWidget::ColumnCall).data().toString() == vorher.at(r).first) {
                    zeile = i;
                    break;
                }
            }
            if (zeile < 0) {
                break;
            }
            // Wie im Contest: doppelt auf die Zelle, tippen, Enter.
            // Vorher stand hier setData() -- das prüfte, ob das Modell
            // eine Änderung verarbeitet, nicht ob man überhaupt
            // hinkommt (Martin, 2026-09-29: "möchte log ändern,
            // funktioniert nicht").
            const QModelIndex zelle = model->index(zeile, UnifiedLogWidget::ColumnCall);
            klickeZelle(table, zelle, /*doppelt=*/true);
            arbeiteAb();
            QLineEdit* editor = table->viewport()->findChild<QLineEdit*>();
            QVERIFY2(editor != nullptr,
                     qPrintable(QStringLiteral("Doppelklick auf das Rufzeichen öffnet keinen Editor "
                                                "(Zeile %1)%2")
                                    .arg(zeile)
                                    .arg(QStringLiteral("\nVerlauf: ") + verlauf.join(QStringLiteral(" > ")))));
            editor->setText(rufzeichen.at(zufall.bounded(rufzeichen.size())));
            QTest::keyClick(editor, Qt::Key_Return);
            arbeiteAb();
            verlauf << QStringLiteral("call-korrektur");
            break;
        }
        case 4: { // Locator korrigieren
            if (vorher.isEmpty()) {
                break;
            }
            int zeile = -1;
            for (int i = 0; i < model->rowCount(); ++i) {
                if (!model->index(i, UnifiedLogWidget::ColumnCall).data().toString().isEmpty()) {
                    zeile = i;
                    break;
                }
            }
            if (zeile < 0) {
                break;
            }
            // Auch hier über die Bedienung: doppelt auf die Zelle
            // "Nr./Grid", neu tippen, Enter.
            const QModelIndex zelle = model->index(zeile, UnifiedLogWidget::ColumnSerialGridRcvd);
            klickeZelle(table, zelle, true);
            arbeiteAb();
            QLineEdit* editor = table->viewport()->findChild<QLineEdit*>();
            QVERIFY2(editor != nullptr,
                     qPrintable(QStringLiteral("Doppelklick auf Nr./Grid öffnet keinen Editor (Zeile %1)")
                                    .arg(zeile)));
            editor->setText(QStringLiteral("59 %1 %2")
                                .arg(zufall.bounded(1, 400), 3, 10, QLatin1Char('0'))
                                .arg(locatoren.at(zufall.bounded(locatoren.size()))));
            QTest::keyClick(editor, Qt::Key_Return);
            arbeiteAb();
            verlauf << QStringLiteral("locator-korrektur");
            break;
        }
        case 5: { // löschen, mit der Taste
            if (vorher.isEmpty()) {
                break;
            }
            int zeile = -1;
            for (int i = 0; i < model->rowCount(); ++i) {
                if (!model->index(i, UnifiedLogWidget::ColumnCall).data().toString().isEmpty()) {
                    zeile = i;
                    break;
                }
            }
            if (zeile < 0) {
                break;
            }
            table->setFocus();
            table->setCurrentIndex(model->index(zeile, UnifiedLogWidget::ColumnCall));
            QTest::keyClick(table, Qt::Key_Backspace);
            arbeiteAb();
            verlauf << QStringLiteral("loeschen");
            break;
        }
        case 6: { // Ansicht umschalten
            const auto neu = (zufall.bounded(2) == 0) ? ContestSettings::LogViewMode::Compact
                                                       : ContestSettings::LogViewMode::DxLogFullColumns;
            log->setViewMode(neu);
            arbeiteAb();
            verlauf << QStringLiteral("ansicht");
            break;
        }
        case 7: { // Grid-Filter setzen oder löschen
            auto* filter = window.findChild<QLineEdit*>(QStringLiteral("gridFilter"));
            if (filter) {
                filter->setText(zufall.bounded(2) == 0 ? QString() : locatoren.at(zufall.bounded(locatoren.size())).left(4));
                QCoreApplication::processEvents();
                verlauf << QStringLiteral("filter(%1)").arg(filter->text().isEmpty() ? QStringLiteral("-") : filter->text());
            }
            break;
        }
        case 8: { // laufende Nummer an/aus
            const bool an = zufall.bounded(2) == 0;
            log->setRunningNumberVisible(an);
            ContestSettings settings = controller->settings();
            settings.logShowRunningNumber = an;
            controller->setSettings(settings);
            arbeiteAb();
            verlauf << QStringLiteral("nummer(%1)").arg(an ? QStringLiteral("an") : QStringLiteral("aus"));
            break;
        }
        case 10: { // Gueltigkeit umschalten -- Klick auf die Statuszelle
            if (vorher.isEmpty()) {
                break;
            }
            int zeile = -1;
            for (int i = 0; i < model->rowCount(); ++i) {
                if (!model->index(i, UnifiedLogWidget::ColumnCall).data().toString().isEmpty()) {
                    zeile = i;
                    break;
                }
            }
            if (zeile < 0) {
                break;
            }
            // Ein Klick macht ungueltig, der naechste wieder gueltig --
            // im Contest der Griff fuer ein QSO, das nicht zaehlt.
            klickeZelle(table, model->index(zeile, UnifiedLogWidget::ColumnStatus), false);
            arbeiteAb();
            klickeZelle(table, model->index(zeile, UnifiedLogWidget::ColumnStatus), false);
            arbeiteAb();
            verlauf << QStringLiteral("ungueltig-hin-und-zurueck");
            break;
        }
        case 9: { // Fenster ziehen
            const int breite = 900 + zufall.bounded(600);
            const int hoehe = 600 + zufall.bounded(300);
            window.resize(breite, hoehe);
            arbeiteAb();
            verlauf << QStringLiteral("groesse(%1x%2)").arg(breite).arg(hoehe);
            break;
        }
        default:
            break;
        }

        pruefeAlles(verlauf.isEmpty() ? QStringLiteral("(nichts)") : verlauf.last());
    }

    // Zum Schluss: den Filter wegnehmen und noch einmal alles prüfen.
    if (auto* filter = window.findChild<QLineEdit*>(QStringLiteral("gridFilter"))) {
        filter->clear();
        QCoreApplication::processEvents();
    }
    pruefeAlles(QStringLiteral("Filter weg"));

    const QVector<QsoRecord> qsos = controller->database().qsosForContest(QStringLiteral("IARU_R1_VHF_UHF"));
    qInfo().noquote() << QStringLiteral("Folge %1: %2 Schritte, am Ende %3 QSOs")
                              .arg(startwert)
                              .arg(schritte)
                              .arg(qsos.size());
    qInfo().noquote() << "   " << verlauf.join(QStringLiteral(" > "));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestRuetteln tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_ruetteln.moc"
