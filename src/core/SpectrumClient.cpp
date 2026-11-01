#include "core/SpectrumClient.h"

#include <QTimer>
#include <QUrl>
#include <QWebSocket>

#include <algorithm>
#include <iterator>

namespace Contestprogramm {

namespace {

// Die Wartestufen des Wiederanlaufs. Dieselbe Treppe, die Longpath für
// seine Geräteverbindungen benutzt. Oben bleibt sie stehen: ein Logbuch
// soll nicht schneller klopfen, nur weil länger niemand aufmacht.
constexpr int kStufenMs[] = {1000, 2000, 5000, 10000, 30000, 60000};
constexpr int kStufenAnzahl = int(std::size(kStufenMs));

quint32 leseUInt32LE(const QByteArray& b, int versatz)
{
    // Aufrufer prüft die Länge. Hier wird nur gelesen.
    const auto* p = reinterpret_cast<const quint8*>(b.constData()) + versatz;
    return quint32(p[0]) | (quint32(p[1]) << 8) | (quint32(p[2]) << 16)
           | (quint32(p[3]) << 24);
}

} // namespace

SpectrumClient::SpectrumClient(QObject* parent)
    : QObject(parent)
    , m_socket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
    , m_anlaufTakt(new QTimer(this))
{
    m_anlaufTakt->setSingleShot(true);
    connect(m_anlaufTakt, &QTimer::timeout, this, &SpectrumClient::wiederAnlauf);

    connect(m_socket, &QWebSocket::connected, this, &SpectrumClient::aufVerbunden);
    connect(m_socket, &QWebSocket::disconnected, this, &SpectrumClient::aufGetrennt);
    connect(m_socket, &QWebSocket::binaryMessageReceived,
            this, &SpectrumClient::aufBinaerNachricht);

    // Ein Fehler ist hier kein Ereignis, das jemanden interessiert: der
    // Wiederanlauf hängt an `disconnected`, und QWebSocket schickt das
    // auch nach einem gescheiterten Verbindungsversuch. Ohne diese
    // Verbindung würde Qt die Fehlermeldung auf die Konsole schreiben.
    connect(m_socket, &QWebSocket::errorOccurred, this, [](QAbstractSocket::SocketError) {});
}

SpectrumClient::~SpectrumClient() = default;

void SpectrumClient::setEndpunkt(const QString& host, quint16 port)
{
    if (host == m_host && port == m_port) { return; }
    m_host = host.trimmed().isEmpty() ? QStringLiteral("127.0.0.1") : host.trimmed();
    m_port = port == 0 ? quint16(50001) : port;
    if (m_gewollt) {
        // Neue Adresse: alte Verbindung fallen lassen, Treppe zurück auf
        // die erste Stufe.
        m_stufe = 0;
        m_socket->abort();
        planeWiederAnlauf();
    }
}

void SpectrumClient::setGeometrie(int punkte, int bilderJeSekunde)
{
    const int p = std::clamp(punkte, kMinPunkte, kMaxPunkte);
    const int f = std::clamp(bilderJeSekunde, 1, 30);
    if (p == m_punkte && f == m_fps) { return; }
    m_punkte = p;
    m_fps = f;
    // Longpath nimmt den jüngsten spectrum_start; ein zweiter ersetzt den
    // ersten, es braucht kein spectrum_stop dazwischen.
    if (istVerbunden()) { sendeStart(); }
}

void SpectrumClient::starten()
{
    if (m_gewollt) { return; }
    m_gewollt = true;
    m_stufe = 0;
    wiederAnlauf();
}

void SpectrumClient::beenden()
{
    if (!m_gewollt) { return; }
    m_gewollt = false;
    m_anlaufTakt->stop();
    if (istVerbunden()) {
        // Erst absagen, dann auflegen -- und zwar sauber.
        //
        // Gefunden von tests/test_wasserfall_leitung.cpp am 2026-11-01:
        // hier stand sendTextMessage() und direkt danach abort().
        // abort() verwirft, was noch im Ausgangspuffer liegt, also kam
        // die Absage nie an: Longpath hätte weiter Bilder in eine
        // geschlossene Leitung gerechnet, bis es selbst merkt, dass
        // niemand mehr da ist. flush() schiebt sie hinaus, close()
        // legt dann geordnet auf.
        m_socket->sendTextMessage(QStringLiteral("spectrum_stop:0;"));
        m_socket->flush();
        m_socket->close();
    } else {
        // Noch nicht verbunden: ein laufender Verbindungsversuch wird
        // abgebrochen, da gibt es nichts abzusagen.
        m_socket->abort();
    }
}

bool SpectrumClient::istVerbunden() const
{
    return m_socket != nullptr && m_socket->state() == QAbstractSocket::ConnectedState;
}

void SpectrumClient::sendeStart()
{
    // Nur Empfänger 0 -- siehe Klassenkommentar. Das Semikolon ist TCIs
    // Befehlsabschluss.
    m_socket->sendTextMessage(QStringLiteral("spectrum_start:0,%1,%2;")
                                  .arg(m_punkte).arg(m_fps));
}

void SpectrumClient::planeWiederAnlauf()
{
    if (!m_gewollt) { return; }
    const int ms = kStufenMs[std::min(m_stufe, kStufenAnzahl - 1)];
    if (m_stufe < kStufenAnzahl - 1) { ++m_stufe; }
    m_anlaufTakt->start(ms);
}

void SpectrumClient::wiederAnlauf()
{
    if (!m_gewollt) { return; }
    QUrl url;
    url.setScheme(QStringLiteral("ws"));
    url.setHost(m_host);
    url.setPort(m_port);
    m_socket->open(url);
}

void SpectrumClient::aufVerbunden()
{
    m_stufe = 0;
    sendeStart();
    emit verbindungGeaendert(true);
}

void SpectrumClient::aufGetrennt()
{
    emit verbindungGeaendert(false);
    planeWiederAnlauf();
}

// ── Das Lesen eines Rahmens ──────────────────────────────────────────────
//
// Alles, was hier hereinkommt, ist fremd, bis es sich als unseres erwiesen
// hat. Die Reihenfolge der Prüfungen ist Absicht: erst die Länge, dann die
// Nummern, dann die Nutzlast. Vor der ersten Längenprüfung wird kein Byte
// gelesen.
QVector<quint8> SpectrumClient::leseRahmenFuerTest(const QByteArray& rahmen,
                                                   int* bilderJeSekunde)
{
    if (rahmen.size() < kKopfBytes) { return {}; }

    const quint32 stromTyp   = leseUInt32LE(rahmen, 24);
    const quint32 abtastTyp  = leseUInt32LE(rahmen, 8);
    if (int(stromTyp) != kStromTypSpektrum) { return {}; }   // nicht unserer
    if (int(abtastTyp) != kAbtastTypByteDbm) { return {}; }

    const quint32 punkteRoh = leseUInt32LE(rahmen, 20);
    if (punkteRoh < quint32(kMinPunkte) || punkteRoh > quint32(kMaxPunkte)) {
        return {};
    }
    const int punkte = int(punkteRoh);

    // Die Länge im Kopf MUSS zur wirklichen Rahmenlänge passen. Ein Rahmen,
    // der mehr verspricht als er hält, wäre genau der Fall, in dem ein
    // ungeprüfter Leser über den Rand liefe.
    if (rahmen.size() - kKopfBytes != punkte) { return {}; }

    QVector<quint8> werte(punkte);
    const auto* p = reinterpret_cast<const quint8*>(rahmen.constData()) + kKopfBytes;
    std::copy(p, p + punkte, werte.begin());

    if (bilderJeSekunde != nullptr) {
        // Im Spektrumrahmen trägt das Feld an Versatz 4 die BILDRATE, nicht
        // die Abtastrate -- ein Spektrum hat keine. So steht es in Longpaths
        // buildSpectrumPayload.
        const quint32 fps = leseUInt32LE(rahmen, 4);
        *bilderJeSekunde = (fps >= 1 && fps <= 120) ? int(fps) : 0;
    }
    return werte;
}

void SpectrumClient::aufBinaerNachricht(const QByteArray& rahmen)
{
    int fps = 0;
    const QVector<quint8> werte = leseRahmenFuerTest(rahmen, &fps);
    if (werte.isEmpty()) {
        // Zählen, nicht melden. Fremde Ströme teilen sich dieselbe
        // Leitung; ein verworfener Rahmen ist für sich genommen kein
        // Fehler, eine WACHSENDE Zahl schon. Die liest die Diagnose.
        ++m_verworfen;
        return;
    }
    emit spektrumDa(werte, fps);
}

} // namespace Contestprogramm
