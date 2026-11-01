// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/test_empfangener_austausch_sichtbar.cpp  (Contestprogramm)
// =================================================================
//
// Betreiber am 2026-10-24, mitten in CQ WW SSB: „kann ich zwar die
// nummer 15 für den locator eingeben, wird aber nicht ersichtlich."
//
// Er hatte recht, und es war schlimmer als es klang. Die Spalte
// „Nr./Grid" zeigte genau zwei Dinge: die laufende Nummer und den
// Locator. Der CQ-WW-Austausch ist RST + ZONE — beides leer, die
// Zelle leer. Und die zusammengesetzte Spalte, in der „59 15" stünde,
// ist in BEIDEN Ansichten ausgeblendet (columnWantedByViewMode: „the
// composed received exchange never"). Es gab im ganzen Fenster keine
// Stelle, an der die empfangene Zone zu sehen war.
//
// Gespeichert war sie die ganze Zeit — in seinem Log standen vier von
// sechs QSOs mit „59 15", und das Cabrillo trägt genau dieses Feld.
// Der Fehler war allein die Anzeige, und genau deshalb hätte er ihn
// beinahe für Datenverlust gehalten.
//
// Modification history (Contestprogramm):
//   2026-10-24 — Original von Martin Fischer (OE5SOS), KI-gestützt
//                 über Anthropic Claude.
// =================================================================

#include <QtTest>

#include <QApplication>
#include <QTableView>

#include "models/LogTableModel.h"
#include "data/QsoRecord.h"
#include "ui/UnifiedLogWidget.h"

using namespace Contestprogramm;

class TestEmpfangenerAustauschSichtbar : public QObject
{
    Q_OBJECT

private slots:
    void dieZoneStehtInDerSpalte();
    void nummerUndLocatorBleibenWieSieWaren();

private:
    static QsoRecord basis(const QString& ruf)
    {
        QsoRecord r;
        r.id           = 1;
        r.callsign     = ruf;
        r.band         = QStringLiteral("7");
        r.mode         = QStringLiteral("SSB");
        r.timestampUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        r.rstSent      = QStringLiteral("59");
        r.rstRcvd      = QStringLiteral("59");
        return r;
    }

    static QString zelle(UnifiedLogWidget& log)
    {
        auto* tabelle = log.findChild<QTableView*>(
            QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
        if (!tabelle || !tabelle->model()) { return {}; }
        QAbstractItemModel* m = tabelle->model();
        for (int r = 0; r < m->rowCount(); ++r) {
            const QVariant ruf = m->index(r, UnifiedLogWidget::ColumnCall).data();
            if (!ruf.toString().isEmpty()) {
                return m->index(r, UnifiedLogWidget::ColumnSerialGridRcvd).data().toString();
            }
        }
        return {};
    }
};

// ── CQ WW: RST + Zone, und die Zone muss dastehen ───────────────────
void TestEmpfangenerAustauschSichtbar::dieZoneStehtInDerSpalte()
{
    // Genau so, wie es in seinem Log steht: kein serialRcvd, kein
    // gridSquare, die Zone nur im zusammengesetzten Austausch.
    QsoRecord r = basis(QStringLiteral("9AEV"));
    r.exchangeSent = QStringLiteral("59 15");
    r.exchangeRcvd = QStringLiteral("59 15");

    UnifiedLogWidget log;
    LogTableModel modell;
    modell.setRecords({r});
    log.setLogModel(&modell);

    const QString steht = zelle(log);
    QVERIFY2(steht.contains(QStringLiteral("15")),
             qPrintable(QStringLiteral("in der Spalte steht \"%1\" -- die empfangene "
                                       "Zone fehlt").arg(steht)));
    // Und der Rapport NICHT: der steht in seiner eigenen Spalte, und
    // zweimal 59 nebeneinander liest niemand gern.
    QCOMPARE(steht.trimmed(), QStringLiteral("15"));
}

// ── Gegenprobe: wo Nummer und Locator stehen, bleibt alles ──────────
//
// Ohne diesen Lauf wäre nicht belegt, dass der Rückfall nur ein
// Rückfall ist. Er darf die UKW-Conteste nicht anfassen.
void TestEmpfangenerAustauschSichtbar::nummerUndLocatorBleibenWieSieWaren()
{
    QsoRecord r = basis(QStringLiteral("DL1ABC"));
    r.band         = QStringLiteral("144");
    r.serialRcvd   = 42;
    r.gridSquare   = QStringLiteral("JN88TC");
    r.exchangeRcvd = QStringLiteral("59 042 JN88TC");

    UnifiedLogWidget log;
    LogTableModel modell;
    modell.setRecords({r});
    log.setLogModel(&modell);

    const QString steht = zelle(log);
    QVERIFY2(steht.contains(QStringLiteral("042")), qPrintable(steht));
    QVERIFY2(steht.contains(QStringLiteral("JN88TC")), qPrintable(steht));
    // Der Rapport bleibt draussen, wie bisher.
    QVERIFY2(!steht.startsWith(QStringLiteral("59")), qPrintable(steht));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestEmpfangenerAustauschSichtbar tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_empfangener_austausch_sichtbar.moc"
