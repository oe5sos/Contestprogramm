#include <QtTest>

#include "core/CheckPartialIndex.h"

using namespace Contestprogramm;

namespace {

QStringList calls(const QVector<CheckPartialMatch>& matches)
{
    QStringList result;
    for (const CheckPartialMatch& m : matches) {
        result << m.callsign;
    }
    return result;
}

const CheckPartialMatch* find(const QVector<CheckPartialMatch>& matches, const QString& call)
{
    for (const CheckPartialMatch& m : matches) {
        if (m.callsign == call) {
            return &m;
        }
    }
    return nullptr;
}

} // namespace

// CheckPartialIndex is a pure index over pushed-in sources (see its
// header), so everything the Check panel shows is decided here.
class TestCheckPartialIndex : public QObject
{
    Q_OBJECT

private slots:
    void needsTwoCharactersAndMatchesCaseInsensitively();
    void ranksPrefixThenSubstringThenNearMissAndOwnKnowledgeFirst();
    void flagsDupesOnTheCurrentBandOnly();
    void mergesSourcesAndKeepsTheBestGrid();
    void nearMissOnlyFromFourCharacters();
    void parsesScpFiles();
    void oneEditApart();
    void capsResults();
};

void TestCheckPartialIndex::needsTwoCharactersAndMatchesCaseInsensitively()
{
    CheckPartialIndex index;
    index.setScpCalls({QStringLiteral("OE5SOS"), QStringLiteral("DL1ABC")});
    QVERIFY(index.matches(QStringLiteral("O"), QStringLiteral("144")).isEmpty());
    QVERIFY(index.matches(QStringLiteral(" "), QStringLiteral("144")).isEmpty());
    QCOMPARE(calls(index.matches(QStringLiteral("oe"), QStringLiteral("144"))), QStringList{QStringLiteral("OE5SOS")});
    QCOMPARE(calls(index.matches(QStringLiteral("5so"), QStringLiteral("144"))), QStringList{QStringLiteral("OE5SOS")});
}

void TestCheckPartialIndex::ranksPrefixThenSubstringThenNearMissAndOwnKnowledgeFirst()
{
    CheckPartialIndex index;
    index.setScpCalls({QStringLiteral("DL1ABC"), QStringLiteral("OE1ABC"), QStringLiteral("OE3ABC"), QStringLiteral("OE1ABD")});
    index.setHistoryCalls({{QStringLiteral("OE3ABC"), QStringLiteral("JN88TC")}});
    index.addSeenCall(QStringLiteral("OE1ABX"), QStringLiteral("JN88AA"));

    // "OE1AB": prefix matches only, own-knowledge OE1ABX before the
    // SCP-only OE1ABC/OE1ABD; OE3ABC and DL1ABC are two edits away from
    // the fragment, so neither shows up as a near miss.
    const auto matches = index.matches(QStringLiteral("OE1AB"), QStringLiteral("144"));
    QCOMPARE(calls(matches), (QStringList{QStringLiteral("OE1ABX"), QStringLiteral("OE1ABC"), QStringLiteral("OE1ABD")}));

    // A substring hit sorts after the prefix hits.
    index.setScpCalls({QStringLiteral("OE5SOS"), QStringLiteral("SOS1AA"), QStringLiteral("DK9SOS")});
    const auto sos = index.matches(QStringLiteral("SOS"), QStringLiteral("144"));
    QCOMPARE(calls(sos), (QStringList{QStringLiteral("SOS1AA"), QStringLiteral("DK9SOS"), QStringLiteral("OE5SOS")}));
    QVERIFY(!find(sos, QStringLiteral("DK9SOS"))->nearMiss);
}

void TestCheckPartialIndex::flagsDupesOnTheCurrentBandOnly()
{
    CheckPartialIndex index;
    index.setLogCalls({{QStringLiteral("OE5XYZ"), QStringLiteral("144")}, {QStringLiteral("DL1ABC"), QStringLiteral("432")}});
    index.setScpCalls({QStringLiteral("OE5XYZ"), QStringLiteral("OE5XYA")});

    auto on144 = index.matches(QStringLiteral("OE5X"), QStringLiteral("144"));
    const CheckPartialMatch* xyz = find(on144, QStringLiteral("OE5XYZ"));
    QVERIFY(xyz);
    QVERIFY(xyz->workedThisBand);
    QVERIFY(xyz->sources & CheckPartialMatch::Log);
    QVERIFY(xyz->sources & CheckPartialMatch::Scp);
    QVERIFY(!find(on144, QStringLiteral("OE5XYA"))->workedThisBand);

    auto on432 = index.matches(QStringLiteral("OE5X"), QStringLiteral("432"));
    QVERIFY(!find(on432, QStringLiteral("OE5XYZ"))->workedThisBand);
    // Log-known calls rank before SCP-only ones.
    QCOMPARE(calls(on432).first(), QStringLiteral("OE5XYZ"));
}

void TestCheckPartialIndex::mergesSourcesAndKeepsTheBestGrid()
{
    CheckPartialIndex index;
    index.setHistoryCalls({{QStringLiteral("oe5xyz"), QStringLiteral("jn67ut")}});
    index.addSeenCall(QStringLiteral("OE5XYZ")); // no grid: must not wipe the history one
    index.setScpCalls({QStringLiteral("OE5XYZ")});
    auto m = index.matches(QStringLiteral("OE5"), QStringLiteral("144"));
    QCOMPARE(m.size(), 1);
    QCOMPARE(m.first().grid, QStringLiteral("JN67UT"));
    QCOMPARE(m.first().sources, int(CheckPartialMatch::History | CheckPartialMatch::Seen | CheckPartialMatch::Scp));

    // A seen call that only later reports a grid picks it up.
    index.addSeenCall(QStringLiteral("DL9ZZZ"));
    QVERIFY(index.matches(QStringLiteral("DL9"), QStringLiteral("144")).first().grid.isEmpty());
    index.addSeenCall(QStringLiteral("DL9ZZZ"), QStringLiteral("JO50XX"));
    QCOMPARE(index.matches(QStringLiteral("DL9"), QStringLiteral("144")).first().grid, QStringLiteral("JO50XX"));
    QCOMPARE(index.seenCount(), 2);
    index.clearSeen();
    QCOMPARE(index.seenCount(), 0);
    QVERIFY(index.matches(QStringLiteral("DL9"), QStringLiteral("144")).isEmpty());
}

void TestCheckPartialIndex::nearMissOnlyFromFourCharacters()
{
    CheckPartialIndex index;
    index.setScpCalls({QStringLiteral("OE5SOS")});
    // Three typed characters, one off: no near-miss search yet.
    QVERIFY(index.matches(QStringLiteral("OE6"), QStringLiteral("144")).isEmpty());
    // Four or more: a busted character still finds the call, flagged.
    auto m = index.matches(QStringLiteral("OE5SUS"), QStringLiteral("144"));
    QCOMPARE(m.size(), 1);
    QVERIFY(m.first().nearMiss);
    // ...and a missing character too.
    QVERIFY(find(index.matches(QStringLiteral("OE5SS"), QStringLiteral("144")), QStringLiteral("OE5SOS"))->nearMiss);
}

void TestCheckPartialIndex::parsesScpFiles()
{
    const QByteArray data("# master.scp style comment\r\n\r\noe5sos\r\nDL1ABC extra note\nOE5SOS\n  hb9zzz  \n");
    QCOMPARE(CheckPartialIndex::parseScp(data),
             (QStringList{QStringLiteral("OE5SOS"), QStringLiteral("DL1ABC"), QStringLiteral("HB9ZZZ")}));
}

void TestCheckPartialIndex::oneEditApart()
{
    QVERIFY(CheckPartialIndex::isOneEditApart(QStringLiteral("OE5SOS"), QStringLiteral("OE5SUS")));
    QVERIFY(CheckPartialIndex::isOneEditApart(QStringLiteral("OE5SOS"), QStringLiteral("OE5SO")));
    QVERIFY(CheckPartialIndex::isOneEditApart(QStringLiteral("OE5SOS"), QStringLiteral("OE5SOSS")));
    QVERIFY(!CheckPartialIndex::isOneEditApart(QStringLiteral("OE5SOS"), QStringLiteral("OE5SOS")));
    QVERIFY(!CheckPartialIndex::isOneEditApart(QStringLiteral("OE5SOS"), QStringLiteral("OE6SUS")));
    QVERIFY(!CheckPartialIndex::isOneEditApart(QStringLiteral("OE5SOS"), QStringLiteral("OE5S")));
}

void TestCheckPartialIndex::capsResults()
{
    CheckPartialIndex index;
    QStringList many;
    for (int i = 0; i < 200; ++i) {
        many << QStringLiteral("OE%1ABC").arg(i);
    }
    index.setScpCalls(many);
    QCOMPARE(index.matches(QStringLiteral("OE"), QStringLiteral("144"), 60).size(), 60);
    QCOMPARE(index.matches(QStringLiteral("OE"), QStringLiteral("144"), 1000).size(), 200);
}

QTEST_APPLESS_MAIN(TestCheckPartialIndex)
#include "test_check_partial_index.moc"
