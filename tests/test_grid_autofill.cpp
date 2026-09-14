#include <QtTest>

#include <QApplication>
#include <QTemporaryDir>

#include "data/ContestDatabase.h"
#include "data/ContestDefinition.h"
#include "data/QsoRecord.h"
#include "ui/UnifiedLogWidget.h"

using namespace Contestprogramm;

namespace {

QsoRecord makeRecord(const QString& callsign, const QString& contestId, const QString& timestampUtc,
                      const QString& gridSquare, int serialRcvd)
{
    QsoRecord record;
    record.callsign = callsign;
    record.band = QStringLiteral("144");
    record.mode = QStringLiteral("SSB");
    record.timestampUtc = timestampUtc;
    record.contestId = contestId;
    record.gridSquare = gridSquare;
    record.serialRcvd = serialRcvd;
    record.exchangeRcvd = QString::number(serialRcvd).rightJustified(3, QLatin1Char('0')) + QLatin1Char(' ') + gridSquare;
    return record;
}

// The shipped contest_definitions' exchange_fields shape (serial +
// grid) -- see resources/contest_definitions/oe_vhf_uhf.json.
QVector<ContestDefinition::ExchangeField> serialGridFields()
{
    ContestDefinition::ExchangeField serial;
    serial.key = QStringLiteral("serial");
    serial.label = QStringLiteral("Serial");
    serial.type = QStringLiteral("int");
    serial.autoIncrement = true;

    ContestDefinition::ExchangeField grid;
    grid.key = QStringLiteral("grid");
    grid.label = QStringLiteral("Grid");
    grid.type = QStringLiteral("grid6");

    return {serial, grid};
}

} // namespace

class TestGridAutofill : public QObject
{
    Q_OBJECT

private slots:
    void databaseHitReturnsMostRecentGridAndSerial();
    void databaseMissReturnsNullopt();
    void widgetPrefillsEmptyFieldsOnly();
    void widgetDoesNotOverwriteAlreadyFilledFields();
};

void TestGridAutofill::databaseHitReturnsMostRecentGridAndSerial()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("autofill_hit.sqlite")), QStringLiteral("autofill_hit")));

    QsoRecord older = makeRecord(QStringLiteral("OE1ABC"), QStringLiteral("OE_VHF_UHF"),
                                  QStringLiteral("2026-06-13T10:00:00Z"), QStringLiteral("JN77QT"), 2);
    QVERIFY(db.insertQso(older));
    QsoRecord newer = makeRecord(QStringLiteral("OE1ABC"), QStringLiteral("OE_VHF_UHF"),
                                  QStringLiteral("2026-06-13T12:00:00Z"), QStringLiteral("JN88TC"), 7);
    QVERIFY(db.insertQso(newer));

    // Case/whitespace-normalized, like DupeChecker::isDupe.
    const auto found = db.knownExchangeForCallsign(QStringLiteral("  oe1abc "), QStringLiteral("OE_VHF_UHF"));
    QVERIFY(found.has_value());
    QCOMPARE(found->gridSquare, QStringLiteral("JN88TC")); // the more recent of the two
    QVERIFY(found->serialRcvd.has_value());
    QCOMPARE(*found->serialRcvd, 7);
}

void TestGridAutofill::databaseMissReturnsNullopt()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("autofill_miss.sqlite")), QStringLiteral("autofill_miss")));

    QsoRecord record = makeRecord(QStringLiteral("OE1ABC"), QStringLiteral("OE_VHF_UHF"),
                                   QStringLiteral("2026-06-13T10:00:00Z"), QStringLiteral("JN77QT"), 2);
    QVERIFY(db.insertQso(record));

    // Unknown callsign.
    QVERIFY(!db.knownExchangeForCallsign(QStringLiteral("OE9ZZZ"), QStringLiteral("OE_VHF_UHF")).has_value());
    // Known callsign, but a different contest.
    QVERIFY(!db.knownExchangeForCallsign(QStringLiteral("OE1ABC"), QStringLiteral("IARU_R1_VHF_UHF")).has_value());
}

void TestGridAutofill::widgetPrefillsEmptyFieldsOnly()
{
    UnifiedLogWidget widget;
    widget.setExchangeFields(serialGridFields());
    QVERIFY(widget.exchangeReceived().value(QStringLiteral("grid")).isEmpty());
    QVERIFY(widget.exchangeReceived().value(QStringLiteral("serial")).isEmpty());

    widget.applyKnownExchange(QStringLiteral("JN77QT"), 5);
    QCOMPARE(widget.exchangeReceived().value(QStringLiteral("grid")), QStringLiteral("JN77QT"));
    QCOMPARE(widget.exchangeReceived().value(QStringLiteral("serial")), QStringLiteral("5"));
}

void TestGridAutofill::widgetDoesNotOverwriteAlreadyFilledFields()
{
    UnifiedLogWidget widget;
    widget.setExchangeFields(serialGridFields());

    // Simulate an explicit fill (operator typing, or a click-to-fill
    // from a spot/chat candidate row) that happens before a
    // debounced lookup resolves.
    widget.setExchangeFieldValue(QStringLiteral("grid"), QStringLiteral("JN88TC"));
    widget.applyKnownExchange(QStringLiteral("JN77QT"), 5);

    // The explicit value must win -- autofill only ever fills a field
    // that is still empty, per the plan's "weiter überschreibbar" note
    // (here read the other way: what is already there is never
    // silently replaced).
    QCOMPARE(widget.exchangeReceived().value(QStringLiteral("grid")), QStringLiteral("JN88TC"));
    // Serial-received was still empty, so that one does get filled.
    QCOMPARE(widget.exchangeReceived().value(QStringLiteral("serial")), QStringLiteral("5"));

    // Once the operator has also filled serial-received explicitly, a
    // later autofill call must not touch it either.
    widget.resetForNextEntry();
    QVERIFY(widget.exchangeReceived().value(QStringLiteral("grid")).isEmpty());
    QVERIFY(widget.exchangeReceived().value(QStringLiteral("serial")).isEmpty());
}

// Not QTEST_APPLESS_MAIN: UnifiedLogWidget is a QWidget subclass, which
// needs a live QApplication (not just QCoreApplication) to construct.
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestGridAutofill tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_grid_autofill.moc"
