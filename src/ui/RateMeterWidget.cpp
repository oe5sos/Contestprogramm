#include "ui/RateMeterWidget.h"

#include "data/ContestDatabase.h"
#include "ui/StyleKit.h"

#include <QDateTime>
#include <QMap>
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>

#include <algorithm>

namespace Contestprogramm {

namespace {
// No push-exact-second precision needed for a rate display -- see the
// plan's UI section.
constexpr int kRefreshIntervalMs = 15000;

// A caps label beside a monospace number, as one rich-text run --
// the same technique Longpath's FrequencyInstrument::refreshVfoRow()
// uses to mix a caps label and a value inside one QLabel. Unlike a
// stylesheet set on a *container* (see StyleKit.h's note on why sizing
// never goes through setStyleSheet() there), a font-size inside a leaf
// QLabel's own rich-text *content* has no descendant widgets to leak
// onto -- it only describes this one label's own glyphs, so it is safe
// here.
QString readoutSpan(const QString& label, const QString& value, const QString& valueColor)
{
    return QStringLiteral(
               "<span style='color:%1; font-size:%2px;'>%3</span>"
               "&nbsp;<span style='font-family:Menlo; font-size:%4px; color:%5;'>%6</span>")
        .arg(Style::kTextScale())
        .arg(Style::kFontCaption)
        .arg(label.toUpper())
        .arg(Style::kFontSub)
        .arg(valueColor)
        .arg(value);
}

// The rate trend glyph -- shape carries the meaning (▲/▼/—), not
// colour: up/down is a neutral reading, not a warning, so it stays in
// the same "measured value" amber family the numbers themselves and
// RotorWidget's own needle already use (StyleKit.h's kAmberText) rather
// than a red/green good/bad pairing. Deliberate: this codebase reserves
// kRedText for actual warnings (DUPE/UNGÜLTIG pills, terrain-blocked
// -- HAUSSTIL "Rot bleibt der Warnung"), and "fewer QSOs than the
// previous 10 minutes" is not one.
QString trendGlyph(RateBreakdown::Trend trend)
{
    switch (trend) {
    case RateBreakdown::Trend::Up: return QStringLiteral("&nbsp;&#9650;"); // ▲
    case RateBreakdown::Trend::Down: return QStringLiteral("&nbsp;&#9660;"); // ▼
    case RateBreakdown::Trend::Flat: return QStringLiteral("&nbsp;&mdash;");
    }
    return QString();
}

// A compact "144: 3   432: 2" style breakdown line -- a caps caption
// (same styling as readoutSpan()'s own label half) followed by one
// "name: count" mono span per entry, one size step down from the
// headline row's own value size (kFontCaption+1, not kFontSub) since
// this is a secondary reading, not the primary rate.
QString breakdownLine(const QString& caption, const QVector<QPair<QString, int>>& entries)
{
    if (entries.isEmpty()) {
        return QString();
    }
    const QString captionSpan = QStringLiteral("<span style='color:%1; font-size:%2px;'>%3</span>&nbsp;")
                                     .arg(Style::kTextScale())
                                     .arg(Style::kFontCaption)
                                     .arg(caption.toUpper());
    QStringList parts;
    for (const auto& entry : entries) {
        parts << QStringLiteral("<span style='font-family:Menlo; font-size:%1px; color:%2;'>%3: %4</span>")
                     .arg(Style::kFontCaption + 1)
                     .arg(Style::kTextSecondary())
                     .arg(entry.first, QString::number(entry.second));
    }
    return captionSpan + parts.join(QStringLiteral("&nbsp;&nbsp;"));
}

} // namespace

RateBreakdown computeRateBreakdown(const QVector<QsoRecord>& records, const QDateTime& nowUtc)
{
    RateBreakdown result;
    result.total = records.size();

    const QDateTime tenMinAgo = nowUtc.addSecs(-600);
    const QDateTime twentyMinAgo = nowUtc.addSecs(-1200);
    const QDateTime hourAgo = nowUtc.addSecs(-3600);

    int previous10Min = 0;
    QMap<QString, int> bandCounts;
    QMap<QString, int> modeCounts;

    for (const QsoRecord& record : records) {
        const QDateTime ts = QDateTime::fromString(record.timestampUtc, Qt::ISODate);
        if (!ts.isValid()) {
            continue;
        }
        if (ts >= tenMinAgo) {
            ++result.last10Min;
        } else if (ts >= twentyMinAgo) {
            ++previous10Min;
        }
        if (ts >= hourAgo) {
            ++result.lastHour;
        }

        if (!record.band.isEmpty()) {
            bandCounts[record.band]++;
        }
        if (!record.mode.isEmpty()) {
            modeCounts[record.mode]++;
        }
    }

    if (result.last10Min > previous10Min) {
        result.trend = RateBreakdown::Trend::Up;
    } else if (result.last10Min < previous10Min) {
        result.trend = RateBreakdown::Trend::Down;
    } else {
        result.trend = RateBreakdown::Trend::Flat;
    }

    // Count descending (most-active first), name ascending as a
    // deterministic tie-breaker -- QMap already iterates keys sorted,
    // so the tie-breaker falls out of stable_sort for free.
    const auto byCountDesc = [](const QPair<QString, int>& a, const QPair<QString, int>& b) {
        return a.second > b.second;
    };
    for (auto it = bandCounts.constBegin(); it != bandCounts.constEnd(); ++it) {
        result.byBand.append({it.key(), it.value()});
    }
    std::stable_sort(result.byBand.begin(), result.byBand.end(), byCountDesc);
    for (auto it = modeCounts.constBegin(); it != modeCounts.constEnd(); ++it) {
        result.byMode.append({it.key(), it.value()});
    }
    std::stable_sort(result.byMode.begin(), result.byMode.end(), byCountDesc);

    return result;
}

RateMeterWidget::RateMeterWidget(QWidget* parent)
    : QWidget(parent)
    , m_timer(new QTimer(this))
    , m_headlineLabel(new QLabel(this))
    , m_bandLabel(new QLabel(this))
    , m_modeLabel(new QLabel(this))
{
    // A plain bordered/rounded panel, no header bar -- matches the
    // design mockups' bare `.panel` treatment for this row.
    Style::applyPanelFrameStyle(this);

    for (QLabel* label : {m_headlineLabel, m_bandLabel, m_modeLabel}) {
        label->setTextFormat(Qt::RichText);
        label->setFont(Style::capsFont(label->font()));
        // 2026-09-13, kartendominante Anordnung: this panel moved out of
        // its old full-width (1276px) row into the suggestion panel's
        // narrower column (300px) -- the same setWordWrap(true)
        // SuggestionPanel's own labels already use, so a long combined
        // "10 min X · Stunde Y · Gesamt Z" line degrades onto a second
        // line instead of clipping at the panel edge.
        label->setWordWrap(true);
    }

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 6, 14, 6);
    layout->setSpacing(2);
    layout->addWidget(m_headlineLabel);
    layout->addWidget(m_bandLabel);
    layout->addWidget(m_modeLabel);
    layout->addStretch(1);

    m_timer->setInterval(kRefreshIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &RateMeterWidget::refresh);
    m_timer->start();

    refresh();
}

void RateMeterWidget::setSource(ContestDatabase* database, const QString& contestId)
{
    m_database = database;
    m_contestId = contestId;
    refresh();
}

void RateMeterWidget::refresh()
{
    if (!m_database || m_contestId.isEmpty()) {
        // No source wired up yet -- genuinely unknown, not a zero rate.
        // HAUSSTIL rule 7 ("Unbekannt ist ein Strich, keine Null") names
        // "no rate data yet" as one of its own worked examples.
        m_headlineLabel->setText(readoutSpan(QStringLiteral("Rate"), Style::unknownDash(), Style::kTextInactive()));
        m_bandLabel->clear();
        m_modeLabel->clear();
        emit last10MinRateChanged(0);
        return;
    }

    // One full-row fetch feeds every metric below (10-min/hour/total,
    // trend, band/mode tallies) via computeRateBreakdown() -- a single
    // DB round trip instead of three separate COUNT queries, and the
    // breakdown logic itself is a plain, unit-tested function over the
    // resulting QVector<QsoRecord> (see tests/test_rate_breakdown.cpp),
    // not SQL. Contest QSO volumes (at most a few thousand over 24h)
    // make this trivially cheap at a 15s refresh cadence.
    const QVector<QsoRecord> records = m_database->qsosForContest(m_contestId);
    const RateBreakdown breakdown = computeRateBreakdown(records, QDateTime::currentDateTimeUtc());

    // These three are real, measured counts, including a genuine zero
    // ("0 QSOs in the last 10 minutes" -- HAUSSTIL rule 7's own
    // counter-example of when NOT to show a dash) -- unlike the
    // no-source case above, 0 here is a true reading and stays 0.
    const QString separator = QStringLiteral("&nbsp;&nbsp;&middot;&nbsp;&nbsp;");
    m_headlineLabel->setText(
        readoutSpan(QStringLiteral("10 min"), QString::number(breakdown.last10Min), Style::kTextPrimary())
        + trendGlyph(breakdown.trend) + separator
        + readoutSpan(QStringLiteral("Stunde"), QString::number(breakdown.lastHour), Style::kTextPrimary())
        + separator
        + readoutSpan(QStringLiteral("Gesamt"), QString::number(breakdown.total), Style::kTextPrimary()));

    // Band/mode rows only appear once there is something to show --
    // an empty log has nothing to break down, matching the "no rate
    // data yet" no-source case's own restraint rather than rendering
    // two empty caption rows under a fresh contest start.
    m_bandLabel->setText(breakdownLine(QStringLiteral("Band"), breakdown.byBand));
    // A single-mode contest leg (the common VHF/UHF case, per the
    // operator's own default -- see MainWindow.cpp's defaultRstForMode())
    // never needs a one-entry mode row telling the operator what they
    // already know; it only earns its place once a second mode appears.
    m_modeLabel->setText(breakdown.byMode.size() > 1 ? breakdownLine(QStringLiteral("Mode"), breakdown.byMode)
                                                      : QString());

    emit last10MinRateChanged(breakdown.last10Min);
}

} // namespace Contestprogramm
