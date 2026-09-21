#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVector>

class QTimer;

namespace Contestprogramm {

class ContestDatabase;

// Periodic copies of the contest database, the way N1MM+ and DXLog.net
// keep a backup folder beside the log: every few minutes, only when a
// QSO was written since the last copy (ContestDatabase::
// qsoWriteCounter()), one timestamped file in `directory`, the oldest
// pruned once there are more than kKeepFiles. Written with SQLite's
// VACUUM INTO (see ContestDatabase::backupTo()), so a copy is complete
// and consistent even mid-WAL -- the one thing a plain file copy of
// the .sqlite cannot promise during a contest.
//
// A backup is a file the operator can hand back to this program by
// replacing the database with it; there is deliberately no "restore"
// UI -- that is a decision to make calmly after the contest, not a
// button next to the log.
class LogBackup : public QObject {
    Q_OBJECT

public:
    static constexpr int kDefaultIntervalMs = 5 * 60 * 1000;
    static constexpr int kKeepFiles = 300; // 25 h of five-minute copies

    LogBackup(ContestDatabase& database, const QString& directory, QObject* parent = nullptr);

    const QString& directory() const { return m_directory; }

    void start(int intervalMs = kDefaultIntervalMs);

    // Writes a copy now. Without `force`, does nothing (and returns an
    // empty path) when no QSO was written since the last copy. On
    // failure returns an empty path and fills `errorOut`.
    QString backupNow(bool force, QString* errorOut = nullptr);

    // "contestprogramm-20261003-1405.sqlite" for the given UTC time.
    static QString fileNameFor(const QDateTime& utc);

    // The copies in `directory`, newest first -- what Datei > Sicherung
    // wiederherstellen offers. The time is read back from the file name
    // (fileNameFor's own format), the size from the file.
    struct Entry {
        QString path;
        QDateTime utc;
        qint64 bytes = 0;
    };
    static QVector<Entry> listBackups(const QString& directory);

signals:
    void backupWritten(const QString& path);
    void backupFailed(const QString& error);

private:
    void prune();

    ContestDatabase* m_database;
    QString m_directory;
    QTimer* m_timer;
    int m_counterAtLastBackup = -1;
};

} // namespace Contestprogramm
