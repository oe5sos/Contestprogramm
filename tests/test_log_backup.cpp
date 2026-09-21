#include <QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "data/ContestDatabase.h"
#include "data/LogBackup.h"
#include "data/QsoRecord.h"

using namespace Contestprogramm;

namespace {

QsoRecord makeQso(const QString& call)
{
    QsoRecord r;
    r.callsign = call;
    r.band = QStringLiteral("144");
    r.mode = QStringLiteral("SSB");
    r.timestampUtc = QStringLiteral("2026-10-03T14:01:00Z");
    r.contestId = QStringLiteral("IARU_R1_VHF_UHF");
    return r;
}

int qsoCountIn(const QString& path, const QString& connection)
{
    ContestDatabase copy;
    if (!copy.open(path, connection)) {
        return -1;
    }
    return copy.qsoCountForContest(QStringLiteral("IARU_R1_VHF_UHF"));
}

} // namespace

// LogBackup (data/LogBackup.h): a consistent copy only when the log
// changed, distinct file names, and pruning of the oldest copies.
class TestLogBackup : public QObject
{
    Q_OBJECT

private slots:
    void copiesOnlyWhenTheLogChangedAndTheCopyIsComplete();
    void prunesToTheNewestFiles();
    void fileNamesAreTimeOrdered();
    void mirrorsEveryCopyAndSurvivesAMissingMirror();
};

void TestLogBackup::copiesOnlyWhenTheLogChangedAndTheCopyIsComplete()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("log.sqlite")), QStringLiteral("backup_src")));
    QsoRecord a = makeQso(QStringLiteral("DL1ABC"));
    QVERIFY(db.insertQso(a));

    const QString backupDir = dir.filePath(QStringLiteral("backups"));
    LogBackup backup(db, backupDir);
    QSignalSpy written(&backup, &LogBackup::backupWritten);

    QString error;
    const QString first = backup.backupNow(false, &error);
    QVERIFY2(!first.isEmpty(), qPrintable(error));
    QVERIFY(QFileInfo::exists(first));
    QCOMPARE(written.size(), 1);
    // The copy is a real, openable database holding the QSO -- VACUUM
    // INTO, not a raw file copy that could miss the WAL.
    QCOMPARE(qsoCountIn(first, QStringLiteral("backup_copy_1")), 1);

    // Nothing changed: no second file.
    QVERIFY(backup.backupNow(false, &error).isEmpty());
    QVERIFY(error.isEmpty());
    QCOMPARE(written.size(), 1);

    // A new QSO: a new copy, with a name of its own even within the
    // same minute.
    QsoRecord b = makeQso(QStringLiteral("OE3XYZ"));
    QVERIFY(db.insertQso(b));
    const QString second = backup.backupNow(false, &error);
    QVERIFY2(!second.isEmpty(), qPrintable(error));
    QVERIFY(second != first);
    QCOMPARE(qsoCountIn(second, QStringLiteral("backup_copy_2")), 2);

    // Forced: a copy even without a change.
    const QString third = backup.backupNow(true, &error);
    QVERIFY2(!third.isEmpty(), qPrintable(error));
    QVERIFY(third != second);
    QCOMPARE(QDir(backupDir).entryList({QStringLiteral("contestprogramm-*.sqlite")}, QDir::Files).size(), 3);
}

void TestLogBackup::prunesToTheNewestFiles()
{
    // Copies come every minute now: every one of the last two hours
    // stays, older ones thin to the newest per ten minutes, and the
    // folder never holds more than kKeepFiles.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("log.sqlite")), QStringLiteral("backup_prune")));
    const QString backupDir = dir.filePath(QStringLiteral("backups"));
    QVERIFY(QDir().mkpath(backupDir));
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const auto touch = [&](const QDateTime& utc) {
        QFile file(QDir(backupDir).filePath(LogBackup::fileNameFor(utc)));
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        file.close();
        return true;
    };
    // 100 copies by the minute inside the last two hours, 300 older
    // ones by the minute (three to eight hours ago).
    for (int m = 1; m <= 100; ++m) {
        QVERIFY(touch(now.addSecs(-60 * m)));
    }
    for (int m = 0; m < 300; ++m) {
        QVERIFY(touch(now.addSecs(-3 * 3600 - 60 * m)));
    }

    LogBackup backup(db, backupDir);
    QString error;
    QVERIFY2(!backup.backupNow(true, &error).isEmpty(), qPrintable(error));
    const QVector<LogBackup::Entry> remaining = LogBackup::listBackups(backupDir);
    // Today's real copy + the 100 recent + one per ten minutes of the
    // 300 older (30, give or take the bucket edges).
    QVERIFY2(remaining.size() >= 128 && remaining.size() <= 133, qPrintable(QString::number(remaining.size())));
    int recent = 0;
    for (const LogBackup::Entry& entry : remaining) {
        if (entry.utc.secsTo(now) <= 2 * 3600) {
            ++recent;
        }
    }
    QCOMPARE(recent, 101);

    // The cap: 450 copies ten minutes apart (75 hours) all survive the
    // thinning but not the cap -- the oldest go.
    QTemporaryDir dir2;
    const QString capDir = dir2.filePath(QStringLiteral("backups"));
    QVERIFY(QDir().mkpath(capDir));
    for (int i = 0; i < 450; ++i) {
        QFile file(QDir(capDir).filePath(LogBackup::fileNameFor(now.addSecs(-600 * i))));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
    }
    LogBackup::pruneDirectory(capDir);
    const QVector<LogBackup::Entry> capped = LogBackup::listBackups(capDir);
    QCOMPARE(capped.size(), LogBackup::kKeepFiles);
    QCOMPARE(capped.first().utc.toString(QStringLiteral("yyyyMMdd-HHmm")), now.toString(QStringLiteral("yyyyMMdd-HHmm")));
    QVERIFY(capped.last().utc.secsTo(now) < 450 * 600);
}

void TestLogBackup::mirrorsEveryCopyAndSurvivesAMissingMirror()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("log.sqlite")), QStringLiteral("backup_mirror")));
    QsoRecord qso1 = makeQso(QStringLiteral("DL1ABC"));
    QVERIFY(db.insertQso(qso1));

    LogBackup backup(db, dir.filePath(QStringLiteral("backups")));
    const QString stick = dir.filePath(QStringLiteral("stick/Contestprogramm"));
    backup.setMirrorDirectory(stick);
    QSignalSpy failed(&backup, &LogBackup::mirrorFailed);
    QString error;
    const QString path = backup.backupNow(true, &error);
    QVERIFY2(!path.isEmpty(), qPrintable(error));
    // The same file, same name, on the "stick" (its folder created).
    const QString mirrored = QDir(stick).filePath(QFileInfo(path).fileName());
    QVERIFY(QFileInfo::exists(mirrored));
    QCOMPARE(QFileInfo(mirrored).size(), QFileInfo(path).size());
    QCOMPARE(failed.count(), 0);

    // The stick gone (a file where the folder should be): the backup
    // beside the database is still written, the mirror reports.
    QVERIFY(QDir(dir.path()).rename(QStringLiteral("stick"), QStringLiteral("stick-gone")));
    QFile blocker(dir.filePath(QStringLiteral("stick")));
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    blocker.close();
    QsoRecord qso2 = makeQso(QStringLiteral("DL2ABC"));
    QVERIFY(db.insertQso(qso2));
    const QString second = backup.backupNow(true, &error);
    QVERIFY2(!second.isEmpty(), qPrintable(error));
    QCOMPARE(failed.count(), 1);
    QVERIFY(failed.first().at(0).toString().contains(QStringLiteral("nicht erreichbar")));

    // No mirror: nothing else happens.
    backup.setMirrorDirectory(QString());
    QsoRecord qso3 = makeQso(QStringLiteral("DL3ABC"));
    QVERIFY(db.insertQso(qso3));
    QVERIFY(!backup.backupNow(true, &error).isEmpty());
    QCOMPARE(failed.count(), 1);
}

void TestLogBackup::fileNamesAreTimeOrdered()
{
    const QDateTime earlier(QDate(2026, 10, 3), QTime(14, 5), QTimeZone::utc());
    const QDateTime later(QDate(2026, 10, 3), QTime(14, 10), QTimeZone::utc());
    QCOMPARE(LogBackup::fileNameFor(earlier), QStringLiteral("contestprogramm-20261003-1405.sqlite"));
    QVERIFY(LogBackup::fileNameFor(earlier) < LogBackup::fileNameFor(later));
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    TestLogBackup tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_log_backup.moc"
