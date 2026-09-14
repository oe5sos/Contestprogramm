#pragma once

// =================================================================
// src/core/On4kstClient.h  (Contestprogramm)
// =================================================================
//
// Contestprogramm-original. Architecture ported from
// Longpath/NereusSDR's DxClusterClient.h/.cpp (QTcpSocket +
// exponential-backoff reconnect + telnet-IAC stripping + line-buffered
// reads + signal-based output, public *ForTest() seams for the parser)
// -- see the plan's "ON4KST-Telnet-Client" section for the concrete
// protocol this speaks instead of DxClusterClient's free-text
// "DX de ..." parsing:
//
//   Port 23001, line-based text protocol.
//   Login:      LOGIN|callsign|password|chat_id|client_version|
//   Confirm:    SDONE|chat_id|
//   Failure:    LOGSTAT|code|message|
//   Room:       chat_id 2 = 144/432 MHz (this program's target room);
//               /CHAT value switches rooms after login ("50"/"144"/
//               "GHZ"/"EME"/"HF").
//   Spot:       DL|unix_time|dx_utc|spotter|qrg|dx|info|spotter_locator|dx_locator|
//   Chat:       CH|chat_id|date|callsign|firstname|destination|msg|highlight|
//               (CR|... in the login batch, same fields)
//   Keepalive:  server sends CK|, client replies with a bare "\r\n".
//   Commands:   /AWAY, /BACK, /CQ callsign msg, /SHLOC callsign, /QUIT.
//
// Per the plan's own "Offene Punkte": this is implemented against the
// wtKST protocol documentation quoted in the plan, not verified live --
// that verification is a deliberate separate manual step for the
// operator, later, with real credentials. This class and its tests
// never connect to the real www.on4kst.org:23001.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-09-09 — Created in C++20/Qt6, architecture ported from
//                 Longpath/NereusSDR's DxClusterClient. AI-assisted via
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

class On4kstClient : public QObject {
    Q_OBJECT

public:
    // ON4KST's combined 144/432 MHz room, per the plan's chat-id table
    // (1=50/70 MHz, 2=144/432 MHz, 3=microwave, 4=EME/JT65, 5=low band,
    // 7=50 MHz IARU R2).
    static constexpr int kChatIdVhfUhf = 2;

    explicit On4kstClient(QObject* parent = nullptr);
    ~On4kstClient() override;

    void connectAndLogin(const QString& host, quint16 port,
                          const QString& callsign, const QString& password,
                          int chatId = kChatIdVhfUhf);
    void disconnectFromServer();
    bool isConnected() const { return m_connected; }
    bool isLoggedIn() const { return m_loggedIn; }

    void switchRoom(const QString& value); // "/CHAT <value>"
    void sendChatMessage(const QString& text);
    void sendCqCall(const QString& callsign, const QString& message); // "/CQ callsign message"
    void sendAway();
    void sendBack();

    // Public test seams -- same idea as DxClusterClient's *ForTest()
    // methods: exercise the parser/stripper without a live socket.
    static bool parseDxSpotLineForTest(const QString& line, SpotCandidate& candidateOut);
    static bool parseChatLineForTest(const QString& line, SpotCandidate& candidateOut);
    static void stripTelnetIACForTest(QByteArray& buf) { stripTelnetIACBuffer(buf); }

signals:
    void connected();
    void disconnected();
    void connectionError(const QString& error);
    void loggedIn(int chatId);
    void loginFailed(int code, const QString& message);
    void spotReceived(const SpotCandidate& candidate);
    void chatLineReceived(const SpotCandidate& candidate); // grid may be empty
    void rawLineReceived(const QString& line);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError err);
    void onReconnectTimer();

private:
    void sendRaw(const QString& line); // appends "\r\n"
    void handleLine(const QString& line);
    void stripTelnetIAC();
    static void stripTelnetIACBuffer(QByteArray& buf);

    QTcpSocket* m_socket;
    QByteArray  m_readBuffer;
    QTimer*     m_reconnectTimer;

    QString m_host;
    quint16 m_port{23001};
    QString m_callsign;
    QString m_password;
    int     m_chatId{kChatIdVhfUhf};

    std::atomic<bool> m_connected{false};
    bool    m_loggedIn{false};
    bool    m_intentionalDisconnect{false};
    int     m_reconnectAttempts{0};

    static constexpr int kMaxReconnectDelayMs = 60000;
    static constexpr int kInitialReconnectDelayMs = 5000;
    static constexpr int kConnectTimeoutMs = 10000;
};

} // namespace Contestprogramm
