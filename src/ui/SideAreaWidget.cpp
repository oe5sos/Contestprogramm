#include "ui/SideAreaWidget.h"

#include "ui/SideAreaIcons.h"
#include "ui/StyleKit.h"



#include <QHBoxLayout>
#include <QApplication>
#include <QIcon>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>

namespace Contestprogramm {

namespace {



// Ein Knopf in der Leiste: Symbol links, Name daneben. Martin,
// 2026-09-28, aus drei Blättern gewählt ("C"): "schön wäre, wenn wir
// vielleicht icons dazu hätten".
//
// Die Symbolfarbe folgt dem Zustand, deshalb zwei Bilder in einem
// QIcon -- Qt wählt bei einem ankreuzbaren Knopf selbst zwischen
// QIcon::Off und QIcon::On, ganz ohne Zutun beim Umschalten.
class RailButton : public QToolButton {
public:
    RailButton(const QString& id, const QString& title, QWidget* parent)
        : QToolButton(parent)
        , m_id(id)
        , m_title(title)
    {
        setObjectName(QStringLiteral("sideRail_%1").arg(id));
        setCheckable(true);
        setAutoRaise(true);
        setFixedWidth(SideAreaWidget::kRailWidth - 6);
        setToolTip(title);
        setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        setIconSize(QSize(kIconPx, kIconPx));
        setFont(Style::capsFont(font()));

        QIcon symbol;
        const qreal faktor = devicePixelRatioF() > 0.0 ? devicePixelRatioF() : 1.0;
        const QPixmap still = sideAreaIconPixmap(id, kIconPx, QColor(Style::kTextInactive()), faktor);
        const QPixmap aktiv = sideAreaIconPixmap(id, kIconPx, QColor(Style::kBlueBg()), faktor);
        if (!still.isNull()) {
            symbol.addPixmap(still, QIcon::Normal, QIcon::Off);
            symbol.addPixmap(aktiv, QIcon::Normal, QIcon::On);
            // Beim Daraufzeigen ebenfalls das stille Bild -- die Farbe
            // wechselt sonst zweimal (Hintergrund und Symbol), was
            // unruhig wirkt.
            symbol.addPixmap(still, QIcon::Active, QIcon::Off);
            symbol.addPixmap(aktiv, QIcon::Active, QIcon::On);
            setIcon(symbol);
        }
        // Der Name wird beim Zeichnen gekürzt, nicht hier: die Breite
        // steht erst fest, wenn der Knopf sie hat.
        setText(title);
    }

    QString id() const { return m_id; }

    // Wird gerufen, wenn der Knopf aus der Leiste HERAUSgezogen wurde.
    // Ein Rückruf statt eines Signals: diese Klasse liegt im anonymen
    // Namensraum und hat kein Q_OBJECT.
    std::function<void(const QPoint&)> onDragOut;

    static constexpr int kIconPx = 17;

protected:
    // Ziehen als Rückweg aus dem Bereich. Martin, 2026-09-28: "die
    // widgets sollte man aber auch wieder per drag and drop rausziehen
    // können, in dem fall nach rechts." Hinein geht es schon so (am
    // Panelkopf packen); hinaus gab es nur den Rechtsklick, und den
    // findet man nicht von selbst.
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_pressGlobal = event->globalPosition().toPoint();
            m_zieht = false;
        }
        QToolButton::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if ((event->buttons() & Qt::LeftButton) && !m_zieht && !m_pressGlobal.isNull()) {
            const QPoint jetzt = event->globalPosition().toPoint();
            if ((jetzt - m_pressGlobal).manhattanLength() >= QApplication::startDragDistance()) {
                m_zieht = true;
                setCursor(Qt::ClosedHandCursor);
            }
        }
        QToolButton::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (m_zieht && event->button() == Qt::LeftButton) {
            m_zieht = false;
            m_pressGlobal = QPoint();
            unsetCursor();
            setDown(false);
            const QPoint wo = event->globalPosition().toPoint();
            // NICHT an die Basisklasse weitergeben: die würde den Knopf
            // umschalten, und ein Zug wäre zugleich ein Klick.
            if (onDragOut) {
                onDragOut(wo);
            }
            return;
        }
        m_pressGlobal = QPoint();
        QToolButton::mouseReleaseEvent(event);
    }

    // Ein zu langer Name ("Karte / Verbindungen") würde den Knopf
    // aufblähen; QToolButton kürzt von sich aus nicht.
    void resizeEvent(QResizeEvent* event) override
    {
        QToolButton::resizeEvent(event);
        // Symbol, Randbalken, beide Polster und der Abstand zwischen
        // Symbol und Text gehen ab. Zu knapp gerechnet kürzt Qt selbst
        // noch einmal nach -- und zwar in der MITTE ("KARTE ...ERBIN"),
        // was schlechter lesbar ist als ein sauberes Ende.
        const int fuerText = width() - kIconPx - 34;
        const QString gekuerzt = fontMetrics().elidedText(m_title, Qt::ElideRight, std::max(10, fuerText));
        if (gekuerzt != text()) {
            setText(gekuerzt);
        }
    }

private:
    QString m_id;
    QString m_title;
    QPoint m_pressGlobal;
    bool m_zieht = false;
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
        // toggled, NICHT clicked: ein ankreuzbarer Knopf, der über die
        // Bedienungshilfen gedrückt wird (VoiceOver, Automatisierung),
        // bekommt seinen Zustand direkt gesetzt -- das ergibt toggled,
        // aber kein clicked. Live gefunden 2026-09-28: der Knopf wurde
        // hervorgehoben, die Seite wechselte nicht, und im Mitschrieb
        // stand keine einzige Zeile. Ein Mausklick löst beides aus,
        // toggled deckt also beide Wege ab.
        //
        // Doppelt läuft dabei nichts: updateRailState() setzt den
        // Zustand mit QSignalBlocker, ein programmatisches Nachziehen
        // landet also nicht wieder hier.
        connect(button, &QToolButton::toggled, this, [this, id](bool) { railClicked(id); });
        // Herausziehen: der Bereich meldet es nur, entschieden wird
        // draußen (MainWindow weiß, wohin das Panel auf der Fläche
        // gehört und ob die Stelle überhaupt außerhalb liegt).
        button->onDragOut = [this, id](const QPoint& globalPos) { emit pageDraggedOut(id, globalPos); };
        button->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(button, &QWidget::customContextMenuRequested, this,
                [this, id](const QPoint&) { emit removeRequested(id); });
        m_railLayout->addWidget(button);
    }
    m_railLayout->addStretch();
}

} // namespace Contestprogramm
