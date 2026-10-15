#include "data/BackupRestore.h"

#include "data/ContestDatabase.h"

#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTemporaryDir>
#include <QUuid>

#include <memory>

namespace Contestprogramm {

bool restoreDatabaseFile(const QString& backupPath, const QString& livePath, QString* errorOut)
{
    const auto fail = [errorOut](const QString& why) {
        if (errorOut) {
            *errorOut = why;
        }
        return false;
    };
    if (!QFileInfo::exists(backupPath)) {
        return fail(QStringLiteral("Sicherung nicht gefunden: %1").arg(backupPath));
    }
    // SQLite's companions of the live file would otherwise replay old
    // pages into the restored copy.
    for (const QString& companion : {livePath, livePath + QStringLiteral("-wal"), livePath + QStringLiteral("-shm"),
                                     livePath + QStringLiteral("-journal")}) {
        if (QFileInfo::exists(companion) && !QFile::remove(companion)) {
            return fail(QStringLiteral("%1 konnte nicht ersetzt werden").arg(companion));
        }
    }
    if (!QFile::copy(backupPath, livePath)) {
        return fail(QStringLiteral("Kopieren nach %1 fehlgeschlagen").arg(livePath));
    }
    return true;
}

QString summarizeBackup(const QString& backupPath)
{
    ContestDatabase db;
    const QString connection = QStringLiteral("backup-summary-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    // Read-only, so merely LOOKING at a backup in the restore dialog
    // cannot rewrite it (ensureSchema()'s pragmas and migrations would,
    // and would leave a -wal/-shm pair beside it) -- see
    // ContestDatabase::openReadOnly and tests/test_backup_restore.cpp::
    // summaryLeavesTheBackupFileExactlyAsItWas.
    //
    // Fallback: a copy whose own file SQLite refuses to open read-only
    // (an older one still carrying a -wal that would need replaying) is
    // read from a throwaway duplicate instead. Still never the original.
    std::unique_ptr<QTemporaryDir> scratch;
    if (!db.openReadOnly(backupPath, connection)) {
        if (!QFileInfo::exists(backupPath)) {
            return QStringLiteral("nicht lesbar");
        }
        scratch = std::make_unique<QTemporaryDir>();
        if (!scratch->isValid()) {
            return QStringLiteral("nicht lesbar");
        }
        const QString duplicate = scratch->filePath(QStringLiteral("sicherung.sqlite"));
        if (!QFile::copy(backupPath, duplicate) || !db.open(duplicate, connection)) {
            return QStringLiteral("nicht lesbar");
        }
    }
    QStringList parts;
    for (const QString& contestId : db.contestIdsInLog()) {
        const int count = db.qsoCountForContest(contestId);
        parts << QStringLiteral("%1 QSOs · %2").arg(count).arg(contestId);
    }
    db.close();
    if (parts.isEmpty()) {
        return QStringLiteral("keine QSOs");
    }
    return parts.join(QStringLiteral("; "));
}

} // namespace Contestprogramm
