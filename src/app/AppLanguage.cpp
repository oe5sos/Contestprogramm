#include "app/AppLanguage.h"

#include <QCoreApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QStringList>
#include <QTranslator>

namespace Contestprogramm {

bool installGermanQtTranslations(QCoreApplication& app)
{
    const QStringList candidates{
        QLibraryInfo::path(QLibraryInfo::TranslationsPath),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../Resources/translations"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/translations"),
    };
    // Der Übersetzer muss den Aufrufer überleben: QCoreApplication
    // merkt sich nur den Zeiger.
    static QTranslator translator;
    if (!translator.isEmpty()) {
        return true; // schon installiert (ein zweiter Aufruf im selben Prozess)
    }
    for (const QString& dir : candidates) {
        if (dir.isEmpty() || !QDir(dir).exists()) {
            continue;
        }
        if (translator.load(QStringLiteral("qtbase_de"), dir)) {
            return app.installTranslator(&translator);
        }
    }
    return false;
}

} // namespace Contestprogramm
