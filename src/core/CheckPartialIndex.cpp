#include "core/CheckPartialIndex.h"

#include <algorithm>
#include <cstdlib>

namespace Contestprogramm {

namespace {

QString normalizeCall(const QString& call)
{
    return call.trimmed().toUpper();
}

QString normalizeGrid(const QString& grid)
{
    return grid.trimmed().toUpper();
}

constexpr int kMinPartialLength = 2;
constexpr int kNearMissMinLength = 4;

} // namespace

void CheckPartialIndex::setLogCalls(const QVector<QPair<QString, QString>>& callAndBand)
{
    m_logCalls = callAndBand;
    rebuildKnown();
}

void CheckPartialIndex::setHistoryCalls(const QHash<QString, QString>& callToGrid)
{
    m_history.clear();
    for (auto it = callToGrid.constBegin(); it != callToGrid.constEnd(); ++it) {
        const QString call = normalizeCall(it.key());
        if (!call.isEmpty()) {
            m_history.insert(call, normalizeGrid(it.value()));
        }
    }
    rebuildKnown();
}

void CheckPartialIndex::setScpCalls(const QStringList& calls)
{
    m_scp.clear();
    m_scpSet.clear();
    for (const QString& raw : calls) {
        const QString call = normalizeCall(raw);
        if (!call.isEmpty() && !m_scpSet.contains(call)) {
            m_scpSet.insert(call);
            m_scp << call;
        }
    }
}

void CheckPartialIndex::addSeenCall(const QString& callsign, const QString& grid)
{
    const QString call = normalizeCall(callsign);
    if (call.isEmpty()) {
        return;
    }
    const QString g = normalizeGrid(grid);
    auto it = m_seenGrid.find(call);
    if (it == m_seenGrid.end()) {
        m_seenGrid.insert(call, g);
    } else if (!g.isEmpty()) {
        it.value() = g;
    }
    Known& known = m_known[call];
    known.sources |= CheckPartialMatch::Seen;
    if (known.grid.isEmpty() && !g.isEmpty()) {
        known.grid = g;
    }
}

void CheckPartialIndex::clearSeen()
{
    m_seenGrid.clear();
    rebuildKnown();
}

void CheckPartialIndex::rebuildKnown()
{
    m_known.clear();
    // Log first: a locator actually received in a QSO beats any list.
    for (const auto& [rawCall, band] : m_logCalls) {
        const QString call = normalizeCall(rawCall);
        if (call.isEmpty()) {
            continue;
        }
        Known& known = m_known[call];
        known.sources |= CheckPartialMatch::Log;
        known.bands.insert(band.trimmed());
    }
    for (auto it = m_history.constBegin(); it != m_history.constEnd(); ++it) {
        Known& known = m_known[it.key()];
        known.sources |= CheckPartialMatch::History;
        if (known.grid.isEmpty()) {
            known.grid = it.value();
        }
    }
    for (auto it = m_seenGrid.constBegin(); it != m_seenGrid.constEnd(); ++it) {
        Known& known = m_known[it.key()];
        known.sources |= CheckPartialMatch::Seen;
        if (known.grid.isEmpty()) {
            known.grid = it.value();
        }
    }
}

QVector<CheckPartialMatch> CheckPartialIndex::matches(const QString& partial, const QString& currentBand,
                                                      int maxResults) const
{
    const QString fragment = normalizeCall(partial);
    QVector<CheckPartialMatch> result;
    if (fragment.size() < kMinPartialLength || maxResults <= 0) {
        return result;
    }
    const QString band = currentBand.trimmed();
    const bool nearMisses = fragment.size() >= kNearMissMinLength;

    // rank: 0 prefix, 1 substring, 2 near miss; then own-knowledge
    // before SCP-only; then alphabetical.
    struct Ranked {
        int rank;
        bool scpOnly;
        CheckPartialMatch match;
    };
    QVector<Ranked> ranked;
    QSet<QString> emitted;

    const auto consider = [&](const QString& call, const Known* known) {
        if (emitted.contains(call)) {
            return;
        }
        int rank = -1;
        if (call.startsWith(fragment)) {
            rank = 0;
        } else if (call.contains(fragment)) {
            rank = 1;
        } else if (nearMisses && isOneEditApart(call, fragment)) {
            rank = 2;
        }
        if (rank < 0) {
            return;
        }
        emitted.insert(call);
        CheckPartialMatch match;
        match.callsign = call;
        match.nearMiss = rank == 2;
        if (known) {
            match.grid = known->grid;
            match.sources = known->sources;
            match.workedThisBand = !band.isEmpty() && known->bands.contains(band);
        }
        if (m_scpSet.contains(call)) {
            match.sources |= CheckPartialMatch::Scp;
        }
        ranked.append({rank, known == nullptr, match});
    };

    for (auto it = m_known.constBegin(); it != m_known.constEnd(); ++it) {
        consider(it.key(), &it.value());
    }
    for (const QString& call : m_scp) {
        if (!m_known.contains(call)) {
            consider(call, nullptr);
        }
    }

    std::stable_sort(ranked.begin(), ranked.end(), [](const Ranked& a, const Ranked& b) {
        if (a.rank != b.rank) {
            return a.rank < b.rank;
        }
        if (a.scpOnly != b.scpOnly) {
            return !a.scpOnly;
        }
        return a.match.callsign < b.match.callsign;
    });

    for (const Ranked& entry : ranked) {
        result.append(entry.match);
        if (result.size() >= maxResults) {
            break;
        }
    }
    return result;
}

QStringList CheckPartialIndex::parseScp(const QByteArray& data)
{
    QStringList calls;
    QSet<QString> seen;
    for (const QByteArray& rawLine : data.split('\n')) {
        const QString line = QString::fromLatin1(rawLine).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        // Some lists carry trailing notes after whitespace; the call is
        // the first token.
        const QString call = line.section(QLatin1Char(' '), 0, 0).toUpper();
        if (call.isEmpty() || seen.contains(call)) {
            continue;
        }
        seen.insert(call);
        calls << call;
    }
    return calls;
}

bool CheckPartialIndex::isOneEditApart(const QString& a, const QString& b)
{
    const int la = a.size();
    const int lb = b.size();
    if (std::abs(la - lb) > 1 || (la == lb && a == b)) {
        return false;
    }
    if (la == lb) {
        int differences = 0;
        for (int i = 0; i < la; ++i) {
            if (a.at(i) != b.at(i) && ++differences > 1) {
                return false;
            }
        }
        return differences == 1;
    }
    // One insertion/deletion: walk both, allow exactly one skip in the
    // longer string.
    const QString& longer = la > lb ? a : b;
    const QString& shorter = la > lb ? b : a;
    int i = 0;
    int j = 0;
    bool skipped = false;
    while (i < longer.size() && j < shorter.size()) {
        if (longer.at(i) == shorter.at(j)) {
            ++i;
            ++j;
        } else if (!skipped) {
            skipped = true;
            ++i;
        } else {
            return false;
        }
    }
    return true;
}

} // namespace Contestprogramm
