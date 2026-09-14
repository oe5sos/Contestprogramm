// Tests for MessageDrafter -- pure string assembly, see src/core/
// assistant/MessageDrafter.h's own doc comment for the funkbetrieblich-
// correct "CQ only for a general call, never for a directed one"
// distinction this exercises.

#include <QtTest>

#include "core/assistant/MessageDrafter.h"

using namespace Contestprogramm;

class TestMessageDrafter : public QObject
{
    Q_OBJECT

private slots:
    void directedCallNeverCarriesCqPrefix();
    void directedCallOmitsExchangeWhenEmpty();
    void cqCallHasNoTargetCallsign();
    void cqCallCanIncludeContestTag();
};

void TestMessageDrafter::directedCallNeverCarriesCqPrefix()
{
    const QString text = MessageDrafter::draftDirectedCall(QStringLiteral("SP9XYZ"), QStringLiteral("OE5SOS"),
                                                             QStringLiteral("047 JN67VV"));
    QCOMPARE(text, QStringLiteral("SP9XYZ DE OE5SOS 047 JN67VV"));
    QVERIFY(!text.contains(QStringLiteral("CQ")));
}

void TestMessageDrafter::directedCallOmitsExchangeWhenEmpty()
{
    const QString text = MessageDrafter::draftDirectedCall(QStringLiteral("SP9XYZ"), QStringLiteral("OE5SOS"), QString());
    QCOMPARE(text, QStringLiteral("SP9XYZ DE OE5SOS"));
}

void TestMessageDrafter::cqCallHasNoTargetCallsign()
{
    const QString text = MessageDrafter::draftCqCall(QStringLiteral("OE5SOS"));
    QCOMPARE(text, QStringLiteral("CQ DE OE5SOS"));
}

void TestMessageDrafter::cqCallCanIncludeContestTag()
{
    const QString text = MessageDrafter::draftCqCall(QStringLiteral("OE5SOS"), /*includeContestTag=*/true);
    QCOMPARE(text, QStringLiteral("CQ CONTEST DE OE5SOS"));
}

QTEST_MAIN(TestMessageDrafter)
#include "test_message_drafter.moc"
