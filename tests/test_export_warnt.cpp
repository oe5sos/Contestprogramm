// Ein QSO ohne Locator oder ohne empfangene Nummer zählt auf UKW null
// Punkte, und die Auswertung rechnet es gegen. Bis 2026-09-28 ging so
// ein Log kommentarlos in den Export -- erfahren hätte man es erst,
// wenn die Auswertung kommt.
//
// Der Anlass war Martins eigenes Log: eine Zeile vom 11. September mit
// "59003" im RST-Feld, ohne Nummer und ohne Locator (ein Schaden aus
// der alten Austausch-Zerlegung, am 21.09. behoben). "Log prüfen"
// hätte sie gefunden -- nur schaut da niemand von selbst hinein.

#include <QtTest>

#include <QApplication>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"
#include "data/ContestDefinition.h"
#include "data/LogCheck.h"
#include "data/QsoRecord.h"

#include <memory>

using namespace Contestprogramm;

class TestExportWarnt : public QObject
{
    Q_OBJECT

private slots:
    void theCheckFindsTheRowsThatScoreNothing();
    void aCleanLogHasNothingToStopIt();

private:
    std::unique_ptr<AppController> makeController(QTemporaryDir& dir, const QString& file);
    LogCheckResult check(AppController& controller);
};

std::unique_ptr<AppController> TestExportWarnt::makeController(QTemporaryDir& dir, const QString& file)
{
    auto controller = std::make_unique<AppController>();
    if (!controller->openDatabase(dir.filePath(file))) {
        return nullptr;
    }
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.activeContestId = QStringLiteral("IARU_R1_VHF_UHF");
    settings.rigctldHost.clear();
    settings.rotor1Enabled = false;
    settings.rotor2Enabled = false;
    controller->setSettings(settings);
    return controller;
}

LogCheckResult TestExportWarnt::check(AppController& controller)
{
    const ContestSettings settings = controller.settings();
    const ContestDefinition* def = controller.findContestDefinition(settings.activeContestId);
    if (!def) {
        return LogCheckResult();
    }
    const QVector<QsoRecord> records = controller.database().qsosForContest(settings.activeContestId);
    return checkLog(records, logCheckContextFor(*def, settings, records, QDateTime::currentDateTimeUtc()));
}

namespace {

QsoRecord qso(const QString& call, int serialSent)
{
    QsoRecord r;
    r.callsign = call;
    r.band = QStringLiteral("144");
    r.mode = QStringLiteral("SSB");
    r.timestampUtc = QDateTime::currentDateTimeUtc().addSecs(-60 * serialSent).toString(Qt::ISODate);
    r.rstSent = QStringLiteral("59");
    r.rstRcvd = QStringLiteral("59");
    r.serialSent = serialSent;
    r.serialRcvd = serialSent;
    r.gridSquare = QStringLiteral("JN58SD");
    r.contestId = QStringLiteral("IARU_R1_VHF_UHF");
    return r;
}

} // namespace

void TestExportWarnt::theCheckFindsTheRowsThatScoreNothing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("kaputt.sqlite"));
    QVERIFY(controller);

    QsoRecord gut = qso(QStringLiteral("DL1ABC"), 1);
    QVERIFY(controller->database().insertQso(gut));

    // Genau die Zeile aus Martins Log: RST zu lang, keine Nummer, kein
    // Locator.
    QsoRecord kaputt = qso(QStringLiteral("OE5DCM"), 2);
    kaputt.rstRcvd = QStringLiteral("59003");
    kaputt.serialRcvd = std::nullopt;
    kaputt.gridSquare.clear();
    QVERIFY(controller->database().insertQso(kaputt));

    const LogCheckResult result = check(*controller);
    QVERIFY2(result.errors >= 2, qPrintable(result.countsText()));
    QVERIFY2(!result.submittable(), qPrintable(result.countsText()));

    QStringList codes;
    for (const LogCheckIssue& issue : result.issues) {
        if (issue.callsign == QStringLiteral("OE5DCM")) {
            codes << issue.code;
        }
    }
    QVERIFY2(codes.contains(QStringLiteral("no_locator")), qPrintable(codes.join(QLatin1Char(','))));
    QVERIFY2(codes.contains(QStringLiteral("serial_rcvd_missing")), qPrintable(codes.join(QLatin1Char(','))));
    QVERIFY2(codes.contains(QStringLiteral("rst_odd")), qPrintable(codes.join(QLatin1Char(','))));
}

void TestExportWarnt::aCleanLogHasNothingToStopIt()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("sauber.sqlite"));
    QVERIFY(controller);
    for (int i = 1; i <= 3; ++i) {
        QsoRecord r = qso(QStringLiteral("DL%1ABC").arg(i), i);
        QVERIFY(controller->database().insertQso(r));
    }
    const LogCheckResult result = check(*controller);
    QVERIFY2(result.errors == 0, qPrintable(result.countsText()));
    QVERIFY(result.submittable());
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestExportWarnt tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_export_warnt.moc"
