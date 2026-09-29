#include "data/QsoJournal.h"

#include "app/ContestSettings.h"
#include "data/AdifExporter.h"
#include "data/QsoRecord.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

#include <unistd.h>

namespace Contestprogramm {

QsoJournal::QsoJournal(const QString& pfad) : m_pfad(pfad) {}

bool QsoJournal::schreibe(const QsoRecord& record, const ContestSettings& settings)
{
    if (m_pfad.isEmpty()) {
        m_letzterFehler = QStringLiteral("kein Journalpfad gesetzt");
        return false;
    }
    QDir().mkpath(QFileInfo(m_pfad).absolutePath());

    QFile datei(m_pfad);
    // Anhängen, nie überschreiben: eine Zeile, die einmal drinsteht,
    // wird von diesem Programm nie wieder angefasst.
    if (!datei.open(QIODevice::Append | QIODevice::WriteOnly | QIODevice::Text)) {
        m_letzterFehler = datei.errorString();
        return false;
    }
    const QString zeile = toAdifRecord(record, settings);
    QTextStream aus(&datei);
    aus << zeile << "\n";
    aus.flush();
    if (aus.status() != QTextStream::Ok) {
        m_letzterFehler = QStringLiteral("Schreiben fehlgeschlagen");
        return false;
    }
    // Bis auf die Platte, nicht nur in den Puffer des Betriebssystems.
    // Ohne das wäre das Journal bei Stromausfall genauso leer wie die
    // Datenbank vor der Umstellung auf FULL -- und damit wertlos.
    if (!datei.flush()) {
        m_letzterFehler = datei.errorString();
        return false;
    }
    ::fsync(datei.handle());
    datei.close();
    m_letzterFehler.clear();
    return true;
}

} // namespace Contestprogramm
