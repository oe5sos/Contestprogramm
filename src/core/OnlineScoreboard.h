#pragma once

#include "data/ContestScoring.h"
#include "data/QsoRecord.h"

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVector>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace Contestprogramm {

// Live score reporting the way N1MM+ and DXLog.net do it: every few
// minutes one HTTP POST with the "Contest Online Score" XML
// (<dynamicresults>...) to a scoreboard server, HTTP Basic auth with
// the scoreboard account. Nothing here affects the contest itself --
// it is the public "where do I stand" board -- so it stays off until
// a URL and account are set (ContestSettings::scoreboardEnabled).
//
// The XML is built by a pure function over the records (testable, and
// the same ContestScore the panel and the EDI file use), the posting
// by a QNetworkAccessManager owned here.
struct ScoreboardConfig {
    bool enabled = false;
    QUrl url;
    QString username;
    QString password;
    int intervalMinutes = 5;
    QString contestName; // the board's own contest id, e.g. "IARU-R1-VHF"
    QString callsign;
    QString grid;
    QString club;
    QString scoring = QStringLiteral("distance_km");
    QStringList bandOrder;
};

class OnlineScoreboard : public QObject {
    Q_OBJECT

public:
    explicit OnlineScoreboard(QObject* parent = nullptr);

    void setConfig(const ScoreboardConfig& config);
    const ScoreboardConfig& config() const { return m_config; }

    // The records the next post reports -- refreshed by the caller
    // whenever the log changes (MainWindow after every logged QSO).
    void setRecords(const QVector<QsoRecord>& records);

    // Posts now, regardless of the timer, when enabled and configured.
    // Returns false (with `errorOut`) when nothing was sent.
    bool postNow(QString* errorOut = nullptr);

    static QByteArray buildXml(const QVector<QsoRecord>& records, const ScoreboardConfig& config, const QDateTime& stampUtc);

signals:
    void posted(const QString& summary);
    void failed(const QString& error);

private:
    void handleReply(QNetworkReply* reply);

    ScoreboardConfig m_config;
    QVector<QsoRecord> m_records;
    QNetworkAccessManager* m_network;
    QTimer* m_timer;
};

} // namespace Contestprogramm
