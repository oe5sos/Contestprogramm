#include "app/AppController.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTimer>

namespace Contestprogramm {

namespace {

// N1MM's own documented RadioInfo cadence ("at least every 10 seconds,
// or when radio frequency or mode change") -- matched here rather than
// invented, since publishRadioInfoNow() already covers the on-change
// half via m_rigctldClient's signals.
constexpr int kRadioInfoIntervalMs = 10000;

// Search order for resources/contest_definitions/*.json: next to the
// built binary first (matches how a packaged app would ship them via
// macdeployqt later), falling back to the source tree so a plain
// `cmake --build build` dev run finds them too without a packaging
// step. Moved here from MainWindow.cpp when contest-definition loading
// moved into AppController.
QString findContestDefinitionsDir()
{
    const QString besideBinary = QCoreApplication::applicationDirPath() + QStringLiteral("/resources/contest_definitions");
    if (QDir(besideBinary).exists()) {
        return besideBinary;
    }
#ifdef CONTESTPROGRAMM_SOURCE_DIR
    const QString besideSource = QStringLiteral(CONTESTPROGRAMM_SOURCE_DIR) + QStringLiteral("/resources/contest_definitions");
    if (QDir(besideSource).exists()) {
        return besideSource;
    }
#endif
    return QString();
}

// ON4KST's public server. Not a ContestSettings field: it is a fixed
// public service (unlike the CAT rig, which is on the operator's own
// machine), so only credentials are configurable, per the existing
// schema.
const QString kOn4kstHost = QStringLiteral("www.on4kst.org");
constexpr quint16 kOn4kstPort = 23001;

} // namespace

AppController::AppController(QObject* parent)
    : QObject(parent)
    , m_dupeChecker(m_database)
    , m_multiplierTracker(m_database)
    , m_callsignLocatorLookup(m_database, this)
    , m_broadcastPublisher(this)
    , m_radioInfoTimer(new QTimer(this))
    , m_on4kstFeedModel(m_geoFilter, m_dupeChecker)
    , m_clusterFeedModel(m_geoFilter, m_dupeChecker)
{
    // Terrain line-of-sight (Phase 2) -- fixed at 144 MHz for now: this
    // codebase has one shared GeoFilter across both spot feeds/bands
    // (see GeoFilter.h's own setTerrainDataManager() comment on what a
    // per-band-live frequency would need instead), and 144 MHz is the
    // wider/more conservative first-Fresnel-zone case of the two VHF/UHF
    // bands this project targets (144/432 MHz) -- a path Clear at 144
    // MHz is also Clear at 432 MHz, never the reverse, so this never
    // hides a 432 MHz candidate that would actually have been fine.
    m_geoFilter.setTerrainDataManager(&m_terrainDataManager, 144.0);

    connect(&m_on4kstClient, &On4kstClient::spotReceived, &m_on4kstFeedModel, &ChatFeedModel::addCandidate);
    connect(&m_on4kstClient, &On4kstClient::chatLineReceived, &m_on4kstFeedModel, &ChatFeedModel::addCandidate);
    connect(&m_dxClusterClient, &DxClusterClient::spotReceived, &m_clusterFeedModel, &ChatFeedModel::addCandidate);

    m_on4kstFeedModel.setMultiplierTracker(&m_multiplierTracker);
    m_clusterFeedModel.setMultiplierTracker(&m_multiplierTracker);
    m_on4kstFeedModel.setRecentPropagationTracker(&m_propagationTracker);
    m_clusterFeedModel.setRecentPropagationTracker(&m_propagationTracker);

    // BroadcastPublisher's <RadioInfo> half -- on every CAT change, plus
    // the periodic heartbeat below, mirroring N1MM's own documented
    // cadence (see BroadcastPublisher.h's class comment).
    connect(&m_rigctldClient, &RigctldClient::frequencyChanged, this, &AppController::publishRadioInfoNow);
    connect(&m_rigctldClient, &RigctldClient::modeChanged, this, &AppController::publishRadioInfoNow);
    connect(&m_rigctldClient, &RigctldClient::pttChanged, this, &AppController::publishRadioInfoNow);
    m_radioInfoTimer->setInterval(kRadioInfoIntervalMs);
    connect(m_radioInfoTimer, &QTimer::timeout, this, &AppController::publishRadioInfoNow);
    m_radioInfoTimer->start();
}

AppController::~AppController()
{
    if (m_logBackup && m_database.isOpen()) {
        m_logBackup->backupNow(false);
    }
}

bool AppController::openDatabase(const QString& path, QString* errorOut)
{
    if (!m_database.open(path)) {
        if (errorOut) {
            *errorOut = m_database.lastError();
        }
        return false;
    }

    m_settings.loadFrom(m_database);
    // Backups live beside the database itself ("backups/" next to the
    // .sqlite), where a rescue after a crash looks first.
    if (m_logBackup == nullptr) {
        m_logBackup = new LogBackup(m_database, QFileInfo(path).dir().filePath(QStringLiteral("backups")), this);
        m_logBackup->start();
    }
    loadAvailableContestDefinitions();
    if (m_settings.activeContestId.isEmpty() && !m_availableContestDefinitions.isEmpty()) {
        // A fresh database starts in the first all-mode contest of the
        // list (file-name order), not in a single-mode one like the
        // Marconi Memorial -- that would put a new install into CW.
        m_settings.activeContestId = m_availableContestDefinitions.first().id();
        for (const ContestDefinition& def : m_availableContestDefinitions) {
            if (def.modes().isEmpty()) {
                m_settings.activeContestId = def.id();
                break;
            }
        }
    }

    applyNetworkSettings();
    return true;
}

void AppController::setSettings(const ContestSettings& settings)
{
    m_settings = settings;
    m_settings.saveTo(m_database);
    applyNetworkSettings();
}

void AppController::loadAvailableContestDefinitions()
{
    m_availableContestDefinitions.clear();
    const QString dir = findContestDefinitionsDir();
    if (dir.isEmpty()) {
        return;
    }
    const QStringList files = QDir(dir).entryList(QStringList{QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const QString& fileName : files) {
        QString error;
        ContestDefinition def = ContestDefinition::loadFromFile(dir + QLatin1Char('/') + fileName, &error);
        if (!def.isValid()) {
            continue;
        }
        // Prefer a ContestRulesEditor-saved override for this contest
        // id, if one exists (see ContestDefinition::overrideFilePath
        // and the plan's "Änderung zusätzlich möglich wenn falsch"
        // note). Falls back to the shipped definition on any load
        // failure -- e.g. a hand-corrupted override file -- rather
        // than silently dropping the contest from the list.
        const QString overridePath = ContestDefinition::overrideFilePath(def.id());
        if (QFile::exists(overridePath)) {
            QString overrideError;
            ContestDefinition overrideDef = ContestDefinition::loadFromFile(overridePath, &overrideError);
            if (overrideDef.isValid()) {
                def = overrideDef;
            }
        }
        m_availableContestDefinitions.append(def);
    }
}

void AppController::reloadContestDefinitions()
{
    loadAvailableContestDefinitions();
    if (const ContestDefinition* def = findContestDefinition(m_settings.activeContestId)) {
        m_multiplierTracker.recompute(m_settings.activeContestId, *def);
    }
    emit contestDefinitionsChanged();
}

RotctldClient* AppController::rotorForSlot(ContestSettings::RotorSlot slot)
{
    if (slot == ContestSettings::RotorSlot::Slot1 && m_settings.rotor1Enabled) {
        return &m_rotor1;
    }
    if (slot == ContestSettings::RotorSlot::Slot2 && m_settings.rotor2Enabled) {
        return &m_rotor2;
    }
    return nullptr;
}

RotctldClient* AppController::activeRotorForBand(const QString& band)
{
    if (band == QStringLiteral("144")) {
        return rotorForSlot(m_settings.band144RotorSlot);
    }
    if (band == QStringLiteral("432")) {
        return rotorForSlot(m_settings.band432RotorSlot);
    }
    if (band == QStringLiteral("1296")) {
        // Shares one of the two rotor slots -- no third rotor. See
        // ContestSettings::band1296RotorSlot.
        return rotorForSlot(m_settings.band1296RotorSlot);
    }
    return nullptr;
}

const ContestDefinition* AppController::findContestDefinition(const QString& contestId) const
{
    for (const ContestDefinition& def : m_availableContestDefinitions) {
        if (def.id() == contestId) {
            return &def;
        }
    }
    return nullptr;
}

void AppController::applyNetworkSettings()
{
    m_geoFilter.setOwnGrid(m_settings.ownGrid);
    m_geoFilter.setRadiusKm(m_settings.radiusKm);
    // Re-runs every already-requested candidate's terrain classification
    // against the (possibly just-changed) own grid/elevation/antenna
    // height -- does NOT re-fetch already-loaded SRTM tiles, only the
    // cheap profile+classification step (see TerrainDataManager's own
    // doc comment).
    m_terrainDataManager.setOwnStation(m_settings.ownGrid, m_settings.ownElevationM, m_settings.antennaHeightM);
    m_weatherClient.setOwnGrid(m_settings.ownGrid);
    m_on4kstFeedModel.setActiveContest(m_settings.activeContestId);
    m_clusterFeedModel.setActiveContest(m_settings.activeContestId);

    if (const ContestDefinition* def = findContestDefinition(m_settings.activeContestId)) {
        m_multiplierTracker.recompute(m_settings.activeContestId, *def);
        // "Worked" in the feeds is per band (see ChatFeedModel's class
        // comment) -- the models need the contest's band list for it.
        m_on4kstFeedModel.setContestBands(def->bands());
        m_clusterFeedModel.setContestBands(def->bands());
    }

    m_rigctldClient.setTarget(m_settings.rigctldHost, static_cast<quint16>(m_settings.rigctldPort));
    if (!m_settings.rigctldHost.isEmpty() && !m_rigctldClient.isConnected()) {
        m_rigctldClient.connectToRig();
    }

    // A disabled slot is never dialed -- and is actively hung up if it
    // was connected before the operator just disabled it (a live
    // settings-save toggle, not only the startup path) -- matching
    // "does not exist" rather than "exists but idle".
    if (m_settings.rotor1Enabled) {
        m_rotor1.setTarget(m_settings.rotor1Host, static_cast<quint16>(m_settings.rotor1Port));
        if (!m_settings.rotor1Host.isEmpty() && !m_rotor1.isConnected()) {
            m_rotor1.connectToRotor();
        }
    } else if (m_rotor1.isConnected()) {
        m_rotor1.disconnectFromRotor();
    }
    if (m_settings.rotor2Enabled) {
        m_rotor2.setTarget(m_settings.rotor2Host, static_cast<quint16>(m_settings.rotor2Port));
        if (!m_settings.rotor2Host.isEmpty() && !m_rotor2.isConnected()) {
            m_rotor2.connectToRotor();
        }
    } else if (m_rotor2.isConnected()) {
        m_rotor2.disconnectFromRotor();
    }

    // ON4KST needs real credentials to be worth dialing out for. Per
    // the plan's own "Offene Punkte": live-server verification is a
    // deliberate separate manual step for the operator, with real
    // credentials -- this program does not attempt it on its own with
    // placeholder/empty settings.
    if (!m_settings.on4kstUsername.isEmpty() && !m_on4kstClient.isConnected()) {
        m_on4kstClient.connectAndLogin(kOn4kstHost, kOn4kstPort,
                                        m_settings.on4kstUsername, m_settings.on4kstPassword);
    }

    // Classic DX cluster: no fixed public host (see ContestSettings.h),
    // so only dial out once the operator has actually configured one --
    // same "no attempt with placeholder/empty settings" reasoning as
    // ON4KST above. Needs a callsign too (cluster login is "send your
    // callsign back", not credentials).
    if (!m_settings.clusterHost.isEmpty() && !m_settings.ownCallsign.isEmpty() && !m_dxClusterClient.isConnected()) {
        m_dxClusterClient.connectToCluster(m_settings.clusterHost, static_cast<quint16>(m_settings.clusterPort),
                                            m_settings.ownCallsign);
    }

    m_broadcastPublisher.setEnabled(m_settings.broadcastEnabled);
    m_broadcastPublisher.setTarget(m_settings.broadcastHost, static_cast<quint16>(m_settings.broadcastPort));

    // Tier 3 of callsign->grid autofill (core/CallsignLocatorLookup.h) --
    // kept in sync here rather than only at startup, so a mid-session
    // Settings save (e.g. the operator entering QRZ credentials for the
    // first time) takes effect on the very next lookup.
    m_callsignLocatorLookup.setProviderSettings(m_settings.callbookProvider, m_settings.callbookUsername,
                                                 m_settings.callbookPassword);
}

void AppController::publishQso(const QsoRecord& record)
{
    const ContestDefinition* def = findContestDefinition(record.contestId);
    const QString contestName = def ? def->id() : record.contestId;
    m_broadcastPublisher.publishContact(record, m_settings.ownCallsign, contestName);
}

void AppController::publishRadioInfoNow()
{
    m_broadcastPublisher.publishRadioInfo(m_settings.ownCallsign, m_rigctldClient.frequencyHz(),
                                           m_rigctldClient.mode(), m_rigctldClient.pttActive());
}

} // namespace Contestprogramm
