#include "ui/ContestPickerDialog.h"

#include "core/Maidenhead.h"
#include "ui/PanelHeaderBar.h"
#include "ui/StyleKit.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
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
    // The name is what one reads; the id and the bands are short.
    m_table->setColumnWidth(kColName, 250);
    m_table->setColumnWidth(kColId, 150);
    m_table->setColumnWidth(kColBands, 90);

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
    // -- unless the locator below is not valid yet; then it only selects.
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this] {
        if (m_okButton && m_okButton->isEnabled()) {
            accept();
        }
    });

    // The own locator, asked every time -- see the class comment.
    auto* gridCaption = new QLabel(QStringLiteral("Eigener Locator für diesen Contest:"), this);
    m_gridEdit = new QLineEdit(this);
    m_gridEdit->setObjectName(QStringLiteral("contestPickerGridEdit"));
    m_gridEdit->setPlaceholderText(QStringLiteral("z.B. JN67UT"));
    m_gridEdit->setMaxLength(8);
    m_gridEdit->setFont(Style::monoFont(m_gridEdit->font(), Style::kFontBody, QFont::DemiBold));
    m_gridEdit->setFixedWidth(120);
    m_exactGridButton = new QPushButton(this);
    m_exactGridButton->setObjectName(QStringLiteral("contestPickerExactGridButton"));
    m_exactGridButton->hide();
    connect(m_exactGridButton, &QPushButton::clicked, this, [this] { m_gridEdit->setText(m_exactLocationGrid); });
    auto* gridHint = new QLabel(
        QStringLiteral("Der Locator des Standorts, von dem aus gefunkt wird — er geht mit jedem Exchange raus."), this);
    gridHint->setWordWrap(true);
    gridHint->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextSecondary()));
    auto* gridRow = new QHBoxLayout();
    gridRow->addWidget(gridCaption);
    gridRow->addWidget(m_gridEdit);
    gridRow->addWidget(m_exactGridButton);
    gridRow->addStretch();
    connect(m_gridEdit, &QLineEdit::textChanged, this, [this] { refreshGridState(); });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okButton = buttons->button(QDialogButtonBox::Ok);
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Abbrechen"));
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
    bodyLayout->addLayout(gridRow);
    bodyLayout->addWidget(gridHint);
    bodyLayout->addWidget(buttons);
    layout->addLayout(bodyLayout, 1);

    resize(600, 420);
    refreshGridState();
}

void ContestPickerDialog::setOwnGrid(const QString& currentGrid, const QString& exactLocationGrid)
{
    m_exactLocationGrid = exactLocationGrid.trimmed().toUpper();
    m_gridEdit->setText(currentGrid.trimmed().toUpper());
    m_gridEdit->selectAll();
    m_gridEdit->setFocus();
    refreshGridState();
}

QString ContestPickerDialog::ownGrid() const
{
    return m_gridEdit->text().trimmed().toUpper();
}

void ContestPickerDialog::refreshGridState()
{
    const QString grid = ownGrid();
    const bool valid = isValidGridSquare(grid);
    if (m_okButton) {
        m_okButton->setEnabled(valid && !m_availableContests.isEmpty());
    }
    m_gridEdit->setStyleSheet(valid ? QString() : QStringLiteral("QLineEdit { border: 1px solid %1; }").arg(Style::kAmberWarn()));
    // The settings' exact position in another square than the one
    // typed: offer it -- the usual mistake is the home locator left in
    // place at a portable site.
    const bool offerExact = !m_exactLocationGrid.isEmpty() && isValidGridSquare(m_exactLocationGrid)
        && m_exactLocationGrid.left(6) != grid.left(6);
    m_exactGridButton->setVisible(offerExact);
    m_exactGridButton->setText(QStringLiteral("Exakter Standort: %1 übernehmen").arg(m_exactLocationGrid));
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
