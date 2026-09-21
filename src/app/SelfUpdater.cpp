#include "app/SelfUpdater.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QTextStream>

namespace Contestprogramm {

namespace {
const QString kFeedOverride = QStringLiteral("CONTESTPROGRAMM_UPDATE_FEED");
const QString kInstallerSuffix = QStringLiteral("-Windows-x64-setup.exe");
const QString kPortableSuffix = QStringLiteral("-Windows-x64-portable.zip");

QString runCommand(const QString& program, const QStringList& arguments, int timeoutMs, bool* ok)
{
    QProcess process;
    process.start(program, arguments);
    *ok = process.waitForStarted(5000) && process.waitForFinished(timeoutMs)
        && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    const QString err = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
    return err.isEmpty() ? QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed() : err;
}

bool isHexDigest(const QString& s)
{
    if (s.size() != 64) {
        return false;
    }
    for (const QChar c : s) {
        if (!c.isDigit() && !(c.toLower() >= QLatin1Char('a') && c.toLower() <= QLatin1Char('f'))) {
            return false;
        }
    }
    return true;
}
} // namespace

UpdateTarget UpdateTarget::detect(bool hasInstaller)
{
    UpdateTarget target;
    const QString arch = QSysInfo::currentCpuArchitecture();
#if defined(Q_OS_MACOS)
    Q_UNUSED(hasInstaller);
    // .../Contestprogramm.app/Contents/MacOS/Contestprogramm
    QDir dir(QCoreApplication::applicationDirPath());
    const bool inBundle = dir.dirName() == QLatin1String("MacOS") && dir.cdUp()
        && dir.dirName() == QLatin1String("Contents") && dir.cdUp()
        && dir.dirName().endsWith(QLatin1String(".app"));
    if (!inBundle) {
        target.reason = QStringLiteral("dieses Programm läuft nicht aus einem App-Paket (Entwicklungsbau)");
        return target;
    }
    const QString bundle = dir.absolutePath();
    if (!dir.cdUp() || !QFileInfo(dir.absolutePath()).isWritable()) {
        target.reason = QStringLiteral("der Ordner %1 ist nicht beschreibbar").arg(QDir::toNativeSeparators(dir.absolutePath()));
        return target;
    }
    target.kind = Kind::MacBundle;
    target.installPath = bundle;
    target.assetSuffix = arch == QLatin1String("arm64") ? QStringLiteral("-macOS-apple-silicon.dmg")
                                                          : QStringLiteral("-macOS-intel.dmg");
#elif defined(Q_OS_WIN)
    const QString exeDir = QCoreApplication::applicationDirPath();
    if (hasInstaller && QFileInfo::exists(exeDir + QStringLiteral("/uninstall.exe"))) {
        // Put there by the setup program: the next setup replaces it,
        // with its own elevation prompt.
        target.kind = Kind::WindowsInstalled;
        target.assetSuffix = kInstallerSuffix;
    } else if (!QFileInfo(exeDir).isWritable()) {
        target.reason = QStringLiteral("der Programmordner %1 ist nicht beschreibbar").arg(QDir::toNativeSeparators(exeDir));
        return target;
    } else {
        target.kind = Kind::WindowsPortable;
        target.assetSuffix = kPortableSuffix;
    }
    target.installPath = exeDir;
#elif defined(Q_OS_LINUX)
    Q_UNUSED(hasInstaller);
    const QString image = qEnvironmentVariable("APPIMAGE");
    if (image.isEmpty() || !QFileInfo::exists(image)) {
        target.reason = QStringLiteral("dieses Programm läuft nicht als AppImage");
        return target;
    }
    if (!QFileInfo(QFileInfo(image).absolutePath()).isWritable()) {
        target.reason = QStringLiteral("der Ordner %1 ist nicht beschreibbar").arg(QFileInfo(image).absolutePath());
        return target;
    }
    target.kind = Kind::LinuxAppImage;
    target.installPath = image;
    target.assetSuffix = arch == QLatin1String("arm64") ? QStringLiteral("-aarch64.AppImage")
                                                          : QStringLiteral("-x86_64.AppImage");
#else
    Q_UNUSED(hasInstaller);
    Q_UNUSED(arch);
    target.reason = QStringLiteral("auf diesem System nicht vorgesehen");
#endif
    return target;
}

SelfUpdater::SelfUpdater(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    m_network->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
}

void SelfUpdater::setRepository(const QString& repository)
{
    m_repository = repository;
}

void SelfUpdater::setCurrentVersion(const QVersionNumber& version)
{
    m_currentVersion = version;
}

void SelfUpdater::setTarget(const UpdateTarget& target)
{
    m_target = target;
}

QUrl SelfUpdater::feedUrl() const
{
    const QString override = qEnvironmentVariable(kFeedOverride.toLatin1().constData());
    if (!override.isEmpty()) {
        return QUrl(override);
    }
    return QUrl(QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(m_repository));
}

QNetworkReply* SelfUpdater::get(const QUrl& url)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Contestprogramm/%1").arg(m_currentVersion.toString()));
    request.setRawHeader("Accept", "application/vnd.github+json, application/octet-stream, */*");
    request.setTransferTimeout(30000);
    return m_network->get(request);
}

void SelfUpdater::check()
{
    QNetworkReply* reply = get(feedUrl());
    m_activeReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (m_activeReply == reply) {
            m_activeReply = nullptr;
        }
        if (reply->error() == QNetworkReply::OperationCanceledError) {
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            emit failed(QStringLiteral("GitHub ist nicht erreichbar: %1").arg(reply->errorString()));
            return;
        }
        // The version first, without asking for a package: an older or
        // equal release is "up to date" even when it has nothing for
        // this machine (Windows CI, 2026-09-21: the feed offered only a
        // DMG and the dialog reported a failure instead).
        const QByteArray json = reply->readAll();
        QString error;
        const auto latest = parseLatest(json, QString(), &error);
        if (!latest) {
            emit failed(error);
            return;
        }
        if (!isNewer(latest->version, m_currentVersion)) {
            emit upToDate(m_currentVersion);
            return;
        }
        const auto info = parseLatest(json, m_target.assetSuffix, &error);
        if (!info) {
            emit failed(error);
            return;
        }
        m_pending = *info;
        emit updateAvailable(*info);
    });
}

std::optional<ReleaseInfo> SelfUpdater::parseLatest(const QByteArray& json, const QString& assetSuffix, QString* error)
{
    const auto fail = [error](const QString& message) {
        if (error) {
            *error = message;
        }
        return std::nullopt;
    };
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (!doc.isObject()) {
        return fail(QStringLiteral("Die Antwort von GitHub ist nicht lesbar (%1).").arg(parseError.errorString()));
    }
    const QJsonObject release = doc.object();
    ReleaseInfo info;
    info.tag = release.value(QStringLiteral("tag_name")).toString();
    QString versionText = info.tag;
    if (versionText.startsWith(QLatin1Char('v'))) {
        versionText.remove(0, 1);
    }
    info.version = QVersionNumber::fromString(versionText);
    if (info.version.isNull()) {
        return fail(QStringLiteral("Das Release „%1“ trägt keinen Versionsstand.").arg(info.tag));
    }
    info.title = release.value(QStringLiteral("name")).toString();
    info.notes = release.value(QStringLiteral("body")).toString();
    info.pageUrl = QUrl(release.value(QStringLiteral("html_url")).toString());

    QHash<QString, QJsonObject> byName;
    const QJsonArray assets = release.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue& value : assets) {
        const QJsonObject asset = value.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString();
        byName.insert(name, asset);
        if (!assetSuffix.isEmpty() && info.assetName.isEmpty() && name.endsWith(assetSuffix, Qt::CaseInsensitive)) {
            info.assetName = name;
            info.assetUrl = QUrl(asset.value(QStringLiteral("browser_download_url")).toString());
            info.assetSize = static_cast<qint64>(asset.value(QStringLiteral("size")).toDouble());
        }
    }
    if (!assetSuffix.isEmpty() && info.assetName.isEmpty()) {
        return fail(QStringLiteral("Version %1 hat kein Paket „…%2“ für diesen Rechner.")
                        .arg(info.version.toString(), assetSuffix));
    }
    if (!info.assetName.isEmpty()) {
        const QString own = info.assetName + QStringLiteral(".sha256");
        if (byName.contains(own)) {
            info.checksumUrl = QUrl(byName.value(own).value(QStringLiteral("browser_download_url")).toString());
        } else {
            for (const QString& sums : {QStringLiteral("SHA256SUMS.txt"), QStringLiteral("SHA256SUMS"),
                                        QStringLiteral("sha256sums.txt")}) {
                if (byName.contains(sums)) {
                    info.checksumUrl = QUrl(byName.value(sums).value(QStringLiteral("browser_download_url")).toString());
                    break;
                }
            }
        }
    }
    return info;
}

bool SelfUpdater::isNewer(const QVersionNumber& latest, const QVersionNumber& current)
{
    return !latest.isNull() && (current.isNull() || QVersionNumber::compare(latest, current) > 0);
}

QString SelfUpdater::expectedSha256(const QByteArray& checksumText, const QString& assetName)
{
    const QString wanted = QFileInfo(assetName).fileName();
    const QStringList lines = QString::fromUtf8(checksumText).split(QLatin1Char('\n'));
    QString bare;
    int bareCount = 0;
    for (const QString& raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        const QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (parts.isEmpty() || !isHexDigest(parts.first())) {
            continue;
        }
        if (parts.size() == 1) {
            bare = parts.first();
            ++bareCount;
            continue;
        }
        QString name = parts.mid(1).join(QLatin1Char(' '));
        if (name.startsWith(QLatin1Char('*'))) {
            name.remove(0, 1);
        }
        if (QFileInfo(name).fileName() == wanted) {
            return parts.first().toLower();
        }
    }
    // A "<asset>.sha256" that names nothing: the one digest it holds.
    return bareCount == 1 ? bare.toLower() : QString();
}

QString SelfUpdater::sha256Of(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        return QString();
    }
    return QString::fromLatin1(hash.result().toHex());
}

QString SelfUpdater::downloadDirectory() const
{
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/Contestprogramm-Update");
}

void SelfUpdater::download(const ReleaseInfo& info)
{
    m_pending = info;
    if (!QDir().mkpath(downloadDirectory())) {
        emit failed(QStringLiteral("Der Ordner %1 lässt sich nicht anlegen.").arg(downloadDirectory()));
        return;
    }
    const QString path = downloadDirectory() + QLatin1Char('/') + info.assetName;
    auto* file = new QFile(path, this);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit failed(QStringLiteral("%1 lässt sich nicht schreiben: %2").arg(path, file->errorString()));
        file->deleteLater();
        return;
    }
    QNetworkReply* reply = get(info.assetUrl);
    m_activeReply = reply;
    connect(reply, &QNetworkReply::readyRead, this, [reply, file] { file->write(reply->readAll()); });
    connect(reply, &QNetworkReply::downloadProgress, this, &SelfUpdater::downloadProgress);
    connect(reply, &QNetworkReply::finished, this, [this, reply, file, path] {
        reply->deleteLater();
        if (m_activeReply == reply) {
            m_activeReply = nullptr;
        }
        file->write(reply->readAll());
        file->close();
        file->deleteLater();
        if (reply->error() == QNetworkReply::OperationCanceledError) {
            QFile::remove(path);
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            QFile::remove(path);
            emit failed(QStringLiteral("Der Download ist fehlgeschlagen: %1").arg(reply->errorString()));
            return;
        }
        const qint64 size = QFileInfo(path).size();
        if (m_pending.assetSize > 0 && size != m_pending.assetSize) {
            QFile::remove(path);
            emit failed(QStringLiteral("Der Download ist unvollständig (%1 von %2 Bytes).")
                            .arg(size).arg(m_pending.assetSize));
            return;
        }
        if (m_pending.checksumUrl.isEmpty()) {
            emit downloaded(path, false);
            return;
        }
        fetchChecksum(path);
    });
}

void SelfUpdater::fetchChecksum(const QString& packagePath)
{
    QNetworkReply* reply = get(m_pending.checksumUrl);
    m_activeReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, packagePath] {
        reply->deleteLater();
        if (m_activeReply == reply) {
            m_activeReply = nullptr;
        }
        if (reply->error() == QNetworkReply::OperationCanceledError) {
            QFile::remove(packagePath);
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            QFile::remove(packagePath);
            emit failed(QStringLiteral("Die Prüfsumme ließ sich nicht laden: %1").arg(reply->errorString()));
            return;
        }
        const QString expected = expectedSha256(reply->readAll(), m_pending.assetName);
        if (expected.isEmpty()) {
            QFile::remove(packagePath);
            emit failed(QStringLiteral("Die Prüfsummenliste des Release nennt %1 nicht.").arg(m_pending.assetName));
            return;
        }
        const QString actual = sha256Of(packagePath);
        if (actual != expected) {
            QFile::remove(packagePath);
            emit failed(QStringLiteral("Die Prüfsumme stimmt nicht -- der Download wurde verworfen."));
            return;
        }
        emit downloaded(packagePath, true);
    });
}

void SelfUpdater::cancel()
{
    if (m_activeReply) {
        m_activeReply->abort();
    }
}

bool SelfUpdater::install(const QString& packagePath, QString* error)
{
    switch (m_target.kind) {
    case UpdateTarget::Kind::MacBundle:
        return installMacBundle(packagePath, error);
    case UpdateTarget::Kind::WindowsPortable:
    case UpdateTarget::Kind::WindowsInstalled:
        return installWindows(packagePath, error);
    case UpdateTarget::Kind::LinuxAppImage:
        return installAppImage(packagePath, error);
    case UpdateTarget::Kind::None:
        break;
    }
    if (error) {
        *error = m_target.reason;
    }
    return false;
}

// The image is mounted out of sight, the program inside copied next to
// the running bundle (same volume, so the two renames below are
// atomic), then old and new swap places. The running process keeps its
// files -- macOS holds them open -- so the old bundle can go at once.
bool SelfUpdater::installMacBundle(const QString& dmgPath, QString* error)
{
    const auto fail = [error](const QString& message) {
        if (error) {
            *error = message;
        }
        return false;
    };
    const QFileInfo bundleInfo(m_target.installPath);
    const QString parent = bundleInfo.absolutePath();
    const QString stamp = QString::number(QCoreApplication::applicationPid());

    QTemporaryDir mount(QDir::tempPath() + QStringLiteral("/Contestprogramm-Abbild-XXXXXX"));
    if (!mount.isValid()) {
        return fail(QStringLiteral("Kein Platz für das Abbild in %1.").arg(QDir::tempPath()));
    }
    bool ok = false;
    QString output = runCommand(QStringLiteral("/usr/bin/hdiutil"),
                                {QStringLiteral("attach"), QStringLiteral("-nobrowse"), QStringLiteral("-noautoopen"),
                                 QStringLiteral("-readonly"), QStringLiteral("-quiet"), QStringLiteral("-mountpoint"),
                                 mount.path(), dmgPath},
                                60000, &ok);
    if (!ok) {
        return fail(QStringLiteral("Das Abbild lässt sich nicht öffnen: %1").arg(output));
    }
    const auto detach = [&mount] {
        bool detached = false;
        runCommand(QStringLiteral("/usr/bin/hdiutil"),
                   {QStringLiteral("detach"), QStringLiteral("-quiet"), QStringLiteral("-force"), mount.path()}, 30000, &detached);
    };
    const QStringList apps = QDir(mount.path()).entryList({QStringLiteral("*.app")}, QDir::Dirs | QDir::NoDotAndDotDot);
    if (apps.isEmpty()) {
        detach();
        return fail(QStringLiteral("Im Abbild ist kein Programm."));
    }
    const QString source = mount.path() + QLatin1Char('/') + apps.first();
    const QString staging = parent + QStringLiteral("/.") + bundleInfo.fileName() + QStringLiteral(".neu-") + stamp;
    QDir(staging).removeRecursively();
    output = runCommand(QStringLiteral("/usr/bin/ditto"), {source, staging}, 300000, &ok);
    detach();
    if (!ok) {
        QDir(staging).removeRecursively();
        return fail(QStringLiteral("Das Kopieren ist fehlgeschlagen: %1").arg(output));
    }
    const QString old = parent + QStringLiteral("/.") + bundleInfo.fileName() + QStringLiteral(".alt-") + stamp;
    QDir(old).removeRecursively();
    if (!QFile::rename(m_target.installPath, old)) {
        QDir(staging).removeRecursively();
        return fail(QStringLiteral("%1 lässt sich nicht ersetzen (Rechte?).").arg(bundleInfo.fileName()));
    }
    if (!QFile::rename(staging, m_target.installPath)) {
        QFile::rename(old, m_target.installPath);
        QDir(staging).removeRecursively();
        return fail(QStringLiteral("Das neue %1 lässt sich nicht an seinen Platz legen.").arg(bundleInfo.fileName()));
    }
    QDir(old).removeRecursively();
    QFile::remove(dmgPath);
    return true;
}

// Windows keeps a running .exe locked, so a small batch file does the
// swap after this process has ended (restart() starts it): a portable
// copy is unpacked here first and copied over by the script; an
// installed copy is handed to the new setup program, which brings its
// own elevation prompt.
bool SelfUpdater::installWindows(const QString& packagePath, QString* error)
{
    const auto fail = [error](const QString& message) {
        if (error) {
            *error = message;
        }
        return false;
    };
    const QString exeDir = QDir::toNativeSeparators(m_target.installPath);
    const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const QString exeName = QFileInfo(exe).fileName();
    const QString stamp = QString::number(QCoreApplication::applicationPid());
    const QString scriptPath = QDir::toNativeSeparators(downloadDirectory() + QStringLiteral("/aktualisieren-") + stamp
                                                        + QStringLiteral(".cmd"));
    QStringList lines;
    lines << QStringLiteral("@echo off");
    // The script is written in the ANSI code page (QStringConverter::
    // System below); cmd.exe would read it in the OEM one.
    lines << QStringLiteral("chcp 1252 >nul");
    lines << QStringLiteral(":warten");
    lines << QStringLiteral("tasklist /FI \"PID eq %1\" 2>nul | find \"%1\" >nul").arg(stamp);
    // ping as the one-second sleep: "timeout" refuses to run without a
    // console of its own.
    lines << QStringLiteral("if not errorlevel 1 (ping -n 2 127.0.0.1 >nul & goto warten)");
    if (m_target.kind == UpdateTarget::Kind::WindowsInstalled) {
        lines << QStringLiteral("start \"\" /wait \"%1\" /S").arg(QDir::toNativeSeparators(packagePath));
        lines << QStringLiteral("del \"%1\"").arg(QDir::toNativeSeparators(packagePath));
    } else {
        const QString staging = downloadDirectory() + QStringLiteral("/entpackt-") + stamp;
        QDir(staging).removeRecursively();
        if (!QDir().mkpath(staging)) {
            return fail(QStringLiteral("Der Ordner %1 lässt sich nicht anlegen.").arg(staging));
        }
        bool ok = false;
        // bsdtar ships with Windows 10 and later and reads ZIP.
        const QString output = runCommand(QStringLiteral("tar.exe"),
                                          {QStringLiteral("-xf"), QDir::toNativeSeparators(packagePath),
                                           QStringLiteral("-C"), QDir::toNativeSeparators(staging)},
                                          300000, &ok);
        if (!ok) {
            return fail(QStringLiteral("Das Paket lässt sich nicht entpacken: %1").arg(output));
        }
        QString source;
        QDirIterator it(staging, {exeName}, QDir::Files, QDirIterator::Subdirectories);
        if (it.hasNext()) {
            source = QFileInfo(it.next()).absolutePath();
        }
        if (source.isEmpty()) {
            return fail(QStringLiteral("Im Paket ist kein %1.").arg(exeName));
        }
        lines << QStringLiteral("robocopy \"%1\" \"%2\" /E /IS /IT /R:10 /W:1 /NFL /NDL /NJH /NJS >nul")
                     .arg(QDir::toNativeSeparators(source), exeDir);
        lines << QStringLiteral("rmdir /s /q \"%1\"").arg(QDir::toNativeSeparators(staging));
        lines << QStringLiteral("del \"%1\"").arg(QDir::toNativeSeparators(packagePath));
    }
    lines << QStringLiteral("start \"\" \"%1\"").arg(exe);
    lines << QStringLiteral("del \"%~f0\"");

    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return fail(QStringLiteral("%1 lässt sich nicht schreiben.").arg(scriptPath));
    }
    QTextStream out(&script);
    out.setEncoding(QStringConverter::System);
    for (const QString& line : lines) {
        out << line << "\r\n";
    }
    script.close();
    m_relaunchScript = scriptPath;
    return true;
}

// The new image is copied next to the old one (same file system) and
// renamed over it; a running AppImage keeps its mounted copy.
bool SelfUpdater::installAppImage(const QString& imagePath, QString* error)
{
    const auto fail = [error](const QString& message) {
        if (error) {
            *error = message;
        }
        return false;
    };
    const QString target = m_target.installPath;
    const QString staging = target + QStringLiteral(".neu");
    const QString old = target + QStringLiteral(".alt");
    QFile::remove(staging);
    if (!QFile::copy(imagePath, staging)) {
        return fail(QStringLiteral("%1 lässt sich nicht neben das laufende AppImage kopieren.").arg(imagePath));
    }
    QFile::setPermissions(staging, QFile::permissions(staging) | QFileDevice::ExeOwner | QFileDevice::ExeGroup
                                       | QFileDevice::ExeOther);
    QFile::remove(old);
    if (!QFile::rename(target, old)) {
        QFile::remove(staging);
        return fail(QStringLiteral("%1 lässt sich nicht ersetzen (Rechte?).").arg(target));
    }
    if (!QFile::rename(staging, target)) {
        QFile::rename(old, target);
        QFile::remove(staging);
        return fail(QStringLiteral("Das neue AppImage lässt sich nicht an seinen Platz legen."));
    }
    QFile::remove(old);
    QFile::remove(imagePath);
    return true;
}

void SelfUpdater::restart()
{
    const QString pid = QString::number(QCoreApplication::applicationPid());
#if defined(Q_OS_WIN)
    if (!m_relaunchScript.isEmpty()) {
        QProcess::startDetached(QStringLiteral("cmd.exe"), {QStringLiteral("/c"), m_relaunchScript});
    }
#elif defined(Q_OS_MACOS)
    QProcess::startDetached(QStringLiteral("/bin/sh"),
                            {QStringLiteral("-c"),
                             QStringLiteral("while kill -0 \"$1\" 2>/dev/null; do sleep 0.2; done; exec /usr/bin/open \"$2\""),
                             QStringLiteral("neustart"), pid, m_target.installPath});
#else
    QProcess::startDetached(QStringLiteral("/bin/sh"),
                            {QStringLiteral("-c"),
                             QStringLiteral("while kill -0 \"$1\" 2>/dev/null; do sleep 0.2; done; exec \"$2\""),
                             QStringLiteral("neustart"), pid, m_target.installPath});
#endif
    QCoreApplication::quit();
}

} // namespace Contestprogramm
