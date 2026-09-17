#pragma once

#include "app/ContestSettings.h"
#include "core/BroadcastPublisher.h"
#include "core/CallsignLocatorLookup.h"
#include "core/DxClusterClient.h"
#include "core/GeoFilter.h"
#include "core/On4kstClient.h"
#include "core/RecentPropagationTracker.h"
#include "core/RigctldClient.h"
#include "core/RotctldClient.h"
#include "core/WeatherClient.h"
#include "core/terrain/TerrainDataManager.h"
#include "data/ContestDatabase.h"
#include "data/LogBackup.h"
#include "data/ContestDefinition.h"
#include "data/DupeChecker.h"
#include "data/MultiplierTracker.h"
#include "data/QsoRecord.h"
#include "models/ChatFeedModel.h"

#include <QObject>
#include <QString>
#include <QVector>

class QTimer;

namespace Contestprogramm {

// Owns the pieces that used to be wired directly inside MainWindow /
// main.cpp: the database connection, the dupe checker, the multiplier
// tracker, the three network clients (RigctldClient for CAT,
// On4kstClient for ON4KST chat/spots, DxClusterClient for a classic
// telnet DX cluster -- a second, independent spot source), two more
// RotctldClient instances (Kern-Welle 2 -- two independent rotor
// "slots", each optional and freely relabelable, see
// ContestSettings::rotor1Enabled/-Label etc. and activeRotorForBand()),
// the GeoFilter that classifies both spot clients' candidates, and the
// two ChatFeedModel
// instances (one per spot source) that present the result. See the
// plan's module table -- AppController "verdrahtet TciClient/
// On4kstClient, DB, GeoFilter, Assistant" (TciClient/Assistant are not
// built in this phase; RigctldClient stands in as the active radio
// source, per the plan's own radio-integration pivot).
//
// MainWindow reaches all of this through one reference instead of
// constructing/wiring each piece itself; MainWindow still owns and
// wires the UI-only pieces (UnifiedLogWidget's entry-row autofill,
// RateMeterWidget, status bar labels) since AppController deliberately
// has no QWidget dependency.
class AppController : public QObject {
    Q_OBJECT

public:
    explicit AppController(QObject* parent = nullptr);

    // Opens `path`, loads settings + available ContestDefinitions, and
    // applies them to the network clients/GeoFilter. Returns false and
    // fills `errorOut` (if given) on failure.
    bool openDatabase(const QString& path, QString* errorOut = nullptr);

    ContestDatabase& database() { return m_database; }
    // Periodic copies of the database (data/LogBackup.h); created in
    // openDatabase() beside the database file, null before that.
    LogBackup* logBackup() { return m_logBackup; }
    const ContestDatabase& database() const { return m_database; }
    DupeChecker& dupeChecker() { return m_dupeChecker; }
    RigctldClient& rigctldClient() { return m_rigctldClient; }
    On4kstClient& on4kstClient() { return m_on4kstClient; }
    DxClusterClient& dxClusterClient() { return m_dxClusterClient; }
    // Two independent rotor slots -- see ContestSettings::rotor1Host/
    // -Port etc. and the plan's Phase 3 "Zwei getrennte Rotoren"
    // section. Both objects always exist regardless of rotor1Enabled/
    // rotor2Enabled (a disabled slot simply never gets connectToRotor()
    // called on it, see applyNetworkSettings) -- "does not exist in the
    // UI" is MainWindow's concern (it adds/removes the RotorWidget), not
    // AppController's.
    RotctldClient& rotor1Client() { return m_rotor1; }
    RotctldClient& rotor2Client() { return m_rotor2; }
    // Tier 2 (imported_locators)/tier 3 (QRZ/HamQTH) of callsign->grid
    // autofill -- see core/CallsignLocatorLookup.h. Owned here alongside
    // the other network-touching core/ objects (On4kstClient etc.);
    // provider/username/password are kept in sync with ContestSettings
    // by applyNetworkSettings(), same as every other credential-bearing
    // client below.
    CallsignLocatorLookup& callsignLocatorLookup() { return m_callsignLocatorLookup; }
    // Which RotctldClient (if any) is responsible for `band` ("144"/
    // "432"/"1296") per ContestSettings::band144RotorSlot etc. Returns
    // nullptr for an unrecognized band, for a band assigned
    // RotorSlot::None, or for a band assigned to a slot that is
    // currently disabled -- callers never need to separately check
    // rotor1Enabled/rotor2Enabled themselves.
    RotctldClient* activeRotorForBand(const QString& band);
    GeoFilter& geoFilter() { return m_geoFilter; }
    // Phase 2 terrain line-of-sight -- see setTerrainDataManager()'s own
    // wiring in the constructor (m_geoFilter consults this already;
    // exposed here too for a future UI consumer, e.g. RotorWidget's own
    // Sperrzone-arc, to query/connect to classificationChanged()
    // directly without going through GeoFilter).
    TerrainDataManager& terrainDataManager() { return m_terrainDataManager; }
    // Rough tropo-ducting indicator (plan's "Wetter-/Tropo-Daten"
    // section) -- own-grid pressure/temperature/humidity from Open-Meteo,
    // kept in sync with ContestSettings::ownGrid by applyNetworkSettings()
    // below, same as every other grid-dependent client here.
    WeatherClient& weatherClient() { return m_weatherClient; }
    // The plan's "Rate-Potenzial aus einem frischen eigenen QSO"
    // refinement -- MainWindow records each logged QSO's band+bearing
    // here (recordQso()); both ChatFeedModel instances are wired to
    // read it (see the constructor) for their own importance scoring.
    RecentPropagationTracker& recentPropagationTracker() { return m_propagationTracker; }
    MultiplierTracker& multiplierTracker() { return m_multiplierTracker; }
    // UDP-Contact-Broadcast-Standard (plan section of the same name) --
    // MainWindow calls publishQso() right after a successful
    // ContestDatabase::insertQso(), the one QSO-insert path that
    // already exists (see MainWindow::handleLogRequested). RadioInfo
    // publishing needs no equivalent call from MainWindow: it is wired
    // entirely inside this class, from m_rigctldClient's own signals --
    // see the constructor.
    BroadcastPublisher& broadcastPublisher() { return m_broadcastPublisher; }
    void publishQso(const QsoRecord& record);
    // ON4KST and classic-DX-cluster candidates deliberately do not
    // share one ChatFeedModel -- see ChatFeedModel.h's class comment
    // and the plan's two-feed-panel UI direction ("ON4KST" and
    // "Cluster", not one blended feed).
    ChatFeedModel& on4kstFeedModel() { return m_on4kstFeedModel; }
    ChatFeedModel& clusterFeedModel() { return m_clusterFeedModel; }

    ContestSettings settings() const { return m_settings; }
    // Persists `settings`, then re-applies it to the network clients/
    // GeoFilter (see applyNetworkSettings). This is what
    // MainWindow::openSettingsDialog used to do by hand against
    // ContestDatabase directly.
    void setSettings(const ContestSettings& settings);

    const QVector<ContestDefinition>& availableContestDefinitions() const { return m_availableContestDefinitions; }
    const ContestDefinition* findContestDefinition(const QString& contestId) const;

    // Re-scans resources/contest_definitions/*.json plus any
    // ContestRulesEditor-saved overrides (see ContestDefinition::
    // overrideDirectory()), re-applies the active contest to
    // MultiplierTracker, and emits contestDefinitionsChanged(). Call
    // after a ContestRulesEditor dialog.exec() == Accepted -- the
    // mechanism behind "Änderung zusätzlich möglich wenn falsch"
    // actually reaching the running UI.
    void reloadContestDefinitions();

signals:
    // MainWindow::applyActiveContestDefinition is connected to this so
    // UnifiedLogWidget's dynamic exchange fields (and mode list)
    // immediately reflect a ContestRulesEditor save -- the same
    // "dialog closes, MainWindow re-applies" shape as
    // MainWindow::openSettingsDialog's own follow-up calls, just
    // signal-driven so any future non-modal caller of
    // reloadContestDefinitions() gets the same propagation for free.
    void contestDefinitionsChanged();

private:
    void loadAvailableContestDefinitions();
    // Pushes m_settings into GeoFilter (own grid/radius), RigctldClient
    // (host/port -- connects if a host is set) and ChatFeedModel (active
    // contest for the worked-check), and starts an On4kstClient login
    // if credentials are present. Called once from openDatabase() and
    // again whenever setSettings() changes something relevant.
    void applyNetworkSettings();
    // Connected to m_rigctldClient's frequencyChanged/modeChanged/
    // pttChanged signals -- publishes a <RadioInfo> broadcast reflecting
    // current CAT state on every change. A zero-parameter slot matches
    // all three signals (Qt allows a slot with fewer parameters than
    // the signal it is connected to).
    void publishRadioInfoNow();
    // Resolves a RotorSlot to its RotctldClient -- nullptr for
    // RotorSlot::None or for a slot that is currently disabled. Shared
    // by activeRotorForBand() for all three bands.
    RotctldClient* rotorForSlot(ContestSettings::RotorSlot slot);

    ContestSettings m_settings;
    ContestDatabase m_database;
    LogBackup* m_logBackup = nullptr; // owned via QObject parent
    DupeChecker m_dupeChecker; // holds ContestDatabase& -- declared after m_database
    MultiplierTracker m_multiplierTracker; // holds ContestDatabase& -- declared after m_database
    RigctldClient m_rigctldClient;
    On4kstClient m_on4kstClient;
    DxClusterClient m_dxClusterClient;
    RotctldClient m_rotor1;
    RotctldClient m_rotor2;
    CallsignLocatorLookup m_callsignLocatorLookup; // holds ContestDatabase& -- declared after m_database
    TerrainDataManager m_terrainDataManager; // declared before m_geoFilter -- setTerrainDataManager() takes its address
    GeoFilter m_geoFilter;
    WeatherClient m_weatherClient;
    RecentPropagationTracker m_propagationTracker;
    BroadcastPublisher m_broadcastPublisher;
    QTimer* m_radioInfoTimer;
    // Both hold GeoFilter&/DupeChecker& -- declared after those.
    ChatFeedModel m_on4kstFeedModel;
    ChatFeedModel m_clusterFeedModel;
    QVector<ContestDefinition> m_availableContestDefinitions;
};

} // namespace Contestprogramm
