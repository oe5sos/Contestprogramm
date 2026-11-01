// Martin, 2026-10-08: "multiplikatoren nur bei kurzwelle!" und
// "grundsätzlich ist das programm für ukw".
//
// Das deckt sich mit der Ausschreibung: IARU Region 1 wertet auf UKW
// Kilometer, Punkt -- ein Großfeld ist dort kein Faktor. Die Bandmap
// hat es trotzdem bernsteinfarben als "fehlender Multiplikator"
// markiert, das Check-Panel hat eine Multiplikatorzeile dazugeschrieben
// und das Rate-Panel eine Kachel dafür geführt.
//
// Geblieben ist, was auf UKW wirklich hilft: die Liste der Großfelder
// -- jetzt mit der Entfernung zu ihrer Mitte als eigener Spalte.

#include <QtTest>

#include <QApplication>
#include <QHeaderView>
#include <QTableWidget>
#include <QTemporaryDir>

#include "core/Maidenhead.h"
#include "data/ContestDatabase.h"
#include "data/ContestDefinition.h"
#include "data/MultiplierTracker.h"
#include "ui/MultiplierWindow.h"

using namespace Contestprogramm;

class TestMultiplikatorenNurKw : public QObject
{
    Q_OBJECT

private slots:
    void ukwHatKeineMultiplikatorenKurzwelleSchon();
    void dieFelderlisteZeigtKilometer();
};

void TestMultiplikatorenNurKw::ukwHatKeineMultiplikatorenKurzwelleSchon()
{
    QString fehler;
    for (const QString& datei : {QStringLiteral("iaru_r1_vhf_uhf"), QStringLiteral("iaru_r1_vhf"),
                                 QStringLiteral("iaru_r1_uhf"), QStringLiteral("iaru_r1_marconi"),
                                 QStringLiteral("oe_vhf_uhf")}) {
        const ContestDefinition def = ContestDefinition::loadFromFile(
            QStringLiteral(CP_SOURCE_DIR "/resources/contest_definitions/") + datei
                + QStringLiteral(".json"),
            &fehler);
        QVERIFY2(def.isValid(), qPrintable(fehler));
        QCOMPARE(def.scoring(), QStringLiteral("distance_km"));
        QVERIFY2(!def.hasMultipliers(),
                 qPrintable(QStringLiteral("%1 führt Multiplikatoren, obwohl es nach km wertet").arg(datei)));
    }

    const ContestDefinition kw = ContestDefinition::loadFromFile(
        QStringLiteral(CP_SOURCE_DIR "/resources/contest_definitions/uebung_kurzwelle.json"), &fehler);
    QVERIFY2(kw.isValid(), qPrintable(fehler));
    QCOMPARE(kw.multiplierField(), QStringLiteral("prefix"));
    QVERIFY2(kw.hasMultipliers(), "Auf Kurzwelle müssen die Multiplikatoren bleiben");
}

void TestMultiplikatorenNurKw::dieFelderlisteZeigtKilometer()
{
    QString fehler;
    const ContestDefinition def = ContestDefinition::loadFromFile(
        QStringLiteral(CP_SOURCE_DIR "/resources/contest_definitions/iaru_r1_vhf_uhf.json"), &fehler);
    QVERIFY2(def.isValid(), qPrintable(fehler));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("mult.sqlite")), QStringLiteral("mult_probe")));
    MultiplierTracker tracker(db);
    MultiplierWindow fenster(tracker);
    fenster.setOwnGrid(QStringLiteral("JN67UT"));
    fenster.setContest(QStringLiteral("IARU_R1_VHF_UHF"), &def);

    auto* tabelle = fenster.findChild<QTableWidget*>();
    QVERIFY(tabelle);
    QCOMPARE(tabelle->columnCount(), 4);
    QCOMPARE(tabelle->horizontalHeaderItem(2)->text(), QStringLiteral("km"));
    QVERIFY2(!tabelle->isColumnHidden(2), "Die km-Spalte fehlt, obwohl der eigene Locator steht");

    // Ohne vollständigen eigenen Locator gibt es nichts zu messen.
    fenster.setOwnGrid(QStringLiteral("JN67"));
    QVERIFY(tabelle->isColumnHidden(2));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestMultiplikatorenNurKw tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_multiplikatoren_nur_kw.moc"
