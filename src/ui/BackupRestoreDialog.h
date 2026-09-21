#pragma once

#include "data/LogBackup.h"

#include <QDialog>
#include <QString>
#include <QVector>

class QLabel;
class QPushButton;
class QTableWidget;

namespace Contestprogramm {

// Datei > Sicherung wiederherstellen...: the five-minute copies
// (data/LogBackup.h), newest first, with what each one holds once it
// is selected (data/BackupRestore.h's summary, read on demand -- 300
// files are not opened up front). "Wiederherstellen" accepts the
// dialog; MainWindow does the replacing and the restart.
class BackupRestoreDialog : public QDialog {
    Q_OBJECT

public:
    explicit BackupRestoreDialog(const QVector<LogBackup::Entry>& backups, QWidget* parent = nullptr);

    QString selectedPath() const { return m_selectedPath; }

    // Tests
    int rowCount() const;
    void selectRow(int row);
    QString detailText() const;

private:
    void updateSelection();

    QVector<LogBackup::Entry> m_backups;
    QString m_selectedPath;
    QTableWidget* m_table = nullptr;
    QLabel* m_detailLabel = nullptr;
    QPushButton* m_restoreButton = nullptr;
};

} // namespace Contestprogramm
