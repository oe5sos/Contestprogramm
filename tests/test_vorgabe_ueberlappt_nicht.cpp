// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/test_vorgabe_ueberlappt_nicht.cpp  (Contestprogramm)
// =================================================================
//
// Betreiber am 2026-10-24, nach vier Entwurfsblättern: „3" —
// „Karte als Bühne".
//
// Beim Zeichnen von Blatt 0 (der Stand von heute) fiel auf, was
// vorher niemand nachgerechnet hatte: die Vorgabeplätze aus
// registerPanel() überlappen sich ACHT Mal. Der Seitenbereich deckte
// 340 x 501 Bildpunkte der Karte zu, Bandmap und Check lagen im Log,
// das Log selbst war 95 Bildpunkte hoch.
//
// Die Fläche kennt keine Kollisionsvermeidung: PanelContainerWidget::
// trySetGeometry() klemmt nur Mindestbreite und -höhe und setzt die
// Lage dann genau so. Wer zwei Panels übereinanderlegt, bekommt zwei
// Panels übereinander — welches man sieht, entscheidet die Stapel-
// reihenfolge.
//
// Dieser Lauf rechnet den ENTWURF nach, nicht die gerade sichtbare
// Lage: er fragt jedes angemeldete Panel nach seinem Vorgabeplatz und
// prüft alle Paare. Damit fällt es beim nächsten neuen Panel sofort
// auf, statt erst auf einem Entwurfsblatt.
//
// Modification history (Contestprogramm):
//   2026-10-24 — Original von Martin Fischer (OE5SOS), KI-gestützt
//                 über Anthropic Claude.
// =================================================================

#include <QtTest>

#include <QApplication>
#include <QRect>
#include <QDir>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include <QHeaderView>
#include <QLayout>
#include <QLineEdit>
#include <QTableView>

#include "ui/MainWindow.h"
#include "ui/UnifiedLogWidget.h"
#include "ui/PanelContainerWidget.h"

#include <memory>

using namespace Contestprogramm;

namespace {
// Die Fläche, für die der grosse Entwurf gilt.
constexpr int kEntwurfBreite = 1440;
constexpr int kEntwurfHoehe  = 982;
// Und das kleine Feld, fuer das der kompakte Entwurf gilt -- das ist
// der, den man auf einem gewoehnlichen Schirm wirklich zu sehen
// bekommt (PanelLayoutManager::kCompactDesignCanvas).
constexpr int kKompaktBreite = 1372;
constexpr int kKompaktHoehe  = 692;
} // namespace

class TestVorgabeUeberlapptNicht : public QObject
{
    Q_OBJECT

private slots:
    void keinPanelLiegtAufEinemAnderen();
    void keinPlatzIstSchmalerAlsSeinInhalt();
    void allesBleibtInnerhalbDerFlaeche();
    void derLeereSeitenbereichStehtNichtImWeg();
    void dieEingabezeileStehtBeiJederBreiteInDerFlucht();
    void zuruecksetzenHoltAllesHeim();

private:
    std::unique_ptr<AppController> baueSteuerung(QTemporaryDir& dir, const QString& datei)
    {
        auto c = std::make_unique<AppController>();
        if (!c->openDatabase(dir.filePath(datei))) { return nullptr; }
        ContestSettings s = c->settings();
        s.ownCallsign   = QStringLiteral("OE5SOS");
        s.ownGrid       = QStringLiteral("JN67UT");
        s.rigctldHost.clear();
        s.rotor1Enabled = false;
        s.rotor2Enabled = false;
        s.esmEnabled    = false;
        c->setSettings(s);
        return c;
    }

    // Die CW-Makrozeile bleibt draussen: sie ist per Vorgabe
    // ausgeschaltet (ContestSettings::cwMacroPanelVisible = false) und
    // liegt als Leiste ueber der ganzen Breite. Wer sie einschaltet,
    // bekommt sie bewusst ueber die obere Reihe gelegt.
    static bool zaehltMit(const QString& id)
    {
        return id != QStringLiteral("cwMacroRow");
    }
};

void TestVorgabeUeberlapptNicht::keinPanelLiegtAufEinemAnderen()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto c = baueSteuerung(dir, QStringLiteral("vorgabe.sqlite"));
    QVERIFY(c);

    MainWindow fenster(*c);
    const QStringList ids = fenster.panelIdsForTest();
    QVERIFY2(ids.size() >= 9, qPrintable(QStringLiteral("nur %1 Panels").arg(ids.size())));

    QStringList befunde;
    for (int i = 0; i < ids.size(); ++i) {
        for (int j = i + 1; j < ids.size(); ++j) {
            if (!zaehltMit(ids[i]) || !zaehltMit(ids[j])) { continue; }
            for (int entwurf = 0; entwurf < 2; ++entwurf) {
            const QRect a = entwurf == 0 ? fenster.defaultGeometryForTest(ids[i])
                                         : fenster.compactGeometryForTest(ids[i]);
            const QRect b = entwurf == 0 ? fenster.defaultGeometryForTest(ids[j])
                                         : fenster.compactGeometryForTest(ids[j]);
            // Ein Panel ohne Platz im kleinen Entwurf wird dort
            // ausgeblendet -- es kann niemanden verdecken.
            if (!a.isValid() || !b.isValid()) { continue; }
            // Der Seitenbereich ist der Behaelter, in den die anderen
            // hineinfallen -- er steht nur da, wenn etwas drin ist
            // (MainWindow::syncSideAreaVisibility), und sein Platz darf
            // deshalb auf einem anderen liegen.
            if (ids[i] == QStringLiteral("sidearea") || ids[j] == QStringLiteral("sidearea")) { continue; }
            const QRect schnitt = a.intersected(b);
            if (!schnitt.isEmpty()) {
                befunde << QStringLiteral("%1 %2 x %3 = %4x%5")
                               .arg(entwurf == 0 ? QStringLiteral("gross") : QStringLiteral("klein"))
                               .arg(ids[i], ids[j])
                               .arg(schnitt.width()).arg(schnitt.height());
            }
            }
        }
    }
    qInfo().noquote() << "Panels im Entwurf:" << ids.size() << "— Ueberlappungen:" << befunde.size();
    QVERIFY2(befunde.isEmpty(),
             qPrintable(QStringLiteral("Vorgabeplaetze liegen uebereinander: %1")
                            .arg(befunde.join(QStringLiteral(", ")))));
}

// ── Kein Platz schmaler als sein Inhalt ─────────────────────────────
//
// trySetGeometry() klemmt die Breite auf die Mindestbreite des Inhalts
// hoch (PanelContainerWidget::kMinWidth bzw. minimumSizeHint). Ein zu
// eng geschnittener Vorgabeplatz wird also AUFGEBLASEN -- und deckt
// dann seinen Nachbarn zu, obwohl die Zahlen im Entwurf sauber
// nebeneinanderlagen.
//
// Genau so ist mir der erste Zuschnitt von Entwurf 3 um die Ohren
// geflogen: Skeds (220 breit geplant) blies sich auf und verdeckte
// Rotoren, Rate und Naechstes Ziel. Auf dem Bild fehlten drei Panels.
void TestVorgabeUeberlapptNicht::keinPlatzIstSchmalerAlsSeinInhalt()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto c = baueSteuerung(dir, QStringLiteral("mindestbreite.sqlite"));
    QVERIFY(c);

    MainWindow fenster(*c);
    QStringList zuEng;
    for (const QString& id : fenster.panelIdsForTest()) {
        PanelContainerWidget* panel = fenster.panelForTest(id);
        if (!panel || !panel->content()) { continue; }
        const int noetig = qMax(PanelContainerWidget::kMinWidth, panel->content()->minimumSizeHint().width());
        for (const QRect r : {fenster.defaultGeometryForTest(id), fenster.compactGeometryForTest(id)}) {
            if (r.isValid() && r.width() < noetig) {
                zuEng << QStringLiteral("%1 %2 statt %3").arg(id).arg(r.width()).arg(noetig);
            }
        }
        qInfo().noquote() << QStringLiteral("  %1: mindestens %2 breit").arg(id, -14).arg(noetig);
    }
    QVERIFY2(zuEng.isEmpty(),
             qPrintable(QStringLiteral("Vorgabeplaetze schmaler als ihr Inhalt: %1")
                            .arg(zuEng.join(QStringLiteral(", ")))));
}

void TestVorgabeUeberlapptNicht::allesBleibtInnerhalbDerFlaeche()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto c = baueSteuerung(dir, QStringLiteral("flaeche.sqlite"));
    QVERIFY(c);

    MainWindow fenster(*c);
    QStringList draussen;
    for (const QString& id : fenster.panelIdsForTest()) {
        const QRect gross(0, 0, kEntwurfBreite, kEntwurfHoehe);
        const QRect klein(0, 0, kKompaktBreite, kKompaktHoehe);
        const QPair<QRect, QRect> paare[] = {{fenster.defaultGeometryForTest(id), gross},
                                             {fenster.compactGeometryForTest(id), klein}};
        for (const auto& [r, flaeche] : paare) {
            if (r.isValid() && !flaeche.contains(r)) {
                draussen << QStringLiteral("%1 %2,%3 %4x%5").arg(id).arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
            }
        }
    }
    QVERIFY2(draussen.isEmpty(),
             qPrintable(QStringLiteral("ragt ueber die Flaeche hinaus: %1").arg(draussen.join(QStringLiteral(", ")))));
}

// ── Der Behaelter steht nur da, wenn etwas drin ist ─────────────────
void TestVorgabeUeberlapptNicht::derLeereSeitenbereichStehtNichtImWeg()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto c = baueSteuerung(dir, QStringLiteral("seitenbereich.sqlite"));
    QVERIFY(c);

    MainWindow fenster(*c);
    fenster.resize(kEntwurfBreite, kEntwurfHoehe);
    fenster.show();
    QVERIFY(QTest::qWaitForWindowExposed(&fenster));

    // Beleg fuer den Betreiber: das Fenster, wie es nach dem Umbau
    // wirklich aussieht -- nicht das Entwurfsblatt. CP_SHEET_DIR setzen
    // und den Lauf starten.
    const QByteArray blattOrdner = qgetenv("CP_SHEET_DIR");
    if (!blattOrdner.isEmpty()) {
        QDir().mkpath(QString::fromLocal8Bit(blattOrdner));
        const QString pfad = QString::fromLocal8Bit(blattOrdner) + QStringLiteral("/vorgabe-karte-als-buehne.png");
        QVERIFY(fenster.grab().save(pfad));
        qInfo().noquote() << "Blatt:" << pfad;
    }

    auto* bereich = fenster.panelForTest(QStringLiteral("sidearea"));
    QVERIFY2(bereich, "der Seitenbereich ist gar nicht angemeldet");
    QVERIFY2(bereich->isHidden(),
             "der leere Seitenbereich steht da und deckt die Karte zu");

    // Legt man etwas hinein, ist er wieder da.
    QMetaObject::invokeMethod(&fenster, "putPanelIntoSideArea", Qt::DirectConnection,
                              Q_ARG(QString, QStringLiteral("skeds")), Q_ARG(QString, QStringLiteral("Skeds")));
    QTRY_VERIFY_WITH_TIMEOUT(!bereich->isHidden(), 3000);
}

// ── Die Eingabezeile steht bei jeder Panelbreite in der Flucht ──────
//
// Beim Umbau auf Entwurf 3 wurde das Logpanel schmaler -- und
// test_ruetteln wurde rot: „Rufzeichenfeld bei 108, Spalte bei 110".
//
// Dieser Lauf misst den Versatz bei Breiten, die der Bediener
// jederzeit von Hand ziehen kann. Gemessen wird bei jeder Breite mit
// EIN- und mit AUSgeschalteter laufender Nummer, weil dabei eine
// Spalte kommt und geht.
//
// ── Stand 2026-10-25 ───────────────────────────────────────────────
//
// Ab 700 Bildpunkten Panelbreite steht die Zeile sauber; das ist die
// Breite der Arbeitsspalte in Entwurf 3 und damit die Vorgabe.
// Darunter bleibt beim AUSschalten der Nummer ein Rest:
//
//     Breite 640: Feld bei 122, Spalte bei 130
//     Breite 560: Feld bei 112, Spalte bei 120
//     Breite 500: Feld bei 103, Spalte bei 110
//     Breite 420: stimmt wieder
//
// Die Ursache ist eingekreist, aber nicht behoben. Belegt ist: die
// Zeile ist zu SCHMAL fuer ihre eigenen Zellen und wird deshalb vom
// QHBoxLayout gestaucht -- bei Breite 500 ist sie 322 Bildpunkte
// breit, waehrend das Layout 540 braucht. Die Stauchung verteilt sich
// auf die Zellen davor: die Zeit-Zelle liegt dann auf 55 statt 62,
// und das Rufzeichenfeld faengt acht Bildpunkte zu frueh an. Der
// Zustand ist STABIL -- auch nach acht weiteren Layout-Durchlaeufen
// mit erzwungenem LayoutRequest bleibt er stehen.
//
// Die Zuordnung ist eindeutig -- der Lauf schreibt beide Zahlen mit:
//
//     Breite 640: Zeile 370 breit, Layout braucht 615  -> daneben
//     Breite 560: Zeile 342 breit, Layout braucht 576  -> daneben
//     Breite 500: Zeile 322 breit, Layout braucht 540  -> daneben
//     Breite 420: Zeile 520 breit, Layout braucht 520  -> stimmt
//
// Wo die Zeile ihre volle Breite hat, stimmt die Flucht. Dass
// ausgerechnet die SCHMALSTE Panelbreite die breiteste Zeile hat,
// zeigt auch, woran es nicht liegt: nicht am Platz, sondern daran,
// WANN die Breite gesetzt wird.
//
// syncEntryRowWidth() ist die einzige Stelle, die sie setzt, und sie
// nimmt m_entryRow->sizeHint(). Der kommt aus einem Zwischenspeicher
// und meldet beim Aufruf 322, waehrend dasselbe Layout kurz darauf
// 540 sagt. Ihn durch den Layout-Wert zu ersetzen reicht nicht: auch
// der steht an dieser Stelle noch auf 322, weil apply() die hinteren
// Zellen (Austausch, km, Grad) zu dem Zeitpunkt noch nicht gesetzt
// hat.
//
// Offen ist, warum die Zeile nicht auf die 540 waechst:
// syncEntryRowWidth() sieht zum Zeitpunkt seines Aufrufs einen
// Layout-sizeHint von 322 (gemessen), der erst spaeter auf 540 geht.
//
// Was NICHT die Ursache ist -- alles gemessen, alles wieder
// ausgebaut, damit es niemand erneut probiert:
//
//   * die waagrechte Bildlaufleiste (QEvent::LayoutRequest an die
//     Tabelle schicken und neu fitten aendert nichts)
//   * ein zu frueher activate() nach replaceWidget() in
//     rebuildExchangeCell()
//   * m_entryRow->setMinimumWidth(Summe der sichtbaren Zellen) -- das
//     ist genau der zu kleine Wert (322)
//   * m_entryRowLayout->invalidate() + activate() bzw. setGeometry()
//     am Ende von applyEntryRowWidths()
//   * applyEntryRowWidths() am Ende von rebuildEntryRowLayout()
//   * applyEntryRowWidths() als LETZTEN Schritt in eventFilter() und
//     resizeEvent() (macht es schlimmer: dann ist auch 420 daneben)
//   * der verzoegerte Nachschlag aus PR #48 (abgeschaltet gemessen --
//     derselbe Versatz)
//
// Der Nachschlag aus #48 bleibt trotzdem: er hat den Fall „Spalte
// EIN" geheilt und test_ruetteln von fuenf Fehlschlaegen auf null
// gebracht.
void TestVorgabeUeberlapptNicht::dieEingabezeileStehtBeiJederBreiteInDerFlucht()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto c = baueSteuerung(dir, QStringLiteral("flucht.sqlite"));
    QVERIFY(c);

    MainWindow fenster(*c);
    fenster.resize(kEntwurfBreite, kEntwurfHoehe);
    fenster.show();
    QVERIFY(QTest::qWaitForWindowExposed(&fenster));

    auto* log = fenster.findChild<UnifiedLogWidget*>();
    QVERIFY(log);
    auto* panel = fenster.panelForTest(QStringLiteral("unifiedlog"));
    QVERIFY(panel);
    auto* tabelle = log->findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(tabelle);

    QStringList daneben;
    // Auch mit der laufenden Nummer an und aus: dabei wird der
    // Austauschteil der Zeile ausgetauscht (replaceWidget), und genau
    // danach meldete test_ruetteln den Versatz.
    // Gemessen wird ueber alle Breiten, hart geprueft ab 700 (siehe
    // den Kopf dieser Datei). Die schmalen Breiten laufen als Zahl
    // mit: wer den Rest behebt, sieht die Wirkung sofort im
    // Protokoll, und wer etwas verschlechtert, auch.
    constexpr int kGeprueftAb = 700;
    for (int breite : {1440, 1010, 760, 700, 640, 560, 500, 420}) {
      for (bool nummer : {true, false}) {
        panel->trySetGeometry(QRect(0, 0, breite, 450));
        log->setRunningNumberVisible(nummer);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
        auto* zeile = log->findChild<QWidget*>(QLatin1String(UnifiedLogWidget::kEntryRowObjectName));
        auto* ruf = zeile ? zeile->findChild<QLineEdit*>(QLatin1String(UnifiedLogWidget::kCallsignEditObjectName))
                          : nullptr;
        if (!zeile || !ruf) { continue; }
        // Bis zu drei Durchlaeufe abwarten, nicht einen: eine Spalte
        // ein oder aus laesst die waagrechte Bildlaufleiste kommen oder
        // gehen, und der Viewport traegt seine neue Breite erst nach
        // dem naechsten Layout. Die Zeile wird deshalb nachgefuehrt
        // (UnifiedLogWidget::applyEntryRowWidths), und das sind zwei
        // Schritte. Geprueft wird hier, dass es sich EINPENDELT --
        // schwingt es, bleibt die Abweichung stehen und der Lauf faellt.
        int zelleX = 0;
        int spalteX = 0;
        for (int runde = 0; runde < 8; ++runde) {
            if (QLayout* l = zeile->layout()) { l->activate(); }
            zelleX  = ruf->mapTo(zeile, QPoint(0, 0)).x();
            spalteX = tabelle->horizontalHeader()->sectionPosition(UnifiedLogWidget::ColumnCall);
            if (std::abs(zelleX - spalteX) <= 1) {
                if (runde > 0) { qInfo() << "      eingependelt nach Runde" << runde; }
                break;
            }
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
            QCoreApplication::processEvents();
        }
        qInfo().noquote() << QStringLiteral("  Breite %1, Nummer %2: Feld bei %3, Spalte bei %4%5")
                                 .arg(breite, 5).arg(nummer ? QStringLiteral("an ") : QStringLiteral("aus"))
                                 .arg(zelleX).arg(spalteX)
                                 .arg(std::abs(zelleX - spalteX) <= 1 ? QString()
                                                                      : QStringLiteral("   <= daneben"));
        if (QLayout* l = zeile->layout(); l && breite < kGeprueftAb) {
            // Die Stauchung mitschreiben: ist die Zeile schmaler als
            // ihr eigenes Layout braucht, verteilt das QHBoxLayout die
            // fehlenden Bildpunkte auf die Zellen. Sie steht AUCH bei
            // 420 da, wo die Flucht stimmt -- die Stauchung allein
            // erklaert den Versatz also nicht, sie ist nur die
            // Voraussetzung.
            qInfo().noquote() << QStringLiteral("        Zeile %1 breit, Layout braucht %2")
                                     .arg(zeile->width()).arg(l->minimumSize().width());
        }
        if (std::abs(zelleX - spalteX) > 1 && breite >= kGeprueftAb) {
            // Zeigen, WO die Abweichung entsteht -- Zellen und Spalten
            // nebeneinander, wie im Rüttel-Prüfstand.
            QStringList zellen;
            if (QLayout* l = zeile->layout()) {
                for (int i = 0; i < l->count(); ++i) {
                    QWidget* w = l->itemAt(i)->widget();
                    if (!w) {
                        zellen << QStringLiteral("[Abstandhalter x=%1+%2]")
                                      .arg(l->itemAt(i)->geometry().x()).arg(l->itemAt(i)->geometry().width());
                        continue;
                    }
                    zellen << QStringLiteral("%1%2 Widget=%3+%4 Layout=%5+%6%7")
                                  .arg(w->objectName().isEmpty() ? QStringLiteral("(ohne Namen)") : w->objectName())
                                  .arg(w->isHidden() ? QStringLiteral(" VERSTECKT") : QString())
                                  .arg(w->mapTo(zeile, QPoint(0, 0)).x())
                                  .arg(w->width())
                                  .arg(l->itemAt(i)->geometry().x())
                                  .arg(l->itemAt(i)->geometry().width())
                                  .arg(w == ruf ? QStringLiteral(" <= RUF") : QString());
                    if (w == ruf) { zellen << QStringLiteral("...(weitere folgen)"); break; }
                }
            }
            QStringList spalten;
            QHeaderView* kopf = tabelle->horizontalHeader();
            for (int v = 0; v < kopf->count(); ++v) {
                const int c = kopf->logicalIndex(v);
                if (kopf->sectionPosition(c) > spalteX) { continue; }
                spalten << QStringLiteral("Sp%1%2 x=%3 b=%4")
                               .arg(c)
                               .arg(kopf->isSectionHidden(c) ? QStringLiteral(" VERSTECKT") : QString())
                               .arg(kopf->sectionPosition(c))
                               .arg(tabelle->columnWidth(c));
            }
            if (QLayout* l = zeile->layout()) {
                qInfo() << "      Zeile breit" << zeile->width()
                        << "| Layout minimumSize" << l->minimumSize().width()
                        << "sizeHint" << l->sizeHint().width()
                        << "| Scrollbereich" << (zeile->parentWidget() ? zeile->parentWidget()->width() : -1);
            }
            qInfo().noquote() << "      Zellen: " << zellen.join(QStringLiteral(" | "));
            qInfo().noquote() << "      Spalten:" << spalten.join(QStringLiteral(" | "));
            daneben << QStringLiteral("%1/%2 (%3 statt %4)")
                           .arg(breite).arg(nummer ? QStringLiteral("an") : QStringLiteral("aus"))
                           .arg(zelleX).arg(spalteX);
        }
      }
    }
    QVERIFY2(daneben.isEmpty(),
             qPrintable(QStringLiteral("Eingabezeile nicht in der Flucht bei Breite: %1")
                            .arg(daneben.join(QStringLiteral(", ")))));
}

// ── „Zuruecksetzen" holt alles heim ─────────────────────────────────
//
// Betreiber am 2026-10-25, mit Bild: er drueckt „Fenster
// zuruecksetzen", und die Flaeche bleibt leer -- und im Vollbild sind
// „alle weg". Der Grund: seine Panels lagen gar nicht auf der Flaeche.
// Sechs steckten als Reiter im Seitenbereich, Log und Karte schwebten
// als eigene Fenster. resetToDefaultLayout() setzt aber nur
// GEOMETRIEN, und ein Panel, das anderswo wohnt, bekommt davon nichts
// mit.
void TestVorgabeUeberlapptNicht::zuruecksetzenHoltAllesHeim()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto c = baueSteuerung(dir, QStringLiteral("heim.sqlite"));
    QVERIFY(c);

    MainWindow fenster(*c);
    fenster.resize(kEntwurfBreite, kEntwurfHoehe);
    fenster.show();
    QVERIFY(QTest::qWaitForWindowExposed(&fenster));

    // Genau sein Stand: drei Panels in den Seitenbereich, eines
    // schwebend, eines ausgeblendet.
    for (const QString& id : {QStringLiteral("chat"), QStringLiteral("skeds"),
                              QStringLiteral("ratemeter")}) {
        QMetaObject::invokeMethod(&fenster, "putPanelIntoSideArea", Qt::DirectConnection,
                                  Q_ARG(QString, id), Q_ARG(QString, id));
    }
    auto* karte = fenster.panelForTest(QStringLiteral("map"));
    QVERIFY(karte);
    karte->setFloating(true);
    auto* check = fenster.panelForTest(QStringLiteral("checkpartial"));
    QVERIFY(check);
    check->setVisible(false);
    QCoreApplication::processEvents();

    auto* reset = fenster.findChild<QAction*>();
    QAction* zuruecksetzen = nullptr;
    for (QAction* a : fenster.findChildren<QAction*>()) {
        if (a->text().contains(QStringLiteral("zur\u00fccksetzen"))) { zuruecksetzen = a; break; }
    }
    Q_UNUSED(reset);
    QVERIFY2(zuruecksetzen, "kein Eintrag „Fenster zuruecksetzen\" gefunden");
    zuruecksetzen->trigger();
    QCoreApplication::processEvents();

    QStringList nichtHeim;
    for (const QString& id : {QStringLiteral("chat"), QStringLiteral("skeds"),
                              QStringLiteral("ratemeter"), QStringLiteral("map"),
                              QStringLiteral("checkpartial")}) {
        PanelContainerWidget* panel = fenster.panelForTest(id);
        if (!panel) { nichtHeim << id + QStringLiteral(" (fehlt)"); continue; }
        if (panel->isFloating()) { nichtHeim << id + QStringLiteral(" (schwebt)"); }
        if (panel->isHidden())   { nichtHeim << id + QStringLiteral(" (versteckt)"); }
        if (panel->parentWidget() != fenster.panelForTest(QStringLiteral("unifiedlog"))->parentWidget()) {
            nichtHeim << id + QStringLiteral(" (nicht auf der Flaeche)");
        }
    }
    QVERIFY2(nichtHeim.isEmpty(),
             qPrintable(QStringLiteral("nach dem Zuruecksetzen nicht daheim: %1")
                            .arg(nichtHeim.join(QStringLiteral(", ")))));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestVorgabeUeberlapptNicht tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_vorgabe_ueberlappt_nicht.moc"
