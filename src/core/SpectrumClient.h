#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVector>

class QTimer;
class QWebSocket;

namespace Contestprogramm {

// Holt von Longpath ein fertig gerechnetes Spektrum und sonst nichts.
//
// ── Warum nicht rohes I/Q ───────────────────────────────────────────────
//
// TCI kann einen I/Q-Strom. Ein Client, der ein BILD will, müsste dann den
// ganzen Strom ziehen und selbst eine FFT rechnen -- gemessen 404 kB/s bei
// 48 kHz, hochgerechnet rund 1,5 MB/s bei 192 kHz, dazu die FFT. Longpath
// hat dafür einen eigenen Stromtyp, der die fertigen dBm-Werte schickt
// (TciStreamType::SpectrumStream = 100, Longpath src/core/TciBinaryFrame.h,
// seit 2026-09-30). Gemessen rund 27 kB/s, also ein Sechzigstel, und hier
// ist weder eine FFT nötig noch ein DSP-Faden.
//
// Der Befehl:
//
//     spectrum_start:<rx>[,<punkte>[,<bilder je sekunde>]]
//     spectrum_stop:<rx>
//
// Die PUNKTZAHL nennt der Client -- nur er weiß, wie breit sein Panel ist.
// Longpath verdichtet darauf über den SPITZENWERT, nicht den Mittelwert:
// ein Mittel über 64 Bins ließe einen schmalen Träger im Rauschen
// verschwinden, und genau den sucht man im Contest.
//
// Nur Empfänger 0. Longpath hält eine FFTEngine; `spectrum_start:1` wird
// dort bewusst SCHWEIGEND verworfen, damit ein Client den anderen Weg nimmt
// statt auf ein Bild zu warten, das nie kommt.
//
// ── Was diese Klasse NICHT tut, und warum das hier zählt ────────────────
//
// Betreiber am 2026-11-01: „WICHTIG ist, dass das logprogramm nie
// abstürzt." Dieser Client hängt in einem LOGBUCH, nicht in einem
// Funkprogramm. Darum:
//
//   * Er sendet NIE etwas, das Longpath bedient -- kein Abstimmen, kein
//     Tasten, keine Betriebsart. Nur `spectrum_start` und `spectrum_stop`.
//   * Jeder eingehende Rahmen wird grenzgeprüft, bevor ein einziges Byte
//     gelesen wird (siehe leseRahmen). Ein unerwarteter Rahmen wird
//     gezählt und verworfen, nicht gedeutet.
//   * Kein eigener Faden, kein geteilter Zustand. Alles im Hauptfaden,
//     Qt-Signale.
//   * Fehlt der Server, passiert nichts Lautes: der Wiederanlauf wartet
//     in Stufen (1, 2, 5, 10, 30, 60 s) und bleibt bei 60 s stehen.
//     Ein Logbuch darf nicht in einer Schleife hängen, weil ein
//     Panadapter fehlt.
class SpectrumClient : public QObject {
    Q_OBJECT

public:
    // Longpath klemmt die Punktzahl selbst; hier steht die Grenze noch
    // einmal, damit ein verdorbener Rahmen keine Riesenbelegung auslöst.
    static constexpr int kMaxPunkte = 8192;
    static constexpr int kMinPunkte = 16;

    // Der Kopf eines TCI-Binärrahmens ist immer 64 Byte.
    static constexpr int kKopfBytes = 64;

    // Longpath-eigene Nummern (TciBinaryFrame.h). Beide werden geprüft:
    // ein fremder TCI-Server kennt sie nicht und schickt sie nie, ein
    // Rahmen mit anderen Werten ist also nicht für uns.
    static constexpr int kStromTypSpektrum = 100;
    static constexpr int kAbtastTypByteDbm = 100;

    // Kodierung der Nutzlast: byte = dBm + 200, geklemmt auf 0..255.
    // Von piHPSDR übernommen, siehe Longpaths Header.
    static constexpr int kDbmVersatz = 200;

    explicit SpectrumClient(QObject* parent = nullptr);
    ~SpectrumClient() override;

    void setEndpunkt(const QString& host, quint16 port);

    // Punktzahl = Breite des Panels in Bildpunkten, Bildrate in Bildern
    // je Sekunde. Beides wird geklemmt. Ändert sich die Breite im Betrieb,
    // wird `spectrum_start` mit der neuen Zahl nachgeschickt -- der Server
    // nimmt den jüngsten Wert.
    void setGeometrie(int punkte, int bilderJeSekunde);

    void starten();
    void beenden();

    bool istVerbunden() const;

    // Zähler für die Diagnose. Ein Rahmen, der nicht zu uns gehört, ist
    // kein Fehler (fremde Ströme teilen sich dieselbe Leitung); einer, der
    // zu uns gehört und nicht stimmt, schon.
    int verworfeneRahmen() const { return m_verworfen; }

    // Prüfnaht: deutet einen Rahmen, ohne dass eine Verbindung besteht.
    // Gibt leer zurück, wenn der Rahmen nicht zu uns gehört oder nicht
    // stimmt -- dieselbe Entscheidung wie im Betrieb.
    static QVector<quint8> leseRahmenFuerTest(const QByteArray& rahmen,
                                              int* bilderJeSekunde = nullptr);

signals:
    // Ein vollständiges Bild, ein Wert je Bildpunkt, roh als byte.
    // Die Umrechnung in dBm macht der Empfänger (byte - kDbmVersatz),
    // damit hier nichts gerechnet wird, was dort ohnehin skaliert wird.
    void spektrumDa(const QVector<quint8>& werte, int bilderJeSekunde);

    void verbindungGeaendert(bool verbunden);

private slots:
    void aufVerbunden();
    void aufGetrennt();
    void aufBinaerNachricht(const QByteArray& rahmen);
    void wiederAnlauf();

private:
    void sendeStart();
    void planeWiederAnlauf();

    QWebSocket* m_socket = nullptr;
    QTimer*     m_anlaufTakt = nullptr;

    QString  m_host = QStringLiteral("127.0.0.1");
    quint16  m_port = 50001;
    int      m_punkte = 256;
    int      m_fps = 10;

    bool m_gewollt = false;   // hat der Bediener es eingeschaltet?
    int  m_stufe = 0;         // Index in die Wartestufen
    int  m_verworfen = 0;
};

} // namespace Contestprogramm
