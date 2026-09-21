#pragma once

#include "data/ReadinessCheck.h"

#include <QString>
#include <QWidget>

class QLabel;
class QPushButton;
class QTableWidget;
class QTimer;

namespace Contestprogramm {

class PanelHeaderBar;

// Datei > Startcheck (Bereit?): the result of data/ReadinessCheck.h as
// a list -- status, area, item, what was found -- with the verdict on
// top. A top-level window like LogCheckWindow; it runs no check
// itself: MainWindow fills the snapshot and hands the result in
// (setResult), and is asked again on refreshRequested() -- every few
// seconds while the window is open, because the links it shows come
// and go, and on the button. settingsRequested() opens the settings
// dialog for whatever needs fixing.
class ReadinessWindow : public QWidget {
    Q_OBJECT

public:
    explicit ReadinessWindow(QWidget* parent = nullptr);

    void setResult(const ReadinessResult& result);

    // Tests: what the table shows.
    int rowCount() const;
    QString rowText(int row, int column) const;
    QString summaryText() const;

signals:
    void refreshRequested();
    void settingsRequested();

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    PanelHeaderBar* m_header = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QTableWidget* m_table = nullptr;
    QPushButton* m_refreshButton = nullptr;
    QPushButton* m_settingsButton = nullptr;
    QTimer* m_refreshTimer = nullptr;
};

} // namespace Contestprogramm
