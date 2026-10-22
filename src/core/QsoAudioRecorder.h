#pragma once

// =================================================================
// src/core/QsoAudioRecorder.h  (Contestprogramm)
// =================================================================
//
// Stundenlang alles mitschneiden -- unabhaengig von der Konsole.
//
// Betreiber am 2026-10-22: „ziel wäre, dass das logprogramm das alles
// kann, unabhängig von longpath. sprich ich würde gerne stundenlang
// alles automatisch mitschneiden wollen."
//
// Warum hier und nicht in Longpath: Longpaths QSO-Mitschnitt greift
// seine eigenen DSP-Abgriffe ab. Das ist genau, was hier NICHT
// gewuenscht ist -- am Flex laeuft SmartSDR oder AetherSDR, an einem
// anderen Geraet etwas drittes, und das Logprogramm ist das einzige,
// das an jeder Station offen ist. Also haengt dieser Mitschnitt an
// einem gewoehnlichen Eingangsgeraet des Rechners und weiss von keiner
// Konsole etwas.
//
// ── Ein Geraet, nicht zwei ──────────────────────────────────────────
//
// Diese Klasse nimmt EINEN Eingang auf, mit den Kanaelen, die er
// liefert. Wer Empfang UND eigene Stimme will, baut sich ein
// Aggregat-Geraet (macOS: Audio-MIDI-Setup) -- links Empfang, rechts
// Mikrofon -- und waehlt das hier aus.
//
// Das ist Absicht und keine Bequemlichkeit: zwei getrennte Geraete
// haben zwei Takte. Ueber eine Viertelstunde faellt das nicht auf,
// ueber die vom Betreiber verlangten STUNDEN laufen sie um Sekunden
// auseinander, und dann liegt die eigene Stimme neben der Antwort
// darauf. Ein Aggregat-Geraet hat EINEN Taktgeber; die Ausrichtung
// macht damit das Betriebssystem, das sie als einziges richtig machen
// kann.
//
// ── Die Abtastwerte kommen von aussen ────────────────────────────────
//
// feed() nimmt verschachtelte int16 und weiss nicht, woher. Damit ist
// diese Klasse ohne Tongeraet pruefbar -- und die CI hat keines. Das
// Anschalten des Geraets ist nicht hier, sondern in
// QsoAudioCapture (eigene Datei, eigene Abhaengigkeit auf
// Qt Multimedia).
//
// ── Warum stundenweise gewechselt wird ──────────────────────────────
//
// Eine WAV-Datei fasst wegen ihrer 32-Bit-Groessenfelder knapp 4 GB.
// 48 kHz, zwei Kanaele, 16 Bit sind 192 kB/s -- also gut sechs
// Stunden, danach ist die Datei kaputt, nicht voll. Darum wird
// spaetestens nach `rollMinutes` eine neue begonnen. Zweiter Grund:
// ein Absturz nach fuenf Stunden kostet dann die letzte Stunde und
// nicht den ganzen Abend, weil die Groessenfelder jeder
// abgeschlossenen Datei schon stimmen.
//
// ── Der Index ist der eigentliche Nutzen ─────────────────────────────
//
// Ein Mitschnitt von acht Stunden, in dem man eine Verbindung sucht,
// ist wertlos. Darum schreibt markQso() neben die Tondatei eine
// Indexdatei: je geloggtem QSO eine Zeile mit der Sekunde IM
// MITSCHNITT, Rufzeichen, Band, Betriebsart und der UTC-Zeit. Damit
// springt man hin, statt zu suchen. Das Logprogramm ruft das ohnehin
// schon an der Stelle, an der es den QSO-Broadcast abschickt
// (AppController::publishQso).
//
// =================================================================

#include <QByteArray>
#include <QDateTime>
#include <QFile>
#include <QObject>
#include <QString>


namespace Contestprogramm {

class QsoAudioRecorder : public QObject {
    Q_OBJECT
public:
    explicit QsoAudioRecorder(QObject* parent = nullptr);
    ~QsoAudioRecorder() override;

    struct Settings {
        QString folder;            // leer = nicht aufnehmen
        int     rollMinutes{60};   // 0 = nie wechseln (nicht empfohlen, siehe Kopf)
        int     minFreePercent{5}; // darunter hoert er auf, mit Grund
    };

    void setSettings(const Settings& s);
    Settings settings() const { return m_settings; }

    // Muss VOR start() stimmen; waehrend der Aufnahme abgelehnt, weil
    // ein Formatwechsel mitten in einer WAV-Datei sie unlesbar macht.
    bool setFormat(int sampleRateHz, int channels);
    int  sampleRateHz() const { return m_rateHz; }
    int  channels() const { return m_channels; }

    bool start(QString* error);
    void stop();
    bool isRecording() const { return m_file.isOpen(); }

    /// Verschachtelte int16, `frames` Rahmen (also frames*channels
    /// Werte). Wird vom Tonfaden gerufen: kein Speicher wird hier
    /// angefordert ausser dem Schreibpuffer, der einmal steht.
    void feed(const qint16* samples, int frames);

    /// Direkt nach dem Loggen rufen. Schreibt eine Indexzeile; ohne
    /// laufende Aufnahme ein stiller Leerlauf (das Logprogramm soll
    /// nicht wissen muessen, ob mitgeschnitten wird).
    void markQso(const QString& call, const QString& band,
                 const QString& mode, const QDateTime& utc);

    qint64  recordedFrames() const { return m_framesTotal; }
    double  recordedSeconds() const;
    QString currentFile() const { return m_path; }
    QString currentIndexFile() const;

    /// Fuer den Prueflauf: die Platzwache mit einem festen Wert fahren
    /// statt mit der echten Platte. -1 hebt es wieder auf.
    static void setFreePercentOverrideForTest(int percent);

signals:
    void recordingChanged(bool on);
    /// Aufgehoert, und warum -- „Platte fast voll", „Schreibfehler".
    /// Ein Mitschnitt, der stumm endet, ist schlimmer als keiner.
    void stoppedWithReason(const QString& reason);
    void fileRolled(const QString& newPath);

private:
    bool openNewFile(QString* error);
    void closeFile();
    void writeWavHeader();
    void patchWavHeader();
    bool rollDueNow() const;
    bool enoughSpace() const;

    Settings m_settings;
    int      m_rateHz{48000};
    int      m_channels{2};

    QFile     m_file;
    QString   m_path;
    qint64    m_framesInFile{0};   // seit dem Dateiwechsel
    qint64    m_framesTotal{0};    // seit start()
    QDateTime m_fileStartedUtc;
    QByteArray m_puffer;
};

} // namespace Contestprogramm
