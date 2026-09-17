#include <QtTest>

#include "core/EsmPlanner.h"
#include "data/ContestDefinition.h"

using namespace Contestprogramm;

namespace {

const char* kIaruJson = R"JSON(
{
  "id": "IARU_R1_VHF_UHF", "name": "IARU", "bands": ["144"], "dupe_scope": ["callsign", "band", "mode"],
  "exchange_fields": [
    { "key": "rst",    "label": "RST",  "type": "rst" },
    { "key": "serial", "label": "Nr.",  "type": "int", "auto_increment": true },
    { "key": "grid",   "label": "Grid", "type": "grid6" }
  ]
}
)JSON";

} // namespace

// The N1MM+/DXLog.net Enter-Sends-Message flow as core/EsmPlanner.h
// states it, one row per state.
class TestEsmPlanner : public QObject
{
    Q_OBJECT

private slots:
    void runModeWalksCqExchangeTu();
    void searchAndPounceWalksMyCallThenExchange();
    void exchangeIsCompleteOnlyWhenEveryNonRstFieldHasAValue();
    void substitutesAllThreePlaceholders();
};

void TestEsmPlanner::runModeWalksCqExchangeTu()
{
    EsmTemplates t;
    EsmPlan p = planEnter(EsmMode::Run, false, false, t);
    QCOMPARE(p.templateText, t.cq);
    QVERIFY(!p.logQso);
    QVERIFY(!p.focusExchange);

    p = planEnter(EsmMode::Run, true, false, t);
    QCOMPARE(p.templateText, t.runExchange);
    QVERIFY(!p.logQso);
    QVERIFY(p.focusExchange);

    p = planEnter(EsmMode::Run, true, true, t);
    QCOMPARE(p.templateText, t.tu);
    QVERIFY(p.logQso);
    QVERIFY(!p.focusExchange);
}

void TestEsmPlanner::searchAndPounceWalksMyCallThenExchange()
{
    EsmTemplates t;
    EsmPlan p = planEnter(EsmMode::SearchAndPounce, false, false, t);
    QCOMPARE(p.templateText, t.myCall);
    QVERIFY(!p.logQso);

    p = planEnter(EsmMode::SearchAndPounce, true, false, t);
    QCOMPARE(p.templateText, t.myCall);
    QVERIFY(p.focusExchange);
    QVERIFY(!p.logQso);

    p = planEnter(EsmMode::SearchAndPounce, true, true, t);
    QCOMPARE(p.templateText, t.spExchange);
    QVERIFY(p.logQso);
}

void TestEsmPlanner::exchangeIsCompleteOnlyWhenEveryNonRstFieldHasAValue()
{
    QString error;
    const ContestDefinition def = ContestDefinition::loadFromJson(QByteArray(kIaruJson), &error);
    QVERIFY2(def.isValid(), qPrintable(error));

    QMap<QString, QString> received;
    QVERIFY(!exchangeComplete(def, received));
    received.insert(QStringLiteral("rst"), QStringLiteral("599"));
    QVERIFY(!exchangeComplete(def, received));
    received.insert(QStringLiteral("serial"), QStringLiteral("003"));
    QVERIFY(!exchangeComplete(def, received));
    received.insert(QStringLiteral("grid"), QStringLiteral(" "));
    QVERIFY(!exchangeComplete(def, received));
    received.insert(QStringLiteral("grid"), QStringLiteral("JN58SD"));
    QVERIFY(exchangeComplete(def, received));
    // RST alone never gates completeness -- the program defaults it.
    received.remove(QStringLiteral("rst"));
    QVERIFY(exchangeComplete(def, received));
}

void TestEsmPlanner::substitutesAllThreePlaceholders()
{
    QCOMPARE(substituteEsm(QStringLiteral("{call} {exchange}"), QStringLiteral(" dl1abc "), QStringLiteral("599 001 JN67UT"),
                           QStringLiteral("oe5sos")),
             QStringLiteral("DL1ABC 599 001 JN67UT"));
    QCOMPARE(substituteEsm(QStringLiteral("CQ TEST {mycall} {mycall} TEST"), QString(), QString(), QStringLiteral("OE5SOS")),
             QStringLiteral("CQ TEST OE5SOS OE5SOS TEST"));
    QCOMPARE(substituteEsm(QStringLiteral("TU {mycall}"), QStringLiteral("DL1ABC"), QString(), QString()),
             QStringLiteral("TU"));
}

QTEST_APPLESS_MAIN(TestEsmPlanner)
#include "test_esm_planner.moc"
