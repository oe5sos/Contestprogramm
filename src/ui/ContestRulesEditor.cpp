#include "ui/ContestRulesEditor.h"

#include "ui/PanelHeaderBar.h"
#include "ui/StyleKit.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace Contestprogramm {

namespace {
constexpr int kColKey = 0;
constexpr int kColLabel = 1;
constexpr int kColType = 2;
constexpr int kColAutoIncrement = 3;

QTableWidgetItem* makeAutoIncrementItem(bool checked)
{
    auto* item = new QTableWidgetItem();
    item->setFlags((item->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
    item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    return item;
}
} // namespace

ContestRulesEditor::ContestRulesEditor(const QVector<ContestDefinition>& availableContests,
                                        const QString& initialContestId,
                                        QWidget* parent)
    : QDialog(parent)
    , m_availableContests(availableContests)
    , m_contestCombo(new QComboBox(this))
    , m_fieldsTable(new QTableWidget(this))
    , m_addButton(new QPushButton(QStringLiteral("+ Feld"), this))
    , m_removeButton(new QPushButton(QStringLiteral("- Feld"), this))
    , m_moveUpButton(new QPushButton(QStringLiteral("Auf"), this))
    , m_moveDownButton(new QPushButton(QStringLiteral("Ab"), this))
{
    setWindowTitle(QStringLiteral("Contestprogramm - Contest-Regeln"));
    setModal(true);

    for (const ContestDefinition& def : m_availableContests) {
        m_contestCombo->addItem(def.name(), def.id());
    }
    const int initialIdx = m_contestCombo->findData(initialContestId);
    m_contestCombo->setCurrentIndex(initialIdx >= 0 ? initialIdx : 0);

    m_fieldsTable->setColumnCount(4);
    m_fieldsTable->setHorizontalHeaderLabels(
        {QStringLiteral("Key"), QStringLiteral("Label"), QStringLiteral("Typ"), QStringLiteral("Serial (auto)")});
    m_fieldsTable->horizontalHeader()->setStretchLastSection(true);
    m_fieldsTable->horizontalHeader()->setFont(Style::capsFont(m_fieldsTable->horizontalHeader()->font()));
    m_fieldsTable->setFont(Style::monoFont(m_fieldsTable->font(), Style::kFontSmall));
    m_fieldsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fieldsTable->setSelectionMode(QAbstractItemView::SingleSelection);

    auto* toolRow = new QWidget(this);
    auto* toolLayout = new QHBoxLayout(toolRow);
    toolLayout->setContentsMargins(0, 0, 0, 0);
    toolLayout->addWidget(m_addButton);
    toolLayout->addWidget(m_removeButton);
    toolLayout->addWidget(m_moveUpButton);
    toolLayout->addWidget(m_moveDownButton);
    toolLayout->addStretch();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Save), &QPushButton::clicked, this, &ContestRulesEditor::onSave);

    // No rounded-panel wrapper: like MultiplierWindow, this is a real
    // top-level (dialog) window, and a QSS border-radius on a top-level
    // widget's own rect leaves square corner gaps showing whatever the
    // OS paints behind it -- see MultiplierWindow.cpp's own comment on
    // this. Only the header bar gets its own chrome.
    auto* header = new PanelHeaderBar(QStringLiteral("Contest-Regeln"), this);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(header);

    auto* bodyLayout = new QVBoxLayout();
    bodyLayout->setContentsMargins(10, 10, 10, 10);
    bodyLayout->addWidget(m_contestCombo);
    bodyLayout->addWidget(m_fieldsTable, 1);
    bodyLayout->addWidget(toolRow);
    bodyLayout->addWidget(buttons);
    layout->addLayout(bodyLayout, 1);

    connect(m_contestCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ContestRulesEditor::onContestSelectionChanged);
    connect(m_addButton, &QPushButton::clicked, this, &ContestRulesEditor::onAddField);
    connect(m_removeButton, &QPushButton::clicked, this, &ContestRulesEditor::onRemoveField);
    connect(m_moveUpButton, &QPushButton::clicked, this, &ContestRulesEditor::onMoveFieldUp);
    connect(m_moveDownButton, &QPushButton::clicked, this, &ContestRulesEditor::onMoveFieldDown);

    onContestSelectionChanged(m_contestCombo->currentIndex());

    resize(480, 420);
}

const ContestDefinition* ContestRulesEditor::selectedDefinition() const
{
    const QString id = m_contestCombo->currentData().toString();
    for (const ContestDefinition& def : m_availableContests) {
        if (def.id() == id) {
            return &def;
        }
    }
    return nullptr;
}

void ContestRulesEditor::loadFieldsIntoTable(const QVector<ContestDefinition::ExchangeField>& fields)
{
    m_fieldsTable->setRowCount(0);
    for (const ContestDefinition::ExchangeField& field : fields) {
        const int row = m_fieldsTable->rowCount();
        m_fieldsTable->insertRow(row);
        m_fieldsTable->setItem(row, kColKey, new QTableWidgetItem(field.key));
        m_fieldsTable->setItem(row, kColLabel, new QTableWidgetItem(field.label));
        m_fieldsTable->setItem(row, kColType, new QTableWidgetItem(field.type));
        m_fieldsTable->setItem(row, kColAutoIncrement, makeAutoIncrementItem(field.autoIncrement));
    }
}

QVector<ContestDefinition::ExchangeField> ContestRulesEditor::fieldsFromTable() const
{
    QVector<ContestDefinition::ExchangeField> fields;
    for (int row = 0; row < m_fieldsTable->rowCount(); ++row) {
        QTableWidgetItem* keyItem = m_fieldsTable->item(row, kColKey);
        const QString key = keyItem ? keyItem->text().trimmed() : QString();
        if (key.isEmpty()) {
            // A blank scratch row (e.g. "+ Feld" clicked but never
            // filled in) -- dropped rather than saved as a nameless
            // field.
            continue;
        }
        ContestDefinition::ExchangeField field;
        field.key = key;
        QTableWidgetItem* labelItem = m_fieldsTable->item(row, kColLabel);
        field.label = labelItem ? labelItem->text().trimmed() : QString();
        QTableWidgetItem* typeItem = m_fieldsTable->item(row, kColType);
        field.type = typeItem ? typeItem->text().trimmed() : QString();
        QTableWidgetItem* autoItem = m_fieldsTable->item(row, kColAutoIncrement);
        field.autoIncrement = autoItem && autoItem->checkState() == Qt::Checked;
        fields.append(field);
    }
    return fields;
}

void ContestRulesEditor::onContestSelectionChanged(int /*index*/)
{
    if (const ContestDefinition* def = selectedDefinition()) {
        loadFieldsIntoTable(def->exchangeFields());
    } else {
        m_fieldsTable->setRowCount(0);
    }
}

void ContestRulesEditor::onAddField()
{
    const int row = m_fieldsTable->rowCount();
    m_fieldsTable->insertRow(row);
    m_fieldsTable->setItem(row, kColKey, new QTableWidgetItem(QString()));
    m_fieldsTable->setItem(row, kColLabel, new QTableWidgetItem(QString()));
    m_fieldsTable->setItem(row, kColType, new QTableWidgetItem(QStringLiteral("text")));
    m_fieldsTable->setItem(row, kColAutoIncrement, makeAutoIncrementItem(false));
    m_fieldsTable->setCurrentCell(row, kColKey);
    m_fieldsTable->editItem(m_fieldsTable->item(row, kColKey));
}

void ContestRulesEditor::onRemoveField()
{
    const int row = m_fieldsTable->currentRow();
    if (row >= 0) {
        m_fieldsTable->removeRow(row);
    }
}

void ContestRulesEditor::onMoveFieldUp()
{
    const int row = m_fieldsTable->currentRow();
    if (row <= 0) {
        return;
    }
    for (int col = 0; col < m_fieldsTable->columnCount(); ++col) {
        QTableWidgetItem* above = m_fieldsTable->takeItem(row - 1, col);
        QTableWidgetItem* current = m_fieldsTable->takeItem(row, col);
        m_fieldsTable->setItem(row - 1, col, current);
        m_fieldsTable->setItem(row, col, above);
    }
    m_fieldsTable->setCurrentCell(row - 1, 0);
}

void ContestRulesEditor::onMoveFieldDown()
{
    const int row = m_fieldsTable->currentRow();
    if (row < 0 || row >= m_fieldsTable->rowCount() - 1) {
        return;
    }
    for (int col = 0; col < m_fieldsTable->columnCount(); ++col) {
        QTableWidgetItem* below = m_fieldsTable->takeItem(row + 1, col);
        QTableWidgetItem* current = m_fieldsTable->takeItem(row, col);
        m_fieldsTable->setItem(row + 1, col, current);
        m_fieldsTable->setItem(row, col, below);
    }
    m_fieldsTable->setCurrentCell(row + 1, 0);
}

void ContestRulesEditor::onSave()
{
    const ContestDefinition* def = selectedDefinition();
    if (!def) {
        QMessageBox::warning(this, QStringLiteral("Contestprogramm"), QStringLiteral("Kein Contest ausgewählt."));
        return;
    }

    const QVector<ContestDefinition::ExchangeField> fields = fieldsFromTable();
    if (fields.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Contestprogramm"),
                              QStringLiteral("Mindestens ein Exchange-Feld wird benötigt."));
        return;
    }

    QSet<QString> seenKeys;
    for (const ContestDefinition::ExchangeField& field : fields) {
        if (seenKeys.contains(field.key)) {
            QMessageBox::warning(this, QStringLiteral("Contestprogramm"),
                                  QStringLiteral("Feld-Key \"%1\" ist mehrfach vergeben.").arg(field.key));
            return;
        }
        seenKeys.insert(field.key);
    }

    const ContestDefinition updated = def->withExchangeFields(fields);
    QString error;
    if (!updated.saveToFile(ContestDefinition::overrideFilePath(def->id()), &error)) {
        QMessageBox::warning(this, QStringLiteral("Contestprogramm"),
                              QStringLiteral("Konnte nicht gespeichert werden:\n%1").arg(error));
        return;
    }

    accept();
}

} // namespace Contestprogramm
