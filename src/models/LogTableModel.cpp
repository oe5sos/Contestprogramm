#include "models/LogTableModel.h"

#include "ui/StyleKit.h"

#include <QDateTime>

namespace Contestprogramm {

LogTableModel::LogTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int LogTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_records.size();
}

int LogTableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return ColumnCount;
}

QVariant LogTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_records.size()) {
        return QVariant();
    }
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    const QsoRecord& record = m_records.at(index.row());
    switch (index.column()) {
    case ColumnTime: {
        const QDateTime dt = QDateTime::fromString(record.timestampUtc, Qt::ISODate);
        return dt.isValid() ? dt.time().toString(QStringLiteral("HH:mm")) : record.timestampUtc;
    }
    case ColumnCallsign:
        return record.callsign;
    case ColumnBand:
        return record.band;
    case ColumnMode:
        return record.mode;
    case ColumnGrid:
        return record.gridSquare;
    case ColumnDistanceKm:
        // Unknown is a dash, not a zero -- HAUSSTIL rule 7. distanceKm
        // is unset for a manually logged QSO with no own-grid/worked-
        // grid pair to compute from (see MainWindow::handleLogRequested),
        // which is genuinely unknown, not a real zero-km contact.
        return record.distanceKm ? QString::number(*record.distanceKm, 'f', 1) : Style::unknownDash();
    case ColumnBearingDeg:
        return record.bearingDeg ? QString::number(*record.bearingDeg, 'f', 0) : Style::unknownDash();
    case ColumnExchangeSent:
        return record.exchangeSent;
    case ColumnExchangeRcvd:
        return record.exchangeRcvd;
    default:
        return QVariant();
    }
}

QVariant LogTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }
    switch (section) {
    case ColumnTime: return QStringLiteral("Zeit");
    case ColumnCallsign: return QStringLiteral("Call");
    case ColumnBand: return QStringLiteral("Band");
    case ColumnMode: return QStringLiteral("Mode");
    case ColumnGrid: return QStringLiteral("Grid");
    case ColumnDistanceKm: return QStringLiteral("km");
    case ColumnBearingDeg: return QStringLiteral("°");
    case ColumnExchangeSent: return QStringLiteral("Exch TX");
    case ColumnExchangeRcvd: return QStringLiteral("Exch RX");
    default: return QVariant();
    }
}

void LogTableModel::setRecords(const QVector<QsoRecord>& records)
{
    beginResetModel();
    m_records = records;
    endResetModel();
}

const QsoRecord& LogTableModel::recordAt(int row) const
{
    return m_records.at(row);
}

bool LogTableModel::updateRecord(const QsoRecord& updated)
{
    for (int row = 0; row < m_records.size(); ++row) {
        if (m_records.at(row).id == updated.id) {
            m_records[row] = updated;
            emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
            return true;
        }
    }
    return false;
}

} // namespace Contestprogramm
