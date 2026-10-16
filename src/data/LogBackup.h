#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVector>

class QTimer;

namespace Contestprogramm {

class ContestDatabase;

// Periodic copies of the contest database, the way N1MM+ and DXLog.net
// keep a backup folder beside the log: every minute (operator,
// 2026-09-21: "speichern sollte jede minute automatisch gehen"), only
// when a QSO was written since the last copy (ContestDatabase::
// qsoWriteCounter()), one timestamped file in `directory`. Thinned
// with age (see pruneDirectory()): every copy of the last two hours
// stays, older ones one per ten minutes, and never more than
// kKeepFiles. Written with SQLite's
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
    static constexpr int kDefaultIntervalMs = 60 * 1000;
    static constexpr int kKeepFiles = 400; // two hours by the minute plus ~40 h by ten minutes
    static constexpr int kKeepEveryCopySecs = 2 * 3600;
    static constexpr int kThinnedBucketSecs = 10 * 60;

    LogBackup(ContestDatabase& database, const QString& directory, QObject* parent = nullptr);

    const QString& directory() const { return m_directory; }

    // A second place every copy also goes -- a USB stick or a cloud
    // folder, so the log survives the laptop. Empty = none. Failures
    // there never fail the backup itself; they come as mirrorFailed().
    void setMirrorDirectory(const QString& directory);
    const QString& mirrorDirectory() const { return m_mirrorDirectory; }

    // Richtet den zweiten Sicherungsordner ein und legt dort eine
    // Markierungsdatei ab. Vom Menüpunkt "Zweiter Sicherungsordner…"
    // aufzurufen, einmal, wenn der Operator den Ordner wählt.
    //
    // Wozu: Wird ein USB-Stick abgezogen, bleibt sein Einhängepunkt
    // unter macOS als gewöhnlicher, leerer Ordner auf der INTERNEN
    // Platte zurück. mkpath() gelingt dort, QFile::copy() auch -- und
    // das Programm schreibt die "Sicherung auf dem Stick" fortan
    // stillschweigend neben das Log, auf dieselbe Platte, deren
    // Ausfall der zweite Ordner ja gerade abfangen soll (nachgemessen
    // 2026-10-16: die Kopie landete auf /System/Volumes/Data).
    //
    // Die Markierung liegt auf dem Datenträger selbst. Ist der Stick
    // weg, fehlt sie, und mirror() sagt es, statt ins Leere zu
    // schreiben. Ein Cloud-Ordner auf derselben Platte ist davon nicht
    // betroffen -- er behält seine Markierung und ist weiter gültig,
    // denn "dieselbe Platte" ist hier nicht das Kriterium, sondern
    // "derselbe Ort wie beim Einrichten".
    bool prepareMirrorDirectory(const QString& directory, QString* errorOut = nullptr);

    // Der Name der Markierungsdatei -- öffentlich für die Prüfstände.
    static QString mirrorMarkerName();

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
    // Thins `directory` with age and caps it at kKeepFiles -- the rule
    // both folders (and the tests) share.
    static void pruneDirectory(const QString& directory);

signals:
    void backupWritten(const QString& path);
    void backupFailed(const QString& error);
    void mirrorFailed(const QString& error);

private:
    void prune();
    void mirror(const QString& path);

    ContestDatabase* m_database;
    QString m_directory;
    QString m_mirrorDirectory;
    QTimer* m_timer;
    int m_counterAtLastBackup = -1;
};

} // namespace Contestprogramm
