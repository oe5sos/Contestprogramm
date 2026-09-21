#include <QtTest>

#include <QApplication>
#include <QDir>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QFile>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/BackupRestore.h"
#include "data/ContestDatabase.h"
#include "data/LogBackup.h"
#include "data/QsoRecord.h"
#include "ui/BackupRestoreDialog.h"
#include "ui/MainWindow.h"

#include <memory>

using namespace Contestprogramm;

namespace {

QsoRecord qso(const QString& call, const QString& contestId)
{
    QsoRecord r;
    r.callsign = call;
    r.band = QStringLiteral("144");
    r.mode = QStringLiteral("SSB");
    r.timestampUtc = QStringLiteral("2026-10-03T14:01:00Z");
    r.gridSquare = QStringLiteral("JN58SD");
    r.contestId = contestId;
    return r;
}

} // namespace

// data/BackupRestore.h + ui/BackupRestoreDialog.h: listing the copies,
// what one holds, and putting one back in place of the live file.
class TestBackupRestore : public QObject
{
    Q_OBJECT

private slots:
    void listsBackupsNewestFirstWithTheirTime();
    void summaryReadsTheCopyWithoutTouchingTheLiveConnection();
    void restoreReplacesTheLiveFileAndItsCompanions();
    void dialogShowsTheListAndTheSelectionsSummary();
    void mainWindowRestoreBacksUpClosesReplacesAndAsksForARestart();
};

void TestBackupRestore::listsBackupsNewestFirstWithTheirTime()
{
    QTemporaryDir dir;
    const QString older = QDir(dir.path()).filePath(LogBackup::fileNameFor(QDateTime(QDate(2026, 10, 3), QTime(14, 5), QTimeZone::UTC)));
    const QString newer = QDir(dir.path()).filePath(LogBackup::fileNameFor(QDateTime(QDate(2026, 10, 3), QTime(14, 10), QTimeZone::UTC)));
    for (const QString& path : {older, newer}) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("x");
    }
    QFile other(QDir(dir.path()).filePath(QStringLiteral("notes.txt")));
    QVERIFY(other.open(QIODevice::WriteOnly));
    const QVector<LogBackup::Entry> entries = LogBackup::listBackups(dir.path());
    QCOMPARE(entries.size(), 2);
    QCOMPARE(entries.at(0).path, newer);
    QCOMPARE(entries.at(0).utc, QDateTime(QDate(2026, 10, 3), QTime(14, 10), QTimeZone::UTC));
    QCOMPARE(entries.at(1).path, older);
    QCOMPARE(entries.at(1).bytes, qint64(1));
    QVERIFY(LogBackup::listBackups(dir.path() + QStringLiteral("/missing")).isEmpty());
}

void TestBackupRestore::summaryReadsTheCopyWithoutTouchingTheLiveConnection()
{
    QTemporaryDir dir;
    ContestDatabase live;
    QVERIFY(live.open(dir.filePath(QStringLiteral("live.sqlite")), QStringLiteral("restore_live")));
    QsoRecord a = qso(QStringLiteral("DL1ABC"), QStringLiteral("IARU_R1_UHF"));
    QsoRecord b = qso(QStringLiteral("DL2ABC"), QStringLiteral("IARU_R1_UHF"));
    QsoRecord c = qso(QStringLiteral("OE3XYZ"), QStringLiteral("OE_VHF_UHF"));
    QVERIFY(live.insertQso(a));
    QVERIFY(live.insertQso(b));
    QVERIFY(live.insertQso(c));
    const QString copy = dir.filePath(QStringLiteral("copy.sqlite"));
    QVERIFY(live.backupTo(copy));
    const QString summary = summarizeBackup(copy);
    QVERIFY2(summary.contains(QStringLiteral("2 QSOs · IARU_R1_UHF")), qPrintable(summary));
    QVERIFY2(summary.contains(QStringLiteral("1 QSOs · OE_VHF_UHF")), qPrintable(summary));
    QVERIFY(live.isOpen()); // untouched
    QCOMPARE(live.filePath(), dir.filePath(QStringLiteral("live.sqlite")));
    QCOMPARE(summarizeBackup(dir.filePath(QStringLiteral("nothing.sqlite"))), QStringLiteral("keine QSOs"));
}

void TestBackupRestore::restoreReplacesTheLiveFileAndItsCompanions()
{
    QTemporaryDir dir;
    const QString livePath = dir.filePath(QStringLiteral("live.sqlite"));
    QString copy;
    {
        ContestDatabase live;
        QVERIFY(live.open(livePath, QStringLiteral("restore_live2")));
        QsoRecord a = qso(QStringLiteral("DL1ABC"), QStringLiteral("IARU_R1_UHF"));
        QVERIFY(live.insertQso(a));
        copy = dir.filePath(QStringLiteral("backup.sqlite"));
        QVERIFY(live.backupTo(copy));
        QsoRecord b = qso(QStringLiteral("DL2ABC"), QStringLiteral("IARU_R1_UHF"));
        QVERIFY(live.insertQso(b)); // after the backup: must be gone after the restore
        live.close();
    }
    // Stray companions from a previous session must not survive.
    QFile wal(livePath + QStringLiteral("-wal"));
    QVERIFY(wal.open(QIODevice::WriteOnly));
    wal.write("junk");
    wal.close();
    QString error;
    QVERIFY2(restoreDatabaseFile(copy, livePath, &error), qPrintable(error));
    QVERIFY(!QFile::exists(livePath + QStringLiteral("-wal")));
    ContestDatabase reopened;
    QVERIFY(reopened.open(livePath, QStringLiteral("restore_live3")));
    QCOMPARE(reopened.qsoCountForContest(QStringLiteral("IARU_R1_UHF")), 1);
    reopened.close();
    QVERIFY(!restoreDatabaseFile(dir.filePath(QStringLiteral("missing.sqlite")), livePath, &error));
    QVERIFY(error.contains(QStringLiteral("nicht gefunden")));
}

void TestBackupRestore::dialogShowsTheListAndTheSelectionsSummary()
{
    QTemporaryDir dir;
    ContestDatabase live;
    QVERIFY(live.open(dir.filePath(QStringLiteral("live.sqlite")), QStringLiteral("restore_live4")));
    QsoRecord a = qso(QStringLiteral("DL1ABC"), QStringLiteral("IARU_R1_UHF"));
    QVERIFY(live.insertQso(a));
    const QString backupsDir = dir.filePath(QStringLiteral("backups"));
    QVERIFY(QDir().mkpath(backupsDir));
    const QString first = QDir(backupsDir).filePath(LogBackup::fileNameFor(QDateTime(QDate(2026, 10, 3), QTime(14, 5), QTimeZone::UTC)));
    QVERIFY(live.backupTo(first));
    live.close();

    BackupRestoreDialog dialog(LogBackup::listBackups(backupsDir));
    QCOMPARE(dialog.rowCount(), 1);
    QVERIFY(dialog.selectedPath().isEmpty());
    dialog.selectRow(0);
    QCOMPARE(dialog.selectedPath(), first);
    QVERIFY2(dialog.detailText().contains(QStringLiteral("1 QSOs · IARU_R1_UHF")), qPrintable(dialog.detailText()));

    BackupRestoreDialog empty({});
    QCOMPARE(empty.rowCount(), 0);
    QVERIFY(empty.detailText().contains(QStringLiteral("keine Sicherung")));
}

void TestBackupRestore::mainWindowRestoreBacksUpClosesReplacesAndAsksForARestart()
{
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir dir;
    auto controller = std::make_unique<AppController>();
    QVERIFY(controller->openDatabase(dir.filePath(QStringLiteral("contestprogramm.sqlite"))));
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.activeContestId = QStringLiteral("IARU_R1_UHF");
    controller->setSettings(settings);
    QsoRecord a = qso(QStringLiteral("DL1ABC"), QStringLiteral("IARU_R1_UHF"));
    QVERIFY(controller->database().insertQso(a));
    QVERIFY(controller->logBackup());
    QString error;
    const QString backupPath = controller->logBackup()->backupNow(true, &error);
    QVERIFY2(!backupPath.isEmpty(), qPrintable(error));
    QsoRecord b = qso(QStringLiteral("DL2ABC"), QStringLiteral("IARU_R1_UHF"));
    QVERIFY(controller->database().insertQso(b)); // logged after the backup
    const int backupsBefore = LogBackup::listBackups(controller->logBackup()->directory()).size();

    MainWindow window(*controller);
    QSignalSpy restart(&window, &MainWindow::restartRequested);
    QVERIFY2(window.performRestore(backupPath, &error), qPrintable(error));
    QCOMPARE(restart.count(), 1);
    QVERIFY(!controller->database().isOpen());
    // The state before the restore is a backup of its own now.
    QVERIFY(LogBackup::listBackups(controller->logBackup()->directory()).size() >= backupsBefore);
    // The live file holds the backup's one QSO again.
    ContestDatabase reopened;
    QVERIFY(reopened.open(dir.filePath(QStringLiteral("contestprogramm.sqlite")), QStringLiteral("restore_check")));
    QCOMPARE(reopened.qsoCountForContest(QStringLiteral("IARU_R1_UHF")), 1);
    reopened.close();
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestBackupRestore tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_backup_restore.moc"
