#include "data/FileWrite.h"

#include <QFile>

namespace Contestprogramm {

bool writeFileChecked(const QString& path, const QByteArray& bytes, QString* errorOut)
{
    const auto fail = [&](const QString& why) {
        if (errorOut) {
            *errorOut = why;
        }
        return false;
    };

    // No QIODevice::Text anywhere: every caller composes its own line
    // endings (EDI needs CRLF, which Text mode would turn into CR CRLF
    // on Windows), so the bytes handed in are written exactly as given.
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return fail(file.errorString());
    }
    const qint64 written = file.write(bytes);
    if (written != static_cast<qint64>(bytes.size())) {
        const QString why = file.errorString();
        file.close();
        return fail(why.isEmpty()
                        ? QStringLiteral("nur %1 von %2 Byte geschrieben").arg(written).arg(bytes.size())
                        : why);
    }
    if (!file.flush()) {
        const QString why = file.errorString();
        file.close();
        return fail(why);
    }
    file.close();
    if (file.error() != QFileDevice::NoError) {
        return fail(file.errorString());
    }
    if (errorOut) {
        errorOut->clear();
    }
    return true;
}

} // namespace Contestprogramm
