#include "ui/UtcClockWidget.h"

#include "ui/StyleKit.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimeZone>
#include <QTimer>

namespace Contestprogramm {

namespace {

// One plain single-line readout -- see UtcClockWidget.h's class comment.
// Matches Longpath's real gui/TitleBar.cpp UTC label style exactly
// (Style::kTextSecondary(), 11px Menlo/SF Mono, not bold, no caption row,
// no box) rather than reinterpreting it through this app's own amber
// "measured value" convention -- TitleBar's own clock isn't styled that
// way either.
QLabel* makeClockLabel(QWidget* parent)
{
    auto* label = new QLabel(parent);
    label->setFont(Style::monoFont(label->font(), Style::kFontSmall));
    label->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextSecondary()));
    return label;
}

} // namespace

UtcClockWidget::UtcClockWidget(QWidget* parent)
    : QWidget(parent)
    , m_timer(new QTimer(this))
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    m_countdownLabel = makeClockLabel(this);
    layout->addWidget(m_countdownLabel);
    m_utcLabel = makeClockLabel(this);
    layout->addWidget(m_utcLabel);

    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &UtcClockWidget::tick);
    m_timer->start();

    tick();
    setCountdownVisible(false);
}

void UtcClockWidget::setContestEndUtc(const QString& iso8601)
{
    QDateTime parsed = QDateTime::fromString(iso8601, Qt::ISODate);
    if (parsed.isValid()) {
        // The field is always meant as UTC (see ContestSettings::
        // contestEndUtc / SettingsDialog's "Contest-Ende (UTC)" label) --
        // enforced here regardless of what timezone fromString() itself
        // inferred from the string.
        parsed.setTimeZone(QTimeZone::utc());
    }
    m_contestStartUtc = QDateTime();
    m_contestEndUtc = parsed;
    tick();
}

void UtcClockWidget::setContestWindow(const QDateTime& startUtc, const QDateTime& endUtc)
{
    m_contestStartUtc = startUtc.isValid() ? startUtc.toUTC() : QDateTime();
    m_contestEndUtc = endUtc.isValid() ? endUtc.toUTC() : QDateTime();
    tick();
}

void UtcClockWidget::setCountdownVisible(bool visible)
{
    m_countdownLabel->setVisible(visible);
}

QString UtcClockWidget::formatRemaining(const QDateTime& nowUtc, const QDateTime& endUtc)
{
    if (!endUtc.isValid()) {
        return Style::unknownDash();
    }
    qint64 secondsLeft = nowUtc.secsTo(endUtc);
    if (secondsLeft < 0) {
        // Reached/passed is a genuine zero, not "unknown" -- HAUSSTIL
        // rule 7's own zero-is-a-real-value case, not a negative
        // countdown running past the contest's actual end.
        secondsLeft = 0;
    }
    const qint64 hours = secondsLeft / 3600;
    const qint64 minutes = (secondsLeft % 3600) / 60;
    const qint64 seconds = secondsLeft % 60;
    return QStringLiteral("%1:%2:%3")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

QString UtcClockWidget::formatCountdown(const QDateTime& nowUtc, const QDateTime& startUtc, const QDateTime& endUtc)
{
    if (endUtc.isValid() && startUtc.isValid()) {
        if (nowUtc < startUtc) {
            constexpr qint64 kWeekSecs = 7 * 24 * 3600;
            if (nowUtc.secsTo(startUtc) > kWeekSecs) {
                static const char* const kDays[] = {"", "Mo", "Di", "Mi", "Do", "Fr", "Sa", "So"};
                const QDateTime start = startUtc.toUTC();
                return QStringLiteral("Start %1 %2 %3 UTC")
                    .arg(QLatin1String(kDays[start.date().dayOfWeek()]), start.date().toString(QStringLiteral("dd.MM.")),
                         start.time().toString(QStringLiteral("HH:mm")));
            }
            return QStringLiteral("Start in ") + formatRemaining(nowUtc, startUtc);
        }
        if (nowUtc > endUtc) {
            return QStringLiteral("Beendet");
        }
    }
    return QStringLiteral("Noch ") + formatRemaining(nowUtc, endUtc);
}

void UtcClockWidget::tick()
{
    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    // "hh:mm:ss UTC", verbatim TitleBar.cpp's own tickUtc() format string.
    m_utcLabel->setText(nowUtc.time().toString(QStringLiteral("hh:mm:ss")) + QStringLiteral(" UTC"));
    m_countdownLabel->setText(formatCountdown(nowUtc, m_contestStartUtc, m_contestEndUtc));
}

} // namespace Contestprogramm
