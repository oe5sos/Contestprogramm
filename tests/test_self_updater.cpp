#include <QtTest>

#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include "app/SelfUpdater.h"
#include "ui/UpdateDialog.h"
#include <QLabel>
#include <QPushButton>

using namespace Contestprogramm;

namespace {

QByteArray releaseJson(const QString& tag, const QStringList& assetNames, const QString& base = QStringLiteral("https://dl.example/"))
{
    QJsonArray assets;
    for (const QString& name : assetNames) {
        QJsonObject asset;
        asset.insert(QStringLiteral("name"), name);
        asset.insert(QStringLiteral("size"), 12345);
        asset.insert(QStringLiteral("browser_download_url"), base + name);
        assets.append(asset);
    }
    QJsonObject release;
    release.insert(QStringLiteral("tag_name"), tag);
    release.insert(QStringLiteral("name"), QStringLiteral("Contestprogramm %1").arg(tag.mid(1)));
    release.insert(QStringLiteral("body"), QStringLiteral("- Neu: alles"));
    release.insert(QStringLiteral("html_url"), QStringLiteral("https://github.com/oe5sos/Contestprogramm/releases/tag/") + tag);
    release.insert(QStringLiteral("assets"), assets);
    return QJsonDocument(release).toJson();
}

// A GitHub stand-in on 127.0.0.1: answers GET by path out of a map.
class FakeReleaseServer : public QObject
{
public:
    explicit FakeReleaseServer(QObject* parent = nullptr)
        : QObject(parent)
    {
        m_server.listen(QHostAddress::LocalHost, 0);
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* client = m_server.nextPendingConnection()) {
                connect(client, &QTcpSocket::readyRead, this, [this, client] {
                    const QByteArray request = client->readAll();
                    const QByteArray path = request.split(' ').value(1);
                    ++m_hits[QString::fromLatin1(path)];
                    QByteArray body = m_files.value(QString::fromLatin1(path));
                    QByteArray head = m_files.contains(QString::fromLatin1(path)) ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 404 Not Found\r\n";
                    head += "Content-Type: application/octet-stream\r\nConnection: close\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n\r\n";
                    client->write(head + body);
                    client->disconnectFromHost();
                });
                connect(client, &QTcpSocket::disconnected, client, &QObject::deleteLater);
            }
        });
    }

    QString url(const QString& path) const
    {
        return QStringLiteral("http://127.0.0.1:%1%2").arg(m_server.serverPort()).arg(path);
    }
    void put(const QString& path, const QByteArray& body) { m_files.insert(path, body); }
    int hits(const QString& path) const { return m_hits.value(path); }

private:
    QTcpServer m_server;
    QHash<QString, QByteArray> m_files;
    QHash<QString, int> m_hits;
};

} // namespace

// app/SelfUpdater.h: Hilfe > Auf neueste Version aktualisieren.
class TestSelfUpdater : public QObject
{
    Q_OBJECT

private slots:
    void parseLatestPicksTheAssetForThisMachineAndItsChecksum();
    void parseLatestFallsBackToTheSumsListAndToNoChecksum();
    void parseLatestWithoutASuffixStillReportsTheVersion();
    void parseLatestRejectsGarbageAndAMissingPackage();
    void isNewerComparesVersions();
    void expectedSha256ReadsEveryListShape();
    void sha256OfHashesAFile();
    void detectKnowsADevelopmentBuildCannotUpdateItself();
    void checkAndDownloadAgainstALocalServer();
    void aWrongChecksumDiscardsTheDownload();
    void anOlderOrEqualReleaseIsUpToDate();
    void dialogReportsAnUpToDateCopy();
    void dialogOffersANewerRelease();
    void installSwapsABundleFromARealDmg();
};

void TestSelfUpdater::parseLatestPicksTheAssetForThisMachineAndItsChecksum()
{
    const QByteArray json = releaseJson(QStringLiteral("v0.2.0"),
                                        {QStringLiteral("Contestprogramm-0.2.0-macOS-intel.dmg"),
                                         QStringLiteral("Contestprogramm-0.2.0-macOS-apple-silicon.dmg"),
                                         QStringLiteral("Contestprogramm-0.2.0-macOS-apple-silicon.dmg.sha256"),
                                         QStringLiteral("Contestprogramm-0.2.0-Windows-x64-portable.zip")});
    QString error;
    const auto info = SelfUpdater::parseLatest(json, QStringLiteral("-macOS-apple-silicon.dmg"), &error);
    QVERIFY2(info.has_value(), qPrintable(error));
    QCOMPARE(info->version, QVersionNumber(0, 2, 0));
    QCOMPARE(info->tag, QStringLiteral("v0.2.0"));
    QCOMPARE(info->assetName, QStringLiteral("Contestprogramm-0.2.0-macOS-apple-silicon.dmg"));
    QCOMPARE(info->assetUrl, QUrl(QStringLiteral("https://dl.example/Contestprogramm-0.2.0-macOS-apple-silicon.dmg")));
    QCOMPARE(info->assetSize, 12345);
    QCOMPARE(info->checksumUrl, QUrl(QStringLiteral("https://dl.example/Contestprogramm-0.2.0-macOS-apple-silicon.dmg.sha256")));
    QCOMPARE(info->notes, QStringLiteral("- Neu: alles"));
    QVERIFY(info->pageUrl.isValid());
}

void TestSelfUpdater::parseLatestFallsBackToTheSumsListAndToNoChecksum()
{
    const auto withSums = SelfUpdater::parseLatest(
        releaseJson(QStringLiteral("v0.6.4"), {QStringLiteral("Longpath-0.6.4-x86_64.AppImage"), QStringLiteral("SHA256SUMS.txt")}),
        QStringLiteral("-x86_64.AppImage"));
    QVERIFY(withSums.has_value());
    QCOMPARE(withSums->checksumUrl, QUrl(QStringLiteral("https://dl.example/SHA256SUMS.txt")));

    const auto without = SelfUpdater::parseLatest(
        releaseJson(QStringLiteral("v0.6.4"), {QStringLiteral("Longpath-0.6.4-x86_64.AppImage")}),
        QStringLiteral("-x86_64.AppImage"));
    QVERIFY(without.has_value());
    QVERIFY(without->checksumUrl.isEmpty());
}

void TestSelfUpdater::parseLatestWithoutASuffixStillReportsTheVersion()
{
    // A copy that cannot update itself (development build) still learns
    // that a newer version exists.
    const auto info = SelfUpdater::parseLatest(releaseJson(QStringLiteral("v1.0.0"), {}), QString());
    QVERIFY(info.has_value());
    QCOMPARE(info->version, QVersionNumber(1, 0, 0));
    QVERIFY(info->assetName.isEmpty());
}

void TestSelfUpdater::parseLatestRejectsGarbageAndAMissingPackage()
{
    QString error;
    QVERIFY(!SelfUpdater::parseLatest("<html>", QStringLiteral("-macOS-intel.dmg"), &error).has_value());
    QVERIFY(!error.isEmpty());

    error.clear();
    QVERIFY(!SelfUpdater::parseLatest(releaseJson(QStringLiteral("v0.2.0"), {QStringLiteral("Contestprogramm-0.2.0-macOS-intel.dmg")}),
                                      QStringLiteral("-x86_64.AppImage"), &error)
                 .has_value());
    QVERIFY2(error.contains(QStringLiteral("0.2.0")) && error.contains(QStringLiteral("-x86_64.AppImage")), qPrintable(error));

    error.clear();
    QVERIFY(!SelfUpdater::parseLatest(releaseJson(QStringLiteral("nightly"), {}), QString(), &error).has_value());
    QVERIFY(error.contains(QStringLiteral("nightly")));
}

void TestSelfUpdater::isNewerComparesVersions()
{
    QVERIFY(SelfUpdater::isNewer(QVersionNumber(0, 1, 1), QVersionNumber(0, 1, 0)));
    QVERIFY(SelfUpdater::isNewer(QVersionNumber(1, 0), QVersionNumber(0, 9, 9)));
    QVERIFY(!SelfUpdater::isNewer(QVersionNumber(0, 1, 0), QVersionNumber(0, 1, 0)));
    QVERIFY(!SelfUpdater::isNewer(QVersionNumber(0, 0, 9), QVersionNumber(0, 1, 0)));
    QVERIFY(!SelfUpdater::isNewer(QVersionNumber(), QVersionNumber(0, 1, 0)));
    // An unknown own version never blocks an update.
    QVERIFY(SelfUpdater::isNewer(QVersionNumber(0, 1, 0), QVersionNumber()));
}

void TestSelfUpdater::expectedSha256ReadsEveryListShape()
{
    const QString hexA = QString(64, QLatin1Char('a'));
    const QString hexB = QString(64, QLatin1Char('b'));
    const QByteArray sums = QStringLiteral("%1  Longpath-0.6.4-macOS-intel.dmg\n%2 *Longpath-0.6.4-x86_64.AppImage\n")
                                .arg(hexA, hexB.toUpper())
                                .toUtf8();
    QCOMPARE(SelfUpdater::expectedSha256(sums, QStringLiteral("Longpath-0.6.4-x86_64.AppImage")), hexB);
    QCOMPARE(SelfUpdater::expectedSha256(sums, QStringLiteral("Longpath-0.6.4-macOS-intel.dmg")), hexA);
    QVERIFY(SelfUpdater::expectedSha256(sums, QStringLiteral("Longpath-0.6.4-Windows-x64-setup.exe")).isEmpty());
    // The single-file ".sha256" beside a DMG, with or without the name.
    QCOMPARE(SelfUpdater::expectedSha256(QStringLiteral("%1  Contestprogramm-0.1.0-macOS-apple-silicon.dmg\n").arg(hexA).toUtf8(),
                                         QStringLiteral("Contestprogramm-0.1.0-macOS-apple-silicon.dmg")),
             hexA);
    QCOMPARE(SelfUpdater::expectedSha256(hexA.toUtf8() + "\n", QStringLiteral("whatever.dmg")), hexA);
    QVERIFY(SelfUpdater::expectedSha256("not a digest", QStringLiteral("whatever.dmg")).isEmpty());
}

void TestSelfUpdater::sha256OfHashesAFile()
{
    QTemporaryFile file;
    QVERIFY(file.open());
    file.write("abc");
    file.close();
    QCOMPARE(SelfUpdater::sha256Of(file.fileName()),
             QStringLiteral("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    QVERIFY(SelfUpdater::sha256Of(QStringLiteral("/nirgendwo/nichts")).isEmpty());
}

void TestSelfUpdater::detectKnowsADevelopmentBuildCannotUpdateItself()
{
    const UpdateTarget target = UpdateTarget::detect();
#if defined(Q_OS_WIN)
    // Any writable directory with the .exe in it is a portable copy.
    QCOMPARE(target.kind, UpdateTarget::Kind::WindowsPortable);
    QVERIFY(target.assetSuffix.endsWith(QStringLiteral(".zip")));
    QVERIFY(!target.installPath.isEmpty());
#else
    // The test binary is no .app bundle and no AppImage.
    QVERIFY(!target.usable());
    QVERIFY(!target.reason.isEmpty());
#endif
}

void TestSelfUpdater::checkAndDownloadAgainstALocalServer()
{
    FakeReleaseServer server;
    const QByteArray package = QByteArray("ein Paket").repeated(1000);
    const QString hex = QString::fromLatin1(QCryptographicHash::hash(package, QCryptographicHash::Sha256).toHex());
    QJsonArray assets;
    for (const QString& name : {QStringLiteral("Contestprogramm-0.9.0-macOS-apple-silicon.dmg"),
                                QStringLiteral("Contestprogramm-0.9.0-macOS-apple-silicon.dmg.sha256")}) {
        QJsonObject asset;
        asset.insert(QStringLiteral("name"), name);
        asset.insert(QStringLiteral("size"), name.endsWith(QStringLiteral(".sha256")) ? 100 : package.size());
        asset.insert(QStringLiteral("browser_download_url"), server.url(QStringLiteral("/dl/") + name));
        assets.append(asset);
    }
    QJsonObject release;
    release.insert(QStringLiteral("tag_name"), QStringLiteral("v0.9.0"));
    release.insert(QStringLiteral("assets"), assets);
    server.put(QStringLiteral("/latest"), QJsonDocument(release).toJson());
    server.put(QStringLiteral("/dl/Contestprogramm-0.9.0-macOS-apple-silicon.dmg"), package);
    server.put(QStringLiteral("/dl/Contestprogramm-0.9.0-macOS-apple-silicon.dmg.sha256"),
               (hex + QStringLiteral("  Contestprogramm-0.9.0-macOS-apple-silicon.dmg\n")).toUtf8());
    qputenv("CONTESTPROGRAMM_UPDATE_FEED", server.url(QStringLiteral("/latest")).toUtf8());

    SelfUpdater updater;
    updater.setRepository(QStringLiteral("oe5sos/Contestprogramm"));
    updater.setCurrentVersion(QVersionNumber(0, 1, 0));
    UpdateTarget target;
    target.kind = UpdateTarget::Kind::MacBundle;
    target.assetSuffix = QStringLiteral("-macOS-apple-silicon.dmg");
    updater.setTarget(target);
    QSignalSpy available(&updater, &SelfUpdater::updateAvailable);
    QSignalSpy upToDate(&updater, &SelfUpdater::upToDate);
    QSignalSpy failed(&updater, &SelfUpdater::failed);
    QSignalSpy downloaded(&updater, &SelfUpdater::downloaded);
    QSignalSpy progress(&updater, &SelfUpdater::downloadProgress);

    updater.check();
    QTRY_COMPARE(available.count() + failed.count() + upToDate.count(), 1);
    QVERIFY2(failed.isEmpty(), qPrintable(failed.isEmpty() ? QString() : failed.first().first().toString()));
    QCOMPARE(available.count(), 1);
    const auto info = available.first().first().value<ReleaseInfo>();
    QCOMPARE(info.version, QVersionNumber(0, 9, 0));

    updater.download(info);
    QTRY_COMPARE(downloaded.count() + failed.count(), 1);
    QVERIFY2(failed.isEmpty(), qPrintable(failed.isEmpty() ? QString() : failed.first().first().toString()));
    const QString path = downloaded.first().at(0).toString();
    QCOMPARE(downloaded.first().at(1).toBool(), true);
    QVERIFY(QFileInfo::exists(path));
    QCOMPARE(QFileInfo(path).fileName(), QStringLiteral("Contestprogramm-0.9.0-macOS-apple-silicon.dmg"));
    QCOMPARE(QFileInfo(path).size(), package.size());
    QVERIFY(progress.count() >= 1);
    QCOMPARE(server.hits(QStringLiteral("/dl/Contestprogramm-0.9.0-macOS-apple-silicon.dmg.sha256")), 1);
    QFile::remove(path);
    qunsetenv("CONTESTPROGRAMM_UPDATE_FEED");
}

void TestSelfUpdater::aWrongChecksumDiscardsTheDownload()
{
    FakeReleaseServer server;
    const QByteArray package = QByteArray("ein Paket");
    QJsonArray assets;
    for (const QString& name : {QStringLiteral("Contestprogramm-0.9.0-x86_64.AppImage"), QStringLiteral("SHA256SUMS.txt")}) {
        QJsonObject asset;
        asset.insert(QStringLiteral("name"), name);
        asset.insert(QStringLiteral("size"), name.endsWith(QStringLiteral(".txt")) ? 100 : package.size());
        asset.insert(QStringLiteral("browser_download_url"), server.url(QStringLiteral("/dl/") + name));
        assets.append(asset);
    }
    QJsonObject release;
    release.insert(QStringLiteral("tag_name"), QStringLiteral("v0.9.0"));
    release.insert(QStringLiteral("assets"), assets);
    server.put(QStringLiteral("/latest"), QJsonDocument(release).toJson());
    server.put(QStringLiteral("/dl/Contestprogramm-0.9.0-x86_64.AppImage"), package);
    server.put(QStringLiteral("/dl/SHA256SUMS.txt"),
               (QString(64, QLatin1Char('0')) + QStringLiteral("  Contestprogramm-0.9.0-x86_64.AppImage\n")).toUtf8());
    qputenv("CONTESTPROGRAMM_UPDATE_FEED", server.url(QStringLiteral("/latest")).toUtf8());

    SelfUpdater updater;
    updater.setCurrentVersion(QVersionNumber(0, 1, 0));
    UpdateTarget target;
    target.kind = UpdateTarget::Kind::LinuxAppImage;
    target.assetSuffix = QStringLiteral("-x86_64.AppImage");
    updater.setTarget(target);
    QSignalSpy available(&updater, &SelfUpdater::updateAvailable);
    QSignalSpy failed(&updater, &SelfUpdater::failed);
    QSignalSpy downloaded(&updater, &SelfUpdater::downloaded);
    updater.check();
    QTRY_COMPARE(available.count(), 1);
    updater.download(available.first().first().value<ReleaseInfo>());
    QTRY_COMPARE(downloaded.count() + failed.count(), 1);
    QCOMPARE(downloaded.count(), 0);
    QVERIFY(failed.first().first().toString().contains(QStringLiteral("Prüfsumme")));
    const QString path = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + QStringLiteral("/Contestprogramm-Update/Contestprogramm-0.9.0-x86_64.AppImage");
    QVERIFY(!QFileInfo::exists(path));
    qunsetenv("CONTESTPROGRAMM_UPDATE_FEED");
}

void TestSelfUpdater::anOlderOrEqualReleaseIsUpToDate()
{
    FakeReleaseServer server;
    server.put(QStringLiteral("/latest"), releaseJson(QStringLiteral("v0.1.0"), {QStringLiteral("Contestprogramm-0.1.0-macOS-intel.dmg")}));
    qputenv("CONTESTPROGRAMM_UPDATE_FEED", server.url(QStringLiteral("/latest")).toUtf8());
    SelfUpdater updater;
    updater.setCurrentVersion(QVersionNumber(0, 1, 0));
    UpdateTarget target;
    target.kind = UpdateTarget::Kind::LinuxAppImage;
    target.assetSuffix = QStringLiteral("-x86_64.AppImage"); // not in that release -- irrelevant when nothing is newer
    updater.setTarget(target);
    QSignalSpy upToDate(&updater, &SelfUpdater::upToDate);
    QSignalSpy available(&updater, &SelfUpdater::updateAvailable);
    QSignalSpy failed(&updater, &SelfUpdater::failed);
    updater.check();
    QTRY_COMPARE(upToDate.count() + available.count() + failed.count(), 1);
    QCOMPARE(upToDate.count(), 1);
    qunsetenv("CONTESTPROGRAMM_UPDATE_FEED");
}

// The window itself: it starts looking as soon as it is shown and
// puts the answer into its status label.
void TestSelfUpdater::dialogReportsAnUpToDateCopy()
{
    FakeReleaseServer server;
    // Deliberately a package for another machine: an older release is
    // "up to date" whatever it ships.
    server.put(QStringLiteral("/latest"), releaseJson(QStringLiteral("v0.0.1"), {QStringLiteral("Contestprogramm-0.0.1-macOS-intel.dmg")}));
    qputenv("CONTESTPROGRAMM_UPDATE_FEED", server.url(QStringLiteral("/latest")).toUtf8());
    UpdateDialog dialog;
    dialog.show();
    auto* status = dialog.findChild<QLabel*>(QStringLiteral("updateStatus"));
    QVERIFY(status);
    QTRY_VERIFY(status->text().contains(QStringLiteral("ist aktuell")));
    QVERIFY(!dialog.findChild<QPushButton*>(QStringLiteral("updatePrimary"))->isVisible());
    qunsetenv("CONTESTPROGRAMM_UPDATE_FEED");
}

void TestSelfUpdater::dialogOffersANewerRelease()
{
    FakeReleaseServer server;
    const QString suffix = UpdateTarget::detect().assetSuffix;
    server.put(QStringLiteral("/latest"),
               releaseJson(QStringLiteral("v99.0.0"),
                           {QStringLiteral("Contestprogramm-99.0.0") + (suffix.isEmpty() ? QStringLiteral("-macOS-intel.dmg") : suffix)}));
    qputenv("CONTESTPROGRAMM_UPDATE_FEED", server.url(QStringLiteral("/latest")).toUtf8());
    UpdateDialog dialog;
    dialog.show();
    auto* status = dialog.findChild<QLabel*>(QStringLiteral("updateStatus"));
    QVERIFY(status);
    QTRY_VERIFY(status->text().contains(QStringLiteral("99.0.0")));
    // A copy that can replace itself gets the button; a development
    // build (this test binary on macOS/Linux) gets the reason instead.
    auto* primary = dialog.findChild<QPushButton*>(QStringLiteral("updatePrimary"));
    QVERIFY(primary);
    QCOMPARE(primary->isVisible(), UpdateTarget::detect().usable());
    if (!UpdateTarget::detect().usable()) {
        QVERIFY(status->text().contains(UpdateTarget::detect().reason.toHtmlEscaped()) || status->text().contains(UpdateTarget::detect().reason));
    }
    qunsetenv("CONTESTPROGRAMM_UPDATE_FEED");
}

// The macOS swap against a real disk image -- hdiutil, ditto, the two
// renames -- needs a DMG and a bundle it may replace, so it runs only
// when CONTESTPROGRAMM_UPDATER_LIVE="<dmg>:<bundle>" names them (a scratch copy of the
// build, never the real installation).
void TestSelfUpdater::installSwapsABundleFromARealDmg()
{
#if !defined(Q_OS_MACOS)
    QSKIP("macOS only");
#else
    const QStringList spec = qEnvironmentVariable("CONTESTPROGRAMM_UPDATER_LIVE").split(QLatin1Char(':'), Qt::SkipEmptyParts);
    if (spec.size() != 2) {
        QSKIP("set CONTESTPROGRAMM_UPDATER_LIVE=<dmg>:<bundle> to run the live swap");
    }
    const QString dmg = spec.at(0);
    const QString bundle = spec.at(1);
    QVERIFY(QFileInfo::exists(dmg));
    QVERIFY(QFileInfo(bundle).isDir() && bundle.endsWith(QStringLiteral(".app")));
    const QString marker = bundle + QStringLiteral("/Contents/Resources/live-swap-marker.txt");
    QFile markerFile(marker);
    QVERIFY(markerFile.open(QIODevice::WriteOnly));
    markerFile.write("old");
    markerFile.close();

    SelfUpdater updater;
    UpdateTarget target;
    target.kind = UpdateTarget::Kind::MacBundle;
    target.installPath = bundle;
    updater.setTarget(target);
    QString error;
    QVERIFY2(updater.install(dmg, &error), qPrintable(error));
    // The bundle at the same path is now the one from the image: the
    // marker written into the old copy is gone, nothing is left behind.
    QVERIFY(QFileInfo(bundle).isDir());
    QVERIFY(!QFileInfo::exists(marker));
    QVERIFY(QFileInfo::exists(bundle + QStringLiteral("/Contents/Info.plist")));
    const QDir parent = QFileInfo(bundle).dir();
    QVERIFY(parent.entryList({QStringLiteral(".*.app.*")}, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty());
    QVERIFY(!QFileInfo::exists(dmg)); // consumed
#endif
}

QTEST_MAIN(TestSelfUpdater)
#include "test_self_updater.moc"
