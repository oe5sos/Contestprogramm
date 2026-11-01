// =================================================================
// src/core/QsoAudioRecorder.cpp  (Contestprogramm)
// =================================================================
//
// Begruendungen stehen im Kopf.
// =================================================================

#include "core/QsoAudioRecorder.h"

#include <QDir>
#include <QFileInfo>
#include <QStorageInfo>
#include <QTextStream>

namespace Contestprogramm {

namespace {

// Nur fuer den Prueflauf: statt der echten Platte ein fester Wert.
int s_freePercentOverride = -1;

// Ein WAV-Kopf ist 44 Byte; die zwei Groessenfelder darin stehen erst
// am Ende fest und werden beim Schliessen nachgetragen.
constexpr int kWavHeaderBytes = 44;

void putLe32(QByteArray& b, int at, quint32 v)
{
    b[at + 0] = static_cast<char>(v & 0xff);
    b[at + 1] = static_cast<char>((v >> 8) & 0xff);
    b[at + 2] = static_cast<char>((v >> 16) & 0xff);
    b[at + 3] = static_cast<char>((v >> 24) & 0xff);
}

void putLe16(QByteArray& b, int at, quint16 v)
{
    b[at + 0] = static_cast<char>(v & 0xff);
    b[at + 1] = static_cast<char>((v >> 8) & 0xff);
}

QString stempel(const QDateTime& utc)
{
    return utc.toUTC().toString(QStringLiteral("yyyyMMdd-HHmmss")) + QLatin1Char('Z');
}

} // namespace

QsoAudioRecorder::QsoAudioRecorder(QObject* parent)
    : QObject(parent)
{
}

QsoAudioRecorder::~QsoAudioRecorder()
{
    // Ohne das bliebe eine Datei mit falschen Groessenfeldern liegen:
    // unlesbar fuer jeden Spieler, obwohl der Ton darin steht.
    if (m_file.isOpen()) {
        patchWavHeader();
        m_file.close();
    }
}

void QsoAudioRecorder::setSettings(const Settings& s)
{
    m_settings = s;
    if (m_settings.rollMinutes < 0) { m_settings.rollMinutes = 0; }
    if (m_settings.minFreePercent < 0) { m_settings.minFreePercent = 0; }
    if (m_settings.minFreePercent > 90) { m_settings.minFreePercent = 90; }
}

bool QsoAudioRecorder::setFormat(int sampleRateHz, int channels)
{
    // Mitten in einer WAV-Datei das Format zu wechseln macht sie
    // unlesbar -- der Kopf nennt genau eine Rate und eine Kanalzahl.
    if (isRecording()) { return false; }
    if (sampleRateHz <= 0 || channels <= 0 || channels > 8) { return false; }
    m_rateHz   = sampleRateHz;
    m_channels = channels;
    return true;
}

void QsoAudioRecorder::setFreePercentOverrideForTest(int percent)
{
    s_freePercentOverride = percent;
}

double QsoAudioRecorder::recordedSeconds() const
{
    if (m_rateHz <= 0) { return 0.0; }
    return static_cast<double>(m_framesTotal) / m_rateHz;
}

QString QsoAudioRecorder::currentIndexFile() const
{
    if (m_path.isEmpty()) { return {}; }
    QString p = m_path;
    if (p.endsWith(QStringLiteral(".wav"), Qt::CaseInsensitive)) { p.chop(4); }
    return p + QStringLiteral(".qsos.txt");
}

bool QsoAudioRecorder::enoughSpace() const
{
    if (m_settings.minFreePercent <= 0) { return true; }
    if (s_freePercentOverride >= 0) {
        return s_freePercentOverride >= m_settings.minFreePercent;
    }
    const QStorageInfo info(m_settings.folder);
    if (!info.isValid() || info.bytesTotal() <= 0) {
        // Eine Platte, die nichts ueber sich sagt, darf die Aufnahme
        // nicht verhindern -- dieselbe Haltung wie bei Thetis'
        // OkToRecord(allowUnknown).
        return true;
    }
    const double frei = 100.0 * static_cast<double>(info.bytesAvailable())
                        / static_cast<double>(info.bytesTotal());
    return frei >= m_settings.minFreePercent;
}

bool QsoAudioRecorder::start(QString* error)
{
    if (isRecording()) { return false; }
    if (m_settings.folder.trimmed().isEmpty()) {
        if (error) { *error = QStringLiteral("Kein Ordner fuer den Mitschnitt eingestellt."); }
        return false;
    }
    if (!QDir().mkpath(m_settings.folder)) {
        if (error) {
            *error = QStringLiteral("Ordner laesst sich nicht anlegen: %1")
                         .arg(m_settings.folder);
        }
        return false;
    }
    if (!enoughSpace()) {
        if (error) {
            *error = QStringLiteral("Auf der Platte sind weniger als %1 %% frei.")
                         .arg(m_settings.minFreePercent);
        }
        return false;
    }

    m_framesTotal = 0;
    if (!openNewFile(error)) { return false; }

    emit recordingChanged(true);
    return true;
}

bool QsoAudioRecorder::openNewFile(QString* error)
{
    const QDateTime nun = QDateTime::currentDateTimeUtc();

    // ── Der Name muss eindeutig sein, nicht nur ueblich ──────────────
    //
    // Der Stempel hat Sekundenauflaesung. Im Betrieb wird stundenweise
    // gewechselt, da kollidiert nichts -- aber bei kurzer Frist, nach
    // einem Neustart in derselben Sekunde oder bei einem zweiten
    // Mitschnitt in dasselbe Verzeichnis traf der neue Name den alten,
    // und `Truncate` LOESCHTE die fertige Datei. Gefunden vom eigenen
    // Prueflauf (`nachDerFristBeginntEineNeueDatei`), nicht im Betrieb;
    // dort haette es eine Stunde Ton gekostet.
    QString ziel = m_settings.folder + QLatin1Char('/')
                   + QStringLiteral("mitschnitt-") + stempel(nun)
                   + QStringLiteral(".wav");
    for (int n = 2; QFileInfo::exists(ziel) && n < 1000; ++n) {
        ziel = m_settings.folder + QLatin1Char('/')
               + QStringLiteral("mitschnitt-") + stempel(nun)
               + QLatin1Char('-') + QString::number(n)
               + QStringLiteral(".wav");
    }

    m_file.setFileName(ziel);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = QStringLiteral("Datei laesst sich nicht schreiben: %1").arg(ziel);
        }
        return false;
    }

    m_path           = ziel;
    m_framesInFile   = 0;
    m_fileStartedUtc = nun;
    writeWavHeader();

    // Der Index gehoert zu GENAU dieser Tondatei. Lag dort noch einer
    // (gleicher Name aus einem frueheren Lauf), wuerde markQso() daran
    // anhaengen und auf Sekunden in einer anderen Aufnahme zeigen.
    QFile::remove(currentIndexFile());
    return true;
}

void QsoAudioRecorder::writeWavHeader()
{
    const int bytesPerSample = 2;
    const quint32 byteRate = static_cast<quint32>(m_rateHz) *
                             static_cast<quint32>(m_channels) *
                             static_cast<quint32>(bytesPerSample);

    QByteArray h(kWavHeaderBytes, '\0');
    h.replace(0, 4, "RIFF");
    putLe32(h, 4, 0);            // Gesamtgroesse -- beim Schliessen
    h.replace(8, 4, "WAVE");
    h.replace(12, 4, "fmt ");
    putLe32(h, 16, 16);          // Laenge des fmt-Blocks
    putLe16(h, 20, 1);           // PCM
    putLe16(h, 22, static_cast<quint16>(m_channels));
    putLe32(h, 24, static_cast<quint32>(m_rateHz));
    putLe32(h, 28, byteRate);
    putLe16(h, 32, static_cast<quint16>(m_channels * bytesPerSample));
    putLe16(h, 34, 16);          // Bits je Wert
    h.replace(36, 4, "data");
    putLe32(h, 40, 0);           // Datengroesse -- beim Schliessen

    m_file.write(h);
}

void QsoAudioRecorder::patchWavHeader()
{
    if (!m_file.isOpen()) { return; }

    const qint64 datenBytes = m_framesInFile * m_channels * 2;
    const qint64 merke = m_file.pos();

    QByteArray feld(4, '\0');
    putLe32(feld, 0, static_cast<quint32>(36 + datenBytes));
    m_file.seek(4);
    m_file.write(feld);

    putLe32(feld, 0, static_cast<quint32>(datenBytes));
    m_file.seek(40);
    m_file.write(feld);

    m_file.seek(merke);
    m_file.flush();
}

void QsoAudioRecorder::closeFile()
{
    if (!m_file.isOpen()) { return; }
    patchWavHeader();
    m_file.close();
}

bool QsoAudioRecorder::rollDueNow() const
{
    if (m_settings.rollMinutes <= 0) { return false; }
    if (!m_fileStartedUtc.isValid()) { return false; }
    const qint64 sekundenInDatei = m_framesInFile / qMax(1, m_rateHz);
    return sekundenInDatei >= static_cast<qint64>(m_settings.rollMinutes) * 60;
}

void QsoAudioRecorder::feed(const qint16* samples, int frames)
{
    if (!m_file.isOpen() || samples == nullptr || frames <= 0) { return; }

    // Die Zeit im Mitschnitt kommt aus den RAHMEN, nicht aus der Uhr.
    // Eine Uhr laeuft weiter, wenn das Tongeraet aussetzt; dann
    // zeigten die Indexzeilen auf Stellen, die es in der Datei nicht
    // gibt. Die Rahmen zaehlen, was wirklich geschrieben wurde.
    const qint64 bytes = static_cast<qint64>(frames) * m_channels * 2;
    const qint64 schrieb = m_file.write(reinterpret_cast<const char*>(samples), bytes);
    if (schrieb != bytes) {
        const QString grund =
            QStringLiteral("Schreibfehler im Mitschnitt (%1 von %2 Byte).")
                .arg(schrieb).arg(bytes);
        closeFile();
        emit recordingChanged(false);
        emit stoppedWithReason(grund);
        return;
    }

    m_framesInFile += frames;
    m_framesTotal  += frames;

    if (rollDueNow()) {
        closeFile();
        if (!enoughSpace()) {
            emit recordingChanged(false);
            emit stoppedWithReason(
                QStringLiteral("Mitschnitt beendet: auf der Platte sind "
                               "weniger als %1 %% frei.")
                    .arg(m_settings.minFreePercent));
            return;
        }
        QString fehler;
        if (!openNewFile(&fehler)) {
            emit recordingChanged(false);
            emit stoppedWithReason(fehler);
            return;
        }
        emit fileRolled(m_path);
    }
}

void QsoAudioRecorder::markQso(const QString& call, const QString& band,
                               const QString& mode, const QDateTime& utc)
{
    // Ohne laufende Aufnahme ein Leerlauf: das Logprogramm soll nicht
    // wissen muessen, ob mitgeschnitten wird.
    if (!m_file.isOpen()) { return; }

    const qint64 sekunde = m_framesInFile / qMax(1, m_rateHz);

    QFile idx(currentIndexFile());
    if (!idx.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }
    QTextStream aus(&idx);
    // Eine Zeile je QSO, mit Tabulatoren: in jeder Tabelle und in
    // jedem Editor lesbar, und die erste Spalte ist die Sekunde IN
    // DIESER Datei -- genau das, was ein Tonspieler als Sprungziel
    // nimmt.
    aus << sekunde << '\t'
        << (sekunde / 3600) << ':'
        << qSetFieldWidth(2) << qSetPadChar(QLatin1Char('0'))
        << ((sekunde / 60) % 60) << qSetFieldWidth(0) << ':'
        << qSetFieldWidth(2) << (sekunde % 60) << qSetFieldWidth(0) << '\t'
        << call << '\t' << band << '\t' << mode << '\t'
        << utc.toUTC().toString(Qt::ISODate) << '\n';
    idx.close();
}

void QsoAudioRecorder::stop()
{
    if (!m_file.isOpen()) { return; }
    closeFile();
    emit recordingChanged(false);
}

} // namespace Contestprogramm
