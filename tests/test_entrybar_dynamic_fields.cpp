#include <QtTest>

#include <QApplication>
#include <QLineEdit>

#include "data/ContestDefinition.h"
#include "ui/UnifiedLogWidget.h"

using namespace Contestprogramm;

namespace {

// Mirrors the shipped contest_definitions' shape (see
// resources/contest_definitions/oe_vhf_uhf.json): serial (int,
// auto_increment) + grid (grid6).
QVector<ContestDefinition::ExchangeField> twoFieldSerialGrid()
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

// A ContestRulesEditor-edited contest: three fields, none named
// "grid"/"serial" -- exercises the type/auto_increment-based
// generalization this task's whole point, not the literal keys the
// shipped JSON files happen to use today.
QVector<ContestDefinition::ExchangeField> threeFieldRenamed()
{
    ContestDefinition::ExchangeField nr;
    nr.key = QStringLiteral("nr");
    nr.label = QStringLiteral("Nr.");
    nr.type = QStringLiteral("int");
    nr.autoIncrement = true;

    ContestDefinition::ExchangeField locator;
    locator.key = QStringLiteral("locator");
    locator.label = QStringLiteral("Locator");
    locator.type = QStringLiteral("grid6");

    ContestDefinition::ExchangeField name;
    name.key = QStringLiteral("name");
    name.label = QStringLiteral("Name");
    name.type = QStringLiteral("text");

    return {nr, locator, name};
}

} // namespace

class TestEntryBarDynamicFields : public QObject
{
    Q_OBJECT

private slots:
    void rebuildsRowFromExchangeFields();
    void exchangeReceivedKeyedByFieldKeyNotFixedNames();
    void setExchangeFieldValueTargetsByKey();
    void applyKnownExchangeGeneralizesToTypeAndAutoIncrement();
    void resetForNextEntryClearsAllDynamicFields();
    void rebuildDropsFieldsNoLongerPresent();
    void spaceTypesASpaceInAFreeTextFieldButJumpsOnElsewhere();
};

void TestEntryBarDynamicFields::rebuildsRowFromExchangeFields()
{
    UnifiedLogWidget widget;
    // Before any setExchangeFields() call, the row is empty -- no
    // hardcoded grid/serial pair left over from the old fixed members.
    QVERIFY(widget.exchangeReceived().isEmpty());

    widget.setExchangeFields(twoFieldSerialGrid());
    QCOMPARE(widget.exchangeReceived().size(), 2);
    QVERIFY(widget.exchangeReceived().contains(QStringLiteral("serial")));
    QVERIFY(widget.exchangeReceived().contains(QStringLiteral("grid")));
}

void TestEntryBarDynamicFields::exchangeReceivedKeyedByFieldKeyNotFixedNames()
{
    UnifiedLogWidget widget;
    widget.setExchangeFields(threeFieldRenamed());

    widget.setExchangeFieldValue(QStringLiteral("nr"), QStringLiteral("7"));
    widget.setExchangeFieldValue(QStringLiteral("locator"), QStringLiteral("jn77qt"));
    widget.setExchangeFieldValue(QStringLiteral("name"), QStringLiteral("Hans"));

    const QMap<QString, QString> received = widget.exchangeReceived();
    QCOMPARE(received.size(), 3);
    QCOMPARE(received.value(QStringLiteral("nr")), QStringLiteral("7"));
    // grid6-typed fields are upper-cased regardless of their key name.
    QCOMPARE(received.value(QStringLiteral("locator")), QStringLiteral("JN77QT"));
    QCOMPARE(received.value(QStringLiteral("name")), QStringLiteral("Hans"));
}

void TestEntryBarDynamicFields::setExchangeFieldValueTargetsByKey()
{
    UnifiedLogWidget widget;
    widget.setExchangeFields(twoFieldSerialGrid());
    widget.setExchangeFieldValue(QStringLiteral("grid"), QStringLiteral("JN88TC"));
    QCOMPARE(widget.exchangeReceived().value(QStringLiteral("grid")), QStringLiteral("JN88TC"));

    // Unknown key: a documented no-op, not a crash.
    widget.setExchangeFieldValue(QStringLiteral("does_not_exist"), QStringLiteral("x"));
    QVERIFY(!widget.exchangeReceived().contains(QStringLiteral("does_not_exist")));
}

void TestEntryBarDynamicFields::applyKnownExchangeGeneralizesToTypeAndAutoIncrement()
{
    UnifiedLogWidget widget;
    widget.setExchangeFields(threeFieldRenamed());

    widget.applyKnownExchange(QStringLiteral("JN77QT"), 42);

    const QMap<QString, QString> received = widget.exchangeReceived();
    // "locator" carries type grid6 (not literally named "grid") --
    // must still get the grid autofill.
    QCOMPARE(received.value(QStringLiteral("locator")), QStringLiteral("JN77QT"));
    // "nr" carries auto_increment (not literally named "serial") --
    // must still get the serial autofill.
    QCOMPARE(received.value(QStringLiteral("nr")), QStringLiteral("42"));
    // The free-text field has neither marker, so the known-exchange
    // autofill leaves it alone.
    QVERIFY(received.value(QStringLiteral("name")).isEmpty());
}

void TestEntryBarDynamicFields::resetForNextEntryClearsAllDynamicFields()
{
    UnifiedLogWidget widget;
    widget.setExchangeFields(threeFieldRenamed());
    widget.setExchangeFieldValue(QStringLiteral("nr"), QStringLiteral("3"));
    widget.setExchangeFieldValue(QStringLiteral("locator"), QStringLiteral("JN77QT"));
    widget.setExchangeFieldValue(QStringLiteral("name"), QStringLiteral("Hans"));

    widget.resetForNextEntry();

    const QMap<QString, QString> received = widget.exchangeReceived();
    QCOMPARE(received.size(), 3);
    for (const QString& key : received.keys()) {
        QVERIFY(received.value(key).isEmpty());
    }
}

void TestEntryBarDynamicFields::rebuildDropsFieldsNoLongerPresent()
{
    UnifiedLogWidget widget;
    widget.setExchangeFields(threeFieldRenamed());
    QCOMPARE(widget.exchangeReceived().size(), 3);

    // Rebuilding for a different (e.g. newly-selected) contest drops
    // whatever the old row held, including keys no longer declared.
    widget.setExchangeFields(twoFieldSerialGrid());
    const QMap<QString, QString> received = widget.exchangeReceived();
    QCOMPARE(received.size(), 2);
    QVERIFY(!received.contains(QStringLiteral("name")));
    QVERIFY(!received.contains(QStringLiteral("locator")));
}

// Not QTEST_APPLESS_MAIN: UnifiedLogWidget is a QWidget subclass, which
// needs a live QApplication (not just QCoreApplication) to construct.

// Die Leertaste springt in der Eingabezeile zum nächsten Feld -- in
// Rufzeichen, RST, Nummer und Locator ist ein Leerzeichen ja nie
// sinnvoll. In einem FREIEN TEXTFELD schon: der Regeleditor legt neue
// Austauschfelder genau als "text" an (ContestRulesEditor.cpp), und ein
// Contest, der Name, QTH oder Sektion austauscht, braucht dort ein
// Leerzeichen ("St. Veit", "OE 05"). Vorher war es in diesem einen
// Feld nicht einzutippen.
void TestEntryBarDynamicFields::spaceTypesASpaceInAFreeTextFieldButJumpsOnElsewhere()
{
    UnifiedLogWidget widget;
    widget.setExchangeFields(threeFieldRenamed()); // nr (int), locator (grid6), name (text)
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    const auto feldMit = [&widget](const QString& platzhalter) -> QLineEdit* {
        for (QLineEdit* edit : widget.findChildren<QLineEdit*>()) {
            if (edit->placeholderText() == platzhalter) {
                return edit;
            }
        }
        return nullptr;
    };

    QLineEdit* name = feldMit(QStringLiteral("Name"));
    QVERIFY2(name, "Das Textfeld der Eingabezeile fehlt");
    name->setFocus();
    name->setText(QStringLiteral("St."));
    name->setCursorPosition(name->text().size());
    QTest::keyClick(name, Qt::Key_Space);
    QTest::keyClicks(name, QStringLiteral("Veit"));
    QCOMPARE(widget.exchangeReceived().value(QStringLiteral("name")), QStringLiteral("St. Veit"));

    // Und überall sonst bleibt die Leertaste der Sprung: im Locator
    // (grid6) kommt kein Leerzeichen an.
    QLineEdit* locator = feldMit(QStringLiteral("Locator"));
    QVERIFY(locator);
    locator->setFocus();
    locator->setText(QStringLiteral("JN67"));
    locator->setCursorPosition(locator->text().size());
    QTest::keyClick(locator, Qt::Key_Space);
    QCOMPARE(widget.exchangeReceived().value(QStringLiteral("locator")), QStringLiteral("JN67"));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestEntryBarDynamicFields tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_entrybar_dynamic_fields.moc"
