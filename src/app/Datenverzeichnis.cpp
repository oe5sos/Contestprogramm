#include "app/Datenverzeichnis.h"

#include <QProcessEnvironment>
#include <QStandardPaths>

namespace Contestprogramm {

QString standardDatenverzeichnis()
{
    const QString vorgabe = QProcessEnvironment::systemEnvironment().value(
        QStringLiteral("CONTESTPROGRAMM_DATA_DIR"));
    if (!vorgabe.isEmpty()) { return vorgabe; }
    // Nicht AppDataLocation: die haengt an applicationName/organizationName
    // des laufenden Prozesses, und der heisst eingebettet "Longpath".
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/Contestprogramm/Contestprogramm");
}

QString datenbankPfad(const QString& datenverzeichnis)
{
    return datenverzeichnis + QStringLiteral("/contestprogramm.sqlite");
}

} // namespace Contestprogramm
