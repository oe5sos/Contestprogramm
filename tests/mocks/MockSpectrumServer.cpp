#include "MockSpectrumServer.h"

#include <QHostAddress>
#include <QWebSocket>
#include <QWebSocketServer>

namespace Contestprogramm {

namespace {

constexpr int kKopfBytes = 64;

void schreibeUInt32LE(QByteArray& rahmen, int versatz, quint32 wert)
{
    auto* p = reinterpret_cast<quint8*>(rahmen.data()) + versatz;
    p[0] = quint8(wert & 0xFF);
    p[1] = quint8((wert >> 8) & 0xFF);
    p[2] = quint8((wert >> 16) & 0xFF);
    p[3] = quint8((wert >> 24) & 0xFF);
}

// Derselbe Rahmen, den Longpaths buildSpectrumPayload baut: 64 Byte
// Kopf, danach ein Byte je Punkt (dBm + 200).
QByteArray baueRahmen(int punkteImKopf, int punkteWirklich, quint32 stromTyp,
                      quint32 abtastTyp, quint8 fuellwert)
{
    QByteArray rahmen(kKopfBytes + punkteWirklich, char(0));
    schreibeUInt32LE(rahmen, 4, 10);            // Bildrate
    schreibeUInt32LE(rahmen, 8, abtastTyp);
    schreibeUInt32LE(rahmen, 20, quint32(punkteImKopf));
    schreibeUInt32LE(rahmen, 24, stromTyp);
    for (int i = 0; i < punkteWirklich; ++i) {
        rahmen[kKopfBytes + i] = char(fuellwert);
    }
    return rahmen;
}

} // namespace

MockSpectrumServer::MockSpectrumServer(QObject* parent)
    : QObject(parent)
    , m_server(new QWebSocketServer(QStringLiteral("MockSpectrum"),
                                     QWebSocketServer::NonSecureMode, this))
{
    connect(m_server, &QWebSocketServer::newConnection,
            this, &MockSpectrumServer::aufNeuerClient);
}

MockSpectrumServer::~MockSpectrumServer() = default;

bool MockSpectrumServer::startListening()
{
    // Nur die Rückschleife, nur ein flüchtiger Port. Ein Prüflauf
    // greift nicht ins Netz.
    return m_server->listen(QHostAddress::LocalHost, 0);
}

quint16 MockSpectrumServer::port() const
{
    return m_server->serverPort();
}

void MockSpectrumServer::aufNeuerClient()
{
    QWebSocket* neu = m_server->nextPendingConnection();
    if (neu == nullptr) {
        return;
    }
    if (m_client != nullptr) {
        neu->close();
        neu->deleteLater();
        return;
    }
    m_client = neu;
    connect(m_client, &QWebSocket::textMessageReceived, this,
            [this](const QString& zeile) { m_befehle << zeile; });
    connect(m_client, &QWebSocket::disconnected, this, [this]() {
        if (m_client != nullptr) {
            m_client->deleteLater();
            m_client = nullptr;
        }
    });
}

void MockSpectrumServer::sendeSpektrum(int punkte, quint8 fuellwert)
{
    if (m_client == nullptr) { return; }
    m_client->sendBinaryMessage(baueRahmen(punkte, punkte, 100, 100, fuellwert));
    m_client->flush();
}

void MockSpectrumServer::sendeVerdorbenenRahmen(int punkteImKopf, int punkteWirklich)
{
    if (m_client == nullptr) { return; }
    m_client->sendBinaryMessage(baueRahmen(punkteImKopf, punkteWirklich, 100, 100, 150));
    m_client->flush();
}

void MockSpectrumServer::sendeFremdenStrom()
{
    if (m_client == nullptr) { return; }
    // Stromtyp 0 ist TCIs eigener IQ-Strom -- dieselbe Leitung, nicht
    // unser Rahmen.
    m_client->sendBinaryMessage(baueRahmen(256, 256, 0, 0, 150));
    m_client->flush();
}

void MockSpectrumServer::trenne()
{
    if (m_client != nullptr) {
        m_client->close();
    }
}

} // namespace Contestprogramm
