#include "ui/ProfileRail.h"

#include "ui/StyleKit.h"

#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>

namespace Contestprogramm {

namespace {
constexpr int kBadgeSize = 32;
constexpr int kBadgeSpacing = 8;
} // namespace

ProfileRail::ProfileRail(QWidget* parent)
    : QWidget(parent)
{
    setFixedWidth(kWidth);
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral("QWidget { background: %1; border-right: 1px solid %2; }")
                      .arg(Style::kAppBg(), Style::kBorderSubtle()));

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(6, 12, 6, 12);
    outer->setSpacing(kBadgeSpacing);

    m_badgeColumn = new QVBoxLayout();
    m_badgeColumn->setSpacing(kBadgeSpacing);
    outer->addLayout(m_badgeColumn);
    outer->addStretch(1);

    auto* addButton = new QPushButton(QStringLiteral("+"), this);
    addButton->setFixedSize(kBadgeSize, kBadgeSize);
    addButton->setCursor(Qt::PointingHandCursor);
    addButton->setToolTip(QStringLiteral("Neues Profil (leer)"));
    addButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; border: 1px dashed %1;"
        " border-radius: %2px; color: %1; font-size: 16px; }"
        "QPushButton:hover { border-color: %3; color: %3; }")
        .arg(Style::kTextInactive())
        .arg(kBadgeSize / 2)
        .arg(Style::kAmberText()));
    connect(addButton, &QPushButton::clicked, this, &ProfileRail::newProfileRequested);
    outer->addWidget(addButton, 0, Qt::AlignHCenter);
}

void ProfileRail::setProfiles(const QStringList& names, const QString& activeName)
{
    m_names = names;
    m_activeName = activeName;
    rebuildBadges();
}

void ProfileRail::rebuildBadges()
{
    QLayoutItem* item = nullptr;
    while ((item = m_badgeColumn->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    for (const QString& name : std::as_const(m_names)) {
        m_badgeColumn->addWidget(makeBadge(name, name == m_activeName), 0, Qt::AlignHCenter);
    }
}

QPushButton* ProfileRail::makeBadge(const QString& name, bool active)
{
    // First character only -- HAUSSTIL rule 1 ("caps, tracked"), and a
    // 32px circle has room for one glyph, not a whole profile name.
    const QString glyph = name.isEmpty() ? QStringLiteral("?") : name.left(1).toUpper();
    auto* badge = new QPushButton(glyph, this);
    badge->setFixedSize(kBadgeSize, kBadgeSize);
    badge->setCursor(Qt::PointingHandCursor);
    badge->setToolTip(name);
    badge->setFont(Style::capsFont(badge->font(), Style::kFontSmall));
    badge->setStyleSheet(active
        ? QStringLiteral(
              "QPushButton { background: %1; border: 2px solid %2; border-radius: %3px; color: %4; }")
              .arg(Style::kBlueBg(), Style::kBlueBorder())
              .arg(kBadgeSize / 2)
              .arg(Style::kBlueText())
        : QStringLiteral(
              "QPushButton { background: %1; border: 1px solid %2; border-radius: %3px; color: %4; }"
              "QPushButton:hover { border-color: %5; }")
              .arg(Style::kButtonBg(), Style::kBorder())
              .arg(kBadgeSize / 2)
              .arg(Style::kTextSecondary(), Style::kAmberText()));
    connect(badge, &QPushButton::clicked, this, [this, name]() { emit profileActivated(name); });

    badge->setContextMenuPolicy(Qt::CustomContextMenu);
    // Removing the last remaining profile would leave the rail with
    // nothing to switch back to -- LayoutProfileManager::removeProfile()
    // already refuses that too, but leaving the action out here avoids
    // an unexplained no-op click.
    const bool canRemove = m_names.size() > 1;
    connect(badge, &QPushButton::customContextMenuRequested, this, [this, badge, name, canRemove](const QPoint& pos) {
        QMenu menu(badge);
        QAction* renameAction = menu.addAction(QStringLiteral("Umbenennen…"));
        QAction* duplicateAction = menu.addAction(QStringLiteral("Duplizieren"));
        QAction* removeAction = canRemove ? menu.addAction(QStringLiteral("Entfernen")) : nullptr;
        QAction* chosen = menu.exec(badge->mapToGlobal(pos));
        if (chosen == renameAction) {
            emit renameRequested(name);
        } else if (chosen == duplicateAction) {
            emit duplicateRequested(name);
        } else if (removeAction != nullptr && chosen == removeAction) {
            emit removeRequested(name);
        }
    });

    return badge;
}

} // namespace Contestprogramm
