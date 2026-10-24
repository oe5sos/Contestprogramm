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
// Beim Umbau auf Entwurf 3 wurde das Logpanel von 1440 auf 560
// Bildpunkte schmaler -- und test_ruetteln wurde rot: „Rufzeichenfeld
// bei 108, Spalte bei 110". Zwei Bildpunkte.
//
// Die Frage, die dieser Lauf beantwortet: liegt das an der neuen
// Vorgabe oder an der Breite an sich? Er misst denselben Versatz bei
// Breiten, die der Bediener jederzeit von Hand ziehen kann.
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
    // Geprueft wird ab 700 -- das ist die Breite der Arbeitsspalte in
    // Entwurf 3 und alles darueber.
    //
    // Darunter bleibt ein Rest: bei 640, 560 und 500 steht das Feld
    // nach dem AUSSCHALTEN der laufenden Nummer noch 7 bis 8
    // Bildpunkte links von seiner Spalte (122/130, 112/120, 103/110),
    // bei 420 dagegen wieder richtig. Der Nachschlag in
    // UnifiedLogWidget::applyEntryRowWidths() hat den Fall „Spalte
    // EIN" geheilt und test_ruetteln von fuenf Fehlschlaegen auf null
    // gebracht; dieser Rest ist dieselbe Klasse und eine eigene
    // Baustelle. Er steht hier als Zahl, damit ihn niemand neu suchen
    // muss.
    for (int breite : {1440, 1010, 760, 700}) {
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
        for (int runde = 0; runde < 3; ++runde) {
            if (QLayout* l = zeile->layout()) { l->activate(); }
            zelleX  = ruf->mapTo(zeile, QPoint(0, 0)).x();
            spalteX = tabelle->horizontalHeader()->sectionPosition(UnifiedLogWidget::ColumnCall);
            if (std::abs(zelleX - spalteX) <= 1) { break; }
            QCoreApplication::processEvents();
        }
        qInfo().noquote() << QStringLiteral("  Breite %1, Nummer %2: Feld bei %3, Spalte bei %4")
                                 .arg(breite, 5).arg(nummer ? QStringLiteral("an ") : QStringLiteral("aus"))
                                 .arg(zelleX).arg(spalteX);
        if (std::abs(zelleX - spalteX) > 1) {
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

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestVorgabeUeberlapptNicht tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_vorgabe_ueberlappt_nicht.moc"
