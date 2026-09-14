#include "core/BandUtils.h"

#include <QString>

namespace Contestprogramm {

QString bandLabelForFrequencyHz(qint64 hz)
{
    if (hz >= 144000000 && hz <= 148000000) {
        return QStringLiteral("144");
    }
    if (hz >= 420000000 && hz <= 450000000) {
        return QStringLiteral("432");
    }
    if (hz >= 1240000000 && hz <= 1300000000) {
        return QStringLiteral("1296");
    }
    return QString();
}

} // namespace Contestprogramm
