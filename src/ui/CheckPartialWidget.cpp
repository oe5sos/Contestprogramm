#include "ui/CheckPartialWidget.h"

#include "ui/StyleKit.h"

#include <QLabel>
#include <QVBoxLayout>

namespace Contestprogramm {

namespace {

QString escaped(const QString& text)
{
    return text.toHtmlEscaped();
}

// One call as a link. Colour carries the reading (see the class
// comment); the href carries call and grid so the click handler needs
// no lookup: "call|grid".
QString callSpan(const CheckPartialMatch& match)
{
    QString color = Style::kTextSecondary();
    QString decoration = QStringLiteral("none");
    QString prefix;
    if (match.workedThisBand) {
        color = Style::kTextInactive();
        decoration = QStringLiteral("line-through");
    } else if (match.sources & (CheckPartialMatch::Log | CheckPartialMatch::History | CheckPartialMatch::Seen)) {
        color = Style::kTextPrimary();
    }
    if (match.nearMiss) {
        prefix = QStringLiteral("&asymp;");
    }
    QString label = prefix + escaped(match.callsign);
    QStringList small;
    if (!match.grid.isEmpty()) {
        small << escaped(match.grid.left(4));
    }
    // Worked on another band already: N1MM+'s "QSO B4 on ..." hint --
    // a station still open on this band, but its locator is known.
    if (!match.workedThisBand && !match.workedBands.isEmpty()) {
        small << QStringLiteral("&#10003;") + escaped(match.workedBands.join(QLatin1Char('/')));
    }
    if (!small.isEmpty()) {
        label += QStringLiteral("<span style='color:%1; font-size:%2px;'>&nbsp;%3</span>")
                     .arg(Style::kTextTertiary())
                     .arg(Style::kFontCaption)
                     .arg(small.join(QStringLiteral("&nbsp;")));
    }
    return QStringLiteral("<a href='%1|%2' style='color:%3; text-decoration:%4;'>%5</a>")
        .arg(escaped(match.callsign), escaped(match.grid), color, decoration, label);
}

} // namespace

CheckPartialWidget::CheckPartialWidget(QWidget* parent)
    : QWidget(parent)
    , m_matchesLabel(new QLabel(this))
    , m_multiplierLabel(new QLabel(this))
    , m_statusLabel(new QLabel(this))
{
    Style::applyPanelFrameStyle(this);

    m_matchesLabel->setTextFormat(Qt::RichText);
    m_matchesLabel->setWordWrap(true);
    m_matchesLabel->setFont(Style::monoFont(m_matchesLabel->font(), Style::kFontBody));
    m_matchesLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    m_matchesLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    connect(m_matchesLabel, &QLabel::linkActivated, this, [this](const QString& href) {
        const int bar = href.indexOf(QLatin1Char('|'));
        const QString call = bar < 0 ? href : href.left(bar);
        const QString grid = bar < 0 ? QString() : href.mid(bar + 1);
        emit callsignChosen(call, grid);
    });

    m_statusLabel->setTextFormat(Qt::RichText);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setFont(Style::capsFont(m_statusLabel->font()));
    m_statusLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    connect(m_statusLabel, &QLabel::linkActivated, this, [this](const QString&) { emit scpLoadRequested(); });

    m_matchesLabel->setObjectName(QStringLiteral("checkMatches"));
    m_multiplierLabel->setObjectName(QStringLiteral("checkMultiplier"));
    m_statusLabel->setObjectName(QStringLiteral("checkSources"));
    m_multiplierLabel->setTextFormat(Qt::RichText);
    m_multiplierLabel->setWordWrap(true);
    m_multiplierLabel->setFont(Style::capsFont(m_multiplierLabel->font()));
    m_multiplierLabel->hide();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 6, 14, 6);
    layout->setSpacing(4);
    layout->addWidget(m_multiplierLabel);
    layout->addWidget(m_matchesLabel, 1);
    layout->addWidget(m_statusLabel);

    setMatches(QString(), {});
    rebuildStatus();
}

void CheckPartialWidget::setMatches(const QString& fragment, const QVector<CheckPartialMatch>& matches)
{
    m_matches = matches;
    if (fragment.trimmed().size() < 2) {
        // Nothing typed yet: not "0 matches", just not asked -- the
        // dash, same reading HAUSSTIL gives every not-yet-known value.
        m_matchesLabel->setText(QStringLiteral("<span style='color:%1;'>%2</span>")
                                    .arg(Style::kTextInactive(), Style::unknownDash()));
        return;
    }
    if (matches.isEmpty()) {
        m_matchesLabel->setText(QStringLiteral("<span style='color:%1;'>%2: kein Treffer</span>")
                                    .arg(Style::kTextSecondary(), escaped(fragment.trimmed().toUpper())));
        return;
    }
    QStringList spans;
    for (const CheckPartialMatch& match : matches) {
        spans << callSpan(match);
    }
    m_matchesLabel->setText(spans.join(QStringLiteral("&nbsp;&nbsp; ")));
}

void CheckPartialWidget::setSources(int scpCount, const QString& scpFileName, int historyCount, int seenCount)
{
    m_scpCount = scpCount;
    m_scpFileName = scpFileName;
    m_historyCount = historyCount;
    m_seenCount = seenCount;
    rebuildStatus();
}

void CheckPartialWidget::setMultiplierStatus(const QString& text)
{
    m_multiplierLabel->setVisible(!text.isEmpty());
    if (text.isEmpty()) {
        return;
    }
    // "neu" in Bernstein, alles andere ruhig -- die Farbe trägt hier
    // die eine Aussage, auf die es ankommt.
    QString html = text.toHtmlEscaped();
    // Ein Zeilenumbruch im Text trennt die beiden Aussagen: oben die
    // Station, darunter der Multiplikator.
    html.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    html.replace(QStringLiteral("neu"),
                 QStringLiteral("<span style='color:%1;'>neu</span>").arg(Style::kAmberText()));
    m_multiplierLabel->setText(QStringLiteral("<span style='color:%1;'>%2</span>")
                                   .arg(Style::kTextSecondary(), html));
}

void CheckPartialWidget::rebuildStatus()
{
    const QString dim = Style::kTextScale();
    QStringList parts;
    if (m_scpFileName.isEmpty()) {
        parts << QStringLiteral("<a href='scp' style='color:%1; text-decoration:none;'>SCP-LISTE LADEN…</a>")
                     .arg(Style::kBlueText());
    } else {
        parts << QStringLiteral("SCP %1 (%2)").arg(m_scpCount).arg(escaped(m_scpFileName));
    }
    parts << QStringLiteral("HISTORIE %1").arg(m_historyCount);
    parts << QStringLiteral("GEHÖRT %1").arg(m_seenCount);
    m_statusLabel->setText(QStringLiteral("<span style='color:%1;'>%2</span>")
                               .arg(dim, parts.join(QStringLiteral(" &middot; "))));
}

} // namespace Contestprogramm
