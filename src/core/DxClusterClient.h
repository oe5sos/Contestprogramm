#pragma once

// =================================================================
// src/core/DxClusterClient.h  (Contestprogramm)
// =================================================================
//
// Contestprogramm-original. Ported nearly verbatim from
// Longpath/NereusSDR's src/core/DxClusterClient.h/.cpp -- per the
// plan's own module table, this one (unlike On4kstClient, which needed
// a whole new ON4KST login state machine) carries over "fast
// unverändert": QTcpSocket, exponential-backoff reconnect (5s->60s),
// telnet IAC stripping, line-buffered reads, a login-prompt detector
// that replies with a bare callsign, and regex-based spot-line parsing
// (classic "DX de <spotter>: <freq> <call> <comment> <time>Z" plus the
// DXSpider fallback format).
//
// Divergences from the NereusSDR original, deliberate:
//   - Produces the same SpotCandidate value type On4kstClient does
//     (source="cluster"), not a separate DxSpot struct, so cluster
//     spots feed the same ChatFeedModel/GeoFilter/DupeChecker pipeline
//     ON4KST spots already do -- see the plan's "zweite, eigenständige
//     Spot-Quelle... praktisch 1:1 portierbar" note.
//   - No per-connection log file. NereusSDR's own AppConfigLocation
//     logging is an artifact of that program's spot-hub infrastructure
//     (SpotHubDialog), not part of the protocol architecture being
//     carried over here.
//   - No RBN source-label promotion (spotterCall ending "-#" / starting
//     "RBN-"). NereusSDR reuses one DxClusterClient instance for both a
//     real cluster and a separate RBN skimmer connection;
//     Contestprogramm has only one cluster connection in this pass, so
//     every spot's source is simply "cluster".
//   - connectToCluster()/disconnectFromCluster() instead of NereusSDR's
//     connectToCluster()/disconnect() -- the latter would shadow/read
//     confusingly next to QObject::disconnect(), and this matches
//     On4kstClient's own connectAndLogin()/disconnectFromServer()
//     naming.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-09-09 — Created in C++20/Qt6, ported nearly verbatim from
//                 Longpath/NereusSDR's DxClusterClient (itself GPL-3.0-
//                 or-later, ported from AetherSDR -- see NOTICE.md for
//                 the full attribution chain). AI-assisted via
//                 Anthropic Claude Code, operator Ralph Martin Fischer.
// =================================================================

#include "core/SpotCandidate.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QTcpSocket>
#include <QTimer>

#include <atomic>

namespace Contestprogramm {

class DxClusterClient : public QObject {
    Q_OBJECT

public:
    explicit DxClusterClient(QObject* parent = nullptr);
    ~DxClusterClient() override;

    void connectToCluster(const QString& host, quint16 port, const QString& callsign);
    void disconnectFromCluster();
    bool isConnected() const { return m_connected; }

    // Free-text command to the cluster (e.g. a band/filter command);
    // out of scope for this pass beyond exposing the raw send path --
    // see On4kstClient::sendRaw for the equivalent on the other client.
    void sendCommand(const QString& cmd);

    QString host() const { return m_host; }
    quint16 port() const { return m_port; }

    // Public test seams -- same idea as On4kstClient's *ForTest()
    // methods: exercise the parser/prompt-detector/stripper without a
    // live socket or a MockDxClusterServer.
    static bool parseDxSpotLineForTest(const QString& line, SpotCandidate& candidateOut);
    static bool isLoginPromptForTest(const QString& line) { return isLoginPrompt(line); }
    static void stripTelnetIACForTest(QByteArray& buf) { stripTelnetIACBuffer(buf); }

signals:
    void connected();
    void disconnected();
    void connectionError(const QString& error);
    void spotReceived(const SpotCandidate& candidate);
    void rawLineReceived(const QString& line);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError err);
    void onReconnectTimer();
    // Arms the exponential-backoff reconnect (once; never after a
    // deliberate disconnect) -- shared by a lost connection and a
    // connect attempt that never got through.
    void scheduleReconnect();

private:
    static bool parseDxSpotLine(const QString& line, SpotCandidate& candidateOut);
    static bool isLoginPrompt(const QString& line);
    void handleLine(const QString& line);
    void stripTelnetIAC();
    static void stripTelnetIACBuffer(QByteArray& buf);

    QTcpSocket* m_socket;
    QByteArray  m_readBuffer;
    QTimer*     m_reconnectTimer;
    QTimer*     m_connectTimer; // one connect attempt's deadline; restarted per attempt, never stale

    QString m_host;
    quint16 m_port{7300}; // a common DXSpider default; operator-set via ContestSettings::clusterPort in practice
    QString m_callsign;
    std::atomic<bool> m_connected{false};
    bool    m_loggedIn{false};
    bool    m_intentionalDisconnect{false};
    int     m_reconnectAttempts{0};

    static constexpr int kMaxReconnectDelayMs = 60000;
    static constexpr int kInitialReconnectDelayMs = 5000;
    static constexpr int kConnectTimeoutMs = 10000;
};

} // namespace Contestprogramm
