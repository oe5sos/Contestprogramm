#include "core/RigModels.h"

#include "core/HamlibBinary.h"

#include <QProcess>
#include <QRegularExpression>

namespace Contestprogramm {

QVector<RigModel> rigModelsFromHamlib()
{
    const QString binary = findHamlibBinary(QStringLiteral("rigctl"));
    if (binary.isEmpty()) {
        return {};
    }

    QProcess process;
    process.setProgram(binary);
    process.setArguments({QStringLiteral("-l")});
    process.start();
    // Short leash: this runs while the settings dialog is being built,
    // and a Hamlib that does not answer must not hold the window shut.
    if (!process.waitForStarted(2000) || !process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished(500);
        return {};
    }

    // " 2037  Kenwood                TS-590SG                20250515.12     Stable      RIG_MODEL_TS590SG"
    // Number, manufacturer, model -- the rest (version, status, macro)
    // is not something to put in front of an operator. The model column
    // may contain spaces, so it is taken up to the version stamp.
    static const QRegularExpression line(
        QStringLiteral("^\\s*(\\d+)\\s{2,}(\\S+(?:\\s\\S+)*?)\\s{2,}(\\S+(?:\\s\\S+)*?)\\s{2,}\\d{8}\\."));

    QVector<RigModel> models;
    const QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
    for (const QString& text : output.split(QLatin1Char('\n'))) {
        const QRegularExpressionMatch match = line.match(text);
        if (!match.hasMatch()) {
            continue; // the header, and anything that is not a rig row
        }
        bool ok = false;
        const int id = match.captured(1).toInt(&ok);
        if (!ok || id <= 0) {
            continue;
        }
        RigModel model;
        model.hamlibId = id;
        model.name = match.captured(2).trimmed() + QLatin1Char(' ') + match.captured(3).trimmed();
        models.append(model);
    }
    return models;
}

} // namespace Contestprogramm
