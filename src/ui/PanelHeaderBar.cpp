#include "ui/PanelHeaderBar.h"

#include "ui/StyleKit.h"
#include "ui/UtcClockWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPushButton>

namespace Contestprogramm {

namespace {
constexpr int kHeaderHeight     = 30;
constexpr int kAccentBarWidth   = 3;
constexpr int kLockButtonW      = 22;
constexpr int kLockButtonH      = 18;
constexpr int kOptionsButtonW   = 20;
constexpr int kOptionsButtonH   = 18;
} // namespace

PanelHeaderBar::PanelHeaderBar(const QString& title, QWidget* parent)
    : QWidget(parent)
    , m_titleLabel(new QLabel(title, this))
    , m_clockWidget(new UtcClockWidget(this))
    , m_lockButton(new QPushButton(this))
    , m_optionsButton(new QPushButton(QString::fromUtf8("⚙"), this))
{
    setFixedHeight(kHeaderHeight);

    m_titleLabel->setFont(Style::capsFont(m_titleLabel->font()));
    m_titleLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                     .arg(Style::kTextSecondary()));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(kAccentBarWidth + 9, 0, 6, 0);
    layout->addWidget(m_titleLabel);
    layout->addStretch(1);

    // Hidden by default, same "opt-in, no space taken until asked for"
    // rule as the lock/options affordances below -- see setClockVisible().
    m_clockWidget->setVisible(false);
    layout->addWidget(m_clockWidget);

    // Options, then lock -- mirrors GridCellWidget::buildCellButtons()'s
    // own ordering (⚙ sits before that row's detach/close pair); lock
    // is this codebase's own right-most affordance, so options goes
    // just before it.
    // Stable objectName -- lets a test (and any future stylesheet ID
    // selector, see StyleKit.h's applyPanelFrameStyle() for the same
    // convention) find this exact button rather than relying on child
    // construction order among PanelHeaderBar's two QPushButtons.
    m_optionsButton->setObjectName(QStringLiteral("panelHeaderOptionsButton"));
    m_optionsButton->setFixedSize(kOptionsButtonW, kOptionsButtonH);
    m_optionsButton->setCursor(Qt::PointingHandCursor);
    m_optionsButton->setToolTip(QStringLiteral("Weitere Einstellungen"));
    m_optionsButton->setStyleSheet(Style::iconButtonStyle());
    m_optionsButton->setVisible(false);
    layout->addWidget(m_optionsButton);
    connect(m_optionsButton, &QPushButton::clicked, this, &PanelHeaderBar::optionsRequested);

    m_lockButton->setFixedSize(kLockButtonW, kLockButtonH);
    m_lockButton->setCursor(Qt::ArrowCursor);
    m_lockButton->setVisible(false);
    layout->addWidget(m_lockButton);
    updateLockButtonAppearance();
    connect(m_lockButton, &QPushButton::clicked, this, [this]() {
        setLocked(!m_locked);
        emit lockToggled(m_locked);
    });
}

void PanelHeaderBar::setTitle(const QString& title)
{
    m_titleLabel->setText(title);
}

void PanelHeaderBar::setLockAffordanceEnabled(bool enabled)
{
    m_lockButton->setVisible(enabled);
}

void PanelHeaderBar::setOptionsAffordanceEnabled(bool enabled)
{
    m_optionsButton->setVisible(enabled);
}

void PanelHeaderBar::setClockVisible(bool visible)
{
    m_clockWidget->setVisible(visible);
}

void PanelHeaderBar::setLocked(bool locked)
{
    if (m_locked == locked) {
        return;
    }
    m_locked = locked;
    updateLockButtonAppearance();
}

void PanelHeaderBar::updateLockButtonAppearance()
{
    // Same glyph pair the real ContainerWidget::updateLockButton() uses
    // (U+1F512/U+1F513) -- reused, not reinvented.
    m_lockButton->setText(m_locked ? QString::fromUtf8("\U0001F512")
                                    : QString::fromUtf8("\U0001F513"));
    m_lockButton->setToolTip(m_locked
        ? QStringLiteral("Gesperrt -- klicken zum Entsperren")
        : QStringLiteral("Panel sperren"));
    m_lockButton->setStyleSheet(Style::lockBadgeStyle(m_locked));
}

void PanelHeaderBar::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    QLinearGradient bg(0, 0, 0, height());
    bg.setColorAt(0.0, QColor(Style::kPanelHeadTop()));
    bg.setColorAt(0.5, QColor(Style::kPanelHeadMid()));
    bg.setColorAt(1.0, QColor(Style::kPanelHeadBot()));
    painter.fillRect(rect(), bg);

    // The accent bar, per HAUSSTIL.md rule 3 -- see the class comment
    // in PanelHeaderBar.h for why there is no "⠿" glyph beside it.
    painter.fillRect(QRect(0, 0, kAccentBarWidth, height()), QColor(Style::kAmberText()));

    painter.setPen(QColor(Style::kTitleBorder()));
    painter.drawLine(0, height() - 1, width(), height() - 1);
}

} // namespace Contestprogramm
