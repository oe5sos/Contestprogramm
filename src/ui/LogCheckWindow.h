#pragma once

#include "data/LogCheck.h"

#include <QString>
#include <QWidget>

class QLabel;
class QPushButton;
class QTableWidget;

namespace Contestprogramm {

class PanelHeaderBar;

// Datei > Log prüfen...: the result of data/LogCheck.h as a list --
// severity, time, call, band, what is wrong -- with a verdict line on
// top ("abgabebereit" or not). A top-level window like StatisticsWindow;
// it holds no database and runs no check itself: MainWindow computes
// the result and hands it in (setResult), asks again on
// refreshRequested(), and jumps to the QSO in the log on qsoActivated()
// (double-click or Enter on a row), so the operator can fix the locator
// right there.
class LogCheckWindow : public QWidget {
    Q_OBJECT

public:
    explicit LogCheckWindow(QWidget* parent = nullptr);

    void setResult(const LogCheckResult& result, const QString& contestName);

    // Tests: what the table shows.
    int rowCount() const;
    QString rowText(int row, int column) const;
    int qsoIdAt(int row) const;
    QString summaryText() const;

signals:
    void refreshRequested();
    void qsoActivated(int qsoId);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void activateRow(int row);

    PanelHeaderBar* m_header = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QTableWidget* m_table = nullptr;
    QPushButton* m_refreshButton = nullptr;
};

} // namespace Contestprogramm
