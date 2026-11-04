// SPDX-License-Identifier: GPL-3.0-or-later
//
// tests/test_rotorzeile.cpp  (Contestprogramm)
//
// Betreiber 2026-11-04: "das fenster rotoren muss sehr gross gezogen
// werden, um grad eigens zu sehen. bitte um loesungen, um fenster klein
// ziehen zu koennen."
//
// Die Rotorzeile (RotorRowWidget) stellt ihre Bedienfelder nebeneinander,
// wenn die Breite reicht, sonst untereinander; ihre Mindestgroesse ist
// die EINES Rotors. Alte Fassung (QHBoxLayout): Mindestbreite = Summe,
// nie untereinander -- Fall 1 und 2 fallen um.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-11-04 -- Original von Martin Fischer, KI-gestuetzt ueber
//                 Anthropic Claude.
// =================================================================
#include <QtTest>

#include "ui/RotorRowWidget.h"
#include "ui/RotorWidget.h"

using namespace Contestprogramm;

class TestRotorzeile : public QObject {
    Q_OBJECT

private slots:
    void dieMindestbreiteIstDieEinesRotors();
    void schmalUndHochStehenSieUntereinander();
    void breitStehenSieNebeneinander();
    void einEntferntesFeldFaelltHeraus();
};

namespace {
struct Zeile {
    RotorRowWidget row;
    RotorWidget* r1;
    RotorWidget* r2;
    Zeile()
        : r1(new RotorWidget(QStringLiteral("144"), &row))
        , r2(new RotorWidget(QStringLiteral("432"), &row))
    {
        row.einfuegen(0, r1);
        row.einfuegen(1, r2);
    }
};
}

void TestRotorzeile::dieMindestbreiteIstDieEinesRotors()
{
    Zeile z;
    const QSize einer = z.r1->minimumSizeHint();
    QVERIFY(einer.width() > 0 && einer.height() > 0);
    QCOMPARE(z.row.minimumSizeHint().width(), einer.width());
    QVERIFY2(z.row.minimumSizeHint().width() < 2 * einer.width(),
             "die Zeile darf nicht die Summe beider Breiten verlangen");
}

void TestRotorzeile::schmalUndHochStehenSieUntereinander()
{
    Zeile z;
    const QSize einer = z.r1->minimumSizeHint();
    z.row.resize(einer.width() + 20, 2 * einer.height() + 60);
    z.row.show();
    QVERIFY(QTest::qWaitForWindowExposed(&z.row, 10000));
    QVERIFY2(!z.row.nebeneinander(), "zu schmal fuer zwei nebeneinander");
    QVERIFY(z.r1->geometry().bottom() < z.r2->geometry().top());
    QCOMPARE(z.r1->geometry().left(), z.r2->geometry().left());
    QVERIFY(z.r1->width() >= einer.width());
    QVERIFY(z.r1->height() >= einer.height());
    QVERIFY(z.r2->height() >= einer.height());
}

void TestRotorzeile::breitStehenSieNebeneinander()
{
    Zeile z;
    const QSize einer = z.r1->minimumSizeHint();
    z.row.resize(2 * einer.width() + 80, einer.height() + 40);
    z.row.show();
    QVERIFY(QTest::qWaitForWindowExposed(&z.row, 10000));
    QVERIFY(z.row.nebeneinander());
    QVERIFY(z.r1->geometry().right() < z.r2->geometry().left());
    QCOMPARE(z.r1->geometry().top(), z.r2->geometry().top());
    QVERIFY(z.r1->width() >= einer.width() && z.r2->width() >= einer.width());

    // Schmaler ziehen: sie wechseln untereinander, wenn die Hoehe reicht.
    z.row.resize(einer.width() + 20, 2 * einer.height() + 60);
    QCoreApplication::processEvents();
    QVERIFY(!z.row.nebeneinander());
}

void TestRotorzeile::einEntferntesFeldFaelltHeraus()
{
    Zeile z;
    z.row.resize(600, 300);
    z.row.show();
    QVERIFY(QTest::qWaitForWindowExposed(&z.row, 10000));
    QCOMPARE(z.row.anzahl(), 2);
    z.row.entfernen(z.r2);
    QCOMPARE(z.row.anzahl(), 1);
    QCoreApplication::processEvents();
    QCOMPARE(z.r1->width(), z.row.width());   // der Rest bekommt die ganze Breite
    delete z.r2;
    z.r2 = nullptr;
    QCOMPARE(z.row.anzahl(), 1);
}

QTEST_MAIN(TestRotorzeile)
#include "test_rotorzeile.moc"
