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

signals:
    // Another start asked this instance to show itself.
    void activateRequested();

private:
    QString m_dataDir;
    std::unique_ptr<QLockFile> m_lock;
    std::unique_ptr<QLocalServer> m_server;
};

} // namespace Contestprogramm
