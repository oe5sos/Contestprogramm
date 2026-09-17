#include "ui/StatisticsWindow.h"

#include "data/ContestDatabase.h"
#include "data/ContestStatistics.h"
#include "data/QsoRecord.h"
#include "ui/PanelHeaderBar.h"
#include "ui/StyleKit.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

namespace Contestprogramm {

namespace {

constexpr int kRefreshMs = 30000;

QString grouped(qint64 value)
{
    return QLocale(QLocale::German, QLocale::Austria).toString(value);
}

QTableWidget* makeTable(QWidget* parent, const QStringList& headers)
{
    auto* table = new QTableWidget(parent);
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setFont(Style::capsFont(table->horizontalHeader()->font()));
    table->verticalHeader()->setVisible(false);
    table->setFont(Style::monoFont(table->font(), Style::kFontSmall));
    table->setAlternatingRowColors(true);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setFocusPolicy(Qt::NoFocus);
    return table;
}

QTableWidgetItem* numberItem(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return item;
}

QLabel* captionLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text.toUpper(), parent);
    label->setFont(Style::capsFont(label->font()));
    label->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextScale()));
    return label;
}

} // namespace

StatisticsWindow::StatisticsWindow(ContestDatabase& database, QWidget* parent)
    : QWidget(parent)
    , m_database(database)
    , m_timer(new QTimer(this))
{
    setWindowTitle(QStringLiteral("Contestprogramm - Statistik"));
    m_header = new PanelHeaderBar(QStringLiteral("Statistik"), this);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setTextFormat(Qt::RichText);
    m_summaryLabel->setWordWrap(true);

    m_bandTable = makeTable(this, {QStringLiteral("Band"), QStringLiteral("QSOs"), QStringLiteral("Dupes"),
                                   QStringLiteral("km"), QStringLiteral("Felder"), QStringLiteral("ODX")});
    m_hourTable = makeTable(this, {QStringLiteral("Stunde UTC"), QStringLiteral("QSOs"), QStringLiteral("km"),
                                   QStringLiteral("")});
    m_odxTable = makeTable(this, {QStringLiteral("km"), QStringLiteral("Call"), QStringLiteral("Locator"),
                                  QStringLiteral("Band"), QStringLiteral("Zeit UTC")});

    auto* refreshButton = new QPushButton(QStringLiteral("Aktualisieren"), this);
    connect(refreshButton, &QPushButton::clicked, this, &StatisticsWindow::refresh);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_header);
    auto* body = new QVBoxLayout();
    body->setContentsMargins(10, 10, 10, 10);
    body->setSpacing(6);
    body->addWidget(m_summaryLabel);
    body->addWidget(captionLabel(QStringLiteral("Je Band"), this));
    body->addWidget(m_bandTable);
    auto* lower = new QHBoxLayout();
    auto* hoursColumn = new QVBoxLayout();
    hoursColumn->addWidget(captionLabel(QStringLiteral("Je Stunde"), this));
    hoursColumn->addWidget(m_hourTable, 1);
    auto* odxColumn = new QVBoxLayout();
    odxColumn->addWidget(captionLabel(QStringLiteral("Längste QSOs"), this));
    odxColumn->addWidget(m_odxTable, 1);
    lower->addLayout(hoursColumn, 1);
    lower->addLayout(odxColumn, 1);
    body->addLayout(lower, 1);
    body->addWidget(refreshButton);
    layout->addLayout(body, 1);

    m_timer->setInterval(kRefreshMs);
    connect(m_timer, &QTimer::timeout, this, &StatisticsWindow::refresh);

    resize(760, 620);
}

void StatisticsWindow::setContest(const QString& contestId, const QString& ownGrid, const QStringList& bandOrder,
                                  const QString& scoring)
{
    m_contestId = contestId;
    m_ownGrid = ownGrid;
    m_bandOrder = bandOrder;
    m_scoring = scoring;
    refresh();
}

void StatisticsWindow::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    refresh();
    m_timer->start();
}

void StatisticsWindow::hideEvent(QHideEvent* event)
{
    m_timer->stop();
    QWidget::hideEvent(event);
}

void StatisticsWindow::refresh()
{
    const QVector<QsoRecord> records = m_database.qsosForContest(m_contestId);
    const ContestStatistics stats = computeContestStatistics(records, m_ownGrid, m_bandOrder, m_scoring);

    const QString dash = Style::unknownDash();
    QString best = dash;
    if (stats.bestHourQsos > 0) {
        best = QStringLiteral("%1 (%2 UTC)")
                   .arg(stats.bestHourQsos)
                   .arg(stats.bestHourStartUtc.toString(QStringLiteral("HH:mm")));
    }
    m_summaryLabel->setText(
        QStringLiteral("<span style='color:%1;'>QSOs</span> <b>%2</b> &nbsp;&middot;&nbsp; "
                       "<span style='color:%1;'>Punkte</span> <b>%3</b> &nbsp;&middot;&nbsp; "
                       "<span style='color:%1;'>Ø km</span> <b>%4</b> &nbsp;&middot;&nbsp; "
                       "<span style='color:%1;'>Beste Stunde</span> <b>%5</b>")
            .arg(Style::kTextScale())
            .arg(stats.score.validQsos)
            .arg(grouped(stats.score.points))
            .arg(stats.averageKm > 0.0 ? QString::number(stats.averageKm, 'f', 0) : dash)
            .arg(best));

    m_bandTable->setRowCount(stats.score.bands.size());
    for (int row = 0; row < stats.score.bands.size(); ++row) {
        const BandScore& band = stats.score.bands.at(row);
        m_bandTable->setItem(row, 0, new QTableWidgetItem(band.band));
        m_bandTable->setItem(row, 1, numberItem(QString::number(band.validQsos)));
        m_bandTable->setItem(row, 2, numberItem(QString::number(band.dupes)));
        m_bandTable->setItem(row, 3, numberItem(grouped(band.points)));
        m_bandTable->setItem(row, 4, numberItem(QString::number(band.largeSquares)));
        m_bandTable->setItem(row, 5, new QTableWidgetItem(
                                         band.odxKm > 0 ? QStringLiteral("%1 %2 %3 km").arg(band.odxCall, band.odxGrid).arg(grouped(band.odxKm))
                                                        : dash));
    }
    m_bandTable->resizeColumnsToContents();
    // Two or three bands: the table sizes to its rows, the hour/ODX
    // tables below get the room.
    int bandTableHeight = m_bandTable->horizontalHeader()->height() + 4;
    for (int row = 0; row < m_bandTable->rowCount(); ++row) {
        bandTableHeight += m_bandTable->rowHeight(row);
    }
    m_bandTable->setFixedHeight(bandTableHeight);

    m_hourTable->setRowCount(stats.hours.size());
    for (int row = 0; row < stats.hours.size(); ++row) {
        const HourStats& hour = stats.hours.at(row);
        m_hourTable->setItem(row, 0, new QTableWidgetItem(hour.hourStartUtc.toString(QStringLiteral("dd.MM. HH:00"))));
        m_hourTable->setItem(row, 1, numberItem(QString::number(hour.qsos)));
        m_hourTable->setItem(row, 2, numberItem(grouped(hour.points)));
        // A bar of blocks, one per QSO, so the shape of the night reads
        // without a chart -- the best hour in amber.
        auto* bar = new QTableWidgetItem(QString(hour.qsos, QChar(0x2588)));
        if (hour.qsos == stats.bestHourQsos && hour.qsos > 0) {
            bar->setForeground(QColor(Style::kAmberText()));
        } else {
            bar->setForeground(QColor(Style::kTextSecondary()));
        }
        m_hourTable->setItem(row, 3, bar);
    }
    m_hourTable->resizeColumnsToContents();

    m_odxTable->setRowCount(stats.longest.size());
    for (int row = 0; row < stats.longest.size(); ++row) {
        const OdxEntry& entry = stats.longest.at(row);
        m_odxTable->setItem(row, 0, numberItem(grouped(entry.km)));
        m_odxTable->setItem(row, 1, new QTableWidgetItem(entry.callsign));
        m_odxTable->setItem(row, 2, new QTableWidgetItem(entry.grid));
        m_odxTable->setItem(row, 3, new QTableWidgetItem(entry.band));
        const QDateTime ts = QDateTime::fromString(entry.timestampUtc, Qt::ISODate);
        m_odxTable->setItem(row, 4, new QTableWidgetItem(ts.isValid() ? ts.toUTC().toString(QStringLiteral("dd.MM. HH:mm")) : entry.timestampUtc));
    }
    m_odxTable->resizeColumnsToContents();
}

} // namespace Contestprogramm
