#include "data/DupeRescore.h"

#include "data/QsoRecord.h"

#include <QDateTime>
#include <QSet>

#include <algorithm>
#include <limits>

namespace Contestprogramm {

QVector<DupeFlagChange> recomputeDupeFlags(const QVector<QsoRecord>& records, const QStringList& dupeScope)
{
    QVector<const QsoRecord*> active;
    for (const QsoRecord& record : records) {
        if (!record.isInvalid) {
            active.append(&record);
        }
    }
    const auto sortKey = [](const QsoRecord* record) {
        const QDateTime when = QDateTime::fromString(record->timestampUtc, Qt::ISODate);
        return when.isValid() ? when.toMSecsSinceEpoch() : std::numeric_limits<qint64>::max();
    };
    std::stable_sort(active.begin(), active.end(), [&](const QsoRecord* a, const QsoRecord* b) {
        const qint64 ka = sortKey(a);
        const qint64 kb = sortKey(b);
        return ka != kb ? ka < kb : a->id < b->id;
    });

    // Same normalisation DupeChecker::isDupe applies: callsign trimmed
    // and upper-cased, band/mode compared without regard to case.
    QSet<QString> seen;
    QVector<DupeFlagChange> changes;
    for (const QsoRecord* record : active) {
        QStringList parts;
        for (const QString& scope : dupeScope) {
            if (scope == QStringLiteral("callsign")) {
                parts << record->callsign.trimmed().toUpper();
            } else if (scope == QStringLiteral("band")) {
                parts << record->band.trimmed().toUpper();
            } else if (scope == QStringLiteral("mode")) {
                parts << record->mode.trimmed().toUpper();
            }
        }
        const QString key = parts.join(QLatin1Char('|'));
        const bool dupe = seen.contains(key);
        seen.insert(key);
        if (dupe != record->isDupe) {
            changes.append({record->id, dupe});
        }
    }
    return changes;
}

} // namespace Contestprogramm
