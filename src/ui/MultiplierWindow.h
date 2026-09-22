#pragma once

#include <QString>
#include <QWidget>

class QTableWidget;

namespace Contestprogramm {

class ContestDefinition;
class MultiplierTracker;
class PanelHeaderBar;

// Minimal multiplier checklist window, per the plan's "Multiplier-
// Tracking + Worked/Needed-Raster pro Band" nachziehen item: "kein
// eigenes Grid-Kachel-Fenster (das ist ein späterer, separater visueller
// Pass) -- nur korrekte, abfragbare Daten in einem funktionsfähigen
// Fenster." A plain sortable table, one row per (band, multiplier)
// combination across every band the active ContestDefinition declares,
// so the "worked" column is meaningful even for a multiplier already
// confirmed on one band but not another (a real multi-band contest
// question), not just a one-band worked/not-worked flag.
class MultiplierWindow : public QWidget {
    Q_OBJECT

public:
    explicit MultiplierWindow(MultiplierTracker& tracker, QWidget* parent = nullptr);

    // Which contest to display; call again whenever the active contest
    // changes (MainWindow does this before showing/raising the window).
    void setContest(const QString& contestId, const ContestDefinition* definition);

public slots:
    void refresh();

private:
    void applyBasisWording();

    MultiplierTracker& m_tracker;
    QString m_contestId;
    const ContestDefinition* m_definition = nullptr;
    QTableWidget* m_table = nullptr;
    PanelHeaderBar* m_header = nullptr;
};

} // namespace Contestprogramm
