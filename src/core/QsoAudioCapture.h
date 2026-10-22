#pragma once

// =================================================================
// src/core/QsoAudioCapture.h  (Contestprogramm)
// =================================================================
//
// Das Tongeraet anschalten -- und nur das.
//
// Getrennt von QsoAudioRecorder, weil DIESE Datei die Abhaengigkeit
// auf Qt Multimedia hat und der Mitschneider selbst keine braucht.
// Damit bleibt die ganze Logik (WAV, Dateiwechsel, Index, Platzwache)
// ohne Tongeraet pruefbar, und die CI braucht keines.
//
// ── Ein Eingang, so viele Kanaele wie er hat ────────────────────────
//
// Wer Empfang UND eigene Stimme mitschneiden will, baut ein
// Aggregat-Geraet (macOS: Audio-MIDI-Setup) mit links Empfang und
// rechts Mikrofon und waehlt es hier. Zwei getrennte Geraete waeren
// zwei Takte, und ueber Stunden laufen die auseinander -- die
// Begruendung steht im Kopf von QsoAudioRecorder.h.
//
// ── Was das Geraet sagt, gilt ───────────────────────────────────────
//
// Die Abtastrate und die Kanalzahl kommen vom Geraet, nicht aus einer
// Einstellung: ein Wunschformat, das das Geraet nicht kann, laesst Qt
// umrechnen oder schlagen, und beides merkt man erst an einer
// unbrauchbaren Aufnahme. Gefragt wird die bevorzugte Form des
// Geraets; abgelehnt wird nur, was gar nicht geht.
//
// =================================================================

#include <QAudioFormat>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

class QAudioSource;
class QIODevice;

namespace Contestprogramm {

class QsoAudioRecorder;

class QsoAudioCapture : public QObject {
    Q_OBJECT
public:
    explicit QsoAudioCapture(QObject* parent = nullptr);
    ~QsoAudioCapture() override;

    /// Die Namen der Eingangsgeraete, wie sie in der Einstellung
    /// stehen sollen. Erster Eintrag ist immer das Standardgeraet des
    /// Systems.
    static QStringList inputDeviceNames();

    /// Nicht besitzend: der Mitschneider lebt laenger als eine
    /// Aufnahme.
    void setRecorder(QsoAudioRecorder* recorder);

    /// Leerer Name = Standardgeraet des Systems.
    bool start(const QString& deviceName, QString* error);
    void stop();
    bool isRunning() const;

    QString deviceName() const { return m_deviceName; }
    int     sampleRateHz() const { return m_format.sampleRate(); }
    int     channels() const { return m_format.channelCount(); }

signals:
    /// Abgebrochen, und warum (Geraet weg, Format unmoeglich).
    void stoppedWithReason(const QString& reason);

private:
    void onReadyRead();

    QsoAudioRecorder* m_recorder{nullptr};
    std::unique_ptr<QAudioSource> m_source;
    QIODevice*    m_stream{nullptr};
    QAudioFormat  m_format;
    QString       m_deviceName;
    QByteArray    m_rest;   // halbe Rahmen zwischen zwei Lesevorgaengen
};

} // namespace Contestprogramm
