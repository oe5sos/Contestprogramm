#pragma once

// =================================================================
// src/core/CallsignLocatorLookup.h  (Contestprogramm)
// =================================================================
//
// Contestprogramm-original. The two extra lookup tiers behind Martin's
// live "OE1W sollte automatisch JN77TX anzeigen" report: the entry row's
// callsign-lookup autofill (UnifiedLogWidget) already checks the operator's own log
// (ContestDatabase::knownExchangeForCallsign) but had nothing for a
// callsign never worked yet this contest. This class owns:
//
//   Tier 2 -- lookupLocal(): a synchronous, always-offline-safe lookup
//   against the `imported_locators` SQLite table (ContestDatabase::
//   importedLocatorForCallsign), populated either by a one-time CSV
//   import (SettingsDialog's "Locator-Liste importieren...", the same
//   concept N1MM+ calls a "Call History File") or by this class caching
//   a successful external lookup -- a live result is exactly as reusable
//   as an imported one, so both end up served from the same table.
//
//   Tier 3 -- lookupExternal(): an async QRZ.com/HamQTH XML callbook
//   lookup (QNetworkAccessManager, never blocking the UI thread), using
//   whichever provider/credentials the operator entered in Settings
//   (see ContestSettings::callbookProvider). Session-key handling (log
//   in once, reuse the key, transparently re-login once on a session
//   failure) happens entirely inside this class -- callers only ever see
//   externalLookupFinished(). A successful result is cached into the
//   same imported_locators table lookupLocal() reads, so a repeat
//   lookup for the same callsign later in the contest is instant and
//   offline-safe even with no network at all.
//
// The XML parsing itself (parseQrzSessionXml/parseQrzLookupXml/
// parseHamQthSessionXml/parseHamQthLookupXml) is split out as pure
// static functions taking a raw XML string, the same "pure logic
// separated from I/O" split core/BeamHeading.h's plan()/MapWidget's
// projectBearingDistance() already use -- so the parsing logic is
// unit-testable against fixture XML strings without a live network or a
// fake HTTPS server; only the thin QNetworkAccessManager wrapper around
// them is not directly unit-tested (see tests/test_callsign_locator_
// lookup.cpp's own comment for what is/is not covered).
//
// Response shapes: verified 2026-09-10 against the real QRZ.com XML Data
// API spec (qrz.com/XML/current_spec.html) and HamQTH's developer page
// (hamqth.com/developers.php) -- both fetched live. QRZ: a <Session>
// block with <Key> on success or <Error> on failure; a <Callsign> block
// whose <grid> child is the locator. HamQTH: a <session> block with
// <session_id> on success or <error> on failure; a <search> block whose
// <grid> child is the locator (or its own <error> for "callsign not
// found"). Both wrap their root in a namespaced element (<QRZDatabase
// xmlns=...>, <HamQTH xmlns=...>) -- irrelevant here since
// QXmlStreamReader::name() returns the local (unprefixed) element name
// either way.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-09-10 — Created in C++20/Qt6. AI-assisted via Anthropic Claude
//                 Code, operator Ralph Martin Fischer.
// =================================================================

#include "app/ContestSettings.h"

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVector>

#include <optional>

class QNetworkAccessManager;

namespace Contestprogramm {

class ContestDatabase;

class CallsignLocatorLookup : public QObject {
    Q_OBJECT

public:
    // Mirrors ContestSettings::CallbookProvider one-to-one -- aliased
    // rather than duplicated, so there is exactly one enum to keep in
    // sync with SettingsDialog's provider combo.
    using Provider = ContestSettings::CallbookProvider;

    explicit CallsignLocatorLookup(ContestDatabase& database, QObject* parent = nullptr);

    // Tier 2: synchronous imported_locators lookup. Empty optional on a
    // miss. Always fast and offline-safe -- safe to call on every
    // callsign-field debounce tick, same as ContestDatabase::
    // knownExchangeForCallsign.
    std::optional<QString> lookupLocal(const QString& callsign) const;

    // Applies (or clears) provider credentials -- call whenever
    // ContestSettings changes (AppController::applyNetworkSettings,
    // mirroring how it already re-applies On4kstClient/DxClusterClient
    // credentials). Any cached session key is dropped the moment the
    // provider, username, or password actually changes: a stale key
    // from a different account (or provider) must never be reused, and
    // any lookup already in flight against the old settings is ignored
    // when it completes rather than applied or cached.
    void setProviderSettings(Provider provider, const QString& username, const QString& password);

    // True once a provider is chosen and both credential fields are
    // non-empty -- MainWindow checks this before ever calling
    // lookupExternal(), so tier 3 only fires when it can plausibly
    // succeed. lookupExternal() itself also guards on this (silently
    // no-op otherwise), so it is always safe to call regardless.
    bool isExternalLookupAvailable() const;

    // Tier 3: fires an async HTTPS round trip (session login first if no
    // session key is cached yet, then the actual lookup; a session
    // failure mid-lookup triggers exactly one transparent re-login+retry
    // before giving up). Never blocks the UI thread; each request is
    // aborted after kRequestTimeoutMs if the server never answers, so
    // this can never hang indefinitely. A no-op (nothing emitted) when
    // isExternalLookupAvailable() is false or `callsign` is empty.
    void lookupExternal(const QString& callsign);

    // CSV import ("callsign,grid[,name]" per line -- N1MM+ "Call History
    // File" concept). Malformed lines are skipped individually rather
    // than aborting the whole import; an optional header row is
    // tolerated for free (see parseCsv's own comment for how).
    struct ImportSummary {
        int imported = 0;
        int skipped = 0;
    };
    // Reads `filePath`, parses it (parseCsv below), and writes every
    // valid row into imported_locators. Returns {0, 0} and fills
    // `errorOut` (if given) if the file itself could not be opened.
    ImportSummary importCsvFile(const QString& filePath, QString* errorOut = nullptr);

    struct CsvRow {
        QString callsign;
        QString grid;
        QString name; // optional third column; empty if absent
    };
    struct CsvParseResult {
        QVector<CsvRow> rows;
        int skipped = 0;
    };
    // Pure parse, no file/DB access -- unit-testable directly against a
    // fixture string. A line is skipped (counted in `skipped`, not
    // fatal to the rest of the import) when it has fewer than two
    // comma-separated fields, when the callsign field is empty, or when
    // the grid field does not pass Maidenhead::isValidGridSquare -- the
    // last check is also what makes an optional header row
    // ("callsign,grid,name" or similar) fall out for free: its "grid"
    // field is not a valid locator either, so it is skipped exactly like
    // any other malformed line, no special-casing needed.
    static CsvParseResult parseCsv(const QString& csvText);

    // -- XML parsing, pure, unit-testable without any network I/O --
    // see this header's own class comment for the response shapes these
    // are built against.
    struct SessionResult {
        bool ok = false;
        QString sessionKey;
        QString error; // human-readable, for logging/status only
    };
    struct GridLookupResult {
        bool found = false;
        QString grid;
        // True when the failure looks like an expired/invalid session
        // (the error text mentions "session"/"expired") rather than a
        // genuine "callsign not found" -- lookupExternal() uses this to
        // decide whether a re-login+retry is worth attempting.
        bool sessionInvalid = false;
        QString error;
    };

    static SessionResult parseQrzSessionXml(const QString& xml);
    static GridLookupResult parseQrzLookupXml(const QString& xml);
    static SessionResult parseHamQthSessionXml(const QString& xml);
    static GridLookupResult parseHamQthLookupXml(const QString& xml);

signals:
    // Emitted once per lookupExternal() call that actually went out to
    // the network, when the whole round trip (including any transparent
    // re-login retry) settles -- `callsign` echoes the request so the
    // caller (MainWindow) can check the entry row's callsign field still
    // matches before applying anything, the same staleness guard
    // UnifiedLogWidget's own debounce timer already applies to itself.
    // `grid` is empty whenever `found` is false (not-found, a network
    // error, or a session failure that survived the one retry) --
    // callers should degrade silently on false, never treat it as an
    // error dialog-worthy condition.
    void externalLookupFinished(const QString& callsign, bool found, const QString& grid);

private:
    void startSessionRequest(const QString& pendingCallsign, bool retryAttempted);
    void startLookupRequest(const QString& callsign, bool retryAttempted);
    QUrl sessionUrl() const;
    QUrl lookupUrl(const QString& callsign) const;

    ContestDatabase& m_database;
    QNetworkAccessManager* m_networkManager;

    Provider m_provider = Provider::None;
    QString m_username;
    QString m_password;
    QString m_sessionKey;
    // Bumped by setProviderSettings() whenever provider/username/
    // password actually change -- captured by in-flight requests so a
    // reply that lands after the operator has since changed Settings is
    // discarded instead of applied/cached against the new account.
    int m_settingsGeneration = 0;

    static constexpr int kRequestTimeoutMs = 8000;
};

} // namespace Contestprogramm
