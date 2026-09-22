#include <QtTest>

#include <QApplication>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>

#include "app/AppController.h"
#include "data/ContestDefinition.h"
#include "ui/ContestRulesEditor.h"

using namespace Contestprogramm;

class TestContestRulesOverride : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void saveToFileRoundTripsExchangeFields();
    void withExchangeFieldsKeepsEverythingElseUnchanged();
    void appControllerPrefersOverrideAfterReload();
    void rulesSurviveTheJsonRoundTrip();
    void validateRulesRejectsWhatWouldBreakTheLog();
    void editorWritesEveryRuleTheFormOffers();
    void ownContestNeedsNoShippedFile();
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

namespace {

// Eine Ausschreibung mit allem, was ContestDefinition::Rules kennt.
ContestDefinition::Rules fullRules()
{
    ContestDefinition::Rules rules;
    rules.name = QStringLiteral("Kurzwelle Probe");
    rules.bands = {QStringLiteral("3.5"), QStringLiteral("7"), QStringLiteral("14")};
    rules.dupeScope = {QStringLiteral("callsign"), QStringLiteral("band"), QStringLiteral("mode")};
    ContestDefinition::ExchangeField rst;
    rst.key = QStringLiteral("rst");
    rst.label = QStringLiteral("RST");
    rst.type = QStringLiteral("rst");
    ContestDefinition::ExchangeField serial;
    serial.key = QStringLiteral("serial");
    serial.label = QStringLiteral("Nr.");
    serial.type = QStringLiteral("int");
    serial.autoIncrement = true;
    rules.exchangeFields = {rst, serial};
    rules.multiplierField = QStringLiteral("none");
    rules.scoring = QStringLiteral("qso_count");
    rules.serialScope = QStringLiteral("contest");
    rules.cabrilloName = QStringLiteral("CQ-WW-CW");
    rules.modes = {QStringLiteral("CW")};
    return rules;
}

QCheckBox* bandCheck(const ContestRulesEditor& editor, const QString& band)
{
    return editor.findChild<QCheckBox*>(QStringLiteral("band_%1").arg(band));
}

void clickSave(ContestRulesEditor& editor)
{
    auto* buttons = editor.findChild<QDialogButtonBox*>();
    QVERIFY(buttons);
    QPushButton* save = buttons->button(QDialogButtonBox::Save);
    QVERIFY(save);
    save->click();
}

} // namespace

// Jede Regel muss den Weg durch die Datei überleben -- sonst schreibt
// der Dialog etwas hin, das beim nächsten Start anders zurückkommt.
void TestContestRulesOverride::rulesSurviveTheJsonRoundTrip()
{
    QString error;
    const ContestDefinition def = ContestDefinition::fromRules(QStringLiteral("KW_PROBE"), fullRules(), &error);
    QVERIFY2(def.isValid(), qPrintable(error));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("kw_probe.json"));
    QVERIFY2(def.saveToFile(path, &error), qPrintable(error));

    const ContestDefinition reread = ContestDefinition::loadFromFile(path, &error);
    QVERIFY2(reread.isValid(), qPrintable(error));
    QCOMPARE(reread.id(), QStringLiteral("KW_PROBE"));
    QCOMPARE(reread.name(), QStringLiteral("Kurzwelle Probe"));
    QCOMPARE(reread.bands(), QStringList({QStringLiteral("3.5"), QStringLiteral("7"), QStringLiteral("14")}));
    QCOMPARE(reread.dupeScope().size(), 3);
    QCOMPARE(reread.scoring(), QStringLiteral("qso_count"));
    QCOMPARE(reread.serialScope(), QStringLiteral("contest"));
    QCOMPARE(reread.multiplierField(), QStringLiteral("none"));
    QCOMPARE(reread.cabrilloName(), QStringLiteral("CQ-WW-CW"));
    QCOMPARE(reread.modes(), QStringList{QStringLiteral("CW")});
    QCOMPARE(reread.exchangeFields().size(), 2);
    QVERIFY(reread.exchangeFields().at(1).autoIncrement);
}

void TestContestRulesOverride::validateRulesRejectsWhatWouldBreakTheLog()
{
    QString error;
    ContestDefinition::Rules rules = fullRules();
    rules.name.clear();
    QVERIFY(!ContestDefinition::validateRules(rules, &error));

    rules = fullRules();
    rules.bands.clear();
    QVERIFY(!ContestDefinition::validateRules(rules, &error));

    rules = fullRules();
    rules.exchangeFields.clear();
    QVERIFY(!ContestDefinition::validateRules(rules, &error));

    rules = fullRules();
    rules.exchangeFields.append(rules.exchangeFields.first()); // gleicher Key zweimal
    QVERIFY(!ContestDefinition::validateRules(rules, &error));

    // Ohne Rufzeichen wäre jedes zweite QSO auf dem Band ein Doppel.
    rules = fullRules();
    rules.dupeScope = {QStringLiteral("band")};
    QVERIFY(!ContestDefinition::validateRules(rules, &error));

    rules = fullRules();
    rules.scoring = QStringLiteral("nach_gefuehl");
    QVERIFY(!ContestDefinition::validateRules(rules, &error));

    // Und eine Kennung, die kein Dateiname sein kann.
    QVERIFY(!ContestDefinition::fromRules(QStringLiteral("kw probe/2"), fullRules(), &error).isValid());
    QVERIFY(!error.isEmpty());
}

// Der Dialog schreibt, was in den Feldern steht -- alle Regeln, nicht
// nur die Exchange-Felder wie bis 2026-09-22.
void TestContestRulesOverride::editorWritesEveryRuleTheFormOffers()
{
    QString error;
    const ContestDefinition shipped = ContestDefinition::loadFromJson(QByteArrayLiteral(R"({
        "id": "FORM_TEST",
        "name": "Formularprobe",
        "bands": ["144"],
        "dupe_scope": ["callsign", "band"],
        "exchange_fields": [
            { "key": "rst", "label": "RST", "type": "rst" },
            { "key": "serial", "label": "Nr.", "type": "int", "auto_increment": true }
        ]
    })"), &error);
    QVERIFY2(shipped.isValid(), qPrintable(error));
    QFile::remove(ContestDefinition::overrideFilePath(shipped.id()));

    ContestRulesEditor editor({shipped}, shipped.id());
    editor.findChild<QLineEdit*>(QStringLiteral("contestName"))->setText(QStringLiteral("Formularprobe KW"));
    QVERIFY(bandCheck(editor, QStringLiteral("144"))->isChecked()); // aus der Definition geladen
    bandCheck(editor, QStringLiteral("144"))->setChecked(false);
    bandCheck(editor, QStringLiteral("14"))->setChecked(true);
    bandCheck(editor, QStringLiteral("21"))->setChecked(true);
    editor.findChild<QComboBox*>(QStringLiteral("contestScoring"))
        ->setCurrentIndex(editor.findChild<QComboBox*>(QStringLiteral("contestScoring"))
                              ->findData(QStringLiteral("qso_count")));
    editor.findChild<QComboBox*>(QStringLiteral("contestSerialScope"))
        ->setCurrentIndex(editor.findChild<QComboBox*>(QStringLiteral("contestSerialScope"))
                              ->findData(QStringLiteral("contest")));
    editor.findChild<QComboBox*>(QStringLiteral("contestMultiplier"))
        ->setCurrentIndex(editor.findChild<QComboBox*>(QStringLiteral("contestMultiplier"))
                              ->findData(QStringLiteral("none")));
    editor.findChild<QCheckBox*>(QStringLiteral("contestDupeMode"))->setChecked(true);
    editor.findChild<QCheckBox*>(QStringLiteral("mode_CW"))->setChecked(true);
    editor.findChild<QLineEdit*>(QStringLiteral("contestCabrillo"))->setText(QStringLiteral("CQ-WW-CW"));
    clickSave(editor);
    QCOMPARE(editor.result(), int(QDialog::Accepted));
    QCOMPARE(editor.savedContestId(), QStringLiteral("FORM_TEST"));

    const ContestDefinition saved =
        ContestDefinition::loadFromFile(ContestDefinition::overrideFilePath(shipped.id()), &error);
    QVERIFY2(saved.isValid(), qPrintable(error));
    QCOMPARE(saved.id(), QStringLiteral("FORM_TEST")); // die Kennung bleibt
    QCOMPARE(saved.name(), QStringLiteral("Formularprobe KW"));
    QCOMPARE(saved.bands(), QStringList({QStringLiteral("14"), QStringLiteral("21")}));
    QCOMPARE(saved.scoring(), QStringLiteral("qso_count"));
    QCOMPARE(saved.serialScope(), QStringLiteral("contest"));
    QCOMPARE(saved.multiplierField(), QStringLiteral("none"));
    QCOMPARE(saved.dupeScope(), QStringList({QStringLiteral("callsign"), QStringLiteral("band"), QStringLiteral("mode")}));
    QCOMPARE(saved.modes(), QStringList{QStringLiteral("CW")});
    QCOMPARE(saved.cabrilloName(), QStringLiteral("CQ-WW-CW"));

    QFile::remove(ContestDefinition::overrideFilePath(shipped.id()));
}

// Ein selbst angelegter Contest hat gar keine ausgelieferte Datei --
// er lebt allein in der eigenen. AppController muss ihn trotzdem
// finden.
void TestContestRulesOverride::ownContestNeedsNoShippedFile()
{
    QString error;
    const ContestDefinition shipped = ContestDefinition::loadFromJson(QByteArrayLiteral(R"({
        "id": "VORLAGE",
        "name": "Vorlage",
        "bands": ["144"],
        "dupe_scope": ["callsign", "band"],
        "exchange_fields": [ { "key": "rst", "label": "RST", "type": "rst" } ]
    })"), &error);
    QVERIFY2(shipped.isValid(), qPrintable(error));

    ContestRulesEditor editor({shipped}, shipped.id());
    const QString newId = editor.createDraftContest(QStringLiteral("Öster Probe 2027"));
    QCOMPARE(newId, QStringLiteral("OESTER_PROBE_2027"));
    QFile::remove(ContestDefinition::overrideFilePath(newId));
    bandCheck(editor, QStringLiteral("7"))->setChecked(true);
    clickSave(editor);
    QCOMPARE(editor.savedContestId(), newId);
    QVERIFY(QFile::exists(ContestDefinition::overrideFilePath(newId)));

    QTemporaryDir dataDir;
    QVERIFY(dataDir.isValid());
    AppController controller;
    QVERIFY(controller.openDatabase(dataDir.filePath(QStringLiteral("eigen.sqlite"))));
    const ContestDefinition* found = controller.findContestDefinition(newId);
    QVERIFY2(found, "Der selbst angelegte Contest taucht in der Liste nicht auf");
    QCOMPARE(found->name(), QStringLiteral("Öster Probe 2027"));
    // Und er drängt sich nicht vor: die Vorbelegung bleibt eine
    // ausgelieferte Ausschreibung.
    QVERIFY(controller.settings().activeContestId != newId);

    QFile::remove(ContestDefinition::overrideFilePath(newId));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestContestRulesOverride tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_contestrules_override.moc"
