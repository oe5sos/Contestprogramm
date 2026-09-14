#include "core/Cities.h"

#include <QCoreApplication>
#include <QFile>
#include <QStringList>

namespace Contestprogramm {

QVector<CityPoint> parseCitiesData(const QByteArray& data)
{
    QVector<CityPoint> cities;
    const QList<QByteArray> lines = data.split('\n');
    cities.reserve(lines.size());

    for (const QByteArray& lineBytes : lines) {
        const QString line = QString::fromUtf8(lineBytes).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }

        const QStringList fields = line.split(QLatin1Char('\t'));
        if (fields.size() != 5) {
            continue;
        }
        bool lonOk = false;
        bool latOk = false;
        const double lon = fields.at(0).toDouble(&lonOk);
        const double lat = fields.at(1).toDouble(&latOk);
        if (!lonOk || !latOk) {
            continue;
        }

        CityPoint city;
        city.lon = lon;
        city.lat = lat;
        city.popMax = fields.at(2).toLongLong();
        city.isCapital = fields.at(3).trimmed() == QLatin1String("1");
        city.name = fields.at(4);
        if (city.name.isEmpty()) {
            continue;
        }
        cities.append(city);
    }
    return cities;
}

QVector<CityPoint> loadCities()
{
    // Same search order as CountryBorders.cpp's loadCountryBorders():
    // next to the built binary first, falling back to the source tree
    // for a plain dev build.
    const QString besideBinary =
        QCoreApplication::applicationDirPath() + QStringLiteral("/resources/geo/cities_110m.dat");
    QString path;
    if (QFile::exists(besideBinary)) {
        path = besideBinary;
    }
#ifdef CONTESTPROGRAMM_SOURCE_DIR
    if (path.isEmpty()) {
        const QString besideSource =
            QStringLiteral(CONTESTPROGRAMM_SOURCE_DIR) + QStringLiteral("/resources/geo/cities_110m.dat");
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
    return parseCitiesData(file.readAll());
}

} // namespace Contestprogramm
