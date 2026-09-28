#include "ui/SideAreaWidget.h"

#include "ui/StyleKit.h"


#include <QHBoxLayout>
#include <QMouseEvent>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace Contestprogramm {

namespace {

// Ein Knopf in der Leiste. Senkrecht schmal, mit dem Anfang des
// Panelnamens -- Symbole gibt es in diesem Programm nicht, und ein
// Kürzel liest sich besser als ein erfundenes Piktogramm.
class RailButton : public QToolButton {
public:
    RailButton(const QString& id, const QString& title, QWidget* parent)
        : QToolButton(parent)
        , m_id(id)
    {
        setObjectName(QStringLiteral("sideRail_%1").arg(id));
        setCheckable(true);
        setAutoRaise(true);
        setFixedWidth(SideAreaWidget::kRailWidth - 6);
        setToolTip(title);
        // Zwei Buchstaben: "Ch" für Chat, "Ba" für Bandmap. Der volle
        // Name steht im Tooltip und oben im Panelkopf.
        setText(title.left(2));
        setFont(Style::capsFont(font()));
    }

    QString id() const { return m_id; }

private:
    QString m_id;
};

} // namespace

SideAreaWidget::SideAreaWidget(QWidget* parent)
    : QWidget(parent)
    , m_rail(new QWidget(this))
    , m_stack(new QStackedWidget(this))
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_rail->setObjectName(QLatin1String(kRailObjectName));
    m_rail->setFixedWidth(kRailWidth);
    m_railLayout = new QVBoxLayout(m_rail);
    m_railLayout->setContentsMargins(3, 3, 3, 3);
    m_railLayout->setSpacing(4);
    m_railLayout->addStretch();
    layout->addWidget(m_rail);

    m_stack->setObjectName(QLatin1String(kStackObjectName));
    layout->addWidget(m_stack, 1);
}

void SideAreaWidget::addPage(const QString& id, const QString& title, QWidget* content)
{
    if (id.isEmpty() || !content || m_order.contains(id)) {
        return;
    }
    m_order.append(id);
    m_titles.insert(id, title);
    m_stack->addWidget(content);
    rebuildRail();
    // Das zuletzt Hineingelegte ist das, was man sehen will.
    setActive(id);
}

QWidget* SideAreaWidget::takePage(const QString& id)
{
    const int index = m_order.indexOf(id);
    if (index < 0) {
        return nullptr;
    }
    QWidget* content = m_stack->widget(index);
    if (content) {
        m_stack->removeWidget(content);
        content->setParent(nullptr);
    }
    m_order.removeAt(index);
    m_titles.remove(id);
    if (m_active == id) {
        m_active = m_order.isEmpty() ? QString() : m_order.first();
    }
    rebuildRail();
    if (!m_active.isEmpty()) {
        setActive(m_active);
    }
    return content;
}

void SideAreaWidget::setActive(const QString& id)
{
    const int index = m_order.indexOf(id);
    if (index < 0) {
        return;
    }
    m_active = id;
    m_stack->setCurrentIndex(index);
    if (m_collapsed) {
        // Eine Seite zeigen heißt: den Bereich aufklappen. Sonst
        // klickte man auf ein Symbol und es passierte sichtbar nichts.
        setCollapsed(false);
    }
    updateRailState();
    emit activeChanged(id);
}

void SideAreaWidget::setCollapsed(bool collapsed)
{
    if (m_collapsed == collapsed) {
        return;
    }
    if (collapsed) {
        // Die aufgeklappte Breite merken, damit das Aufklappen dorthin
        // zurückführt und nicht auf irgendeinen Vorgabewert.
        m_expandedWidth = width();
    }
    m_collapsed = collapsed;
    m_stack->setVisible(!collapsed);
    if (collapsed) {
        setFixedWidth(kRailWidth);
    } else {
        setMinimumWidth(0);
        setMaximumWidth(QWIDGETSIZE_MAX);
        resize(std::max(m_expandedWidth, kRailWidth + 120), height());
    }
    emit collapsedChanged(collapsed);
}

void SideAreaWidget::railClicked(const QString& id)
{
    if (!m_order.contains(id)) {
        return;
    }
    if (id == m_active && !m_collapsed) {
        // Nochmal auf das aktive Symbol: zuklappen. Longpaths Variante 2
        // ("Klick aufs aktive Symbol klappt zu"), die Martin dort aus
        // drei Entwürfen gewählt hat.
        setCollapsed(true);
        updateRailState();
        return;
    }
    setActive(id);
}

void SideAreaWidget::updateRailState()
{
    for (QToolButton* button : m_rail->findChildren<QToolButton*>()) {
        const QString id = button->objectName().mid(QStringLiteral("sideRail_").size());
        QSignalBlocker blocker(button);
        button->setChecked(id == m_active && !m_collapsed);
    }
}

void SideAreaWidget::rebuildRail()
{
    // Die Knöpfe neu setzen -- es sind wenige, und so bleibt die
    // Reihenfolge ohne Buchführung richtig.
    QLayoutItem* item = nullptr;
    while ((item = m_railLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    for (const QString& id : m_order) {
        auto* button = new RailButton(id, m_titles.value(id, id), m_rail);
        button->setChecked(id == m_active && !m_collapsed);
        connect(button, &QToolButton::clicked, this, [this, id]() { railClicked(id); });
        button->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(button, &QWidget::customContextMenuRequested, this,
                [this, id](const QPoint&) { emit removeRequested(id); });
        m_railLayout->addWidget(button);
    }
    m_railLayout->addStretch();
}

} // namespace Contestprogramm
