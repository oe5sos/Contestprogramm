#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;

namespace Contestprogramm {

// Is this machine's clock right? Every QSO time in the log comes from
// it, and the adjudication cross-checks times between logs -- a laptop
// that drifted or was never set at the site quietly spoils every
// entry. The check asks a well-known web server for its `Date` header
// (one-second resolution, more than enough to catch a clock that is
// minutes off) and reports the offset: local minus server, seconds.
// Several hosts are tried in turn so one being down does not count as
// "offline". Result only ever arrives asynchronously (finished()).
class ClockCheck : public QObject {
    Q_OBJECT

public:
    explicit ClockCheck(QObject* parent = nullptr);

    // The hosts asked, in order; replaceable for tests.
    void setUrls(const QStringList& urls);
    // Starts a round; a round already running is left alone.
    void start();
    bool isRunning() const { return m_running; }

    // The last round's outcome.
    bool hasResult() const { return m_checkedAt.isValid(); }
    bool reachable() const { return m_reachable; }
    qint64 offsetSecs() const { return m_offsetSecs; }
    QString source() const { return m_source; }
    QDateTime checkedAtUtc() const { return m_checkedAt; }

    // "Sat, 03 Oct 2026 14:00:00 GMT" -> UTC; invalid when unparseable.
    static QDateTime parseHttpDate(const QString& text);

signals:
    // `reachable` false: no host answered (offline) -- the offset is
    // meaningless then.
    void finished(bool reachable, qint64 offsetSecs, const QString& source);

private:
    void askNext();
    void handleReply(QNetworkReply* reply, const QDateTime& sentAtUtc);

    QNetworkAccessManager* m_network;
    QStringList m_urls;
    int m_next = 0;
    bool m_running = false;
    bool m_reachable = false;
    qint64 m_offsetSecs = 0;
    QString m_source;
    QDateTime m_checkedAt;
};

} // namespace Contestprogramm
