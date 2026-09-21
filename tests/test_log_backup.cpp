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
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("log.sqlite")), QStringLiteral("backup_prune")));

    const QString backupDir = dir.filePath(QStringLiteral("backups"));
    QVERIFY(QDir().mkpath(backupDir));
    // Older copies than kKeepFiles allows, all dated before anything
    // backupNow() will write today.
    for (int i = 0; i < LogBackup::kKeepFiles + 5; ++i) {
        QFile old(QDir(backupDir).filePath(QStringLiteral("contestprogramm-2020%1-%2.sqlite")
                                                .arg(1 + i / 1440, 4, 10, QLatin1Char('0'))
                                                .arg(i % 1440, 4, 10, QLatin1Char('0'))));
        QVERIFY(old.open(QIODevice::WriteOnly));
        old.close();
    }

    LogBackup backup(db, backupDir);
    QString error;
    QVERIFY2(!backup.backupNow(true, &error).isEmpty(), qPrintable(error));
    const QStringList remaining = QDir(backupDir).entryList({QStringLiteral("contestprogramm-*.sqlite")}, QDir::Files, QDir::Name);
    QCOMPARE(remaining.size(), LogBackup::kKeepFiles);
    // The newest (today's real copy) survived, the oldest dummies went.
    QVERIFY(remaining.last().startsWith(QStringLiteral("contestprogramm-20")));
    QVERIFY(!remaining.contains(QStringLiteral("contestprogramm-20200001-0000.sqlite")));
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
