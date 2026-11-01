#include "core/HamlibBinary.h"

#include <QFileInfo>
#include <QStandardPaths>
#include <QStringList>

namespace Contestprogramm {

QString findHamlibBinary(const QString& name)
{
    // PATH first: an operator who installed Hamlib somewhere unusual
    // and put it on PATH has already answered this question.
    const QString onPath = QStandardPaths::findExecutable(name);
    if (!onPath.isEmpty()) {
        return onPath;
    }

    static const QStringList kDirs = {
        QStringLiteral("/opt/homebrew/bin"),   // Apple silicon Homebrew
        QStringLiteral("/usr/local/bin"),      // Intel Homebrew, and most else
        QStringLiteral("/opt/local/bin"),      // MacPorts
        QStringLiteral("/usr/bin"),
    };
    for (const QString& dir : kDirs) {
        const QString candidate = dir + QLatin1Char('/') + name;
        if (QFileInfo(candidate).isExecutable()) {
            return candidate;
        }
    }
    return {};
}

} // namespace Contestprogramm
