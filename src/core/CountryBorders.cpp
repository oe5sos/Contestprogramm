#include "core/CountryBorders.h"

#include <QCoreApplication>
#include <QFile>
#include <QString>
#include <QStringList>

namespace Contestprogramm {

QVector<CountryBorderRing> parseCountryBordersData(const QByteArray& data)
{
    QVector<CountryBorderRing> rings;
    const QList<QByteArray> lines = data.split('\n');
    rings.reserve(lines.size());

    for (const QByteArray& lineBytes : lines) {
        QString line = QString::fromUtf8(lineBytes).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }

        // Optional "Name\t" prefix (2026-09-13) -- a plain line with no
        // tab has no verified name, same as before this field existed.
        QString name;
        const int tabIdx = line.indexOf(QLatin1Char('\t'));
        if (tabIdx >= 0) {
            name = line.left(tabIdx).trimmed();
            line = line.mid(tabIdx + 1).trimmed();
        }

        const QStringList pairs = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        QVector<QPointF> points;
        points.reserve(pairs.size());
        for (const QString& pair : pairs) {
            const QStringList parts = pair.split(QLatin1Char(','));
            if (parts.size() != 2) {
                continue;
            }
            bool lonOk = false;
            bool latOk = false;
            const double lon = parts.at(0).toDouble(&lonOk);
            const double lat = parts.at(1).toDouble(&latOk);
            if (lonOk && latOk) {
                points.append(QPointF(lon, lat));
            }
        }
        if (points.size() >= 2) {
            rings.append(CountryBorderRing{name, points});
        }
    }
    return rings;
}

QVector<CountryBorderRing> loadCountryBorders()
{
    // Same search order as AppController's findContestDefinitionsDir():
    // next to the built binary first (matches a packaged app's layout),
    // falling back to the source tree so a plain dev build finds it
    // without a packaging step.
    const QString besideBinary =
        QCoreApplication::applicationDirPath() + QStringLiteral("/resources/geo/country_borders_110m.dat");
    QString path;
    if (QFile::exists(besideBinary)) {
        path = besideBinary;
    }
#ifdef CONTESTPROGRAMM_SOURCE_DIR
    if (path.isEmpty()) {
        const QString besideSource =
            QStringLiteral(CONTESTPROGRAMM_SOURCE_DIR) + QStringLiteral("/resources/geo/country_borders_110m.dat");
        if (QFile::exists(besideSource)) {
            path = besideSource;
        }
    }
#endif
    if (path.isEmpty()) {
        return {};
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return parseCountryBordersData(file.readAll());
}

} // namespace Contestprogramm
