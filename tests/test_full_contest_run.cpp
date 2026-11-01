#include <QtTest>

#include <QApplication>
#include <QLocale>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "core/RigctldClient.h"
#include "data/AdifExporter.h"
#include "data/CabrilloExporter.h"
#include "data/ContestDatabase.h"
#include "data/ContestDefinition.h"
#include "data/ContestScoring.h"
#include "data/EdiExporter.h"
#include "data/QsoRecord.h"
#include "ui/MainWindow.h"
#include "ui/RateMeterWidget.h"
#include "ui/UnifiedLogWidget.h"

#include <memory>

using namespace Contestprogramm;

namespace {

const QString kContestId = QStringLiteral("IARU_R1_VHF_UHF");

std::unique_ptr<AppController> makeController(QTemporaryDir& dir)
{
    auto controller = std::make_unique<AppController>();
    if (!controller->openDatabase(dir.filePath(QStringLiteral("durchlauf.sqlite")))) {
        return nullptr;
    }
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.activeContestId = kContestId;
    settings.esmEnabled = false;
    controller->setSettings(settings);
    return controller;
}

void logCall(UnifiedLogWidget* log, const QString& call, const QString& serial, const QString& grid)
{
    log->setCallsign(call);
    log->setExchangeFieldValue(QStringLiteral("serial"), serial);
    log->setExchangeFieldValue(QStringLiteral("grid"), grid);
    emit log->logRequested();
}

QString readingsOf(RateMeterWidget* rate)
{
    QString text = rate->readingsText();
    text.replace(QChar(0x00A0), QLatin1Char(' '));
    text.replace(QChar(0x202F), QLatin1Char(' '));
    return text;
}

// The ";"-separated QSO lines of an EDI file, in file order.
QStringList qsoLinesOf(const QString& edi)
{
    QStringList lines;
    bool inRecords = false;
    for (const QString& line : edi.split(QStringLiteral("\r\n"))) {
        if (line.startsWith(QStringLiteral("[QSORecords"))) {
            inRecords = true;
            continue;
        }
        if (inRecords && !line.trimmed().isEmpty()) {
            lines << line;
        }
    }
    return lines;
}

QString headerValueOf(const QString& edi, const QString& key)
{
    for (const QString& line : edi.split(QStringLiteral("\r\n"))) {
        if (line.startsWith(key + QLatin1Char('='))) {
            return line.mid(key.size() + 1);
        }
    }
    return QString();
}

} // namespace

// One contest from the first keystroke to the files that get submitted:
// QSOs typed into the real entry row of the real MainWindow, a band
// change from the radio, a dupe, and then EDI/Cabrillo/ADIF written out
// of the same log. What this catches that the per-module tests cannot
// is DISAGREEMENT -- between the score on screen, the score in the EDI
// header, and the points on the EDI QSO lines themselves. A contest
// entry is rejected over exactly that kind of mismatch.
class TestFullContestRun : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void screenScoreEdiHeaderAndQsoLinesAllAgree();
    void aCorrectionIsOnTheScoreboardAtOnce();
};

void TestFullContestRun::screenScoreEdiHeaderAndQsoLinesAllAgree()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir);
    QVERIFY(controller);
    MainWindow window(*controller);
    auto* log = window.findChild<UnifiedLogWidget*>();
    auto* rate = window.findChild<RateMeterWidget*>();
    QVERIFY(log);
    QVERIFY(rate);

    // 2 m, then 70 cm, then back -- the band follows the radio.
    emit controller->rigctldClient().frequencyChanged(144300000);
    logCall(log, QStringLiteral("DL1ABC"), QStringLiteral("1"), QStringLiteral("JN58SD"));
    logCall(log, QStringLiteral("OE3XYZ"), QStringLiteral("17"), QStringLiteral("JN88TC"));
    logCall(log, QStringLiteral("OK1KIM"), QStringLiteral("42"), QStringLiteral("JO60RN"));
    // Same station again on the same band and mode: a dupe.
    logCall(log, QStringLiteral("DL1ABC"), QStringLiteral("9"), QStringLiteral("JN58SD"));
    emit controller->rigctldClient().frequencyChanged(432200000);
    logCall(log, QStringLiteral("HB9ZZZ"), QStringLiteral("3"), QStringLiteral("JN47PM"));
    logCall(log, QStringLiteral("OE5XYZ"), QStringLiteral("8"), QStringLiteral("JN67UT")); // own square
    emit controller->rigctldClient().frequencyChanged(144300000);
    logCall(log, QStringLiteral("DL9WWW"), QStringLiteral("11"), QStringLiteral("JN49CV"));

    const QVector<QsoRecord> qsos = controller->database().qsosForContest(kContestId);
    QCOMPARE(qsos.size(), 7);

    // The dupe is in the log, marked, and the serial sequence went on
    // past it rather than handing its number out twice.
    QVERIFY(qsos.at(3).isDupe);
    QCOMPARE(qsos.at(3).callsign, QStringLiteral("DL1ABC"));
    QCOMPARE(qsos.at(6).band, QStringLiteral("144"));
    QCOMPARE(qsos.at(6).serialSent.value_or(-1), 5); // 144: 1,2,3,(dupe 4),5

    // ---- the score, computed once ----------------------------------
    const ContestDefinition* definition = controller->findContestDefinition(kContestId);
    QVERIFY(definition);
    const ContestScore score = computeContestScore(qsos, controller->settings().ownGrid, definition->bands(),
                                                   definition->scoring());
    QCOMPARE(score.validQsos, 6);
    QCOMPARE(score.dupes, 1);
    QVERIFY(score.points > 0);

    // ---- the score on screen ---------------------------------------
    const QString shown = readingsOf(rate);
    const QString groupedPoints = QLocale(QLocale::German, QLocale::Austria)
                                      .toString(score.points)
                                      .replace(QChar(0x00A0), QLatin1Char(' '))
                                      .replace(QChar(0x202F), QLatin1Char(' '));
    QVERIFY2(shown.contains(QStringLiteral("Punkte %1").arg(groupedPoints)), qPrintable(shown));
    // The panel counts the dupe as a QSO (it IS a contact, and it goes
    // into the EDI with its "D") -- it just brings no points.
    QVERIFY2(shown.contains(QStringLiteral("QSOs 7")), qPrintable(shown));

    // ---- the EDI files, one per band --------------------------------
    EdiExporter edi(controller->database());
    EdiStationInfo station;
    station.operators = QStringLiteral("OE5SOS");
    const QStringList bands = edi.bandsWithQsos(kContestId, *definition);
    QCOMPARE(bands, QStringList({QStringLiteral("144"), QStringLiteral("432")}));

    qint64 pointsAcrossFiles = 0;
    int validAcrossFiles = 0;
    for (const QString& band : bands) {
        const QString text = edi.exportBand(kContestId, band, *definition, controller->settings(), station);
        const BandScore* bandScore = score.band(band);
        QVERIFY(bandScore);

        // Header claims match the one scoring function.
        QCOMPARE(headerValueOf(text, QStringLiteral("CQSOs")), QStringLiteral("%1;1").arg(bandScore->validQsos));
        QCOMPARE(headerValueOf(text, QStringLiteral("CQSOP")), QString::number(bandScore->points));
        QCOMPARE(headerValueOf(text, QStringLiteral("CToSc")), QString::number(bandScore->points));
        QCOMPARE(headerValueOf(text, QStringLiteral("CWWLs")), QStringLiteral("%1;0;1").arg(bandScore->largeSquares));
        QCOMPARE(headerValueOf(text, QStringLiteral("CODXC")),
                 QStringLiteral("%1;%2;%3").arg(bandScore->odxCall, bandScore->odxGrid).arg(bandScore->odxKm));
        QCOMPARE(headerValueOf(text, QStringLiteral("PCall")), QStringLiteral("OE5SOS"));
        QCOMPARE(headerValueOf(text, QStringLiteral("PWWLo")), QStringLiteral("JN67UT"));

        // And the QSO lines themselves add up to the header's claim.
        const QStringList lines = qsoLinesOf(text);
        QVERIFY(!lines.isEmpty());
        QVERIFY2(text.contains(QStringLiteral("[QSORecords;%1]").arg(lines.size())), qPrintable(text));

        qint64 lineSum = 0;
        int lineValid = 0;
        QString previousStamp;
        for (const QString& line : lines) {
            const QStringList fields = line.split(QLatin1Char(';'));
            QCOMPARE(fields.size(), 15);
            const QString stamp = fields.at(0) + fields.at(1);
            QVERIFY2(previousStamp.isEmpty() || stamp >= previousStamp, qPrintable(line));
            previousStamp = stamp;
            bool ok = false;
            const int points = fields.at(10).toInt(&ok);
            QVERIFY2(ok, qPrintable(line));
            lineSum += points;
            if (fields.at(14) == QStringLiteral("D")) {
                QCOMPARE(points, 0); // a dupe scores nothing, and says so
            } else {
                ++lineValid;
            }
            QCOMPARE(fields.at(3), QStringLiteral("1")); // mode 1 = SSB
            QVERIFY(!fields.at(4).isEmpty());            // RST sent
            QVERIFY(!fields.at(6).isEmpty());            // RST received
        }
        QCOMPARE(lineSum, bandScore->points);
        QCOMPARE(lineValid, bandScore->validQsos);
        pointsAcrossFiles += lineSum;
        validAcrossFiles += lineValid;
    }
    QCOMPARE(pointsAcrossFiles, score.points);
    QCOMPARE(validAcrossFiles, score.validQsos);

    // ---- Cabrillo and ADIF over the same log ------------------------
    const QString cabrillo = CabrilloExporter(controller->database())
                                 .exportContest(kContestId, *definition, controller->settings());
    QCOMPARE(cabrillo.count(QStringLiteral("QSO: ")), 7); // dupes are submitted too; the robot strikes them
    QVERIFY2(cabrillo.contains(QStringLiteral("CALLSIGN: OE5SOS")), qPrintable(cabrillo));

    const QString adif = AdifExporter(controller->database()).exportContest(kContestId, controller->settings());
    QCOMPARE(adif.count(QStringLiteral("<EOR>")), 7);
    QVERIFY2(adif.contains(QStringLiteral("2m")), qPrintable(adif.left(400)));
}

// The Rate panel refreshes itself every 15 seconds. That is fine for a
// clock and wrong for a scoreboard: striking a QSO out and correcting a
// locator both change the claimed score, and the operator makes those
// corrections precisely to see the right number. Both have to land at
// once.
void TestFullContestRun::aCorrectionIsOnTheScoreboardAtOnce()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir);
    QVERIFY(controller);
    MainWindow window(*controller);
    auto* log = window.findChild<UnifiedLogWidget*>();
    auto* rate = window.findChild<RateMeterWidget*>();
    QVERIFY(log);
    QVERIFY(rate);

    emit controller->rigctldClient().frequencyChanged(144300000);
    logCall(log, QStringLiteral("DL1ABC"), QStringLiteral("1"), QStringLiteral("JN58SD"));
    logCall(log, QStringLiteral("OE3XYZ"), QStringLiteral("2"), QStringLiteral("JN88TC"));
    QVERIFY2(readingsOf(rate).contains(QStringLiteral("QSOs 2")), qPrintable(readingsOf(rate)));

    const QVector<QsoRecord> qsos = controller->database().qsosForContest(kContestId);
    QCOMPARE(qsos.size(), 2);

    // Struck out: one QSO fewer on the board, right away. And back.
    emit log->historyInvalidToggleRequested(qsos.at(0).id);
    QVERIFY2(readingsOf(rate).contains(QStringLiteral("QSOs 1")), qPrintable(readingsOf(rate)));
    emit log->historyInvalidToggleRequested(qsos.at(0).id);
    QVERIFY2(readingsOf(rate).contains(QStringLiteral("QSOs 2")), qPrintable(readingsOf(rate)));

    // A corrected locator is a different distance, and distance is the
    // score: the points on the board must follow the correction.
    const QString before = readingsOf(rate);
    emit log->historyExchangeRcvdEditRequested(qsos.at(1).id, QStringLiteral("59 002 JO60RN"));
    const QVector<QsoRecord> corrected = controller->database().qsosForContest(kContestId);
    QCOMPARE(corrected.at(1).gridSquare, QStringLiteral("JO60RN"));
    QVERIFY2(readingsOf(rate) != before, qPrintable(readingsOf(rate)));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestFullContestRun tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_full_contest_run.moc"
