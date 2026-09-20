#include <QtTest>

#include <QApplication>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableWidget>

#include "data/LogCheck.h"
#include "ui/LogCheckWindow.h"

using namespace Contestprogramm;

namespace {

LogCheckIssue issue(LogCheckIssue::Severity severity, int qsoId, const QString& call, const QString& band,
                    const QString& code, const QString& message)
{
    LogCheckIssue i;
    i.severity = severity;
    i.qsoId = qsoId;
    i.timestampUtc = qsoId >= 0 ? QStringLiteral("2026-10-03T15:0%1:00Z").arg(qsoId) : QString();
    i.callsign = call;
    i.band = band;
    i.code = code;
    i.message = message;
    return i;
}

} // namespace

// ui/LogCheckWindow.h: the list a LogCheckResult becomes, the verdict
// line, and the jump-to-QSO signal (only for rows that are about a QSO).
class TestLogCheckWindow : public QObject
{
    Q_OBJECT

private slots:
    void listsIssuesWithVerdict();
    void activatingARowJumpsToItsQsoButNotForLogWideRows();
    void showingAsksForAFreshCheck();
};

void TestLogCheckWindow::listsIssuesWithVerdict()
{
    LogCheckWindow window;
    LogCheckResult result;
    result.checkedQsos = 12;
    result.issues << issue(LogCheckIssue::Severity::Error, 3, QStringLiteral("DL1ABC"), QStringLiteral("432"),
                           QStringLiteral("no_locator"), QStringLiteral("Kein Locator"))
                  << issue(LogCheckIssue::Severity::Warning, 5, QStringLiteral("OE3XYZ"), QStringLiteral("144"),
                           QStringLiteral("rst_odd"), QStringLiteral("RST \"5\" passt nicht zu SSB"))
                  << issue(LogCheckIssue::Severity::Hint, -1, QString(), QStringLiteral("144"),
                           QStringLiteral("serial_sent_gap"), QStringLiteral("Gesendete Nummer 004 fehlt auf 144"));
    result.errors = 1;
    result.warnings = 1;
    result.hints = 1;
    window.setResult(result, QStringLiteral("IARU Region 1 UHF/Microwave Contest"));

    QCOMPARE(window.rowCount(), 3);
    QCOMPARE(window.rowText(0, 0), QStringLiteral("FEHLER"));
    QCOMPARE(window.rowText(0, 1), QStringLiteral("03.10. 15:03"));
    QCOMPARE(window.rowText(0, 2), QStringLiteral("DL1ABC"));
    QCOMPARE(window.rowText(0, 3), QStringLiteral("432"));
    QCOMPARE(window.rowText(0, 4), QStringLiteral("Kein Locator"));
    QCOMPARE(window.rowText(1, 0), QStringLiteral("WARNUNG"));
    QCOMPARE(window.rowText(2, 0), QStringLiteral("HINWEIS"));
    QVERIFY(window.rowText(2, 1).isEmpty());
    QCOMPARE(window.qsoIdAt(0), 3);
    QCOMPARE(window.qsoIdAt(2), -1);
    QVERIFY(window.summaryText().contains(QStringLiteral("12 QSOs geprüft")));
    QVERIFY(window.summaryText().contains(QStringLiteral("1 Fehler, 1 Warnung, 1 Hinweis")));
    QVERIFY(window.summaryText().contains(QStringLiteral("Vor der Abgabe beheben")));

    // Errors gone: submittable, and the old rows are replaced, not appended.
    LogCheckResult clean;
    clean.checkedQsos = 12;
    clean.issues << result.issues.at(2);
    clean.hints = 1;
    window.setResult(clean, QString());
    QCOMPARE(window.rowCount(), 1);
    QVERIFY(window.summaryText().contains(QStringLiteral("Abgabebereit")));
}

void TestLogCheckWindow::activatingARowJumpsToItsQsoButNotForLogWideRows()
{
    LogCheckWindow window;
    LogCheckResult result;
    result.issues << issue(LogCheckIssue::Severity::Error, 42, QStringLiteral("DL1ABC"), QStringLiteral("432"),
                           QStringLiteral("no_locator"), QStringLiteral("Kein Locator"))
                  << issue(LogCheckIssue::Severity::Hint, -1, QString(), QString(), QStringLiteral("dupes"),
                           QStringLiteral("2 Dupes im Log"));
    result.errors = 1;
    result.hints = 1;
    window.setResult(result, QString());
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* table = window.findChild<QTableWidget*>();
    QVERIFY(table);
    QSignalSpy activated(&window, &LogCheckWindow::qsoActivated);

    // QAbstractItemView only reports a double-click on the index the
    // preceding press landed on, as a real mouse does it.
    const auto doubleClickRow = [&](int row) {
        const QPoint at = table->visualItemRect(table->item(row, 4)).center();
        QTest::mouseClick(table->viewport(), Qt::LeftButton, Qt::NoModifier, at);
        QTest::mouseDClick(table->viewport(), Qt::LeftButton, Qt::NoModifier, at);
    };
    doubleClickRow(0);
    QCOMPARE(activated.count(), 1);
    QCOMPARE(activated.takeFirst().at(0).toInt(), 42);

    doubleClickRow(1);
    QCOMPARE(activated.count(), 0);
}

void TestLogCheckWindow::showingAsksForAFreshCheck()
{
    LogCheckWindow window;
    QSignalSpy refresh(&window, &LogCheckWindow::refreshRequested);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCOMPARE(refresh.count(), 1);
    // The button asks as well.
    auto* button = window.findChild<QPushButton*>();
    QVERIFY(button);
    QTest::mouseClick(button, Qt::LeftButton);
    QCOMPARE(refresh.count(), 2);
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestLogCheckWindow tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_log_check_window.moc"
