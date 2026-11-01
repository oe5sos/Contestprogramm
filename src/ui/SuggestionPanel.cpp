#include "ui/SuggestionPanel.h"

#include "ui/StyleKit.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace Contestprogramm {

namespace {
// Same rich-text label+value span technique RateMeterWidget's own
// readoutSpan() uses -- a caps caption in kTextScale followed by a
// monospace value, so this panel's headline reads consistently with
// the rest of the dockable canvas.
QString readoutSpan(const QString& label, const QString& value)
{
    // Die Familienliste aus StyleKit, nicht "Menlo": das gibt es nur auf
    // dem Mac. Auf Windows und Linux fiel genau dieser eine Wert aus der
    // Monoschrift heraus, waehrend alles andere laengst ueber
    // Style::monoFont() mit Rueckfallkette laeuft.
    return QStringLiteral(
               "<span style='color:%1; font-size:%2px;'>%3</span>"
               "&nbsp;<span style='font-family:%7; font-size:%4px; color:%5;'>%6</span>")
        .arg(Style::kTextScale())
        .arg(Style::kFontCaption)
        .arg(label.toUpper())
        .arg(Style::kFontSub)
        .arg(Style::kTextPrimary())
        .arg(value)
        .arg(Style::monoFontFamilyCss());
}
} // namespace

SuggestionPanel::SuggestionPanel(QWidget* parent)
    : QWidget(parent)
    , m_targetLabel(new QLabel(this))
    , m_messageLabel(new QLabel(this))
    , m_acceptButton(new QPushButton(QStringLiteral("Ziel übernehmen"), this))
    , m_sendButton(new QPushButton(QStringLiteral("Ruf senden"), this))
{
    // A plain bordered/rounded panel, no header bar of its own content
    // -- matches RateMeterWidget's own bare `.panel` treatment; this
    // panel is registered chromeless in MainWindow (PanelHeaderBar
    // supplies the draggable title/lock chrome from the outside, same
    // as every other chromeless-content panel).
    Style::applyPanelFrameStyle(this);

    m_targetLabel->setTextFormat(Qt::RichText);
    m_targetLabel->setFont(Style::capsFont(m_targetLabel->font()));
    m_targetLabel->setWordWrap(true);

    m_messageLabel->setFont(Style::monoFont(m_messageLabel->font(), Style::kFontBody));
    m_messageLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextSecondary()));
    m_messageLabel->setWordWrap(true);

    auto* buttonRow = new QWidget(this);
    auto* buttonLayout = new QHBoxLayout(buttonRow);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->addWidget(m_acceptButton);
    buttonLayout->addWidget(m_sendButton);
    buttonLayout->addStretch(1);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(8);
    layout->addWidget(m_targetLabel);
    layout->addWidget(m_messageLabel);
    layout->addWidget(buttonRow);
    layout->addStretch(1);

    connect(m_acceptButton, &QPushButton::clicked, this, [this]() {
        if (!m_currentSuggestion) {
            return;
        }
        emit targetAccepted(m_currentSuggestion->callsign, m_currentSuggestion->grid, m_currentSuggestion->freqHz);
    });
    connect(m_sendButton, &QPushButton::clicked, this, [this]() {
        if (!m_currentSuggestion || m_currentDraft.isEmpty()) {
            return;
        }
        emit sendRequested(m_currentDraft);
    });

    // Empty/disabled until the first setSuggestion() call -- matches
    // RateMeterWidget's own "genuinely unknown, not a zero" no-source
    // state (HAUSSTIL rule 7).
    setSuggestion(std::nullopt, std::nullopt, std::nullopt, QString());
}

void SuggestionPanel::setSuggestion(const std::optional<SpotCandidate>& suggestion,
                                     const std::optional<double>& distanceKm,
                                     const std::optional<double>& bearingDeg,
                                     const QString& draftedMessage)
{
    m_currentSuggestion = suggestion;
    m_currentDraft = suggestion ? draftedMessage : QString();

    if (!suggestion) {
        m_targetLabel->setText(readoutSpan(QStringLiteral("Ziel"), Style::unknownDash()));
        m_messageLabel->clear();
        m_acceptButton->setEnabled(false);
        m_sendButton->setEnabled(false);
        return;
    }

    const QString grid = suggestion->grid.isEmpty() ? Style::unknownDash() : suggestion->grid;
    const QString km = distanceKm ? QStringLiteral("%1 km").arg(QString::number(*distanceKm, 'f', 1))
                                   : Style::unknownDash();
    const QString deg = bearingDeg ? QStringLiteral("%1°").arg(QString::number(*bearingDeg, 'f', 0))
                                    : Style::unknownDash();
    m_targetLabel->setText(readoutSpan(QStringLiteral("Ziel"),
                                        QStringLiteral("%1 &middot; %2 &middot; %3 &middot; %4")
                                            .arg(suggestion->callsign, grid, km, deg)));
    m_messageLabel->setText(draftedMessage);
    m_acceptButton->setEnabled(true);
    m_sendButton->setEnabled(!draftedMessage.isEmpty());
}

} // namespace Contestprogramm
