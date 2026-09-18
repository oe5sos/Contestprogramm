#pragma once

#include "core/SkedList.h"

#include <QDateTime>
#include <QString>
#include <QVector>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace Contestprogramm {

// The next hour as a strip: a tick every ten minutes, the current
// minute as a dashed line, every open sked as a chip at its minute,
// the next one in amber. Painted, not laid out -- a chip's place is
// its time.
class SkedTimeline : public QWidget {
    Q_OBJECT

public:
    explicit SkedTimeline(QWidget* parent = nullptr);
    void setSkeds(const QVector<Sked>& skeds, const QDateTime& nowUtc, int nextId);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<Sked> m_skeds;
    QDateTime m_nowUtc;
    int m_nextId = -1;
};

// The "Skeds" panel (design sheet B, 2026-09-18): timeline, the list
// (Zeit, Call, Loc, QRG, Richtung, km, Status), an entry row. A click on
// an open sked is the whole point -- MainWindow tunes the rig, turns the
// rotor and fills the entry row (skedActivated); a click on a suggestion
// from the ON4KST chat makes it a sked (suggestionAccepted); Delete
// removes the selected one. The panel owns no data: MainWindow keeps
// the skeds in ContestDatabase and pushes them in with setSkeds().
class SkedPanel : public QWidget {
    Q_OBJECT

public:
    explicit SkedPanel(QWidget* parent = nullptr);

    // Own locator for bearing/km per sked (empty: dashes).
    void setOwnGrid(const QString& grid);
    void setSkeds(const QVector<Sked>& skeds, const QDateTime& nowUtc);

    // The entry row's fields, for tests and for MainWindow to clear.
    QString entryCallsign() const;
    void clearEntry();

signals:
    void skedActivated(int skedId);
    void suggestionAccepted(int skedId);
    void addRequested(const QString& callsign, const QString& grid, const QString& qrgText, const QString& timeText);
    void deleteRequested(int skedId);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void handleRowClicked(int row);
    void handleAddClicked();

    QString m_ownGrid;
    QVector<Sked> m_skeds;
    QDateTime m_nowUtc;
    SkedTimeline* m_timeline;
    QTableWidget* m_table;
    QLineEdit* m_callEdit;
    QLineEdit* m_gridEdit;
    QLineEdit* m_qrgEdit;
    QLineEdit* m_timeEdit;
    QPushButton* m_addButton;
    QLabel* m_hintLabel;
};

} // namespace Contestprogramm
