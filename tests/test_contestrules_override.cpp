#include <QtTest>

#include <QApplication>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "data/ContestDefinition.h"

using namespace Contestprogramm;

class TestContestRulesOverride : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void saveToFileRoundTripsExchangeFields();
    void withExchangeFieldsKeepsEverythingElseUnchanged();
    void appControllerPrefersOverrideAfterReload();
};

void TestContestRulesOverride::initTestCase()
{
    // Sandbox QStandardPaths::AppDataLocation for this test process --
    // ContestDefinition::overrideDirectory() is built from it. Without
    // this, running the test would write into (and read stale state
    // back out of) the real operator's app-data directory (Qt calls
    // this "test mode", not process isolation, but the effect --
    // AppDataLocation resolving somewhere disposable -- is the same).
    QStandardPaths::setTestModeEnabled(true);
}

void TestContestRulesOverride::saveToFileRoundTripsExchangeFields()
{
    QString error;
    ContestDefinition def = ContestDefinition::loadFromJson(QByteArrayLiteral(R"({
        "id": "TEST_CONTEST",
        "name": "Test Contest",
        "bands": ["144", "432"],
        "dupe_scope": ["callsign", "band", "mode"],
        "exchange_fields": [
            { "key": "serial", "label": "Serial", "type": "int", "auto_increment": true },
            { "key": "grid", "label": "Grid", "type": "grid6" }
        ]
    })"),
                                                              &error);
    QVERIFY2(def.isValid(), qPrintable(error));

    ContestDefinition::ExchangeField nameField;
    nameField.key = QStringLiteral("name");
    nameField.label = QStringLiteral("Name");
    nameField.type = QStringLiteral("text");
    const ContestDefinition updated = def.withExchangeFields({def.exchangeFields().first(), nameField});

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("test_contest.json"));
    QVERIFY2(updated.saveToFile(path, &error), qPrintable(error));

    ContestDefinition reloaded = ContestDefinition::loadFromFile(path, &error);
    QVERIFY2(reloaded.isValid(), qPrintable(error));
    QCOMPARE(reloaded.id(), QStringLiteral("TEST_CONTEST"));
    QCOMPARE(reloaded.name(), QStringLiteral("Test Contest"));
    QCOMPARE(reloaded.bands(), (QStringList{QStringLiteral("144"), QStringLiteral("432")}));
    QCOMPARE(reloaded.exchangeFields().size(), 2);
    QCOMPARE(reloaded.exchangeFields().at(0).key, QStringLiteral("serial"));
    QVERIFY(reloaded.exchangeFields().at(0).autoIncrement);
    QCOMPARE(reloaded.exchangeFields().at(1).key, QStringLiteral("name"));
    QCOMPARE(reloaded.exchangeFields().at(1).type, QStringLiteral("text"));
    QVERIFY(!reloaded.exchangeFields().at(1).autoIncrement);
}

void TestContestRulesOverride::withExchangeFieldsKeepsEverythingElseUnchanged()
{
    QString error;
    ContestDefinition def = ContestDefinition::loadFromFile(
        QStringLiteral(CONTESTPROGRAMM_SOURCE_DIR "/resources/contest_definitions/oe_vhf_uhf.json"), &error);
    QVERIFY2(def.isValid(), qPrintable(error));

    ContestDefinition::ExchangeField onlyField;
    onlyField.key = QStringLiteral("serial");
    onlyField.label = QStringLiteral("Serial");
    onlyField.type = QStringLiteral("int");
    onlyField.autoIncrement = true;
    const ContestDefinition updated = def.withExchangeFields({onlyField});

    QCOMPARE(updated.id(), def.id());
    QCOMPARE(updated.name(), def.name());
    QCOMPARE(updated.bands(), def.bands());
    QCOMPARE(updated.dupeScope(), def.dupeScope());
    QCOMPARE(updated.multiplierField(), def.multiplierField());
    QCOMPARE(updated.exchangeFields().size(), 1);
}

void TestContestRulesOverride::appControllerPrefersOverrideAfterReload()
{
    QTemporaryDir dbDir;
    QVERIFY(dbDir.isValid());

    AppController controller;
    QString error;
    QVERIFY2(controller.openDatabase(dbDir.filePath(QStringLiteral("override_test.sqlite")), &error), qPrintable(error));

    const ContestDefinition* original = controller.findContestDefinition(QStringLiteral("OE_VHF_UHF"));
    QVERIFY(original != nullptr);
    QCOMPARE(original->exchangeFields().size(), 3); // shipped: rst + serial + grid

    ContestDefinition::ExchangeField onlyField;
    onlyField.key = QStringLiteral("serial");
    onlyField.label = QStringLiteral("Serial");
    onlyField.type = QStringLiteral("int");
    onlyField.autoIncrement = true;
    const ContestDefinition overridden = original->withExchangeFields({onlyField});
    const QString overridePath = ContestDefinition::overrideFilePath(QStringLiteral("OE_VHF_UHF"));
    QVERIFY2(overridden.saveToFile(overridePath, &error), qPrintable(error));

    QSignalSpy spy(&controller, &AppController::contestDefinitionsChanged);
    controller.reloadContestDefinitions();
    QCOMPARE(spy.count(), 1);

    const ContestDefinition* afterReload = controller.findContestDefinition(QStringLiteral("OE_VHF_UHF"));
    QVERIFY(afterReload != nullptr);
    QCOMPARE(afterReload->exchangeFields().size(), 1); // override applied
    QCOMPARE(afterReload->exchangeFields().first().key, QStringLiteral("serial"));

    // Clean up: setTestModeEnabled's sandbox is per-process, not
    // per-test, so leaving this behind could leak into a test that
    // runs later in the same process.
    QFile::remove(overridePath);
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestContestRulesOverride tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_contestrules_override.moc"
