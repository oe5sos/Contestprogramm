#include "data/LogBackup.h"

#include "data/ContestDatabase.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
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
    return path;
}

void LogBackup::prune()
{
    QDir dir(m_directory);
    // Name order is time order (yyyyMMdd-HHmm), oldest first.
    const QStringList files = dir.entryList({kPrefix + QLatin1Char('*') + kSuffix}, QDir::Files, QDir::Name);
    for (int i = 0; i < files.size() - kKeepFiles; ++i) {
        dir.remove(files.at(i));
    }
}

} // namespace Contestprogramm
