#include <QtTest>

#include <QTimeZone>

#include "core/SkedList.h"
#include "core/SpotCandidate.h"

using namespace Contestprogramm;
using namespace Contestprogramm::SkedRules;

namespace {

QDateTime at(int h, int m)
{
    return QDateTime(QDate(2026, 10, 3), QTime(h, m), QTimeZone::UTC);
}

Sked open(int id, int h, int m)
{
    Sked s;
    s.id = id;
    s.callsign = QStringLiteral("DL%1ABC").arg(id);
    s.timeUtc = at(h, m);
    s.state = Sked::State::Open;
    return s;
}

SpotCandidate chatLine(const QString& from, const QString& to, const QString& msg)
{
    SpotCandidate c;
    c.callsign = from;
    c.rawLine = QStringLiteral("CH|1|20261003|%1|Hans|%2|%3|0|").arg(from, to, msg);
    c.timestampUtc = at(14, 31);
    c.source = QStringLiteral("on4kst");
    return c;
}

} // namespace

class TestSkedRules : public QObject
{
    Q_OBJECT

private slots:
    void statusFollowsTheClock();
    void nextOpenSkipsDoneAndOverdue();
    void frequenciesInAllTheirSpellings();
    void manualTimes();
    void suggestionsFromChatLines();
};

void TestSkedRules::statusFollowsTheClock()
{
    Sked s = open(1, 14, 35);
    QCOMPARE(statusText(s, at(14, 31)), QStringLiteral("in 4 min"));
    QCOMPARE(statusText(s, at(14, 34)), QStringLiteral("jetzt"));   // inside the 2-minute lead
    QCOMPARE(statusText(s, at(14, 40)), QStringLiteral("jetzt"));   // within the 10-minute grace
    QCOMPARE(statusText(s, at(14, 50)), QStringLiteral("verpasst"));
    QCOMPARE(statusText(s, at(12, 0)), QStringLiteral("offen"));    // more than 90 min ahead
    QVERIFY(!isDue(s, at(14, 31)));
    QVERIFY(isDue(s, at(14, 34)));
    QVERIFY(isOverdue(s, at(14, 50)));
    s.state = Sked::State::Done;
    QCOMPARE(statusText(s, at(14, 31)), QStringLiteral("erledigt"));
    s.state = Sked::State::Suggested;
    QCOMPARE(statusText(s, at(14, 31)), QStringLiteral("aus KST · übernehmen?"));
    QVERIFY(!isDue(s, at(14, 34)));
}

void TestSkedRules::nextOpenSkipsDoneAndOverdue()
{
    QVector<Sked> skeds = {open(1, 15, 10), open(2, 14, 35), open(3, 14, 0), open(4, 14, 50)};
    skeds[2].state = Sked::State::Done;
    const Sked* next = nextOpen(skeds, at(14, 31));
    QVERIFY(next);
    QCOMPARE(next->id, 2);
    // 14:35 gone by 20 minutes: overdue, the 14:50 one is next.
    next = nextOpen(skeds, at(14, 55));
    QVERIFY(next);
    QCOMPARE(next->id, 4);
    QVERIFY(!nextOpen({}, at(14, 31)));
}

void TestSkedRules::frequenciesInAllTheirSpellings()
{
    QCOMPARE(parseFrequencyHz(QStringLiteral("qrv 144.317 pse")).value_or(0), qint64(144317000));
    QCOMPARE(parseFrequencyHz(QStringLiteral("144,317")).value_or(0), qint64(144317000));
    QCOMPARE(parseFrequencyHz(QStringLiteral("144317 ok?")).value_or(0), qint64(144317000));
    QCOMPARE(parseFrequencyHz(QStringLiteral("432.2")).value_or(0), qint64(432200000));
    QCOMPARE(parseFrequencyHz(QStringLiteral("1296.200")).value_or(0), qint64(1296200000));
    QVERIFY(!parseFrequencyHz(QStringLiteral("59 003 JN58SD")).has_value());   // no frequency here
    QVERIFY(!parseFrequencyHz(QStringLiteral("tnx 73")).has_value());
    QVERIFY(!parseFrequencyHz(QStringLiteral("0144317")).has_value());        // digit before: not a QRG
}

void TestSkedRules::manualTimes()
{
    const QDateTime now = at(14, 31);
    QCOMPARE(parseManualTime(QString(), now).value_or(QDateTime()), now);
    QCOMPARE(parseManualTime(QStringLiteral("+5"), now).value_or(QDateTime()), at(14, 36));
    QCOMPARE(parseManualTime(QStringLiteral("14:50"), now).value_or(QDateTime()), at(14, 50));
    QCOMPARE(parseManualTime(QStringLiteral("1450"), now).value_or(QDateTime()), at(14, 50));
    // 00:05 asked at 23:50 means tomorrow; 14:20 asked at 14:31 is today (only 11 min gone).
    QCOMPARE(parseManualTime(QStringLiteral("00:05"), at(23, 50)).value_or(QDateTime()), at(0, 5).addDays(1));
    QCOMPARE(parseManualTime(QStringLiteral("14:20"), now).value_or(QDateTime()), at(14, 20));
    QVERIFY(!parseManualTime(QStringLiteral("soon"), now).has_value());
}

void TestSkedRules::suggestionsFromChatLines()
{
    const QDateTime now = at(14, 31);
    // Addressed via the destination field, time named.
    auto s = suggestionFromChat(chatLine(QStringLiteral("DL0GTH"), QStringLiteral("OE5SOS"), QStringLiteral("144.317 at 14:35?")),
                                QStringLiteral("oe5sos"), now);
    QVERIFY(s.has_value());
    QCOMPARE(s->callsign, QStringLiteral("DL0GTH"));
    QCOMPARE(s->freqHz, qint64(144317000));
    QCOMPARE(s->band, QStringLiteral("144"));
    QCOMPARE(s->timeUtc, at(14, 35));
    QCOMPARE(s->state, Sked::State::Suggested);
    QCOMPARE(s->note, QStringLiteral("144.317 at 14:35?"));
    // Addressed in the text, "in 5 min".
    s = suggestionFromChat(chatLine(QStringLiteral("OK1KIM"), QString(), QStringLiteral("OE5SOS 432.220 in 5 min ok?")),
                           QStringLiteral("OE5SOS"), now);
    QVERIFY(s.has_value());
    QCOMPARE(s->band, QStringLiteral("432"));
    QCOMPARE(s->timeUtc, at(14, 36));
    // No time at all: the message's own minute.
    s = suggestionFromChat(chatLine(QStringLiteral("S59DEM"), QStringLiteral("OE5SOS"), QStringLiteral("144300 now?")),
                           QStringLiteral("OE5SOS"), now);
    QVERIFY(s.has_value());
    QCOMPARE(s->timeUtc, now);
    // Not for us, or without a frequency, or our own line: nothing.
    QVERIFY(!suggestionFromChat(chatLine(QStringLiteral("DL0GTH"), QStringLiteral("DL1XYZ"), QStringLiteral("144.317 at 14:35?")),
                                QStringLiteral("OE5SOS"), now).has_value());
    QVERIFY(!suggestionFromChat(chatLine(QStringLiteral("DL0GTH"), QStringLiteral("OE5SOS"), QStringLiteral("tnx qso 73")),
                                QStringLiteral("OE5SOS"), now).has_value());
    QVERIFY(!suggestionFromChat(chatLine(QStringLiteral("OE5SOS"), QStringLiteral("DL0GTH"), QStringLiteral("144.317 at 14:35?")),
                                QStringLiteral("OE5SOS"), now).has_value());
    // "OE5SOSX" is not us.
    QVERIFY(!suggestionFromChat(chatLine(QStringLiteral("DL0GTH"), QString(), QStringLiteral("OE5SOSX 144.317")),
                                QStringLiteral("OE5SOS"), now).has_value());
}

QTEST_APPLESS_MAIN(TestSkedRules)
#include "test_sked_rules.moc"
