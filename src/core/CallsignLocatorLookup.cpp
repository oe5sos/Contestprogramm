// =================================================================
// src/core/CallsignLocatorLookup.cpp  (Contestprogramm)
// =================================================================
//
// Contestprogramm-original -- see CallsignLocatorLookup.h.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-09-10 — Created in C++20/Qt6. AI-assisted via Anthropic Claude
//                 Code, operator Ralph Martin Fischer.
// =================================================================

#include "core/CallsignLocatorLookup.h"

#include "core/Maidenhead.h"
#include "data/ContestDatabase.h"

#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrlQuery>
#include <QXmlStreamReader>

namespace Contestprogramm {

namespace {

// Returns the trimmed text content of the first element named `tagName`
// found anywhere in `xml` (case-sensitive), or an empty string if it is
// absent or the document does not parse. QRZ's and HamQTH's response
// shapes are both flat enough (one level of nesting under the root) that
// hunting for a uniquely-named element anywhere in the document, rather
// than hand-rolling a full tree walk that tracks parent context, is
// robust for the two dialects this class needs -- they differ only in
// capitalization ("Key"/"Error"/"Callsign" vs. "session_id"/"error"/
// "search"), never in an element name colliding across a success and a
// failure branch of the same response.
QString firstElementText(const QString& xml, const QString& tagName)
{
    QXmlStreamReader reader(xml);
    while (!reader.atEnd() && !reader.hasError()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name().compare(tagName, Qt::CaseSensitive) == 0) {
            return reader.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
        }
    }
    return QString();
}

QString normalizeCallsign(const QString& callsign)
{
    return callsign.trimmed().toUpper();
}

} // namespace

CallsignLocatorLookup::CallsignLocatorLookup(ContestDatabase& database, QObject* parent)
    : QObject(parent)
    , m_database(database)
    , m_networkManager(new QNetworkAccessManager(this))
{
}

std::optional<QString> CallsignLocatorLookup::lookupLocal(const QString& callsign) const
{
    const auto entry = m_database.importedLocatorForCallsign(callsign);
    if (!entry.has_value()) {
        return std::nullopt;
    }
    return entry->grid;
}

void CallsignLocatorLookup::setProviderSettings(Provider provider, const QString& username, const QString& password)
{
    if (provider != m_provider || username != m_username || password != m_password) {
        // Credentials/provider actually changed -- any cached session key
        // belongs to the old account and must never be reused against the
        // new one; bumping the generation also makes any reply already in
        // flight against the old settings a no-op when it lands (see the
        // generation check in startSessionRequest/startLookupRequest's
        // completion lambdas).
        m_sessionKey.clear();
        m_settingsGeneration++;
    }
    m_provider = provider;
    m_username = username;
    m_password = password;
}

bool CallsignLocatorLookup::isExternalLookupAvailable() const
{
    return m_provider != Provider::None && !m_username.isEmpty() && !m_password.isEmpty();
}

void CallsignLocatorLookup::lookupExternal(const QString& callsign)
{
    if (!isExternalLookupAvailable()) {
        return;
    }
    const QString normalized = normalizeCallsign(callsign);
    if (normalized.isEmpty()) {
        return;
    }
    if (m_sessionKey.isEmpty()) {
        startSessionRequest(normalized, /*retryAttempted=*/false);
    } else {
        startLookupRequest(normalized, /*retryAttempted=*/false);
    }
}

QUrl CallsignLocatorLookup::sessionUrl() const
{
    QUrl url;
    QUrlQuery query;
    if (m_provider == Provider::Qrz) {
        url = QUrl(QStringLiteral("https://xmldata.qrz.com/xml/current/"));
        query.addQueryItem(QStringLiteral("username"), m_username);
        query.addQueryItem(QStringLiteral("password"), m_password);
    } else if (m_provider == Provider::HamQth) {
        url = QUrl(QStringLiteral("https://www.hamqth.com/xml.php"));
        query.addQueryItem(QStringLiteral("u"), m_username);
        query.addQueryItem(QStringLiteral("p"), m_password);
    }
    url.setQuery(query);
    return url;
}

QUrl CallsignLocatorLookup::lookupUrl(const QString& callsign) const
{
    QUrl url;
    QUrlQuery query;
    if (m_provider == Provider::Qrz) {
        url = QUrl(QStringLiteral("https://xmldata.qrz.com/xml/current/"));
        query.addQueryItem(QStringLiteral("s"), m_sessionKey);
        query.addQueryItem(QStringLiteral("callsign"), callsign);
    } else if (m_provider == Provider::HamQth) {
        url = QUrl(QStringLiteral("https://www.hamqth.com/xml.php"));
        query.addQueryItem(QStringLiteral("id"), m_sessionKey);
        query.addQueryItem(QStringLiteral("callsign"), callsign);
        query.addQueryItem(QStringLiteral("prg"), QStringLiteral("Contestprogramm"));
    }
    url.setQuery(query);
    return url;
}

void CallsignLocatorLookup::startSessionRequest(const QString& pendingCallsign, bool retryAttempted)
{
    QNetworkReply* reply = m_networkManager->get(QNetworkRequest(sessionUrl()));

    // Never hang indefinitely on a server that never answers -- abort
    // (which triggers finished() with an error, handled like any other
    // network failure below) after kRequestTimeoutMs, same "a few
    // seconds, not forever" posture On4kstClient's own connect timeout
    // uses.
    QTimer::singleShot(kRequestTimeoutMs, reply, [reply]() {
        if (reply->isRunning()) {
            reply->abort();
        }
    });

    const Provider provider = m_provider;
    const int generation = m_settingsGeneration;
    connect(reply, &QNetworkReply::finished, this, [this, reply, pendingCallsign, retryAttempted, provider, generation]() {
        reply->deleteLater();
        if (generation != m_settingsGeneration) {
            // Settings changed mid-flight (operator switched provider or
            // edited credentials in Settings while this request was in
            // the air) -- discard rather than applying a session key or
            // lookup result against whatever is now configured.
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            emit externalLookupFinished(pendingCallsign, false, QString());
            return;
        }
        const QString xml = QString::fromUtf8(reply->readAll());
        const SessionResult session = (provider == Provider::Qrz) ? parseQrzSessionXml(xml) : parseHamQthSessionXml(xml);
        if (!session.ok) {
            emit externalLookupFinished(pendingCallsign, false, QString());
            return;
        }
        m_sessionKey = session.sessionKey;
        startLookupRequest(pendingCallsign, retryAttempted);
    });
}

void CallsignLocatorLookup::startLookupRequest(const QString& callsign, bool retryAttempted)
{
    QNetworkReply* reply = m_networkManager->get(QNetworkRequest(lookupUrl(callsign)));

    QTimer::singleShot(kRequestTimeoutMs, reply, [reply]() {
        if (reply->isRunning()) {
            reply->abort();
        }
    });

    const Provider provider = m_provider;
    const int generation = m_settingsGeneration;
    connect(reply, &QNetworkReply::finished, this, [this, reply, callsign, retryAttempted, provider, generation]() {
        reply->deleteLater();
        if (generation != m_settingsGeneration) {
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            emit externalLookupFinished(callsign, false, QString());
            return;
        }
        const QString xml = QString::fromUtf8(reply->readAll());
        const GridLookupResult result = (provider == Provider::Qrz) ? parseQrzLookupXml(xml) : parseHamQthLookupXml(xml);
        if (result.found) {
            // Cache into imported_locators -- a repeat lookup for this
            // callsign later in the contest is then instant and
            // offline-safe, exactly as if it had been in the imported
            // CSV all along (see this class's own header comment).
            m_database.upsertImportedLocator(callsign, result.grid, QString());
            emit externalLookupFinished(callsign, true, result.grid);
            return;
        }
        if (result.sessionInvalid && !retryAttempted) {
            // Session expired/invalid -- re-login once, transparently,
            // then retry this same lookup. A second failure
            // (retryAttempted already true) falls through to "not
            // found" below instead of looping forever.
            m_sessionKey.clear();
            startSessionRequest(callsign, /*retryAttempted=*/true);
            return;
        }
        emit externalLookupFinished(callsign, false, QString());
    });
}

CallsignLocatorLookup::SessionResult CallsignLocatorLookup::parseQrzSessionXml(const QString& xml)
{
    SessionResult result;
    const QString key = firstElementText(xml, QStringLiteral("Key"));
    if (!key.isEmpty()) {
        result.ok = true;
        result.sessionKey = key;
        return result;
    }
    result.error = firstElementText(xml, QStringLiteral("Error"));
    if (result.error.isEmpty()) {
        result.error = QStringLiteral("Unerwartete QRZ-Antwort (kein Session-Key)");
    }
    return result;
}

CallsignLocatorLookup::GridLookupResult CallsignLocatorLookup::parseQrzLookupXml(const QString& xml)
{
    GridLookupResult result;
    const QString grid = firstElementText(xml, QStringLiteral("grid"));
    if (!grid.isEmpty()) {
        result.found = true;
        result.grid = grid;
        return result;
    }
    result.error = firstElementText(xml, QStringLiteral("Error"));
    if (result.error.contains(QStringLiteral("session"), Qt::CaseInsensitive)) {
        result.sessionInvalid = true;
    }
    return result;
}

CallsignLocatorLookup::SessionResult CallsignLocatorLookup::parseHamQthSessionXml(const QString& xml)
{
    SessionResult result;
    const QString key = firstElementText(xml, QStringLiteral("session_id"));
    if (!key.isEmpty()) {
        result.ok = true;
        result.sessionKey = key;
        return result;
    }
    result.error = firstElementText(xml, QStringLiteral("error"));
    if (result.error.isEmpty()) {
        result.error = QStringLiteral("Unerwartete HamQTH-Antwort (keine session_id)");
    }
    return result;
}

CallsignLocatorLookup::GridLookupResult CallsignLocatorLookup::parseHamQthLookupXml(const QString& xml)
{
    GridLookupResult result;
    const QString grid = firstElementText(xml, QStringLiteral("grid"));
    if (!grid.isEmpty()) {
        result.found = true;
        result.grid = grid;
        return result;
    }
    result.error = firstElementText(xml, QStringLiteral("error"));
    if (result.error.contains(QStringLiteral("session"), Qt::CaseInsensitive)
        || result.error.contains(QStringLiteral("expired"), Qt::CaseInsensitive)) {
        result.sessionInvalid = true;
    }
    return result;
}

CallsignLocatorLookup::CsvParseResult CallsignLocatorLookup::parseCsv(const QString& csvText)
{
    CsvParseResult result;
    const QStringList lines = csvText.split(QRegularExpression(QStringLiteral("\r\n|\n|\r")), Qt::SkipEmptyParts);
    for (const QString& rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const QStringList fields = line.split(QLatin1Char(','));
        if (fields.size() < 2) {
            result.skipped++;
            continue;
        }
        const QString callsign = fields.at(0).trimmed().toUpper();
        const QString grid = fields.at(1).trimmed().toUpper();
        // isValidGridSquare doubles as the header-row filter: a header
        // like "callsign,grid,name" or "CALL,LOCATOR" has a second field
        // that is not a valid Maidenhead locator either, so it is
        // skipped here exactly like any other malformed data line -- no
        // separate "is this a header" special case needed.
        if (callsign.isEmpty() || !isValidGridSquare(grid)) {
            result.skipped++;
            continue;
        }
        CsvRow row;
        row.callsign = callsign;
        row.grid = grid;
        if (fields.size() >= 3) {
            row.name = fields.at(2).trimmed();
        }
        result.rows.append(row);
    }
    return result;
}

CallsignLocatorLookup::ImportSummary CallsignLocatorLookup::importCsvFile(const QString& filePath, QString* errorOut)
{
    ImportSummary summary;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorOut) {
            *errorOut = file.errorString();
        }
        return summary;
    }
    const QString text = QString::fromUtf8(file.readAll());
    file.close();

    const CsvParseResult parsed = parseCsv(text);
    for (const CsvRow& row : parsed.rows) {
        m_database.upsertImportedLocator(row.callsign, row.grid, row.name);
    }
    summary.imported = parsed.rows.size();
    summary.skipped = parsed.skipped;
    return summary;
}

} // namespace Contestprogramm
