#pragma once

#include <QLocalServer>
#include <QLockFile>
#include <QObject>
#include <QString>

#include <memory>

namespace Contestprogramm {

// One running program per data directory. A second start does not open
// a second window on the same database (2026-09-20: four instances at
// once, all writing the one log and all logged in at ON4KST) -- it
// tells the running one to come to the front and quits.
//
// Two parts, both keyed to the data directory: a QLockFile beside the
// database (stale locks from a crashed process are reclaimed by
// QLockFile itself) and a QLocalServer the second start connects to
// for the "raise yourself" message. If the lock is held but nobody
// answers on the socket, the second start proceeds -- a stuck lock must
// never keep the operator out of the log during a contest.
class SingleInstanceGuard : public QObject {
    Q_OBJECT

public:
    explicit SingleInstanceGuard(const QString& dataDir, QObject* parent = nullptr);
    ~SingleInstanceGuard() override;

    // True when this process holds the instance; false when another
    // instance is running and has been asked to come to the front.
    bool tryAcquire();
    // Gives the instance up early (before a planned restart), so the
    // next start is not treated as a second instance.
    void release();

    static QString serverNameFor(const QString& dataDir);

    // The build this process runs: its own executable's modification
    // time, taken at startup (a later rebuild replaces the file on disk,
    // this stays the old value). Sent along with the hand-over so the
    // running instance can tell a mere second start from "the operator
    // rebuilt and started, expecting the new program". Tests set it.
    void setBuildStamp(const QString& stamp);
    QString buildStamp() const { return m_buildStamp; }

    // The program this process was started as (its absolute executable
    // path), sent along with the build stamp so a restart runs the
    // program the operator started, not the running one's. Tests set it.
    void setProgramPath(const QString& path);
    QString programPath() const { return m_programPath; }

signals:
    // Another start asked this instance to show itself.
    void activateRequested();
    // A second start handed over to this instance, but from a DIFFERENT
    // build than this one (see setBuildStamp()) -- operator, 2026-09-21,
    // three times in a row: built, started, and looked at the old
    // program, because the start had only raised the running window.
    // main.cpp restarts this instance on it, the same way a restored
    // backup restarts.
    //
    // `program` is the executable that second start ran: the new build
    // may lie beside the running one (2026-09-28, Contestprogramm-neu.app
    // next to Contestprogramm.app), where restarting the running one's
    // own path brought the old program back. Empty when the start was
    // an older build that did not send it, or sent nothing usable --
    // main.cpp then restarts its own path, right for a rebuild in place.
    void newerBuildStarted(const QString& program);

private:
    QString m_dataDir;
    QString m_buildStamp;
    QString m_programPath;
    std::unique_ptr<QLockFile> m_lock;
    std::unique_ptr<QLocalServer> m_server;
};

} // namespace Contestprogramm
