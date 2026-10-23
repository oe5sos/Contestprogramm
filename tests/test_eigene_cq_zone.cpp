// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/test_eigene_cq_zone.cpp  (Contestprogramm)
// =================================================================
//
// Einen Tag vor CQ WW SSB gefunden (2026-10-23). Der gesendete
// Austausch wurde aus drei Feldarten gefüllt: laufende Nummer,
// Locator und RST. Ein Zonenfeld kannte diese Stelle nicht.
//
// Bei CQ WW ist der Austausch aber RST + CQ-Zone. Der Betreiber hätte
// „59" gesendet und „59" geloggt -- jedes QSO ohne die 15, und
// nirgends wäre etwas rot geworden. Im Cabrillo stünde dasselbe.
//
// Die Zone kommt aus derselben Länderliste, aus der das Programm die
// Zone der Gegenstation holt, nicht aus einer eigenen Einstellung:
// eine Einstellung mehr ist eine Einstellung mehr, die jemand
// vergisst.
//
// Modification history (Contestprogramm):
//   2026-10-23 — Original von Martin Fischer (OE5SOS), KI-gestützt
//                 über Anthropic Claude.
// =================================================================

#include <QtTest>

#include <QApplication>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"
#include "ui/MainWindow.h"

#include <memory>

using namespace Contestprogramm;

namespace {

// Eine kleine Länderliste im cty.dat-Format, wie in
// tests/test_country_prefix_index.cpp. Das Programm lädt seine aus
// einer Datei, die der Betreiber einstellt -- ein Prüflauf darf sich
// nicht darauf verlassen, dass dort gerade eine liegt.
const char* const kMiniCty = R"CTY(
Austria:                   15:  28:  EU:   47.33:   -13.33:    -1.0:  OE:
    OE,OE1,OE2,OE3,OE4,OE5,OE6,OE7,OE8,OE9;
Fed. Rep. of Germany:      14:  28:  EU:   51.00:   -10.00:    -1.0:  DL:
    DA,DB,DC,DD,DE,DF,DG,DH,DJ,DK,DL,DM,DO,DP,DQ,DR;
)CTY";

std::unique_ptr<AppController> baueSteuerung(QTemporaryDir& dir, const QString& datei,
                                             const QString& ruf, const QString& contest)
{
    auto c = std::make_unique<AppController>();
    if (!c->openDatabase(dir.filePath(datei))) {
        return nullptr;
    }
    ContestSettings s = c->settings();
    s.ownCallsign     = ruf;
    s.ownGrid         = QStringLiteral("JN67UT");
    s.activeContestId = contest;
    s.rigctldHost.clear();
    s.rotor1Enabled   = false;
    s.rotor2Enabled   = false;
    s.esmEnabled      = false;
    c->setSettings(s);
    QString fehler;
    if (!c->countryIndex().loadFromCty(QByteArray(kMiniCty), &fehler)) {
        return nullptr;
    }
    return c;
}

} // namespace

class TestEigeneCqZone : public QObject
{
    Q_OBJECT

private slots:
    void dieEigeneZoneStehtImGesendetenAustausch();
    void ohneZonenfeldAendertSichNichts();
    void einUnbekanntesRufzeichenBelegtNichtsVor();
};

// ── CQ WW: RST + Zone, und die Zone ist die eigene ──────────────────
void TestEigeneCqZone::dieEigeneZoneStehtImGesendetenAustausch()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto c = baueSteuerung(dir, QStringLiteral("zone.sqlite"),
                           QStringLiteral("OE5SOS"), QStringLiteral("CQ_WW_SSB"));
    QVERIFY(c);

    MainWindow fenster(*c);

    // Österreich ist CQ-Zone 15. Die Zahl kommt aus der Länderliste des
    // Programms, nicht aus diesem Prüflauf -- hier steht nur, was
    // herauskommen MUSS.
    QCOMPARE(fenster.eigeneCqZone(), 15);

    const QString gesendet = fenster.currentSentExchangeTextForTest();
    QVERIFY2(gesendet.contains(QStringLiteral("15")),
             qPrintable(QStringLiteral("gesendet würde \"%1\" -- ohne die eigene Zone")
                            .arg(gesendet)));
    // Und der Rapport steht auch drin; die Zone darf ihn nicht
    // verdrängt haben.
    QVERIFY2(gesendet.contains(QStringLiteral("59")),
             qPrintable(QStringLiteral("gesendet würde \"%1\"").arg(gesendet)));
}

// ── Gegenprobe: ein Contest ohne Zonenfeld bleibt, wie er war ───────
//
// Ohne diesen Lauf wäre nicht belegt, dass die Zone nur dort landet,
// wo sie hingehört. Der IARU-R1-VHF/UHF-Austausch ist RST + Nummer +
// Locator -- eine 15 darin wäre ein Fehler.
void TestEigeneCqZone::ohneZonenfeldAendertSichNichts()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto c = baueSteuerung(dir, QStringLiteral("ukw.sqlite"),
                           QStringLiteral("OE5SOS"),
                           QStringLiteral("IARU_R1_VHF_UHF"));
    QVERIFY(c);

    MainWindow fenster(*c);
    const QString gesendet = fenster.currentSentExchangeTextForTest();

    QVERIFY2(gesendet.contains(QStringLiteral("JN67UT")),
             qPrintable(gesendet));
    QVERIFY2(gesendet.contains(QStringLiteral("001")), qPrintable(gesendet));
    // Die Zone kennt das Programm auch hier -- sie darf nur nicht in
    // den Austausch.
    QCOMPARE(fenster.eigeneCqZone(), 15);
    QVERIFY2(!gesendet.split(QLatin1Char(' ')).contains(QStringLiteral("15")),
             qPrintable(QStringLiteral("im UKW-Austausch steht eine Zone: \"%1\"")
                            .arg(gesendet)));
}

// ── Unbekanntes Rufzeichen: lieber leer als geraten ─────────────────
void TestEigeneCqZone::einUnbekanntesRufzeichenBelegtNichtsVor()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // Ein Präfix, das in keiner Länderliste steht.
    auto c = baueSteuerung(dir, QStringLiteral("unbekannt.sqlite"),
                           QStringLiteral("QQ0XYZ"), QStringLiteral("CQ_WW_SSB"));
    QVERIFY(c);

    MainWindow fenster(*c);
    QCOMPARE(fenster.eigeneCqZone(), 0);

    // Kein Absturz, kein geratener Wert -- der Rapport steht, das
    // Zonenfeld bleibt leer und fällt beim Loggen auf.
    const QString gesendet = fenster.currentSentExchangeTextForTest();
    QVERIFY2(gesendet.contains(QStringLiteral("59")), qPrintable(gesendet));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestEigeneCqZone tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_eigene_cq_zone.moc"
