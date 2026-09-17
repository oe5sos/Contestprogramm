#pragma once

#include <QString>
#include <QStringList>
#include <QWidget>

class QLabel;
class QTableWidget;
class QTimer;

namespace Contestprogramm {

class ContestDatabase;
class PanelHeaderBar;

// Fenster > Statistik...: the numbers DXLog.net's Statistics window and
// N1MM+'s Score Summary give -- per band (QSOs, km, squares, ODX), per
// UTC hour (QSOs and km, with the best hour marked), and the ten
// longest QSOs. A top-level window like MultiplierWindow, refreshed
// every 30 s while shown and on demand.
class StatisticsWindow : public QWidget {
    Q_OBJECT

public:
    explicit StatisticsWindow(ContestDatabase& database, QWidget* parent = nullptr);

    void setContest(const QString& contestId, const QString& ownGrid, const QStringList& bandOrder,
                    const QString& scoring);

public slots:
    void refresh();

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    ContestDatabase& m_database;
    QString m_contestId;
    QString m_ownGrid;
    QStringList m_bandOrder;
    QString m_scoring = QStringLiteral("distance_km");
    PanelHeaderBar* m_header = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QTableWidget* m_bandTable = nullptr;
    QTableWidget* m_hourTable = nullptr;
    QTableWidget* m_odxTable = nullptr;
    QTimer* m_timer = nullptr;
};

} // namespace Contestprogramm
