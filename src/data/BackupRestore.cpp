#include "data/BackupRestore.h"

#include "data/ContestDatabase.h"

#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QUuid>

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
    if (!db.open(backupPath, connection)) {
        return QStringLiteral("nicht lesbar");
    }
    QStringList parts;
    int total = 0;
    for (const QString& contestId : db.contestIdsInLog()) {
        const int count = db.qsoCountForContest(contestId);
        total += count;
        parts << QStringLiteral("%1 QSOs · %2").arg(count).arg(contestId);
    }
    db.close();
    if (parts.isEmpty()) {
        return QStringLiteral("keine QSOs");
    }
    return parts.join(QStringLiteral("; "));
}

} // namespace Contestprogramm
