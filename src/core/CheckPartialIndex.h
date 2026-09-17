#pragma once

#include <QByteArray>
#include <QHash>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Contestprogramm {

// "Check Partial" the way N1MM+'s Check window and DXLog.net's Check
// Partial window do it: while the callsign field is being typed, list
// every callsign that contains the fragment, from the sources that
// matter in a VHF/UHF contest --
//   Log      worked in this contest (on some band; flagged when it is
//            the current band, i.e. a dupe)
//   History  the call-history/locator table (N1MM's "Call History
//            File" concept, ContestDatabase::imported_locators)
//   Seen     heard on ON4KST/cluster during this session
//   Scp      a Super Check Partial list (master.scp / a VHF SCP file)
// plus N1MM's "N+1": once four characters are typed, calls one edit
// away (a busted or mis-typed character) are listed too, marked as
// near misses.
//
// Pure data + a query, no widgets and no database handle: the sources
// are pushed in (setLogCalls/setHistoryCalls/setScpCalls/addSeenCall)
// and matches() is a plain function over them, so it is unit-testable
// and cheap enough to run on every keystroke -- a full master.scp is
// ~40 000 short strings, a linear scan of that is well under a
// millisecond of the 15 ms a keystroke has.
struct CheckPartialMatch {
    enum Source { Log = 1, History = 2, Seen = 4, Scp = 8 };
    QString callsign;
    QString grid;        // from History/Seen/Log when any of them knows it
    int sources = 0;     // OR of Source flags
    bool workedThisBand = false; // logged on the queried band already
    QStringList workedBands;     // every band this call is in the log on, sorted
    bool nearMiss = false;       // matched by N+1, not as a substring
};

class CheckPartialIndex {
public:
    // Calls worked in this contest, with the band each was worked on
    // (one pair per QSO; duplicates are fine).
    void setLogCalls(const QVector<QPair<QString, QString>>& callAndBand);
    // Call history: callsign -> locator (locator may be empty).
    void setHistoryCalls(const QHash<QString, QString>& callToGrid);
    // Super Check Partial list, already parsed (see parseScp()).
    void setScpCalls(const QStringList& calls);
    // A call heard on ON4KST/cluster; a later grid overwrites an empty one.
    void addSeenCall(const QString& callsign, const QString& grid = QString());
    void clearSeen();

    int scpCount() const { return m_scp.size(); }
    int historyCount() const { return m_history.size(); }
    int seenCount() const { return m_seenGrid.size(); }

    // Matches for `partial` (case-insensitive, at least two characters,
    // else nothing), ranked: prefix matches, then other substring
    // matches, then near misses; within a group calls this station
    // has some own knowledge of (Log/History/Seen) before SCP-only
    // ones, then alphabetically. At most `maxResults`.
    QVector<CheckPartialMatch> matches(const QString& partial, const QString& currentBand, int maxResults = 60) const;

    // master.scp / VHF.scp format: one callsign per line, '#' comment
    // lines, blank lines, any line ending. Upper-cased, de-duplicated,
    // in file order.
    static QStringList parseScp(const QByteArray& data);

    // True when `a` and `b` differ by exactly one substitution,
    // insertion or deletion -- N1MM's "N+1".
    static bool isOneEditApart(const QString& a, const QString& b);

private:
    struct Known {
        QString grid;
        int sources = 0;
        QSet<QString> bands; // Log only
    };
    QHash<QString, Known> m_known; // Log + History + Seen, keyed by call
    QStringList m_scp;
    QSet<QString> m_scpSet;
    QVector<QPair<QString, QString>> m_logCalls;
    QHash<QString, QString> m_history;
    QHash<QString, QString> m_seenGrid;

    void rebuildKnown();
};

} // namespace Contestprogramm
