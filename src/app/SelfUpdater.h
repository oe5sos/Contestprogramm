#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVersionNumber>

#include <optional>

class QNetworkAccessManager;
class QNetworkReply;

namespace Contestprogramm {

// A release on GitHub, reduced to what the updater needs.
struct ReleaseInfo {
    QVersionNumber version;
    QString tag;
    QString title;
    QString notes;        // the release body, Markdown
    QUrl pageUrl;
    QString assetName;    // the package for this machine
    QUrl assetUrl;
    qint64 assetSize = 0;
    QUrl checksumUrl;     // "<asset>.sha256" or a SHA256SUMS.txt; empty when the release has none
};

// Where this program runs from, and which package replaces it.
struct UpdateTarget {
    enum class Kind { None, MacBundle, WindowsPortable, WindowsInstalled, LinuxAppImage };
    Kind kind = Kind::None;
    QString assetSuffix;  // "-macOS-apple-silicon.dmg", "-Windows-x64-portable.zip", "-x86_64.AppImage", ...
    QString installPath;  // the .app bundle, the directory of the .exe, the AppImage file
    QString reason;       // when kind == None: why this copy cannot update itself

    bool usable() const { return kind != Kind::None; }
    // hasInstaller: the release also carries a "-Windows-x64-setup.exe",
    // and a copy with an uninstall.exe beside it was installed by one.
    static UpdateTarget detect(bool hasInstaller = false);
};

// "Hilfe > Auf neueste Version aktualisieren" (operator, 2026-09-21):
// fetch the latest release from GitHub, download the package for this
// machine, check its SHA-256, put it in place of the running copy and
// start again. The pure parts (parseLatest, expectedSha256, sha256Of)
// are what the tests cover; check()/download() talk to the network,
// install() to the file system.
class SelfUpdater : public QObject {
    Q_OBJECT

public:
    explicit SelfUpdater(QObject* parent = nullptr);

    // "owner/name" on GitHub. CONTESTPROGRAMM_UPDATE_FEED in the
    // environment replaces the releases/latest URL (the tests, a dry
    // run against a local server).
    void setRepository(const QString& repository);
    void setCurrentVersion(const QVersionNumber& version);
    void setTarget(const UpdateTarget& target);
    const UpdateTarget& target() const { return m_target; }

    QUrl feedUrl() const;

    // -> updateAvailable / upToDate / failed
    void check();
    // -> downloadProgress ... downloaded / failed. The checksum is
    // fetched and compared when the release carries one.
    void download(const ReleaseInfo& info);
    void cancel();
    // Puts the downloaded package in place of the running copy.
    bool install(const QString& packagePath, QString* error);
    // Arms the relaunch (it waits for this process to end) and quits.
    void restart();
    // Windows: the .cmd install() wrote, which restart() hands to cmd.exe
    // (empty elsewhere). The tests run it with the wait and the start
    // taken out.
    QString relaunchScriptPath() const { return m_relaunchScript; }

    static std::optional<ReleaseInfo> parseLatest(const QByteArray& json, const QString& assetSuffix,
                                                  QString* error = nullptr);
    static bool isNewer(const QVersionNumber& latest, const QVersionNumber& current);
    // The hex digest for assetName out of a "<hex>  <name>" list (one
    // line per file, or a single bare digest); empty when absent.
    static QString expectedSha256(const QByteArray& checksumText, const QString& assetName);
    static QString sha256Of(const QString& filePath);

signals:
    void updateAvailable(const Contestprogramm::ReleaseInfo& info);
    void upToDate(const QVersionNumber& current);
    void downloadProgress(qint64 received, qint64 total);
    // verified: the SHA-256 matched; false when the release had no checksum.
    void downloaded(const QString& packagePath, bool verified);
    void failed(const QString& message);

private:
    QNetworkReply* get(const QUrl& url);
    void fetchChecksum(const QString& packagePath);
    bool installMacBundle(const QString& dmgPath, QString* error);
    bool installWindows(const QString& packagePath, QString* error);
    bool installAppImage(const QString& imagePath, QString* error);
    QString downloadDirectory() const;

    QString m_repository;
    QVersionNumber m_currentVersion;
    UpdateTarget m_target;
    ReleaseInfo m_pending;
    QNetworkAccessManager* m_network = nullptr;
    QNetworkReply* m_activeReply = nullptr;
    QString m_relaunchScript;   // Windows: the .cmd that copies and restarts
};

} // namespace Contestprogramm

Q_DECLARE_METATYPE(Contestprogramm::ReleaseInfo)
