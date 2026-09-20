#include "ui/LogCheckWindow.h"

#include "ui/PanelHeaderBar.h"
#include "ui/StyleKit.h"

#include <QColor>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace Contestprogramm {

namespace {

enum Column { ColSeverity = 0, ColTime, ColCall, ColBand, ColMessage, ColCount };

constexpr int kQsoIdRole = Qt::UserRole + 1;

QString severityText(LogCheckIssue::Severity severity)
{
    switch (severity) {
    case LogCheckIssue::Severity::Error: return QStringLiteral("FEHLER");
    case LogCheckIssue::Severity::Warning: return QStringLiteral("WARNUNG");
    case LogCheckIssue::Severity::Hint: return QStringLiteral("HINWEIS");
    }
    return QString();
}

QString severityColor(LogCheckIssue::Severity severity)
{
    switch (severity) {
    case LogCheckIssue::Severity::Error: return Style::kRedText();
    case LogCheckIssue::Severity::Warning: return Style::kAmberWarn();
    case LogCheckIssue::Severity::Hint: return Style::kTextSecondary();
    }
    return Style::kTextPrimary();
}

// "2026-10-03T14:01:00Z" -> "03.10. 14:01"
QString shortTime(const QString& iso)
{
    const QDateTime when = QDateTime::fromString(iso, Qt::ISODate);
    if (!when.isValid()) {
        return iso;
    }
    return when.toUTC().toString(QStringLiteral("dd.MM. HH:mm"));
}

} // namespace

LogCheckWindow::LogCheckWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("Contestprogramm - Log prüfen"));
    m_header = new PanelHeaderBar(QStringLiteral("Log prüfen"), this);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setTextFormat(Qt::RichText);
    m_summaryLabel->setWordWrap(true);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(ColCount);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Schwere"), QStringLiteral("Zeit UTC"), QStringLiteral("Call"),
                                        QStringLiteral("Band"), QStringLiteral("Problem")});
    m_table->horizontalHeader()->setFont(Style::capsFont(m_table->horizontalHeader()->font()));
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(20);
    m_table->setFont(Style::monoFont(m_table->font(), Style::kFontSmall));
    m_table->setAlternatingRowColors(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setColumnWidth(ColSeverity, 78);
    m_table->setColumnWidth(ColTime, 92);
    m_table->setColumnWidth(ColCall, 96);
    m_table->setColumnWidth(ColBand, 48);
    // "Activated" is the platform's double-click (or Enter on the
    // current row); wiring cellDoubleClicked as well would fire twice.
    connect(m_table, &QTableWidget::cellActivated, this, [this](int row, int) { activateRow(row); });

    auto* hint = new QLabel(QStringLiteral("Doppelklick springt zum QSO im Log."), this);
    hint->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextSecondary()));
    m_refreshButton = new QPushButton(QStringLiteral("Erneut prüfen"), this);
    connect(m_refreshButton, &QPushButton::clicked, this, &LogCheckWindow::refreshRequested);

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
    footer->addWidget(m_refreshButton);
    body->addLayout(footer);
    layout->addLayout(body, 1);

    resize(900, 440);
}

void LogCheckWindow::setResult(const LogCheckResult& result, const QString& contestName)
{
    m_header->setTitle(contestName.isEmpty() ? QStringLiteral("Log prüfen")
                                             : QStringLiteral("Log prüfen – %1").arg(contestName));

    const QString verdict = result.submittable()
        ? QStringLiteral("<span style=\"color:%1;\">Abgabebereit.</span>").arg(Style::kGreenText())
        : QStringLiteral("<span style=\"color:%1;\">Vor der Abgabe beheben.</span>").arg(Style::kRedText());
    m_summaryLabel->setText(QStringLiteral("%1 QSOs geprüft — %2. %3")
                                .arg(result.checkedQsos)
                                .arg(result.countsText(), verdict));

    m_table->setRowCount(0);
    m_table->setRowCount(result.issues.size());
    for (int row = 0; row < result.issues.size(); ++row) {
        const LogCheckIssue& issue = result.issues.at(row);
        auto* severity = new QTableWidgetItem(severityText(issue.severity));
        severity->setForeground(QColor(severityColor(issue.severity)));
        severity->setFont(Style::capsFont(m_table->font(), Style::kFontSmall));
        severity->setData(kQsoIdRole, issue.qsoId);
        m_table->setItem(row, ColSeverity, severity);
        m_table->setItem(row, ColTime, new QTableWidgetItem(shortTime(issue.timestampUtc)));
        m_table->setItem(row, ColCall, new QTableWidgetItem(issue.callsign));
        m_table->setItem(row, ColBand, new QTableWidgetItem(issue.band));
        auto* message = new QTableWidgetItem(issue.message);
        message->setToolTip(issue.message);
        m_table->setItem(row, ColMessage, message);
        if (issue.qsoId < 0) {
            // Log-wide: nothing to jump to, shown quieter.
            for (int col = 0; col < ColCount; ++col) {
                if (QTableWidgetItem* item = m_table->item(row, col); item && col != ColSeverity) {
                    item->setForeground(QColor(Style::kTextSecondary()));
                }
            }
        }
    }
}

int LogCheckWindow::rowCount() const
{
    return m_table->rowCount();
}

QString LogCheckWindow::rowText(int row, int column) const
{
    const QTableWidgetItem* item = m_table->item(row, column);
    return item ? item->text() : QString();
}

int LogCheckWindow::qsoIdAt(int row) const
{
    const QTableWidgetItem* item = m_table->item(row, ColSeverity);
    return item ? item->data(kQsoIdRole).toInt() : -1;
}

QString LogCheckWindow::summaryText() const
{
    return m_summaryLabel->text();
}

void LogCheckWindow::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    emit refreshRequested();
}

void LogCheckWindow::activateRow(int row)
{
    const int qsoId = qsoIdAt(row);
    if (qsoId >= 0) {
        emit qsoActivated(qsoId);
    }
}

} // namespace Contestprogramm
