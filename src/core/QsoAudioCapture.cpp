// =================================================================
// src/core/QsoAudioCapture.cpp  (Contestprogramm)
// =================================================================
//
// Begruendungen stehen im Kopf.
// =================================================================

#include "core/QsoAudioCapture.h"

#include "core/QsoAudioRecorder.h"

#include <QAudioDevice>
#include <QAudioSource>
#include <QPermissions>
#include <QGuiApplication>
#include <QIODevice>
#include <QMediaDevices>

namespace Contestprogramm {

QsoAudioCapture::QsoAudioCapture(QObject* parent)
    : QObject(parent)
{
}

QsoAudioCapture::~QsoAudioCapture() = default;

QStringList QsoAudioCapture::inputDeviceNames()
{
    QStringList namen;
    const QAudioDevice standard = QMediaDevices::defaultAudioInput();
    if (!standard.isNull()) {
        namen << standard.description();
    }
    const QList<QAudioDevice> alle = QMediaDevices::audioInputs();
    for (const QAudioDevice& d : alle) {
        if (!namen.contains(d.description())) {
            namen << d.description();
        }
    }
    return namen;
}

void QsoAudioCapture::setRecorder(QsoAudioRecorder* recorder)
{
    m_recorder = recorder;
}

bool QsoAudioCapture::isRunning() const
{
    return m_source != nullptr;
}

bool QsoAudioCapture::start(const QString& deviceName, QString* error)
{

    if (isRunning()) { return false; }
    if (m_recorder == nullptr) {
        if (error) { *error = QStringLiteral("Kein Mitschneider angeschlossen."); }
        return false;
    }

    QAudioDevice geraet = QMediaDevices::defaultAudioInput();
    if (!deviceName.trimmed().isEmpty()) {
        bool gefunden = false;
        const QList<QAudioDevice> alle = QMediaDevices::audioInputs();
        for (const QAudioDevice& d : alle) {
            if (d.description() == deviceName) {
                geraet   = d;
                gefunden = true;
                break;
            }
        }
        if (!gefunden) {
            // Nicht stillschweigend auf das Standardgeraet ausweichen:
            // wer „AetherSDR DAX 1" eingestellt hat und das Mikrofon
            // des Rechners bekommt, merkt es erst an der Aufnahme.
            if (error) {
                *error = QStringLiteral("Tongeraet nicht gefunden: %1").arg(deviceName);
            }
            return false;
        }
    }
    if (geraet.isNull()) {
        if (error) { *error = QStringLiteral("Es gibt kein Eingangsgeraet."); }
        return false;
    }

    // Was das Geraet bevorzugt, auf Int16 festgenagelt -- die
    // WAV-Datei ist Int16, und eine Umrechnung hier waere eine
    // zweite Stelle, an der Ton verloren gehen kann.
    // Und jetzt die Freigabe fuers Mikrofon.
    //
    // ERST nach der Geraetewahl: ein unbekannter Geraetename ist ein
    // Fehler fuer sich und soll nicht von der Rechtefrage verdeckt
    // werden (tst_qso_audio_capture prueft genau das).
    //
    // Betreiber am 2026-10-25, am Mac mini: „nimmt nicht auf, die zeit
    // laeuft nicht. die datei ist aber dann gespeichert" -- und die
    // Datei war 44 Bytes gross, also nur der WAV-Kopf. Keine einzige
    // Probe.
    //
    // Ohne Freigabe liefert macOS der App keinen Ton, ohne dass
    // irgendetwas fehlschlaegt: QAudioSource oeffnet, der Strom
    // bleibt leer, und das Programm schreibt eine Datei, in der
    // nichts steht. Das Paket bringt NSMicrophoneUsageDescription
    // mit, die Nachfrage kommt also -- aber wer sie einmal
    // weggeklickt hat, sieht danach nie wieder einen Hinweis.
    //
    // Dazu kommt: die App ist nicht signiert (bewusst, siehe
    // longpath-keine-apple-signierung), und macOS vergisst die
    // Zustimmung, wenn sich die Binaerdatei aendert -- also nach
    // jedem Austausch durch den Auto-Install.
#if QT_CONFIG(permissions)
    QMicrophonePermission mikrofon;
    switch (qApp->checkPermission(mikrofon)) {
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(mikrofon, [](const QPermission&) {});
        if (error) {
            *error = QStringLiteral("Das Mikrofon muss einmal freigegeben werden -- bitte die "
                                    "Nachfrage bestaetigen und den Mitschnitt noch einmal starten.");
        }
        return false;
    case Qt::PermissionStatus::Denied:
        if (error) {
            *error = QStringLiteral("Der Ton bleibt leer: der Zugriff aufs Mikrofon ist fuer das "
                                    "Contestprogramm gesperrt.");
        }
        return false;
    case Qt::PermissionStatus::Granted:
        break;
    }
#endif

    QAudioFormat f = geraet.preferredFormat();
    f.setSampleFormat(QAudioFormat::Int16);
    if (!geraet.isFormatSupported(f)) {
        f = geraet.preferredFormat();
        if (f.sampleFormat() != QAudioFormat::Int16) {
            if (error) {
                *error = QStringLiteral(
                             "%1 liefert kein 16-Bit-Format (sondern %2).")
                             .arg(geraet.description())
                             .arg(static_cast<int>(f.sampleFormat()));
            }
            return false;
        }
    }

    if (!m_recorder->setFormat(f.sampleRate(), f.channelCount())) {
        if (error) {
            *error = QStringLiteral("Format %1 Hz / %2 Kanaele wird nicht angenommen "
                                    "(laeuft der Mitschnitt schon?)")
                         .arg(f.sampleRate()).arg(f.channelCount());
        }
        return false;
    }

    m_format = f;
    m_source = std::make_unique<QAudioSource>(geraet, f);
    m_stream = m_source->start();
    if (m_stream == nullptr) {
        m_source.reset();
        if (error) {
            *error = QStringLiteral("%1 liess sich nicht oeffnen.")
                         .arg(geraet.description());
        }
        return false;
    }

    m_deviceName = geraet.description();
    m_rest.clear();
    connect(m_stream, &QIODevice::readyRead, this, &QsoAudioCapture::onReadyRead);
    return true;
}

void QsoAudioCapture::onReadyRead()
{
    if (m_stream == nullptr || m_recorder == nullptr) { return; }

    m_rest += m_stream->readAll();

    // Nur VOLLE Rahmen weitergeben. Ein halber Rahmen am Ende eines
    // Lesevorgangs ist der Normalfall; gaebe man ihn mit, verschoeben
    // sich ab da links und rechts gegeneinander -- und zwar fuer den
    // Rest der Aufnahme.
    const int bytesPerFrame = m_format.channelCount() * 2;
    if (bytesPerFrame <= 0) { return; }
    const int volleRahmen = static_cast<int>(m_rest.size()) / bytesPerFrame;
    if (volleRahmen <= 0) { return; }

    m_recorder->feed(reinterpret_cast<const qint16*>(m_rest.constData()),
                     volleRahmen);
    m_rest.remove(0, volleRahmen * bytesPerFrame);
}

void QsoAudioCapture::stop()
{
    if (m_stream != nullptr) {
        disconnect(m_stream, &QIODevice::readyRead, this, &QsoAudioCapture::onReadyRead);
        m_stream = nullptr;
    }
    if (m_source) {
        m_source->stop();
        m_source.reset();
    }
    m_rest.clear();
}

} // namespace Contestprogramm
