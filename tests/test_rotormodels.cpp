// Ported from Longpath/NereusSDR's tests/tst_rotctld_process.cpp (the
// model-list slots) and tst_hamlib_installer.cpp (the curated-list
// soundness slots), adapted to this project's own test style
// (camelCase slot names, QTEST_APPLESS_MAIN) -- see core/RotorModels.h.

#include <QtTest>

#include "core/RotorModels.h"

using namespace Contestprogramm;

class TestRotorModels : public QObject
{
    Q_OBJECT

private slots:
    void modelListHasNoDuplicates();
    void modelListCoversTheControllersAskedAbout();
    void everyModelExplainsItself();
    void yaesuGs232aIsTheDefaultFirstEntry();
    void baudListCoversTheCommonRates();
};

void TestRotorModels::modelListHasNoDuplicates()
{
    // Two entries with the same number is a picker where one choice
    // silently does nothing different from another.
    QSet<int> ids;
    QSet<QString> names;
    for (const RotorModel& m : commonRotorModels()) {
        QVERIFY2(!ids.contains(m.hamlibId), qPrintable(QStringLiteral("duplicate id %1").arg(m.hamlibId)));
        QVERIFY2(!names.contains(m.name), qPrintable(m.name));
        ids.insert(m.hamlibId);
        names.insert(m.name);
        QVERIFY(m.hamlibId > 0);
    }
}

void TestRotorModels::modelListCoversTheControllersAskedAbout()
{
    // ERC has its own Hamlib driver at 404. ARCO has none, but speaks
    // GS-232A (601), DCU-1 (403) and SPID (902), so all three have to
    // be reachable from the list or an ARCO owner is stuck.
    QSet<int> ids;
    for (const RotorModel& m : commonRotorModels()) { ids.insert(m.hamlibId); }

    QVERIFY2(ids.contains(404), "ERC missing");
    QVERIFY2(ids.contains(601), "GS-232A missing -- ARCO needs it, and it is the station's own rotor");
    QVERIFY2(ids.contains(403), "DCU-1 missing -- ARCO needs it");
    QVERIFY2(ids.contains(902), "SPID Rot1Prog missing -- ARCO needs it");
    QVERIFY2(ids.contains(603), "GS-232B missing");
}

void TestRotorModels::everyModelExplainsItself()
{
    // The note is not decoration. Someone holding an ARCO box has no
    // idea that "Yaesu GS-232A" is their entry, and a list of model
    // names alone would send them looking for an ARCO line that does
    // not exist.
    for (const RotorModel& m : commonRotorModels()) {
        QVERIFY2(!m.note.trimmed().isEmpty(), qPrintable(m.name));
    }
}

void TestRotorModels::yaesuGs232aIsTheDefaultFirstEntry()
{
    // Operator confirmed 2026-09-11 the station's rotor will be Yaesu --
    // ContestSettings::rotor1HamlibModel/rotor2HamlibModel default to
    // 601, and SettingsDialog's model combo relies on this being the
    // first (index 0) entry so "Yaesu GS-232A vorausgewählt" holds even
    // when the combo is populated straight from commonRotorModels()'s
    // own order, not by searching for the id.
    const QVector<RotorModel> models = commonRotorModels();
    QVERIFY(!models.isEmpty());
    QCOMPARE(models.first().hamlibId, 601);
    QVERIFY(models.first().name.contains(QStringLiteral("Yaesu")));
}

void TestRotorModels::baudListCoversTheCommonRates()
{
    // 9600 is Hamlib's near-universal default (ContestSettings::
    // rotor1Baud/rotor2Baud's own default), so it must be selectable.
    const QVector<int> bauds = commonRotorBauds();
    QVERIFY(bauds.contains(9600));
    QVERIFY(!bauds.isEmpty());
    // Ascending, so a combo box built straight from this list reads
    // low-to-high without a separate sort step.
    for (int i = 1; i < bauds.size(); ++i) {
        QVERIFY(bauds.at(i) > bauds.at(i - 1));
    }
}

QTEST_APPLESS_MAIN(TestRotorModels)
#include "test_rotormodels.moc"
