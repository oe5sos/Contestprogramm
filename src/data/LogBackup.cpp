#include "data/LogBackup.h"

#include "data/ContestDatabase.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QTimeZone>
#include <QTimer>

namespace Contestprogramm {

namespace {
const QString kPrefix = QStringLiteral("contestprogramm-");
const QString kSuffix = QStringLiteral(".sqlite");
} // namespace

LogBackup::LogBackup(ContestDatabase& database, const QString& directory, QObject* parent)
    : QObject(parent)
    , m_database(&database)
    , m_directory(directory)
    , m_timer(new QTimer(this))
{
    connect(m_timer, &QTimer::timeout, this, [this]() {
        QString error;
        const QString path = backupNow(false, &error);
        if (path.isEmpty() && !error.isEmpty()) {
            emit backupFailed(error);
        }
    });
}

void LogBackup::setMirrorDirectory(const QString& directory)
{
    m_mirrorDirectory = directory.trimmed();
}

void LogBackup::start(int intervalMs)
{
    m_timer->setInterval(intervalMs);
    m_timer->start();
}

QString LogBackup::fileNameFor(const QDateTime& utc)
{
    return kPrefix + utc.toString(QStringLiteral("yyyyMMdd-HHmm")) + kSuffix;
}

QString LogBackup::backupNow(bool force, QString* errorOut)
{
    if (errorOut) {
        errorOut->clear();
    }
    const int counter = m_database->qsoWriteCounter();
    if (!force && counter == m_counterAtLastBackup) {
        return QString();
    }
    if (!QDir().mkpath(m_directory)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Sicherungsordner konnte nicht angelegt werden: %1").arg(m_directory);
        }
        return QString();
    }
    // Two copies within the same minute (a forced one right after the
    // timer's) get distinct names rather than a VACUUM INTO refusing an
    // existing target.
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QString path = QDir(m_directory).filePath(fileNameFor(now));
    for (int n = 2; QFileInfo::exists(path); ++n) {
        path = QDir(m_directory).filePath(fileNameFor(now).replace(kSuffix, QStringLiteral("-%1%2").arg(n).arg(kSuffix)));
    }
    QString error;
    if (!m_database->backupTo(path, &error)) {
        if (errorOut) {
            *errorOut = error;
        }
        return QString();
    }
    m_counterAtLastBackup = counter;
    prune();
    emit backupWritten(path);
    mirror(path);
    return path;
}

QString LogBackup::mirrorMarkerName()
{
    return QStringLiteral(".contestprogramm-sicherungsordner");
}

bool LogBackup::prepareMirrorDirectory(const QString& directory, QString* errorOut)
{
    const QString ziel = directory.trimmed();
    if (ziel.isEmpty()) {
        if (errorOut) { *errorOut = QStringLiteral("kein Ordner angegeben"); }
        return false;
    }
    if (!QDir().mkpath(ziel)) {
        if (errorOut) { *errorOut = QStringLiteral("Ordner nicht erreichbar: %1").arg(ziel); }
        return false;
    }
    QFile marke(QDir(ziel).filePath(mirrorMarkerName()));
    if (!marke.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorOut) { *errorOut = marke.errorString(); }
        return false;
    }
    marke.write(QStringLiteral("Contestprogramm: zweiter Sicherungsordner, eingerichtet %1 UTC.\n"
                               "Diese Datei nicht löschen -- an ihr erkennt das Programm, dass der\n"
                               "Datenträger noch derselbe ist (ein abgezogener USB-Stick hinterlässt\n"
                               "sonst einen leeren Ordner, in den stillschweigend weitergeschrieben\n"
                               "würde).\n")
                    .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODate))
                    .toUtf8());
    marke.close();
    if (marke.error() != QFileDevice::NoError) {
        if (errorOut) { *errorOut = marke.errorString(); }
        return false;
    }
    if (errorOut) { errorOut->clear(); }
    return true;
}

void LogBackup::mirror(const QString& path)
{
    if (m_mirrorDirectory.isEmpty()) {
        return;
    }
    // An unplugged stick must not take the backup with it: the copy
    // beside the database is already written when this runs.
    if (!QDir().mkpath(m_mirrorDirectory)) {
        emit mirrorFailed(QStringLiteral("Zweiter Sicherungsordner nicht erreichbar: %1").arg(m_mirrorDirectory));
        return;
    }
    // Ist es noch DERSELBE Ordner? Siehe prepareMirrorDirectory(): ein
    // abgezogener Stick hinterlässt seinen Einhängepunkt als leeren
    // Ordner auf der internen Platte, und ohne diese Prüfung landete
    // die Kopie dort -- auf derselben Platte wie das Log, dessen
    // Ausfall der zweite Ordner abfangen soll.
    if (!QFileInfo::exists(QDir(m_mirrorDirectory).filePath(mirrorMarkerName()))) {
        // Ein Ordner, in dem schon Sicherungen DIESES Programms liegen,
        // ist offenkundig der richtige -- nur eingerichtet, bevor es
        // die Markierung gab. Dann wird sie nachgetragen statt eine
        // Meldung erzeugt, die nach einem Update jeden bestehenden
        // zweiten Sicherungsordner beträfe. Ein abgezogener Stick
        // hinterlässt dagegen einen LEEREN Ordner -- daran scheiden
        // sich die beiden Fälle.
        const bool schonSicherungenDrin = !listBackups(m_mirrorDirectory).isEmpty();
        QString markeFehler;
        if (!schonSicherungenDrin || !prepareMirrorDirectory(m_mirrorDirectory, &markeFehler)) {
            emit mirrorFailed(QStringLiteral("Zweiter Sicherungsordner ist nicht mehr da (Stick abgezogen?): %1 — "
                                             "es wird nur noch neben das Log gesichert")
                                  .arg(m_mirrorDirectory));
            return;
        }
    }
    const QString target = QDir(m_mirrorDirectory).filePath(QFileInfo(path).fileName());
    QFile::remove(target);
    if (!QFile::copy(path, target)) {
        emit mirrorFailed(QStringLiteral("Kopie nach %1 fehlgeschlagen").arg(m_mirrorDirectory));
        return;
    }
    pruneDirectory(m_mirrorDirectory);
}

QVector<LogBackup::Entry> LogBackup::listBackups(const QString& directory)
{
    QVector<Entry> entries;
    const QDir dir(directory);
    const QFileInfoList files = dir.entryInfoList({kPrefix + QLatin1Char('*') + kSuffix}, QDir::Files, QDir::Name | QDir::Reversed);
    for (const QFileInfo& info : files) {
        Entry entry;
        entry.path = info.absoluteFilePath();
        entry.bytes = info.size();
        const QString stamp = info.fileName().mid(kPrefix.size(), info.fileName().size() - kPrefix.size() - kSuffix.size());
        entry.utc = QDateTime::fromString(stamp, QStringLiteral("yyyyMMdd-HHmm"));
        if (entry.utc.isValid()) {
            entry.utc.setTimeZone(QTimeZone::UTC);
        } else {
            entry.utc = info.lastModified().toUTC();
        }
        entries.append(entry);
    }
    return entries;
}

void LogBackup::prune()
{
    pruneDirectory(m_directory);
}

void LogBackup::pruneDirectory(const QString& directory)
{
    // A copy a minute would be 1440 files a day: keep every one of the
    // last two hours (a mistake noticed soon is undone to the minute),
    // then the newest per ten minutes, and never more than kKeepFiles.
    QDir dir(directory);
    const QVector<Entry> entries = listBackups(directory); // newest first
    if (entries.isEmpty()) {
        return;
    }
    const QDateTime newest = entries.first().utc;
    QSet<qint64> bucketsKept;
    int kept = 0;
    for (const Entry& entry : entries) {
        const qint64 age = entry.utc.secsTo(newest);
        bool keep = false;
        if (age <= kKeepEveryCopySecs) {
            keep = true;
        } else {
            const qint64 bucket = entry.utc.toSecsSinceEpoch() / kThinnedBucketSecs;
            if (!bucketsKept.contains(bucket)) {
                bucketsKept.insert(bucket);
                keep = true;
            }
        }
        if (keep && kept < kKeepFiles) {
            ++kept;
            continue;
        }
        dir.remove(entry.path);
    }
}

} // namespace Contestprogramm
