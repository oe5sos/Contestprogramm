#pragma once

// =================================================================
// src/core/WeatherClient.h  (Contestprogramm)
// =================================================================
//
// Contestprogramm-original. The plan's "Wetter-/Tropo-Daten
// (ergänzend)" section, "pragmatischer erster Schritt": Ausbreitung auf
// 2m/70cm hängt stark von Inversionsschichten/Tropo-Ducting ab, nicht
// nur Terrain -- eine echte Ducting-Vorhersage bräuchte vertikale
// Atmosphärenprofile (GRIB/NWP-Daten), ausdrücklich ein eigenes,
// größeres Thema. Diese Klasse liefert stattdessen nur, was der Plan
// selbst als ersten Schritt nennt: eine freie, schlüssellose Wetter-API
// (Open-Meteo, api.open-meteo.com -- kein API-Key, keine Registrierung)
// für Druck/Temperatur/Feuchte am eigenen Standort, roh in der
// Statuszeile angezeigt -- KEIN selbst erfundener "Ducting-Score", der
// eine Genauigkeit vortäuschen würde, die diese drei Zahlen allein nicht
// hergeben.
//
// Response shape verified live 2026-09-12 against a real Open-Meteo
// request for JN67 (47.75N 13.75E):
//   GET https://api.open-meteo.com/v1/forecast
//       ?latitude=47.75&longitude=13.75
//       &current=temperature_2m,relative_humidity_2m,pressure_msl
//       &timezone=UTC
//   { "current": { "time": "2026-09-12T11:45", "temperature_2m": 15.7,
//                  "relative_humidity_2m": 53, "pressure_msl": 1024.7 } }
// `pressure_msl` (mean-sea-level, not `surface_pressure`) is used --
// the standard, elevation-independent reading for "is a high-pressure
// system sitting over the area", the classic rough tropo-ducting
// precondition; `surface_pressure` alone would make a mountain-top QTH
// like Feuerkogel (948 m, confirmed live: surface_pressure=917.1 vs.
// pressure_msl=1024.7 at the same moment) look like an unrelated
// weather regime purely from elevation, not actual conditions.
//
// Same "pure parse function + thin QNetworkAccessManager wrapper" split
// CallsignLocatorLookup.h already established for this project:
// parseOpenMeteoResponse() is a static, unit-testable function over a
// raw JSON byte array; only the fetch itself needs a live network.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-09-12 — Created in C++20/Qt6. AI-assisted via Anthropic Claude
//                 Code, operator Ralph Martin Fischer.
// =================================================================

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QUrl>

#include <optional>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace Contestprogramm {

struct WeatherReading {
    double temperatureC = 0.0;
    double humidityPercent = 0.0;
    double pressureMslHpa = 0.0;
    QDateTime observedUtc;
};

class WeatherClient : public QObject {
    Q_OBJECT

public:
    explicit WeatherClient(QObject* parent = nullptr);

    // Sets the fetch location from a Maidenhead locator (Maidenhead::
    // calculateLatLonFromGridSquare) -- call whenever ContestSettings::
    // ownGrid changes (AppController::applyNetworkSettings, same trigger
    // every other grid-dependent client already uses). Fires an
    // immediate refresh() when `grid` newly becomes valid; a no-op
    // (silently keeps the previous location/reading) for an invalid or
    // unchanged grid, so a mid-typing keystroke in Settings never spams
    // the API.
    void setOwnGrid(const QString& grid);

    // Non-empty only once at least one fetch has ever succeeded --
    // MainWindow shows Style::unknownDash() (HAUSSTIL rule 7, "Unbekannt
    // ist ein Strich, keine Null") until then, then keeps showing the
    // last successful reading even across later failed refreshes (see
    // fetchFailed()'s own doc comment).
    std::optional<WeatherReading> lastReading() const { return m_lastReading; }

    // Pure parse, no network -- unit-testable directly against a
    // fixture JSON byte array (this header's own class comment
    // documents the real response shape). Returns nullopt when the
    // document does not parse or is missing the fields this class
    // needs.
    static std::optional<WeatherReading> parseOpenMeteoResponse(const QByteArray& json);

public slots:
    // Fires an async fetch now -- called once right after setOwnGrid()
    // establishes a valid location and then on a periodic timer
    // (kRefreshIntervalMs; weather changes on the order of tens of
    // minutes, not seconds, so this stays deliberately infrequent). A
    // no-op when no valid location has been set yet.
    void refresh();

signals:
    // Fires every time a fetch succeeds, including when the new reading
    // is numerically identical to the last one -- MainWindow just
    // re-renders its status-bar text unconditionally, the simplest
    // correct behaviour and cheap enough at this refresh cadence.
    void readingChanged(const WeatherReading& reading);
    // A network error or an unparseable response. lastReading() (and
    // whatever MainWindow is currently showing) is deliberately left
    // untouched -- a transient failure should not blank out a
    // perfectly-good reading from 10 minutes ago, the same "don't
    // replace a known value with a blank on a hiccup" restraint
    // RateMeterWidget's own no-source dash case models for a genuinely
    // different (never-had-a-source) situation.
    void fetchFailed();

private:
    QUrl requestUrl() const;

    QNetworkAccessManager* m_networkManager;
    QTimer* m_refreshTimer;
    double m_lat = 0.0;
    double m_lon = 0.0;
    bool m_hasLocation = false;
    std::optional<WeatherReading> m_lastReading;

    static constexpr int kRequestTimeoutMs = 8000;
    static constexpr int kRefreshIntervalMs = 30 * 60 * 1000; // 30 min
};

} // namespace Contestprogramm
