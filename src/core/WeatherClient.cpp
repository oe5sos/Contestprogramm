// =================================================================
// src/core/WeatherClient.cpp  (Contestprogramm)
// =================================================================
//
// Contestprogramm-original -- see WeatherClient.h.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-09-12 — Created in C++20/Qt6. AI-assisted via Anthropic Claude
//                 Code, operator Ralph Martin Fischer.
// =================================================================

#include "core/WeatherClient.h"

#include "core/Maidenhead.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimeZone>
#include <QTimer>
#include <QUrlQuery>

namespace Contestprogramm {

WeatherClient::WeatherClient(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_refreshTimer(new QTimer(this))
{
    m_refreshTimer->setInterval(kRefreshIntervalMs);
    connect(m_refreshTimer, &QTimer::timeout, this, &WeatherClient::refresh);
}

void WeatherClient::setOwnGrid(const QString& grid)
{
    if (!isValidGridSquare(grid)) {
        return;
    }
    double lat = 0.0;
    double lon = 0.0;
    calculateLatLonFromGridSquare(grid, lat, lon);
    if (m_hasLocation && qFuzzyCompare(lat, m_lat) && qFuzzyCompare(lon, m_lon)) {
        return; // same grid re-applied (e.g. an unrelated Settings save) -- nothing to do
    }
    m_lat = lat;
    m_lon = lon;
    m_hasLocation = true;
    m_refreshTimer->start();
    refresh();
}

QUrl WeatherClient::requestUrl() const
{
    QUrl url(QStringLiteral("https://api.open-meteo.com/v1/forecast"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("latitude"), QString::number(m_lat, 'f', 4));
    query.addQueryItem(QStringLiteral("longitude"), QString::number(m_lon, 'f', 4));
    query.addQueryItem(QStringLiteral("current"),
                        QStringLiteral("temperature_2m,relative_humidity_2m,pressure_msl"));
    query.addQueryItem(QStringLiteral("timezone"), QStringLiteral("UTC"));
    url.setQuery(query);
    return url;
}

void WeatherClient::refresh()
{
    if (!m_hasLocation) {
        return;
    }

    QNetworkReply* reply = m_networkManager->get(QNetworkRequest(requestUrl()));

    // Same "abort a reply that never answers" timeout guard
    // CallsignLocatorLookup's own network calls already use.
    QTimer::singleShot(kRequestTimeoutMs, reply, [reply]() {
        if (reply->isRunning()) {
            reply->abort();
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit fetchFailed();
            return;
        }
        const auto reading = parseOpenMeteoResponse(reply->readAll());
        if (!reading) {
            emit fetchFailed();
            return;
        }
        m_lastReading = reading;
        emit readingChanged(*reading);
    });
}

std::optional<WeatherReading> WeatherClient::parseOpenMeteoResponse(const QByteArray& json)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        return std::nullopt;
    }
    const QJsonValue currentValue = doc.object().value(QStringLiteral("current"));
    if (!currentValue.isObject()) {
        return std::nullopt;
    }
    const QJsonObject current = currentValue.toObject();

    const QJsonValue temp = current.value(QStringLiteral("temperature_2m"));
    const QJsonValue humidity = current.value(QStringLiteral("relative_humidity_2m"));
    const QJsonValue pressure = current.value(QStringLiteral("pressure_msl"));
    if (!temp.isDouble() || !humidity.isDouble() || !pressure.isDouble()) {
        return std::nullopt;
    }

    WeatherReading reading;
    reading.temperatureC = temp.toDouble();
    reading.humidityPercent = humidity.toDouble();
    reading.pressureMslHpa = pressure.toDouble();
    // Open-Meteo's "time" field is a local-format ISO string without a
    // trailing "Z" (e.g. "2026-09-12T11:45") -- always UTC here since
    // the request explicitly pins timezone=UTC; parsed as best-effort
    // display-only metadata, not used for any comparison logic, so a
    // parse failure (leaves an invalid/default QDateTime) is harmless.
    reading.observedUtc = QDateTime::fromString(current.value(QStringLiteral("time")).toString(), Qt::ISODate);
    reading.observedUtc.setTimeZone(QTimeZone::UTC);
    return reading;
}

} // namespace Contestprogramm
