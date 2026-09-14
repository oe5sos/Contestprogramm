#include "app/AppController.h"
#include "ui/MainWindow.h"
#include "ui/StyleKit.h"

#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QStandardPaths>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Contestprogramm"));
    QApplication::setOrganizationName(QStringLiteral("Contestprogramm"));

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

    return app.exec();
}
