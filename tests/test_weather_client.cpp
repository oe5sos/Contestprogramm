// Tests for WeatherClient::parseOpenMeteoResponse() -- a pure function
// over a raw JSON byte array, so the Open-Meteo response parsing can be
// exercised without a live network round trip. Fixture below is the
// real response body captured live 2026-09-12 against
// api.open-meteo.com for 47.75N 13.75E (JN67, Feuerkogel) -- see
// src/core/WeatherClient.h's own class comment.

#include <QtTest>

#include "core/WeatherClient.h"

using namespace Contestprogramm;

class TestWeatherClient : public QObject
{
    Q_OBJECT

private slots:
    void parsesRealOpenMeteoResponse();
    void missingCurrentFieldYieldsNullopt();
    void malformedJsonYieldsNullopt();
    void missingRequiredFieldYieldsNullopt();
};

void TestWeatherClient::parsesRealOpenMeteoResponse()
{
    const QByteArray json = R"({
        "latitude": 47.74,
        "longitude": 13.759998,
        "current_units": {
            "time": "iso8601",
            "temperature_2m": "°C",
            "relative_humidity_2m": "%",
            "pressure_msl": "hPa"
        },
        "current": {
            "time": "2026-09-12T11:45",
            "interval": 900,
            "temperature_2m": 15.7,
            "relative_humidity_2m": 53,
            "surface_pressure": 917.1,
            "pressure_msl": 1024.7
        }
    })";

    const auto reading = WeatherClient::parseOpenMeteoResponse(json);
    QVERIFY(reading.has_value());
    QCOMPARE(reading->temperatureC, 15.7);
    QCOMPARE(reading->humidityPercent, 53.0);
    // pressure_msl, not surface_pressure -- see the header's own
    // reasoning (elevation-independent reading).
    QCOMPARE(reading->pressureMslHpa, 1024.7);
    QVERIFY(reading->observedUtc.isValid());
}

void TestWeatherClient::missingCurrentFieldYieldsNullopt()
{
    const QByteArray json = R"({"latitude": 47.74, "longitude": 13.76})";
    QVERIFY(!WeatherClient::parseOpenMeteoResponse(json).has_value());
}

void TestWeatherClient::malformedJsonYieldsNullopt()
{
    const QByteArray json = "not json at all {{{";
    QVERIFY(!WeatherClient::parseOpenMeteoResponse(json).has_value());
}

void TestWeatherClient::missingRequiredFieldYieldsNullopt()
{
    // relative_humidity_2m missing entirely -- e.g. a future API change
    // dropping a field must fail closed (nullopt), never silently
    // default to 0 and be mistaken for a real 0% reading.
    const QByteArray json = R"({
        "current": {
            "time": "2026-09-12T11:45",
            "temperature_2m": 15.7,
            "pressure_msl": 1024.7
        }
    })";
    QVERIFY(!WeatherClient::parseOpenMeteoResponse(json).has_value());
}

QTEST_MAIN(TestWeatherClient)
#include "test_weather_client.moc"
