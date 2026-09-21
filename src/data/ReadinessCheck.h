#pragma once

#include "data/ContestSchedule.h"

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Contestprogramm {

// Datei > Startcheck (Bereit?): everything that should be in order
// before the first CQ, as one list -- station data, the active contest
// and its window, the log, every link (CAT, rotors, ON4KST, cluster)
// and the data on disk (backups, terrain, locator list). A pure
// function over a snapshot MainWindow fills (ReadinessContext), so it
// is unit-testable without a live client or database, like
// data/LogCheck.h.
struct ReadinessItem {
    enum class Level { Ok, Hint, Warning, Error };
    Level level = Level::Ok;
    QString group; // "Station", "Contest", "Verbindungen", "Daten"
    QString title; // "Rufzeichen", "Rotor 1 (2m)", ...
    QString detail; // what was found, or what to do
    QString code; // stable id for tests ("own_call", "rotor1", ...)
};

struct ReadinessResult {
    QVector<ReadinessItem> items;
    int errors = 0;
    int warnings = 0;
    int hints = 0;
    // Ready to start: nothing at Error level. Warnings are worth a look
    // but do not stop a contest (a rotor still off, no cluster).
    bool ready() const { return errors == 0; }
    QString summaryText() const;
};

// One link's state, from whichever client owns it.
enum class LinkState { NotConfigured, Connecting, Connected, Disconnected };

struct ReadinessContext {
    QDateTime nowUtc;

    // Station
    QString ownCallsign;
    QString ownGrid;
    bool useExactOwnLocation = false;
    double ownExactLatitude = 0.0;
    double ownExactLongitude = 0.0;
    double ownElevationM = 0.0;
    double antennaHeightM = 0.0;
    // The machine's clock against a web server's (core/ClockCheck.h):
    // unchecked while the first round runs, unreachable without
    // internet, else the offset local minus server in seconds.
    bool clockChecked = false;
    bool clockReachable = false;
    qint64 clockOffsetSecs = 0;
    QString clockSource;

    // Contest
    bool contestFound = false;
    QString contestName;
    QStringList contestBands;
    ContestWindow window; // invalid when the definition has no schedule
    int qsoCount = 0; // this contest's log
    bool esmEnabled = false;

    // Links
    QString catTarget; // "127.0.0.1:4532", empty when not configured
    LinkState cat = LinkState::NotConfigured;
    struct Rotor {
        bool enabled = false;
        QString label;
        QString target; // "127.0.0.1:4533"
        LinkState link = LinkState::NotConfigured;
        QString rotctldError; // the last failure of a rotctld this program started, if any
    };
    Rotor rotor1;
    Rotor rotor2;
    bool on4kstConfigured = false;
    LinkState on4kst = LinkState::NotConfigured;
    QString clusterTarget;
    LinkState cluster = LinkState::NotConfigured;

    // Data
    QString backupDirectory;
    bool backupDirectoryWritable = false;
    QDateTime lastBackupUtc;
    bool terrainLoadedForOwnLocation = false;
    int importedLocators = 0;
};

ReadinessResult checkReadiness(const ReadinessContext& context);

// "in 12 T 3 h" / "seit 2 h 10 min" style span, for the contest line.
QString describeSpan(qint64 seconds);

} // namespace Contestprogramm
