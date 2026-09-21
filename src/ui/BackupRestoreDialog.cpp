#include "ui/BackupRestoreDialog.h"

#include "data/BackupRestore.h"
#include "ui/StyleKit.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace Contestprogramm {

namespace {

QString sizeText(qint64 bytes)
{
    return QLocale(QLocale::German, QLocale::Austria).formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}

} // namespace

BackupRestoreDialog::BackupRestoreDialog(const QVector<LogBackup::Entry>& backups, QWidget* parent)
    : QDialog(parent)
    , m_backups(backups)
{
    setWindowTitle(QStringLiteral("Sicherung wiederherstellen"));
    setModal(true);

    auto* intro = new QLabel(QStringLiteral("Jede Minute, in der ein QSO dazukam, wird eine Kopie des Logs geschrieben. Eine Wiederherstellung "
                                            "setzt das Log auf diesen Stand zurück; der jetzige Stand wird vorher selbst "
                                            "gesichert. Das Programm startet danach neu."),
                             this);
    intro->setWordWrap(true);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(2);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Stand (UTC)"), QStringLiteral("Größe")});
    m_table->horizontalHeader()->setFont(Style::capsFont(m_table->horizontalHeader()->font()));
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setFont(Style::monoFont(m_table->font(), Style::kFontSmall));
    m_table->setAlternatingRowColors(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setColumnWidth(0, 170);
    m_table->setRowCount(m_backups.size());
    for (int row = 0; row < m_backups.size(); ++row) {
        const LogBackup::Entry& entry = m_backups.at(row);
        m_table->setItem(row, 0, new QTableWidgetItem(entry.utc.toString(QStringLiteral("yyyy-MM-dd HH:mm"))));
        auto* size = new QTableWidgetItem(sizeText(entry.bytes));
        size->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(row, 1, size);
    }
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &BackupRestoreDialog::updateSelection);

    m_detailLabel = new QLabel(this);
    m_detailLabel->setWordWrap(true);
    m_detailLabel->setFont(Style::monoFont(m_detailLabel->font(), Style::kFontSmall));
    m_detailLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextSecondary()));

    auto* buttons = new QDialogButtonBox(this);
    m_restoreButton = buttons->addButton(QStringLiteral("Wiederherstellen…"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QStringLiteral("Abbrechen"), QDialogButtonBox::RejectRole);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addWidget(m_table, 1);
    layout->addWidget(m_detailLabel);
    layout->addWidget(buttons);
    resize(520, 440);

    if (m_backups.isEmpty()) {
        m_detailLabel->setText(QStringLiteral("Noch keine Sicherung vorhanden."));
    }
    updateSelection();
}

void BackupRestoreDialog::updateSelection()
{
    const QList<QTableWidgetItem*> selected = m_table->selectedItems();
    const int row = selected.isEmpty() ? -1 : selected.first()->row();
    if (row < 0 || row >= m_backups.size()) {
        m_selectedPath.clear();
        m_restoreButton->setEnabled(false);
        return;
    }
    m_selectedPath = m_backups.at(row).path;
    m_restoreButton->setEnabled(true);
    m_detailLabel->setText(QStringLiteral("%1\n%2").arg(m_backups.at(row).path, summarizeBackup(m_selectedPath)));
}

int BackupRestoreDialog::rowCount() const
{
    return m_table->rowCount();
}

void BackupRestoreDialog::selectRow(int row)
{
    m_table->selectRow(row);
}

QString BackupRestoreDialog::detailText() const
{
    return m_detailLabel->text();
}

} // namespace Contestprogramm
