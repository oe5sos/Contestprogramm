#include "ui/ContestPickerDialog.h"

#include "ui/PanelHeaderBar.h"
#include "ui/StyleKit.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace Contestprogramm {

namespace {
constexpr int kColName = 0;
constexpr int kColId = 1;
constexpr int kColBands = 2;
constexpr int kColExchange = 3;

// "Serial, Grid" style summary -- enough to tell two contests with
// similar names apart at a glance, same purpose ContestRulesEditor's own
// field table serves in more detail.
QString exchangeFieldSummary(const ContestDefinition& def)
{
    QStringList labels;
    for (const ContestDefinition::ExchangeField& field : def.exchangeFields()) {
        labels << field.label;
    }
    return labels.join(QStringLiteral(", "));
}
} // namespace

ContestPickerDialog::ContestPickerDialog(const QVector<ContestDefinition>& availableContests,
                                          const QString& initialContestId,
                                          QWidget* parent)
    : QDialog(parent)
    , m_availableContests(availableContests)
    , m_table(new QTableWidget(this))
{
    setWindowTitle(QStringLiteral("Contestprogramm - Contest wählen"));
    setModal(true);

    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("Name"), QStringLiteral("ID"), QStringLiteral("Bänder"), QStringLiteral("Exchange")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setFont(Style::capsFont(m_table->horizontalHeader()->font()));
    m_table->setFont(Style::monoFont(m_table->font(), Style::kFontSmall));
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);

    int preselectRow = -1;
    m_table->setRowCount(m_availableContests.size());
    for (int row = 0; row < m_availableContests.size(); ++row) {
        const ContestDefinition& def = m_availableContests.at(row);
        m_table->setItem(row, kColName, new QTableWidgetItem(def.name()));
        m_table->setItem(row, kColId, new QTableWidgetItem(def.id()));
        m_table->setItem(row, kColBands, new QTableWidgetItem(def.bands().join(QStringLiteral(" / "))));
        m_table->setItem(row, kColExchange, new QTableWidgetItem(exchangeFieldSummary(def)));
        if (def.id() == initialContestId) {
            preselectRow = row;
        }
    }
    if (preselectRow < 0 && !m_availableContests.isEmpty()) {
        preselectRow = 0;
    }
    if (preselectRow >= 0) {
        m_table->selectRow(preselectRow);
    }

    // Double-click a row commits it immediately, matching the OK
    // button -- a plain list-picker convention, not something ContestRulesEditor's
    // table (an editable grid, not a picker) needs.
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &QDialog::accept);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Real top-level (dialog) window chrome, same posture as
    // ContestRulesEditor/MultiplierWindow: no rounded-panel wrapper
    // around the whole dialog (a QSS border-radius on a top-level
    // widget's own rect leaves square corner gaps), only the header bar
    // gets house-style chrome.
    auto* header = new PanelHeaderBar(QStringLiteral("Contest wählen"), this);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(header);

    auto* bodyLayout = new QVBoxLayout();
    bodyLayout->setContentsMargins(10, 10, 10, 10);
    bodyLayout->addWidget(m_table, 1);
    bodyLayout->addWidget(buttons);
    layout->addLayout(bodyLayout, 1);

    resize(560, 360);
}

QString ContestPickerDialog::selectedContestId() const
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_availableContests.size()) {
        return QString();
    }
    return m_availableContests.at(row).id();
}

} // namespace Contestprogramm
