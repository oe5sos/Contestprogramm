#include "data/QsoJournal.h"

#include "app/ContestSettings.h"
#include "data/AdifExporter.h"
#include "data/QsoRecord.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

#ifdef Q_OS_WIN
#include <io.h>
#else
#include <unistd.h>
#endif

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
    // Bis auf die Platte, plattformabhängig. Unter Windows gibt es
    // weder <unistd.h> noch fsync(); _commit() auf demselben
    // Dateideskriptor tut dasselbe. Der erste Wurf hatte nur den
    // POSIX-Weg -- die Windows-CI brach schon beim Bauen ab.
#ifdef Q_OS_WIN
    ::_commit(datei.handle());
#else
    ::fsync(datei.handle());
#endif
    datei.close();
    m_letzterFehler.clear();
    return true;
}

QString QsoJournal::vorgangsPfad() const
{
    if (m_pfad.isEmpty()) {
        return QString();
    }
    const QFileInfo info(m_pfad);
    return info.absolutePath() + QLatin1Char('/') + info.completeBaseName()
           + QStringLiteral("-vorgaenge.log");
}

bool QsoJournal::vermerke(const QString& vorgang)
{
    const QString ziel = vorgangsPfad();
    if (ziel.isEmpty()) {
        m_letzterFehler = QStringLiteral("kein Journalpfad gesetzt");
        return false;
    }
    QDir().mkpath(QFileInfo(ziel).absolutePath());
    QFile datei(ziel);
    if (!datei.open(QIODevice::Append | QIODevice::WriteOnly | QIODevice::Text)) {
        m_letzterFehler = datei.errorString();
        return false;
    }
    QTextStream aus(&datei);
    aus << QDateTime::currentDateTimeUtc().toString(Qt::ISODate) << QStringLiteral(" UTC  ")
        << vorgang.simplified() << "\n";
    aus.flush();
    if (aus.status() != QTextStream::Ok) {
        m_letzterFehler = QStringLiteral("Schreiben fehlgeschlagen");
        return false;
    }
    if (!datei.flush()) {
        m_letzterFehler = datei.errorString();
        return false;
    }
#ifdef Q_OS_WIN
    ::_commit(datei.handle());
#else
    ::fsync(datei.handle());
#endif
    datei.close();
    m_letzterFehler.clear();
    return true;
}

} // namespace Contestprogramm
