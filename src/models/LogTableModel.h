#pragma once

#include "data/QsoRecord.h"

#include <QAbstractTableModel>
#include <QVector>

namespace Contestprogramm {

// Backs the log-history section of UnifiedLogWidget's feed table (see
// ui/UnifiedLogWidget.h). One row per logged QSO for whichever contest
// MainWindow currently has loaded.
class LogTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        ColumnTime = 0,
        ColumnCallsign,
        ColumnBand,
        ColumnMode,
        ColumnGrid,
        ColumnDistanceKm,
        ColumnBearingDeg,
        ColumnExchangeSent,
        ColumnExchangeRcvd,
        ColumnCount
    };

    explicit LogTableModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setRecords(const QVector<QsoRecord>& records);
    const QsoRecord& recordAt(int row) const;

    // Patches one already-loaded record in place (matched by id) and
    // emits dataChanged for just that row, instead of a full
    // setRecords() reset -- used by MainWindow's history-row
    // edit-in-place/invalid-toggle handlers (see UnifiedLogWidget) so a
    // hand-correction repaints one row rather than resetting the whole
    // model synchronously out from under the QTableView cell editor
    // that is still mid-commit (see UnifiedLogWidget.h's own
    // reset-safety reasoning for the entry row's live editors -- the
    // same class of risk, one level up, for the feed table's transient
    // cell editors). Returns false (a documented no-op) if no row with
    // that id is currently loaded, e.g. a contest switch happened in
    // between the edit being made and committed.
    bool updateRecord(const QsoRecord& updated);

private:
    QVector<QsoRecord> m_records;
};

} // namespace Contestprogramm
