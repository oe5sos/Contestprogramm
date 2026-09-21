#include "app/AppActivation.h"
#include "app/AppController.h"
#include "app/SingleInstanceGuard.h"
#include "ui/MainWindow.h"
#include "ui/StyleKit.h"

#include "BuildInfo.h"

#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Contestprogramm"));
    QApplication::setOrganizationName(QStringLiteral("Contestprogramm"));
    QApplication::setApplicationVersion(QStringLiteral(CONTESTPROGRAMM_VERSION));

    // Override hook for isolated test runs -- bench-found 2026-09-14: a
    // launch with HOME repointed at a scratch directory still opened the
    // real ~/Library/Application Support database (confirmed via lsof),
    // because QStandardPaths::AppDataLocation resolves the OS-level home
    // directory, not the $HOME env var, for a launched .app bundle on
    // this platform -- a genuinely isolated dry run needs a more direct
    // override than HOME. CONTESTPROGRAMM_DATA_DIR is opt-in (unset in
    // any normal launch) and takes priority over QStandardPaths when
    // present.
    const QString dataDirOverride = QProcessEnvironment::systemEnvironment().value(
        QStringLiteral("CONTESTPROGRAMM_DATA_DIR"));
    const QString dataDir = !dataDirOverride.isEmpty()
        ? dataDirOverride
        : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!QDir().mkpath(dataDir)) {
        QMessageBox::critical(nullptr, QStringLiteral("Contestprogramm"),
                               QStringLiteral("Konnte Datenverzeichnis nicht anlegen:\n%1").arg(dataDir));
        return 1;
    }
    const QString dbPath = dataDir + QStringLiteral("/contestprogramm.sqlite");

    // One instance per data directory: a second start hands over to
    // the running one (see app/SingleInstanceGuard.h).
    Contestprogramm::SingleInstanceGuard instanceGuard(dataDir);
    if (!instanceGuard.tryAcquire()) {
        return 0;
    }

    Contestprogramm::AppController appController;
    QString openError;
    if (!appController.openDatabase(dbPath, &openError)) {
        QMessageBox::critical(nullptr, QStringLiteral("Contestprogramm"),
                               QStringLiteral("Datenbank konnte nicht geöffnet werden:\n%1\n%2").arg(dbPath, openError));
        return 1;
    }

    // House-style visual pass: the real Longpath dark palette (or
    // whichever of the operator's selectable colour themes was last
    // saved, see ContestSettings::colorTheme/core/ColorTheme.h),
    // applied app-wide so every dialog (including ones not individually
    // touched in this pass, e.g. SettingsDialog) picks it up. Must come
    // AFTER openDatabase() above -- the saved theme choice only exists
    // once settings have actually loaded. See ui/StyleKit.h/.cpp.
    Contestprogramm::Style::setActiveTheme(appController.settings().colorTheme);
    app.setStyleSheet(Contestprogramm::Style::appStyleSheet());

    Contestprogramm::MainWindow window(appController);
    window.show();
    // Sicherung wiederherstellen: the database file was replaced under
    // a closed connection; the cleanest way onto it is a fresh process.
    // The instance lock goes first, or the new start would hand over to
    // this dying one and quit.
    const auto restart = [&app, &instanceGuard] {
        instanceGuard.release();
        QProcess::startDetached(QCoreApplication::applicationFilePath(), QCoreApplication::arguments().mid(1),
                                QDir::currentPath());
        app.quit();
    };
    QObject::connect(&window, &Contestprogramm::MainWindow::restartRequested, &app, restart);
    // Built and started while this one is running: the start handed
    // over here (single instance), but with a different build stamp --
    // the operator wants the NEW program, not the old window raised
    // (2026-09-21). Restart into the rebuilt binary.
    QObject::connect(&instanceGuard, &Contestprogramm::SingleInstanceGuard::newerBuildStarted, &app, restart);
    QObject::connect(&instanceGuard, &Contestprogramm::SingleInstanceGuard::activateRequested, &window, [&window] {
        window.setWindowState((window.windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
        window.show();
        window.raise();
        Contestprogramm::activateThisApplication();
        window.activateWindow();
    });

    return app.exec();
}
