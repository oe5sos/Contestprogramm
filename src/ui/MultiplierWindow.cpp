#include "ui/MultiplierWindow.h"

#include "data/ContestDefinition.h"
#include "data/MultiplierTracker.h"
#include "ui/PanelHeaderBar.h"
#include "ui/StyleKit.h"

#include <QColor>
#include <QHeaderView>
#include <QPushButton>
#include <QSet>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace Contestprogramm {

MultiplierWindow::MultiplierWindow(MultiplierTracker& tracker, QWidget* parent)
    : QWidget(parent)
    , m_tracker(tracker)
{
    setWindowTitle(QStringLiteral("Contestprogramm - Multiplikatoren"));
    // No rounded-panel wrapper here: this is a real top-level window
    // (see the class comment), and a QSS border-radius on a top-level
    // widget's own rect leaves square corner gaps showing whatever the
    // OS paints behind it. The app-wide stylesheet already gives every
    // QWidget the correct dark background/text colour (see
    // Style::appStyleSheet()'s base "QWidget {...}" rule); only the
    // header bar below needs its own chrome.
    m_header = new PanelHeaderBar(QStringLiteral("Multiplikatoren"), this);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Band"), QStringLiteral("Multiplikator"), QStringLiteral("gearbeitet")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setFont(Style::capsFont(m_table->horizontalHeader()->font()));
    m_table->setFont(Style::monoFont(m_table->font(), Style::kFontSmall));
    m_table->setAlternatingRowColors(true);
    m_table->setSortingEnabled(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto* refreshButton = new QPushButton(QStringLiteral("Aktualisieren"), this);
    connect(refreshButton, &QPushButton::clicked, this, &MultiplierWindow::refresh);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_header);
    auto* bodyLayout = new QVBoxLayout();
    bodyLayout->setContentsMargins(10, 10, 10, 10);
    bodyLayout->addWidget(m_table, 1);
    bodyLayout->addWidget(refreshButton);
    layout->addLayout(bodyLayout, 1);

    resize(420, 480);
}

void MultiplierWindow::setContest(const QString& contestId, const ContestDefinition* definition)
{
    m_contestId = contestId;
    m_definition = definition;
    refresh();
}

void MultiplierWindow::refresh()
{
    m_table->setSortingEnabled(false);
    m_table->setRowCount(0);

    if (!m_definition || m_contestId.isEmpty()) {
        m_table->setSortingEnabled(true);
        return;
    }

    m_tracker.recompute(m_contestId, *m_definition);

    const QStringList bands = m_tracker.bands();

    // Union of every band's worked keys: a multiplier worked on 144 but
    // not (yet) on 432 gets a real "nein" row for 432, rather than only
    // ever listing multipliers that already have at least one "ja".
    QSet<QString> allKeys;
    for (const QString& band : bands) {
        allKeys.unite(m_tracker.workedMultipliers(band));
    }
    QStringList sortedKeys(allKeys.begin(), allKeys.end());
    sortedKeys.sort();

    int row = 0;
    for (const QString& band : bands) {
        const QSet<QString> workedOnBand = m_tracker.workedMultipliers(band);
        for (const QString& key : sortedKeys) {
            m_table->insertRow(row);
            m_table->setItem(row, 0, new QTableWidgetItem(band));
            m_table->setItem(row, 1, new QTableWidgetItem(key));
            const bool worked = workedOnBand.contains(key);
            auto* workedItem = new QTableWidgetItem(worked ? QStringLiteral("ja") : QStringLiteral("nein"));
            // Green = confirmed/worked, per HAUSSTIL's colour-meaning
            // table -- this column is literally that state, not a
            // decorative choice.
            workedItem->setForeground(worked ? QColor(Style::kGreenText())
                                              : QColor(Style::kTextInactive()));
            m_table->setItem(row, 2, workedItem);
            ++row;
        }
    }

    m_table->setSortingEnabled(true);
}

} // namespace Contestprogramm
