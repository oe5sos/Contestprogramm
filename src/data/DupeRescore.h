#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace Contestprogramm {

struct QsoRecord;

// N1MM+'s "Rescore" / DXLog.net's automatic dupe re-evaluation, for the
// one thing that changes a dupe flag after the fact: a correction. The
// flag is set when a QSO is logged (DupeChecker), against the log as it
// was then. Correct a callsign afterwards and the flag is stale both
// ways -- "DL1ABX" fixed to "DL1ABC" is now the second DL1ABC on the
// band and scores twice in the EDI; a QSO marked dupe of DL1ABC that
// turns out to have been DL1ABD keeps its flag and silently loses its
// points. Likewise a QSO marked invalid frees the callsign for the next
// one, and a time edit can swap which of two is "first".
//
// Pure: given the log and the definition's dupe_scope, the first
// non-invalid QSO per key (in time order, ids as tie-breaker) is the
// valid one, every later one a dupe; only the records whose stored flag
// disagrees are returned. Invalid QSOs are neither counted nor changed.
struct DupeFlagChange {
    int qsoId = -1;
    bool isDupe = false;
};

QVector<DupeFlagChange> recomputeDupeFlags(const QVector<QsoRecord>& records, const QStringList& dupeScope);

} // namespace Contestprogramm
