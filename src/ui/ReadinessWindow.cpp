#include "ui/ReadinessWindow.h"

#include "ui/PanelHeaderBar.h"
#include "ui/StyleKit.h"

#include <QColor>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

namespace Contestprogramm {

namespace {

enum Column { ColLevel = 0, ColGroup, ColTitle, ColDetail, ColCount };

// The links come and go on their own (a rotctld started, ON4KST logged
// in): while the window is open the snapshot is retaken this often.
constexpr int kRefreshIntervalMs = 5000;

QString levelText(ReadinessItem::Level level)
{
    switch (level) {
    case ReadinessItem::Level::Ok: return QStringLiteral("OK");
    case ReadinessItem::Level::Hint: return QStringLiteral("HINWEIS");
    case ReadinessItem::Level::Warning: return QStringLiteral("WARNUNG");
    case ReadinessItem::Level::Error: return QStringLiteral("FEHLER");
    }
    return QString();
}

QString levelColor(ReadinessItem::Level level)
{
    switch (level) {
    case ReadinessItem::Level::Ok: return Style::kGreenText();
    case ReadinessItem::Level::Hint: return Style::kTextSecondary();
    case ReadinessItem::Level::Warning: return Style::kAmberWarn();
    case ReadinessItem::Level::Error: return Style::kRedText();
    }
    return Style::kTextPrimary();
}

} // namespace

ReadinessWindow::ReadinessWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("Contestprogramm - Startcheck"));
    m_header = new PanelHeaderBar(QStringLiteral("Startcheck — bereit?"), this);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setTextFormat(Qt::RichText);
    m_summaryLabel->setWordWrap(true);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(ColCount);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Status"), QStringLiteral("Bereich"), QStringLiteral("Punkt"),
                                        QStringLiteral("Befund")});
    m_table->horizontalHeader()->setFont(Style::capsFont(m_table->horizontalHeader()->font()));
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(20);
    m_table->setFont(Style::monoFont(m_table->font(), Style::kFontSmall));
    m_table->setAlternatingRowColors(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setWordWrap(false);
    m_table->setColumnWidth(ColLevel, 78);
    m_table->setColumnWidth(ColGroup, 110);
    m_table->setColumnWidth(ColTitle, 150);

    auto* hint = new QLabel(QStringLiteral("Wird alle 5 s neu geprüft, solange das Fenster offen ist."), this);
    hint->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextSecondary()));
    m_settingsButton = new QPushButton(QStringLiteral("Einstellungen…"), this);
    connect(m_settingsButton, &QPushButton::clicked, this, &ReadinessWindow::settingsRequested);
    m_refreshButton = new QPushButton(QStringLiteral("Jetzt prüfen"), this);
    connect(m_refreshButton, &QPushButton::clicked, this, &ReadinessWindow::refreshRequested);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(kRefreshIntervalMs);
    connect(m_refreshTimer, &QTimer::timeout, this, &ReadinessWindow::refreshRequested);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_header);
    auto* body = new QVBoxLayout();
    body->setContentsMargins(10, 10, 10, 10);
    body->setSpacing(6);
    body->addWidget(m_summaryLabel);
    body->addWidget(m_table, 1);
    auto* footer = new QHBoxLayout();
    footer->addWidget(hint);
    footer->addStretch();
    footer->addWidget(m_settingsButton);
    footer->addWidget(m_refreshButton);
    body->addLayout(footer);
    layout->addLayout(body, 1);

    resize(960, 480);
}

void ReadinessWindow::setResult(const ReadinessResult& result)
{
    m_summaryLabel->setText(QStringLiteral("<span style=\"color:%1;\">%2</span>")
                                .arg(result.ready() ? Style::kGreenText() : Style::kRedText(), result.summaryText()));

    m_table->setRowCount(0);
    m_table->setRowCount(result.items.size());
    for (int row = 0; row < result.items.size(); ++row) {
        const ReadinessItem& item = result.items.at(row);
        auto* level = new QTableWidgetItem(levelText(item.level));
        level->setForeground(QColor(levelColor(item.level)));
        level->setFont(Style::capsFont(m_table->font(), Style::kFontSmall));
        m_table->setItem(row, ColLevel, level);
        auto* group = new QTableWidgetItem(item.group);
        group->setForeground(QColor(Style::kTextSecondary()));
        m_table->setItem(row, ColGroup, group);
        m_table->setItem(row, ColTitle, new QTableWidgetItem(item.title));
        auto* detail = new QTableWidgetItem(item.detail);
        detail->setToolTip(item.detail);
        if (item.level == ReadinessItem::Level::Ok) {
            detail->setForeground(QColor(Style::kTextSecondary()));
        }
        m_table->setItem(row, ColDetail, detail);
    }
}

int ReadinessWindow::rowCount() const
{
    return m_table->rowCount();
}

QString ReadinessWindow::rowText(int row, int column) const
{
    const QTableWidgetItem* item = m_table->item(row, column);
    return item ? item->text() : QString();
}

QString ReadinessWindow::summaryText() const
{
    return m_summaryLabel->text();
}

void ReadinessWindow::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    m_refreshTimer->start();
}

void ReadinessWindow::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    m_refreshTimer->stop();
}

} // namespace Contestprogramm
