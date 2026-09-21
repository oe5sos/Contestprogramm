#pragma once

#include <QString>
#include <QWidget>

class QTableWidget;

namespace Contestprogramm {

class PanelHeaderBar;

// Hilfe > Tastenkürzel…: every key and gesture the program answers to,
// as one table for the operator at three in the morning -- the entry
// row's keys, the CW keys, the log corrections, what a click on a
// station, a spot or the radar's reading does, and how the panels are
// moved. Static content; the window is only a place to read it.
class ShortcutsWindow : public QWidget {
    Q_OBJECT

public:
    explicit ShortcutsWindow(QWidget* parent = nullptr);

    // Tests: what the table shows.
    int rowCount() const;
    QString rowText(int row, int column) const;

    struct Entry {
        QString keys;
        QString effect;
        QString where;
    };
    static QVector<Entry> entries();

private:
    PanelHeaderBar* m_header = nullptr;
    QTableWidget* m_table = nullptr;
};

} // namespace Contestprogramm
