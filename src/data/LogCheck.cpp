#include "data/LogCheck.h"

#include "app/ContestSettings.h"
#include "core/BandUtils.h"
#include "core/Maidenhead.h"
#include "data/ContestDefinition.h"
#include "data/QsoRecord.h"

#include <QHash>
#include <QLocale>
#include <QMap>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Contestprogramm {

namespace {

using Severity = LogCheckIssue::Severity;

QDateTime parseUtc(const QString& iso)
{
    QDateTime when = QDateTime::fromString(iso, Qt::ISODate);
    if (when.isValid()) {
        when = when.toUTC();
    }
    return when;
}

QString upperTrimmed(const QString& text)
{
    return text.trimmed().toUpper();
}

QString grouped(qint64 value)
{
    return QLocale(QLocale::German, QLocale::Austria).toString(value);
}

QString serialText(int serial)
{
    return QStringLiteral("%1").arg(serial, 3, 10, QLatin1Char('0'));
}

// A callsign the way every real one looks: letters/digits (one '/'
// section allowed for portable/mobile suffixes and prefixes), at least
// one digit and one letter, sensible length. Loose on purpose -- this
// is a "did you fat-finger it" warning, not a licensing database.
bool callsignLooksSane(const QString& call)
{
    static const QRegularExpression shape(QStringLiteral("^[A-Z0-9]+(/[A-Z0-9]+){0,2}$"));
    if (call.size() < 3 || call.size() > 15 || !shape.match(call).hasMatch()) {
        return false;
    }
    bool digit = false;
    bool letter = false;
    for (const QChar& c : call) {
        digit = digit || c.isDigit();
        letter = letter || c.isLetter();
    }
    return digit && letter;
}

// RST as the mode wants it: three digits in CW (and the digital modes
// that report tone), two in phone; unknown mode accepts either.
bool rstLooksSane(const QString& rst, const QString& mode)
{
    static const QRegularExpression phone(QStringLiteral("^[1-5][1-9]$"));
    static const QRegularExpression cw(QStringLiteral("^[1-5][1-9][1-9]$"));
    const bool isCw = mode == QStringLiteral("CW") || mode == QStringLiteral("RTTY") || mode == QStringLiteral("PSK");
    const bool isPhone = mode == QStringLiteral("SSB") || mode == QStringLiteral("FM") || mode == QStringLiteral("AM");
    if (isCw) {
        return cw.match(rst).hasMatch();
    }
    if (isPhone) {
        return phone.match(rst).hasMatch();
    }
    return cw.match(rst).hasMatch() || phone.match(rst).hasMatch();
}

double distanceKmFor(const QsoRecord& record, const QString& ownGrid)
{
    if (record.distanceKm) {
        return *record.distanceKm;
    }
    if (isValidGridSquare(ownGrid) && isValidGridSquare(record.gridSquare)) {
        return calculateDistanceKm(ownGrid, record.gridSquare);
    }
    return -1.0;
}

struct Collector {
    LogCheckResult result;

    void add(Severity severity, const QsoRecord* record, const QString& code, const QString& message,
             const QString& band = QString())
    {
        LogCheckIssue issue;
        issue.severity = severity;
        issue.code = code;
        issue.message = message;
        if (record) {
            issue.qsoId = record->id;
            issue.timestampUtc = record->timestampUtc;
            issue.callsign = upperTrimmed(record->callsign);
            issue.band = record->band;
        } else {
            issue.band = band;
        }
        result.issues.append(issue);
        switch (severity) {
        case Severity::Error: ++result.errors; break;
        case Severity::Warning: ++result.warnings; break;
        case Severity::Hint: ++result.hints; break;
        }
    }
};

} // namespace

QString LogCheckResult::countsText() const
{
    const auto count = [](int n, const QString& one, const QString& many) {
        return QStringLiteral("%1 %2").arg(n).arg(n == 1 ? one : many);
    };
    return QStringLiteral("%1, %2, %3")
        .arg(count(errors, QStringLiteral("Fehler"), QStringLiteral("Fehler")),
             count(warnings, QStringLiteral("Warnung"), QStringLiteral("Warnungen")),
             count(hints, QStringLiteral("Hinweis"), QStringLiteral("Hinweise")));
}

LogCheckContext logCheckContextFor(const ContestDefinition& definition,
                                   const ContestSettings& settings,
                                   const QVector<QsoRecord>& records,
                                   const QDateTime& nowUtc)
{
    LogCheckContext context;
    context.ownCallsign = upperTrimmed(settings.ownCallsign);
    context.ownGrid = upperTrimmed(settings.ownGrid);
    context.bands = definition.bands();
    context.modes = definition.modes();
    context.dupeScope = definition.dupeScope();
    context.serialScope = definition.serialScope();
    context.hasSerialField = false;
    context.hasGridField = false;
    context.hasRstField = false;
    for (const ContestDefinition::ExchangeField& field : definition.exchangeFields()) {
        context.hasSerialField = context.hasSerialField || field.autoIncrement;
        context.hasGridField = context.hasGridField || field.type.startsWith(QStringLiteral("grid"));
        context.hasRstField = context.hasRstField || field.type == QStringLiteral("rst");
    }

    // The period nearest to the log itself: the earliest valid QSO
    // decides, so a November check of the October log still measures
    // against October, not against next year's contest.
    QDateTime earliest;
    for (const QsoRecord& record : records) {
        if (record.isInvalid) {
            continue;
        }
        const QDateTime when = parseUtc(record.timestampUtc);
        if (when.isValid() && (!earliest.isValid() || when < earliest)) {
            earliest = when;
        }
    }
    context.window = effectiveContestWindow(definition.schedule(), settings.contestEndUtc,
                                            earliest.isValid() ? earliest : nowUtc);
    return context;
}

LogCheckResult checkLog(const QVector<QsoRecord>& records, const LogCheckContext& context)
{
    Collector out;

    // Log order = time order, ids as the tie-breaker; hand-edited times
    // move a QSO in this order, which is what the serial tests look at.
    QVector<const QsoRecord*> active;
    int invalidCount = 0;
    int dupeCount = 0;
    for (const QsoRecord& record : records) {
        if (record.isInvalid) {
            ++invalidCount;
            continue;
        }
        if (record.isDupe) {
            ++dupeCount;
        }
        active.append(&record);
    }
    const auto sortKey = [](const QsoRecord* record) {
        const QDateTime when = parseUtc(record->timestampUtc);
        // An unreadable time sorts last; it is reported on its own.
        return when.isValid() ? when.toMSecsSinceEpoch() : std::numeric_limits<qint64>::max();
    };
    std::stable_sort(active.begin(), active.end(), [&](const QsoRecord* a, const QsoRecord* b) {
        const qint64 ka = sortKey(a);
        const qint64 kb = sortKey(b);
        if (ka != kb) {
            return ka < kb;
        }
        return a->id < b->id;
    });
    out.result.checkedQsos = active.size();

    // --- Per-QSO tests -------------------------------------------------
    QHash<QString, QVector<const QsoRecord*>> byCall; // valid, for the locator-consistency test
    QSet<QString> dupeKeys;
    for (const QsoRecord* record : active) {
        const QString call = upperTrimmed(record->callsign);
        const QString grid = upperTrimmed(record->gridSquare);
        const QString mode = upperTrimmed(record->mode);
        const QDateTime when = parseUtc(record->timestampUtc);

        if (!call.isEmpty() && call == context.ownCallsign) {
            out.add(Severity::Error, record, QStringLiteral("own_call"),
                    QStringLiteral("Eigenes Rufzeichen geloggt"));
        } else if (!callsignLooksSane(call)) {
            out.add(Severity::Warning, record, QStringLiteral("callsign_odd"),
                    QStringLiteral("Rufzeichen sieht falsch aus: \"%1\"").arg(call));
        }

        // A dupe scores nothing whatever its exchange says, so only a
        // scoring QSO is held to the exchange rules.
        if (context.hasGridField && !record->isDupe) {
            if (grid.isEmpty()) {
                out.add(Severity::Error, record, QStringLiteral("no_locator"),
                        QStringLiteral("Kein Locator -- das QSO zählt 0 Punkte"));
            } else if (!isValidGridSquare(grid) || grid.size() != 6) {
                out.add(Severity::Error, record, QStringLiteral("bad_locator"),
                        QStringLiteral("Locator \"%1\" ist kein 6-stelliger Locator").arg(grid));
            } else {
                const double km = distanceKmFor(*record, context.ownGrid);
                if (km > context.implausibleKm) {
                    out.add(Severity::Warning, record, QStringLiteral("distance_implausible"),
                            QStringLiteral("%1 km -- Locator prüfen").arg(grouped(static_cast<qint64>(std::floor(km)))));
                }
                byCall[call].append(record);
            }
        }

        if (context.hasSerialField && !record->isDupe && !record->serialRcvd) {
            out.add(Severity::Error, record, QStringLiteral("serial_rcvd_missing"),
                    QStringLiteral("Keine empfangene Nummer -- unvollständiger Exchange"));
        }

        if (context.hasRstField && !record->isDupe) {
            const QString rst = upperTrimmed(record->rstRcvd);
            if (rst.isEmpty()) {
                out.add(Severity::Warning, record, QStringLiteral("rst_missing"),
                        QStringLiteral("Kein empfangener RST"));
            } else if (!rstLooksSane(rst, mode)) {
                out.add(Severity::Warning, record, QStringLiteral("rst_odd"),
                        QStringLiteral("RST \"%1\" passt nicht zu %2").arg(rst, mode.isEmpty() ? QStringLiteral("der Betriebsart") : mode));
            }
        }

        if (!context.modes.isEmpty() && !context.modes.contains(mode)) {
            out.add(Severity::Error, record, QStringLiteral("mode_not_allowed"),
                    QStringLiteral("Betriebsart %1 ist in diesem Contest nicht erlaubt (%2)")
                        .arg(mode.isEmpty() ? QStringLiteral("?") : mode, context.modes.join(QStringLiteral("/"))));
        }

        if (!context.bands.isEmpty() && !context.bands.contains(record->band)) {
            out.add(Severity::Warning, record, QStringLiteral("band_not_in_contest"),
                    QStringLiteral("Band %1 gehört nicht zu diesem Contest").arg(record->band));
        }
        if (record->freqHz && *record->freqHz > 0) {
            const QString freqBand = bandLabelForFrequencyHz(*record->freqHz);
            if (!freqBand.isEmpty() && freqBand != record->band) {
                out.add(Severity::Warning, record, QStringLiteral("freq_band_mismatch"),
                        QStringLiteral("Frequenz %1 MHz liegt auf %2, geloggt auf %3")
                            .arg(QString::number(*record->freqHz / 1e6, 'f', 3), freqBand, record->band));
            }
        }

        if (!when.isValid()) {
            out.add(Severity::Error, record, QStringLiteral("bad_time"),
                    QStringLiteral("Zeit \"%1\" nicht lesbar").arg(record->timestampUtc));
        } else if (context.window.isValid() && !context.window.contains(when)) {
            out.add(Severity::Error, record, QStringLiteral("outside_window"),
                    QStringLiteral("Außerhalb des Contests (%1)").arg(context.window.describe()));
        }

        // A second QSO with the same key that is not marked as a dupe --
        // a callsign hand-corrected into an existing one, typically.
        if (!record->isDupe && !context.dupeScope.isEmpty()) {
            QStringList keyParts;
            for (const QString& scope : context.dupeScope) {
                if (scope == QStringLiteral("callsign")) { keyParts << call; }
                else if (scope == QStringLiteral("band")) { keyParts << record->band; }
                else if (scope == QStringLiteral("mode")) { keyParts << mode; }
            }
            const QString key = keyParts.join(QLatin1Char('|'));
            if (dupeKeys.contains(key)) {
                out.add(Severity::Warning, record, QStringLiteral("unmarked_dupe"),
                        QStringLiteral("Schon im Log auf %1, nicht als Dupe markiert").arg(record->band));
            } else {
                dupeKeys.insert(key);
            }
        }
    }

    // --- Same call, different locators ---------------------------------
    for (auto it = byCall.cbegin(); it != byCall.cend(); ++it) {
        QMap<QString, int> gridCounts;
        for (const QsoRecord* record : it.value()) {
            ++gridCounts[upperTrimmed(record->gridSquare)];
        }
        if (gridCounts.size() < 2) {
            continue;
        }
        // The locator logged most often is presumed right; on a tie the
        // earlier QSO is. Every other QSO of that call gets the warning.
        QString presumed;
        int presumedCount = -1;
        for (const QsoRecord* record : it.value()) {
            const QString grid = upperTrimmed(record->gridSquare);
            if (gridCounts.value(grid) > presumedCount) {
                presumed = grid;
                presumedCount = gridCounts.value(grid);
            }
        }
        for (const QsoRecord* record : it.value()) {
            const QString grid = upperTrimmed(record->gridSquare);
            if (grid != presumed) {
                out.add(Severity::Warning, record, QStringLiteral("locator_inconsistent"),
                        QStringLiteral("Sonst mit %1 geloggt, hier %2 -- einer stimmt nicht").arg(presumed, grid));
            }
        }
    }

    // --- Sent serials: unique, ascending in time, no unexplained gaps ---
    if (context.hasSerialField) {
        QMap<QString, QVector<const QsoRecord*>> byScope;
        for (const QsoRecord* record : active) {
            const QString scope = context.serialScope == QStringLiteral("contest") ? QString() : record->band;
            byScope[scope].append(record);
        }
        // Numbers consumed by invalidated QSOs explain a gap.
        QSet<QString> consumedByInvalid;
        for (const QsoRecord& record : records) {
            if (record.isInvalid && record.serialSent) {
                const QString scope = context.serialScope == QStringLiteral("contest") ? QString() : record.band;
                consumedByInvalid.insert(scope + QLatin1Char('|') + QString::number(*record.serialSent));
            }
        }
        for (auto it = byScope.cbegin(); it != byScope.cend(); ++it) {
            const QString scopeLabel = it.key().isEmpty() ? QString() : QStringLiteral(" auf %1").arg(it.key());
            QHash<int, const QsoRecord*> firstWithSerial;
            int maxSerial = 0;
            // The serials in time order, minus the ones already reported
            // (no number, a second use of a number, an unreadable time).
            QVector<const QsoRecord*> sequence;
            for (const QsoRecord* record : it.value()) {
                if (!record->serialSent) {
                    out.add(Severity::Error, record, QStringLiteral("serial_sent_missing"),
                            QStringLiteral("Keine gesendete Nummer"));
                    continue;
                }
                const int serial = *record->serialSent;
                maxSerial = std::max(maxSerial, serial);
                if (const QsoRecord* earlier = firstWithSerial.value(serial, nullptr)) {
                    out.add(Severity::Error, record, QStringLiteral("serial_sent_duplicate"),
                            QStringLiteral("Nummer %1%2 zweimal gesendet (auch an %3)")
                                .arg(serialText(serial), scopeLabel, upperTrimmed(earlier->callsign)));
                    continue;
                }
                firstWithSerial.insert(serial, record);
                if (parseUtc(record->timestampUtc).isValid()) {
                    sequence.append(record);
                }
            }
            // Out of order = not part of the longest ascending run: one
            // QSO with a mistyped time (or number) is reported once, not
            // every QSO that happens to follow it. Ties go to the
            // earlier QSO, so 1 2 4 3 blames the 3.
            const int n = sequence.size();
            QVector<int> runFrom(n, 1);
            for (int i = n - 2; i >= 0; --i) {
                for (int j = i + 1; j < n; ++j) {
                    if (*sequence[j]->serialSent > *sequence[i]->serialSent) {
                        runFrom[i] = std::max(runFrom[i], runFrom[j] + 1);
                    }
                }
            }
            QSet<const QsoRecord*> inRun;
            int need = 0;
            for (int i = 0; i < n; ++i) {
                need = std::max(need, runFrom[i]);
            }
            int lastSerial = 0;
            for (int i = 0; i < n && need > 0; ++i) {
                if (runFrom[i] == need && *sequence[i]->serialSent > lastSerial) {
                    inRun.insert(sequence[i]);
                    lastSerial = *sequence[i]->serialSent;
                    --need;
                }
            }
            for (const QsoRecord* record : sequence) {
                if (!inRun.contains(record)) {
                    out.add(Severity::Warning, record, QStringLiteral("serial_sent_order"),
                            QStringLiteral("Nummer %1 passt nicht in die Reihenfolge -- Zeit oder Nummer prüfen")
                                .arg(serialText(*record->serialSent)));
                }
            }
            QStringList gaps;
            for (int serial = 1; serial <= maxSerial; ++serial) {
                if (firstWithSerial.contains(serial)
                    || consumedByInvalid.contains(it.key() + QLatin1Char('|') + QString::number(serial))) {
                    continue;
                }
                gaps << serialText(serial);
            }
            if (!gaps.isEmpty()) {
                const QString shown = gaps.size() > 6 ? QStringList(gaps.mid(0, 6)).join(QStringLiteral(", ")) + QStringLiteral(" …")
                                                      : gaps.join(QStringLiteral(", "));
                out.add(Severity::Hint, nullptr, QStringLiteral("serial_sent_gap"),
                        (gaps.size() == 1 ? QStringLiteral("Gesendete Nummer %2 fehlt%1 (lückenlos ist üblich, kein Abzug)")
                                          : QStringLiteral("Gesendete Nummern fehlen%1: %2 (lückenlos ist üblich, kein Abzug)"))
                            .arg(scopeLabel, shown),
                        it.key());
            }
        }
    }

    // --- Log-wide hints -------------------------------------------------
    if (dupeCount > 0) {
        out.add(Severity::Hint, nullptr, QStringLiteral("dupes"),
                QStringLiteral("%1 Dupe%2 im Log (zählen 0 Punkte, bleiben drin)")
                    .arg(dupeCount).arg(dupeCount == 1 ? QString() : QStringLiteral("s")));
    }
    if (invalidCount > 0) {
        out.add(Severity::Hint, nullptr, QStringLiteral("invalid"),
                invalidCount == 1 ? QStringLiteral("1 ungültig markiertes QSO (wird nicht exportiert)")
                                  : QStringLiteral("%1 ungültig markierte QSOs (werden nicht exportiert)").arg(invalidCount));
    }
    if (!context.window.isValid()) {
        out.add(Severity::Hint, nullptr, QStringLiteral("no_window"),
                QStringLiteral("Contestzeitraum unbekannt (Definition ohne Zeitplan, kein Contest-Ende gesetzt) -- "
                               "QSO-Zeiten nicht geprüft"));
    }
    if (active.isEmpty()) {
        out.add(Severity::Hint, nullptr, QStringLiteral("empty"), QStringLiteral("Keine QSOs im Log"));
    }

    // Errors first, then warnings, then hints; within a group the log
    // order from above (log-wide entries after the QSOs of their group).
    std::stable_sort(out.result.issues.begin(), out.result.issues.end(),
                     [](const LogCheckIssue& a, const LogCheckIssue& b) {
                         if (a.severity != b.severity) {
                             return static_cast<int>(a.severity) < static_cast<int>(b.severity);
                         }
                         if ((a.qsoId < 0) != (b.qsoId < 0)) {
                             return a.qsoId >= 0;
                         }
                         if (a.timestampUtc != b.timestampUtc) {
                             return a.timestampUtc < b.timestampUtc;
                         }
                         return a.qsoId < b.qsoId;
                     });
    return out.result;
}

} // namespace Contestprogramm
