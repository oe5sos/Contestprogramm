#include "ui/RateMeterWidget.h"

#include "core/CallsignPrefix.h"
#include "core/CountryPrefixIndex.h"
#include "core/Maidenhead.h"
#include "data/ContestDatabase.h"
#include "data/ContestScoring.h"
#include "ui/StyleKit.h"

#include <QDateTime>
#include <QFontMetricsF>
#include <QLinearGradient>
#include <QLocale>
#include <QMap>
#include <QPainter>
#include <QPainterPath>
#include <QSet>
#include <QTimeZone>
#include <QTimer>

#include <algorithm>

namespace Contestprogramm {

namespace {
// No push-exact-second precision needed for a rate display -- see the
// plan's UI section.
constexpr int kRefreshIntervalMs = 15000;

// Below this width there is no room for the strip's three chips plus a
// per-band block; and a strip is a low band, so it also needs to be at
// least three times as wide as it is high and no taller than a band --
// a wide but tall panel is better served by three columns of tiles.
constexpr int kStripMinWidth = 660;
constexpr int kStripAspect = 3;
constexpr int kStripMaxHeight = 220;

// The tile grid: columns by width, rows by height, six readings at most.
constexpr int kTileGap = 8;
constexpr int kTileMinWidth = 118;
constexpr int kTileMinHeight = 36;
constexpr int kMaxTiles = 6;

// "4 812" (narrow no-break space, the Austrian grouping) -- the km
// sums pass 10 000 on a good 2m weekend and are unreadable as a bare
// digit run.
QString groupedNumber(qint64 value)
{
    return QLocale(QLocale::German, QLocale::Austria).toString(value);
}

// The rate trend as a word (tiles) and a glyph (strip) -- shape carries
// the meaning, not colour: up/down is a neutral reading, not a
// warning, so it stays in the quiet text colours rather than a
// red/green good/bad pairing. Deliberate: this codebase reserves
// kRedText for actual warnings (DUPE/UNGÜLTIG pills, terrain-blocked
// -- HAUSSTIL "Rot bleibt der Warnung"), and "fewer QSOs than the
// previous 10 minutes" is not one.
QString trendWord(RateBreakdown::Trend trend)
{
    switch (trend) {
    case RateBreakdown::Trend::Up: return QStringLiteral("steigend");
    case RateBreakdown::Trend::Down: return QStringLiteral("fallend");
    case RateBreakdown::Trend::Flat: return QStringLiteral("gleichbleibend");
    }
    return QString();
}

QString trendGlyph(RateBreakdown::Trend trend)
{
    switch (trend) {
    case RateBreakdown::Trend::Up: return QStringLiteral("▲"); // ▲
    case RateBreakdown::Trend::Down: return QStringLiteral("▼"); // ▼
    case RateBreakdown::Trend::Flat: return QStringLiteral("—"); // —
    }
    return QString();
}

QColor colour(const QString& hex, int alpha = 255)
{
    QColor c(hex);
    c.setAlpha(alpha);
    return c;
}

// The painting vocabulary the design sheets were drawn with (caps
// captions, monospace readings, sunken glass insets), on the widget's
// own font -- never on painter.font(), which may already carry the
// caps font's AllUppercase capitalization.
struct Ink {
    const QFont& base;

    QFont caps(int px = Style::kFontCaption) const { return Style::capsFont(base, px); }
    QFont mono(int px, QFont::Weight weight = QFont::Normal) const { return Style::monoFont(base, px, weight); }
    double monoWidth(const QString& text, int px, QFont::Weight weight = QFont::Normal) const
    {
        return QFontMetricsF(mono(px, weight)).horizontalAdvance(text);
    }
    double capsWidth(const QString& text, int px = Style::kFontCaption) const
    {
        return QFontMetricsF(caps(px)).horizontalAdvance(text.toUpper());
    }

    void capsAt(QPainter& g, const QPointF& at, const QString& text, const QColor& color,
                int px = Style::kFontCaption) const
    {
        g.setFont(caps(px));
        g.setPen(color);
        g.drawText(at, text.toUpper());
    }
    void monoAt(QPainter& g, const QPointF& at, const QString& text, const QColor& color, int px,
                QFont::Weight weight = QFont::Normal) const
    {
        g.setFont(mono(px, weight));
        g.setPen(color);
        g.drawText(at, text);
    }
    // A quiet line elided to `width`, the way a narrow tile needs it.
    void monoElidedAt(QPainter& g, const QPointF& at, const QString& text, const QColor& color, int px,
                      double width) const
    {
        const QFont f = mono(px);
        g.setFont(f);
        g.setPen(color);
        g.drawText(at, QFontMetricsF(f).elidedText(text, Qt::ElideRight, width));
    }

    // Sunken glass inset: black face, inner shadow along the top, a
    // faint light edge -- the same inset the map's horizon strip and
    // the design sheets use.
    static void inset(QPainter& g, const QRectF& r, double radius = 6.0)
    {
        QPainterPath path;
        path.addRoundedRect(r, radius, radius);
        g.setPen(Qt::NoPen);
        g.setBrush(colour(Style::kInsetBg()));
        g.drawPath(path);
        QLinearGradient shade(r.topLeft(), QPointF(r.left(), r.top() + 12));
        shade.setColorAt(0, colour(QStringLiteral("#000000"), 150));
        shade.setColorAt(1, colour(QStringLiteral("#000000"), 0));
        g.save();
        g.setClipPath(path);
        g.setBrush(shade);
        g.drawRect(QRectF(r.left(), r.top(), r.width(), 12));
        g.restore();
        g.setPen(QPen(colour(QStringLiteral("#ffffff"), 18), 1));
        g.setBrush(Qt::NoBrush);
        g.drawPath(path);
    }
};

} // namespace

RateBreakdown computeRateBreakdown(const QVector<QsoRecord>& records, const QDateTime& nowUtc)
{
    RateBreakdown result;
    result.perTenMinutes = QVector<int>(RateBreakdown::kSparkBuckets, 0);

    const QDateTime tenMinAgo = nowUtc.addSecs(-600);
    const QDateTime twentyMinAgo = nowUtc.addSecs(-1200);
    const QDateTime hourAgo = nowUtc.addSecs(-3600);
    constexpr qint64 kBucketSecs = 600;
    constexpr qint64 kSparkSecs = kBucketSecs * RateBreakdown::kSparkBuckets;

    int previous10Min = 0;
    QMap<QString, int> bandCounts;
    QMap<QString, int> modeCounts;
    QMap<QDateTime, int> hourCounts;

    for (const QsoRecord& record : records) {
        // An invalidated QSO is out of the log for every purpose (see
        // ContestDatabase::setQsoInvalid) -- it neither counts nor rates
        // (2026-09-21: "QSOs 3" with one struck through). Dupes stay: a
        // real contact, in the EDI with its "D", just 0 points.
        if (record.isInvalid) {
            continue;
        }
        ++result.total;
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
        // A timestamp a little ahead of "now" (a hand-corrected time, a
        // rig clock) still belongs to the running bucket.
        const qint64 ago = std::max<qint64>(0, ts.secsTo(nowUtc));
        if (ago < kSparkSecs) {
            ++result.perTenMinutes[RateBreakdown::kSparkBuckets - 1 - int(ago / kBucketSecs)];
        }
        const QDateTime utc = ts.toUTC();
        hourCounts[QDateTime(utc.date(), QTime(utc.time().hour(), 0), QTimeZone::utc())]++;

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

    // The earliest of equally busy hours wins -- QMap iterates the
    // hours in order, and only a strictly better hour replaces it.
    for (auto it = hourCounts.constBegin(); it != hourCounts.constEnd(); ++it) {
        if (it.value() > result.bestHourQsos) {
            result.bestHourQsos = it.value();
            result.bestHourStartUtc = it.key();
        }
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

RateMeterWidget::Layout RateMeterWidget::layoutFor(const QSize& size)
{
    const bool strip = size.width() >= kStripMinWidth && size.height() <= kStripMaxHeight
                       && size.width() >= kStripAspect * size.height();
    return strip ? Layout::Strip : Layout::Tiles;
}

RateMeterWidget::RateMeterWidget(QWidget* parent)
    : QWidget(parent)
    , m_timer(new QTimer(this))
{
    // The panel container draws the frame and the header; this widget
    // is the inside of the panel and paints its own readings.
    setAttribute(Qt::WA_OpaquePaintEvent, false);

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

void RateMeterWidget::setScoring(const QString& ownGrid, const QStringList& bandOrder, const QString& scoring,
                                 const QString& multiplierBasis, const CountryPrefixIndex* countryIndex)
{
    m_countryIndex = countryIndex;
    m_ownGrid = ownGrid.trimmed().toUpper();
    m_bandOrder = bandOrder;
    m_scoring = scoring;
    m_multiplierBasis = multiplierBasis;
    refresh();
}

QSize RateMeterWidget::sizeHint() const
{
    return QSize(270, 130);
}

QSize RateMeterWidget::minimumSizeHint() const
{
    return QSize(120, 56);
}

void RateMeterWidget::refresh()
{
    m_hasSource = m_database && !m_contestId.isEmpty();
    if (!m_hasSource) {
        // No source wired up yet -- genuinely unknown, not a zero rate.
        // HAUSSTIL rule 7 ("Unbekannt ist ein Strich, keine Null") names
        // "no rate data yet" as one of its own worked examples.
        m_breakdown = RateBreakdown();
        m_scoreKnown = false;
        m_score = ContestScore();
        m_largeSquares = 0;
        update();
        emit last10MinRateChanged(0);
        return;
    }

    // One full-row fetch feeds every metric below (10-min/hour/total,
    // trend, band/mode tallies, sparkline, best hour, score, ODX,
    // squares) via computeRateBreakdown()/computeContestScore() -- a
    // single DB round trip instead of a handful of COUNT queries, and
    // the logic itself is plain, unit-tested functions over the
    // resulting QVector<QsoRecord> (see tests/test_rate_breakdown.cpp),
    // not SQL. Contest QSO volumes (at most a few thousand over 24h)
    // make this trivially cheap at a 15s refresh cadence.
    const QVector<QsoRecord> records = m_database->qsosForContest(m_contestId);
    // These are real, measured counts, including a genuine zero ("0
    // QSOs in the last 10 minutes" -- HAUSSTIL rule 7's own
    // counter-example of when NOT to show a dash) -- unlike the
    // no-source case above, 0 here is a true reading and stays 0.
    m_breakdown = computeRateBreakdown(records, QDateTime::currentDateTimeUtc());

    // The claimed score, the way the IARU-R1/ÖVSV rules count it (see
    // ContestScoring.h): km per band and the sum, plus the ODX. Without
    // an own locator every distance would be 0 -- that is "unknown",
    // not a score, so the readings show the dash (HAUSSTIL rule 7).
    m_scoreKnown = isValidGridSquare(m_ownGrid) || m_scoring != QStringLiteral("distance_km");
    m_score = m_scoreKnown ? computeContestScore(records, m_ownGrid, m_bandOrder, m_scoring) : ContestScore();

    // Multiplikatoren über alle Bänder -- einer auf zwei Bändern bleibt
    // einer (die Zahlen je Band stehen in BandScore::largeSquares).
    // Grundlage ist dieselbe wie bei MultiplierTracker: Locator-Großfeld
    // oder WPX-Präfix; auf Kurzwelle gibt es keinen Locator, ein
    // Großfeld-Zähler stünde dort für immer auf 0.
    QSet<QString> keys;
    QHash<QString, QSet<QString>> keysByBand;
    for (const QsoRecord& record : records) {
        if (record.isInvalid) {
            continue;
        }
        QString key;
        if (m_multiplierBasis == QStringLiteral("prefix")) {
            key = wpxPrefix(record.callsign);
        } else if (m_multiplierBasis == QStringLiteral("dxcc")) {
            key = m_countryIndex ? m_countryIndex->lookup(record.callsign).primaryPrefix : QString();
        } else if (record.gridSquare.size() >= 4) {
            key = record.gridSquare.left(4).toUpper();
        }
        if (key.isEmpty()) {
            continue;
        }
        keys.insert(key);
        keysByBand[record.band].insert(key);
    }
    m_largeSquares = keys.size();
    m_multipliersByBand.clear();
    for (auto it = keysByBand.constBegin(); it != keysByBand.constEnd(); ++it) {
        m_multipliersByBand.insert(it.key(), it.value().size());
    }

    update();
    emit last10MinRateChanged(m_breakdown.last10Min);
}

// "144: 31 · 432: 16" (or "31 · 16" without names) -- per-band QSO
// counts in the definition's band order (so the columns never jump
// when a band overtakes another), or, with `points`, the per-band
// points the same way.
QString RateMeterWidget::bandLine(bool withNames, bool points) const
{
    QStringList parts;
    if (points) {
        for (const BandScore& band : m_score.bands) {
            parts << (withNames ? QStringLiteral("%1: %2").arg(band.band, groupedNumber(band.points))
                                : groupedNumber(band.points));
        }
    } else {
        QVector<QPair<QString, int>> ordered;
        for (const QString& band : m_bandOrder) {
            ordered.append({band, 0});
        }
        for (const auto& entry : m_breakdown.byBand) {
            bool found = false;
            for (auto& slot : ordered) {
                if (slot.first == entry.first) {
                    slot.second = entry.second;
                    found = true;
                }
            }
            if (!found) {
                ordered.append(entry);
            }
        }
        for (const auto& entry : ordered) {
            parts << (withNames ? QStringLiteral("%1: %2").arg(entry.first).arg(entry.second)
                                : QString::number(entry.second));
        }
    }
    return parts.join(QStringLiteral(" · "));
}

QVector<RateMeterWidget::Reading> RateMeterWidget::readings() const
{
    const QString primary = Style::kTextPrimary();
    const QString inactive = Style::kTextInactive();
    const QString dash = Style::unknownDash();
    QVector<Reading> out;
    if (!m_hasSource) {
        for (const QString& caption : {QStringLiteral("QSOs"), QStringLiteral("Punkte"), QStringLiteral("10 min"),
                                       QStringLiteral("Stunde"), QStringLiteral("ODX"), QStringLiteral("Felder")}) {
            out.append({caption, dash, QString(), QString(), QString(), inactive});
        }
        return out;
    }

    const bool multiBand = m_bandOrder.size() > 1 || m_breakdown.byBand.size() > 1;
    out.append({QStringLiteral("QSOs"), QString::number(m_breakdown.total), QString(),
                multiBand ? bandLine(true, false) : QString(), multiBand ? bandLine(false, false) : QString(),
                primary});

    if (m_scoreKnown) {
        out.append({QStringLiteral("Punkte"), groupedNumber(m_score.points), QString(),
                    multiBand ? bandLine(true, true) : QString(), multiBand ? bandLine(false, true) : QString(),
                    Style::kAmberText()});
    } else {
        out.append({QStringLiteral("Punkte"), dash, QString(), QStringLiteral("kein eigener Locator"),
                    QStringLiteral("kein Locator"), inactive});
    }

    out.append({QStringLiteral("10 min"), QString::number(m_breakdown.last10Min), QString(),
                m_breakdown.total > 0 ? trendWord(m_breakdown.trend) : QString(),
                m_breakdown.total > 0 ? trendWord(m_breakdown.trend) : QString(), primary});

    QString bestHour;
    QString bestHourShort;
    if (m_breakdown.bestHourQsos > 0) {
        const QString hour = QStringLiteral("%1z").arg(m_breakdown.bestHourStartUtc.time().hour(), 2, 10, QLatin1Char('0'));
        bestHour = QStringLiteral("beste %1 (%2)").arg(m_breakdown.bestHourQsos).arg(hour);
        bestHourShort = QStringLiteral("beste %1").arg(m_breakdown.bestHourQsos);
    }
    out.append({QStringLiteral("Stunde"), QString::number(m_breakdown.lastHour), QString(), bestHour, bestHourShort,
                primary});

    if (m_scoreKnown && m_score.odxKm > 0) {
        out.append({QStringLiteral("ODX"), groupedNumber(m_score.odxKm), QStringLiteral("km"),
                    QStringLiteral("%1 · %2").arg(m_score.odxCall, m_score.odxGrid), m_score.odxCall, primary});
    } else {
        out.append({QStringLiteral("ODX"), dash, QString(), QString(), QString(), inactive});
    }

    // Je Band aus derselben Quelle wie die Gesamtzahl oben -- nicht aus
    // BandScore::largeSquares, das immer Locator-Großfelder zählt und
    // unter einer Präfix-Gesamtzahl lauter Nullen ergeben hätte.
    QStringList squaresPerBand;
    QStringList squaresPerBandShort;
    if (m_scoreKnown && multiBand) {
        for (const BandScore& band : m_score.bands) {
            const int count = m_multipliersByBand.value(band.band);
            squaresPerBand << QStringLiteral("%1: %2").arg(band.band).arg(count);
            squaresPerBandShort << QString::number(count);
        }
    }
    // Ohne Multiplikator entfällt die Kachel ganz -- eine Null wäre
    // dort keine Aussage, sondern eine falsche.
    if (m_multiplierBasis == QStringLiteral("dxcc")) {
        // Ohne geladene Länderliste lässt sich kein Land zählen -- ein
        // Strich mit dem Grund dahinter, keine Null.
        const bool haveList = m_countryIndex != nullptr && !m_countryIndex->isEmpty();
        out.append({QStringLiteral("Länder"), haveList ? QString::number(m_largeSquares) : dash, QString(),
                    haveList ? squaresPerBand.join(QStringLiteral(" · ")) : QStringLiteral("Länderliste fehlt"),
                    haveList ? squaresPerBandShort.join(QStringLiteral(" · ")) : QStringLiteral("keine Liste"),
                    haveList ? primary : inactive});
    } else if (m_multiplierBasis == QStringLiteral("grid") || m_multiplierBasis == QStringLiteral("prefix")) {
        const QString label = m_multiplierBasis == QStringLiteral("prefix") ? QStringLiteral("Präfixe")
                                                                            : QStringLiteral("Felder");
        out.append({label, QString::number(m_largeSquares), QString(),
                    squaresPerBand.join(QStringLiteral(" · ")), squaresPerBandShort.join(QStringLiteral(" · ")),
                    primary});
    }
    return out;
}

QString RateMeterWidget::readingsText() const
{
    QStringList lines;
    for (const Reading& reading : readings()) {
        QString line = reading.caption + QLatin1Char(' ') + reading.value;
        if (!reading.unit.isEmpty()) {
            line += QLatin1Char(' ') + reading.unit;
        }
        if (!reading.sub.isEmpty()) {
            line += QStringLiteral(" (%1)").arg(reading.sub);
        }
        lines << line;
    }
    return lines.join(QLatin1Char('\n'));
}

void RateMeterWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    update();
}

void RateMeterWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    // The container draws the frame and header; this is the inside.
    painter.fillRect(rect(), colour(Style::kPanelBg()));
    if (layoutFor(size()) == Layout::Strip) {
        paintStrip(painter, rect());
    } else {
        paintTiles(painter, rect());
    }
}

// The tile grid (design sheet 2, "Rate: Kacheln"): columns by width,
// rows by height, the readings in priority order -- QSOs and Punkte
// first, then the two rates, then ODX and Felder -- so a 270×130 panel
// shows the four that matter most and a taller one all six.
void RateMeterWidget::paintTiles(QPainter& g, const QRect& area) const
{
    const Ink ink{font()};
    const QVector<Reading> all = readings();
    const int columns = std::clamp((area.width() - kTileGap) / (kTileMinWidth + kTileGap), 1, 3);
    const int rowsAfforded = std::max(1, (area.height() - kTileGap) / (kTileMinHeight + kTileGap));
    const int count = std::min(kMaxTiles, columns * rowsAfforded);
    const int rows = (count + columns - 1) / columns;
    const double w = (area.width() - (columns + 1) * kTileGap) / double(columns);
    const double h = (area.height() - (rows + 1) * kTileGap) / double(rows);
    const double innerWidth = w - 18;

    // One value size for every tile: the size the height suggests,
    // stepped down until the widest value fits -- so "30" and "6 739"
    // never sit side by side in two sizes.
    int px = h >= 110 ? Style::kFontDisplay : h >= 70 ? Style::kFontReading : Style::kFontSub;
    const auto fits = [&](int size) {
        for (int i = 0; i < count; ++i) {
            const Reading& r = all.at(i);
            const double unitWidth = r.unit.isEmpty() ? 0.0 : ink.monoWidth(r.unit, Style::kFontSmall) + 5;
            if (ink.monoWidth(r.value, size, QFont::DemiBold) + unitWidth > innerWidth) {
                return false;
            }
        }
        return true;
    };
    for (int smaller : {Style::kFontReading, Style::kFontSub, Style::kFontSmall}) {
        if (smaller < px && !fits(px)) {
            px = smaller;
        }
    }
    // The quiet line under the value only when there is room for it.
    const bool room = (h - 15 - px - 4) >= 14;

    for (int i = 0; i < count; ++i) {
        const Reading& r = all.at(i);
        const int column = i % columns;
        const int row = i / columns;
        const QRectF box(area.left() + kTileGap + column * (w + kTileGap), area.top() + kTileGap + row * (h + kTileGap),
                         w, h);
        Ink::inset(g, box);

        const bool showSub = room && !r.sub.isEmpty();
        ink.capsAt(g, QPointF(box.left() + 9, box.top() + 15), r.caption, colour(Style::kTextScale()));
        const double valueWidth = ink.monoWidth(r.value, px, QFont::DemiBold);
        // Under the caption with the quiet line below; a compact tile
        // sets the value on its bottom edge instead.
        const double valueBaseline = showSub ? box.top() + 15 + px + 2 : box.bottom() - 7;
        ink.monoAt(g, QPointF(box.left() + 9, valueBaseline), r.value, colour(r.valueColor), px, QFont::DemiBold);
        if (!r.unit.isEmpty()) {
            ink.monoAt(g, QPointF(box.left() + 9 + valueWidth + 5, valueBaseline), r.unit,
                       colour(Style::kTextTertiary()), Style::kFontSmall);
        }
        if (showSub) {
            const QString sub = ink.monoWidth(r.sub, Style::kFontCaption) <= innerWidth || r.subShort.isEmpty()
                                    ? r.sub
                                    : r.subShort;
            ink.monoElidedAt(g, QPointF(box.left() + 9, box.bottom() - 7), sub, colour(Style::kTextTertiary()),
                           Style::kFontCaption, innerWidth);
        }
    }
}

// The counter strip (design sheet 1, "Rate: Zähler"): three glass
// chips -- QSOs, Punkte, ODX -- then, as the width allows, the per-band
// rows with their share bars and the rate block with the six-hour
// sparkline of QSOs per ten minutes.
void RateMeterWidget::paintStrip(QPainter& g, const QRect& full) const
{
    const Ink ink{font()};
    const QVector<Reading> all = readings();
    const Reading& qsos = all.at(0);
    const Reading& points = all.at(1);
    const Reading& tenMin = all.at(2);
    const Reading& hour = all.at(3);
    const Reading& odx = all.at(4);

    // A strip is a band of at most 150 px; extra height becomes air
    // above and below rather than stretched chips.
    QRect area = full;
    if (area.height() > 150) {
        const int excess = area.height() - 150;
        area.adjust(0, excess / 2, 0, -(excess - excess / 2));
    }
    const double chipHeight = area.height() - 24;
    const int bigPx = chipHeight >= 76 ? Style::kFontDisplay : Style::kFontReading;
    const int midPx = chipHeight >= 76 ? Style::kFontReading : Style::kFontSub;

    struct Chip {
        const Reading* reading;
        int px;
        double minWidth;
    };
    const QString odxValue = odx.unit.isEmpty() ? odx.value
                                                : QStringLiteral("%1 %2").arg(m_score.odxCall, odx.value);
    const Chip chips[] = {{&qsos, bigPx, 78.0}, {&points, midPx, 104.0}, {&odx, Style::kFontSub, 124.0}};
    double x = area.left() + 12;
    for (const Chip& chip : chips) {
        const QString value = chip.reading == &odx ? odxValue : chip.reading->value;
        const double unitWidth = chip.reading->unit.isEmpty()
                                     ? 0.0
                                     : ink.monoWidth(chip.reading->unit, Style::kFontSmall) + 5;
        const double width = std::max(chip.minWidth, ink.monoWidth(value, chip.px, QFont::DemiBold) + unitWidth + 20);
        if (x + width > area.right() - 12) {
            break;
        }
        const QRectF box(x, area.top() + 12, width, chipHeight);
        Ink::inset(g, box);
        ink.capsAt(g, QPointF(box.left() + 10, box.top() + 16), chip.reading->caption, colour(Style::kTextScale()));
        ink.monoAt(g, QPointF(box.left() + 10, box.bottom() - 12), value, colour(chip.reading->valueColor), chip.px,
                 QFont::DemiBold);
        if (!chip.reading->unit.isEmpty()) {
            ink.monoAt(g, QPointF(box.left() + 10 + ink.monoWidth(value, chip.px, QFont::DemiBold) + 5, box.bottom() - 12),
                       chip.reading->unit, colour(Style::kTextTertiary()), Style::kFontSmall);
        }
        x += width + 8;
    }

    // Per band: one quiet row per band with its share of the QSOs as a
    // bar, then "QSOs · Punkte".
    x += 4;
    const double bandBlockWidth = 150;
    // Row k's baseline sits at +46 + spacing·k and must stay above the
    // chips' bottom edge (+12 + chipHeight, digits have no descenders);
    // a low strip packs the rows tighter.
    const int rowSpacing = chipHeight >= 76 ? 24 : 20;
    const int bandRows = std::min(int(m_score.bands.isEmpty() ? m_breakdown.byBand.size() : m_score.bands.size()),
                                  chipHeight >= 36 ? int((chipHeight - 36) / rowSpacing) + 1 : 0);
    if (m_hasSource && bandRows > 0 && x + bandBlockWidth <= area.right() - 12) {
        ink.capsAt(g, QPointF(x, area.top() + 26), QStringLiteral("je Band"), colour(Style::kTextScale()));
        double y = area.top() + 46;
        int row = 0;
        QStringList bands;
        for (const BandScore& band : m_score.bands) {
            bands << band.band;
        }
        if (bands.isEmpty()) {
            for (const auto& entry : m_breakdown.byBand) {
                bands << entry.first;
            }
        }
        for (const QString& band : bands) {
            if (row++ >= bandRows) {
                break;
            }
            int count = 0;
            for (const auto& entry : m_breakdown.byBand) {
                if (entry.first == band) {
                    count = entry.second;
                }
            }
            const double share = m_breakdown.total > 0 ? count / double(m_breakdown.total) : 0.0;
            ink.monoAt(g, QPointF(x, y), band, colour(Style::kTextSecondary()), Style::kFontSmall, QFont::DemiBold);
            g.setPen(Qt::NoPen);
            g.setBrush(colour(Style::kBorder()));
            g.drawRoundedRect(QRectF(x + 34, y - 9, 46, 8), 2, 2);
            g.setBrush(colour(Style::kAmberDim()));
            g.drawRoundedRect(QRectF(x + 34, y - 9, 46 * share, 8), 2, 2);
            QString text = QString::number(count);
            if (const BandScore* score = m_scoreKnown ? m_score.band(band) : nullptr) {
                text += QStringLiteral(" · %1").arg(groupedNumber(score->points));
            }
            ink.monoAt(g, QPointF(x + 86, y), text, colour(Style::kTextPrimary()), Style::kFontCaption);
            y += rowSpacing;
        }
        x += bandBlockWidth + 30;
    }

    // Rate: 10 min and hour, then the sparkline in whatever is left.
    const double rateBlockWidth = 96;
    if (m_hasSource && x + rateBlockWidth <= area.right() - 12) {
        ink.capsAt(g, QPointF(x, area.top() + 28), QStringLiteral("Rate"), colour(Style::kTextScale()));
        const double numberWidth = std::max(30.0, ink.monoWidth(tenMin.value, midPx, QFont::DemiBold) + 6);
        ink.monoAt(g, QPointF(x, area.top() + 56), tenMin.value, colour(Style::kTextPrimary()), midPx, QFont::DemiBold);
        ink.capsAt(g, QPointF(x + numberWidth, area.top() + 56),
                 QStringLiteral("/ 10 min %1")
                     .arg(m_breakdown.total > 0 && m_breakdown.trend != RateBreakdown::Trend::Flat
                              ? trendGlyph(m_breakdown.trend)
                              : QString()),
                 colour(Style::kTextTertiary()));
        if (chipHeight >= 70) {
            ink.monoAt(g, QPointF(x, area.top() + 86), hour.value, colour(Style::kTextPrimary()), Style::kFontSub,
                     QFont::DemiBold);
            ink.capsAt(g, QPointF(x + std::max(30.0, ink.monoWidth(hour.value, Style::kFontSub, QFont::DemiBold) + 6),
                                area.top() + 86),
                     QStringLiteral("/ Stunde"), colour(Style::kTextTertiary()));
        }
        x += rateBlockWidth;

        const QRectF spark(x, area.top() + 34, area.right() - 12 - x, chipHeight - 22);
        if (spark.width() >= 70 && spark.height() >= 30) {
            Ink::inset(g, spark, 4);
            const QVector<int>& buckets = m_breakdown.perTenMinutes;
            const int n = buckets.size();
            int peak = 1;
            for (int v : buckets) {
                peak = std::max(peak, v);
            }
            const double barWidth = (spark.width() - 12) / std::max(1, n);
            for (int i = 0; i < n; ++i) {
                const double barHeight = buckets.at(i) / double(peak) * (spark.height() - 16);
                if (barHeight <= 0.0) {
                    continue;
                }
                g.setPen(Qt::NoPen);
                // The last hour in the full amber, the five before it dimmed.
                g.setBrush(i >= n - 6 ? colour(Style::kAmberText(), 220) : colour(Style::kAmberDim(), 200));
                g.drawRect(QRectF(spark.left() + 6 + i * barWidth, spark.bottom() - 6 - barHeight, barWidth - 1.5,
                                  barHeight));
            }
            QString legend = QStringLiteral("je 10 min · 6 h");
            if (!hour.sub.isEmpty()) {
                QString best = hour.sub;
                best.replace(QStringLiteral("beste "), QStringLiteral("beste Stunde "));
                legend += QStringLiteral("   ·   ") + best;
            }
            const double legendWidth = ink.capsWidth(legend);
            if (legendWidth > spark.width() - 12) {
                legend = QStringLiteral("je 10 min · 6 h");
            }
            ink.capsAt(g, QPointF(spark.left() + 6, spark.top() + 11), legend, colour(Style::kTextInactive()));
        }
    }
}

} // namespace Contestprogramm
