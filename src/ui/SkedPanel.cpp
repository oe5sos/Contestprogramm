#include "ui/SkedPanel.h"

#include "core/Maidenhead.h"
#include "ui/StyleKit.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <cmath>

namespace Contestprogramm {

namespace {

enum Column { ColTime = 0, ColCall, ColGrid, ColQrg, ColBearing, ColKm, ColStatus, ColCount };

constexpr int kWindowMinutes = 60;

QString qrgText(qint64 hz)
{
    if (hz <= 0) {
        return Style::unknownDash();
    }
    return QStringLiteral("%1.%2").arg(hz / 1000000).arg((hz / 1000) % 1000, 3, 10, QLatin1Char('0'));
}

} // namespace

// ── SkedTimeline ───────────────────────────────────────────────────

SkedTimeline::SkedTimeline(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(46);
}

void SkedTimeline::setSkeds(const QVector<Sked>& skeds, const QDateTime& nowUtc, int nextId)
{
    m_skeds = skeds;
    m_nowUtc = nowUtc;
    m_nextId = nextId;
    update();
}

void SkedTimeline::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
    if (!m_nowUtc.isValid()) {
        return;
    }
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing, true);
    const int x0 = 8;
    const int x1 = width() - 8;
    const int axisY = 30;
    // Window: from the current ten-minute mark, one hour ahead.
    const QDateTime start = m_nowUtc.addSecs(-(m_nowUtc.time().minute() % 10) * 60 - m_nowUtc.time().second());
    const auto xFor = [&](const QDateTime& t) {
        const double minutes = start.secsTo(t) / 60.0;
        return x0 + int(std::lround((x1 - x0) * minutes / kWindowMinutes));
    };

    g.setPen(QPen(QColor(Style::kBorder()), 1));
    g.drawLine(x0, axisY, x1, axisY);
    g.setFont(Style::monoFont(font(), Style::kFontCaption));
    const QFontMetrics fm(g.font());
    for (int m = 0; m <= kWindowMinutes; m += 10) {
        const QDateTime t = start.addSecs(m * 60);
        const int x = xFor(t);
        g.setPen(QColor(Style::kBorder()));
        g.drawLine(x, axisY - 3, x, axisY + 3);
        const QString label = t.time().toString(QStringLiteral("HH:mm"));
        int tx = x - fm.horizontalAdvance(label) / 2;
        tx = std::max(x0 - 4, std::min(tx, x1 + 4 - fm.horizontalAdvance(label)));
        g.setPen(QColor(Style::kTextScale()));
        g.drawText(tx, axisY + 14, label);
    }
    // Now
    g.setPen(QPen(QColor(Style::kTextSecondary()), 1, Qt::DashLine));
    const int xn = xFor(m_nowUtc);
    g.drawLine(xn, 4, xn, axisY + 4);

    // Chips for open skeds inside the window
    const QFont chipFont = Style::monoFont(font(), Style::kFontSmall);
    const QFontMetrics cfm(chipFont);
    for (const Sked& sked : m_skeds) {
        if (sked.state != Sked::State::Open || !sked.timeUtc.isValid()) {
            continue;
        }
        const double minutes = start.secsTo(sked.timeUtc) / 60.0;
        if (minutes < 0 || minutes > kWindowMinutes) {
            continue;
        }
        const int x = xFor(sked.timeUtc);
        const int w = cfm.horizontalAdvance(sked.callsign) + 10;
        QRect r(x - w / 2, 4, w, 18);
        r.moveLeft(std::max(x0, std::min(r.left(), x1 - w)));
        const bool next = sked.id == m_nextId;
        g.setPen(QPen(QColor(next ? Style::kAmberBorder() : Style::kBorder()), 1));
        g.setBrush(QColor(next ? Style::kAmberBg() : Style::kPanelBg()));
        g.drawRoundedRect(r, 4, 4);
        g.setFont(chipFont);
        g.setPen(QColor(next ? Style::kAmberText() : Style::kTextPrimary()));
        g.drawText(r, Qt::AlignCenter, sked.callsign);
        g.setPen(QPen(QColor(Style::kBorder()), 1));
        g.drawLine(x, r.bottom() + 1, x, axisY - 3);
    }
}

// ── SkedPanel ──────────────────────────────────────────────────────

SkedPanel::SkedPanel(QWidget* parent)
    : QWidget(parent)
    , m_timeline(new SkedTimeline(this))
    , m_table(new QTableWidget(this))
    , m_callEdit(new QLineEdit(this))
    , m_gridEdit(new QLineEdit(this))
    , m_qrgEdit(new QLineEdit(this))
    , m_timeEdit(new QLineEdit(this))
    , m_addButton(new QPushButton(QStringLiteral("+ Sked"), this))
    , m_hintLabel(new QLabel(this))
{
    Style::applyPanelFrameStyle(this);

    m_table->setColumnCount(ColCount);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Zeit"), QStringLiteral("Call"), QStringLiteral("Loc"),
                                        QStringLiteral("QRG"), QStringLiteral("Richt"), QStringLiteral("km"),
                                        QStringLiteral("Status")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setFont(Style::capsFont(m_table->horizontalHeader()->font()));
    m_table->verticalHeader()->setVisible(false);
    // Tight rows: five skeds and the timeline fit the panel's 262 px.
    m_table->verticalHeader()->setDefaultSectionSize(20);
    m_table->setFont(Style::monoFont(m_table->font(), Style::kFontSmall));
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setShowGrid(false);
    m_table->setFocusPolicy(Qt::ClickFocus);
    connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int) { handleRowClicked(row); });

    m_callEdit->setPlaceholderText(QStringLiteral("Call"));
    m_gridEdit->setPlaceholderText(QStringLiteral("Locator"));
    m_qrgEdit->setPlaceholderText(QStringLiteral("QRG"));
    m_timeEdit->setPlaceholderText(QStringLiteral("Zeit"));
    m_qrgEdit->setToolTip(QStringLiteral("MHz, z.B. 144.317 -- leer: Frequenz des Funkgeräts"));
    m_timeEdit->setToolTip(QStringLiteral("UTC \"14:35\" oder \"+5\" (Minuten) -- leer: jetzt"));
    for (auto [edit, w] : {std::pair{m_callEdit, 76}, std::pair{m_gridEdit, 70}, std::pair{m_qrgEdit, 70}, std::pair{m_timeEdit, 52}}) {
        edit->setFixedWidth(w);
        edit->setFont(Style::monoFont(edit->font(), Style::kFontSmall));
        connect(edit, &QLineEdit::returnPressed, this, &SkedPanel::handleAddClicked);
    }
    connect(m_addButton, &QPushButton::clicked, this, &SkedPanel::handleAddClicked);
    m_hintLabel->setFont(Style::capsFont(m_hintLabel->font()));
    m_hintLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextScale()));

    auto* entryRow = new QHBoxLayout();
    entryRow->setContentsMargins(0, 0, 0, 0);
    entryRow->setSpacing(6);
    entryRow->addWidget(m_callEdit);
    entryRow->addWidget(m_gridEdit);
    entryRow->addWidget(m_qrgEdit);
    entryRow->addWidget(m_timeEdit);
    entryRow->addWidget(m_addButton);
    entryRow->addWidget(m_hintLabel, 1);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 6, 12, 8);
    layout->setSpacing(4);
    layout->addWidget(m_timeline);
    layout->addWidget(m_table, 1);
    layout->addLayout(entryRow);

    setSkeds({}, QDateTime::currentDateTimeUtc());
}

void SkedPanel::setOwnGrid(const QString& grid)
{
    m_ownGrid = grid.trimmed().toUpper();
    setSkeds(m_skeds, m_nowUtc.isValid() ? m_nowUtc : QDateTime::currentDateTimeUtc());
}

void SkedPanel::setSkeds(const QVector<Sked>& skeds, const QDateTime& nowUtc)
{
    m_skeds = skeds;
    m_nowUtc = nowUtc;
    const Sked* next = SkedRules::nextOpen(m_skeds, nowUtc);
    const int nextId = next ? next->id : -1;
    m_timeline->setSkeds(m_skeds, nowUtc, nextId);

    m_table->setRowCount(m_skeds.size());
    for (int row = 0; row < m_skeds.size(); ++row) {
        const Sked& sked = m_skeds.at(row);
        QString bearing = Style::unknownDash();
        QString km = Style::unknownDash();
        if (isValidGridSquare(m_ownGrid) && isValidGridSquare(sked.grid)) {
            bearing = QStringLiteral("%1°").arg(int(std::lround(calculateBearingInDegrees(m_ownGrid, sked.grid))));
            km = QString::number(int(std::lround(calculateDistanceKm(m_ownGrid, sked.grid))));
        }
        const QStringList cells = {
            sked.timeUtc.isValid() ? sked.timeUtc.toUTC().time().toString(QStringLiteral("HH:mm")) : Style::unknownDash(),
            sked.callsign,
            sked.grid.isEmpty() ? Style::unknownDash() : sked.grid,
            qrgText(sked.freqHz),
            bearing,
            km,
            SkedRules::statusText(sked, nowUtc),
        };
        QColor color(Style::kTextPrimary());
        bool strike = false;
        switch (sked.state) {
        case Sked::State::Suggested: color = QColor(Style::kTextSecondary()); break;
        case Sked::State::Done: color = QColor(Style::kTextInactive()); strike = true; break;
        case Sked::State::Missed: color = QColor(Style::kTextInactive()); break;
        case Sked::State::Open: if (sked.id == nextId) { color = QColor(Style::kAmberText()); } break;
        }
        for (int col = 0; col < ColCount; ++col) {
            auto* item = new QTableWidgetItem(cells.at(col));
            item->setForeground(color);
            if (strike) {
                QFont f = item->font();
                f.setStrikeOut(true);
                item->setFont(f);
            }
            if (col == ColBearing || col == ColKm) {
                item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            }
            item->setToolTip(sked.note);
            m_table->setItem(row, col, item);
        }
    }
    m_table->resizeColumnsToContents();

    if (next) {
        const qint64 secs = nowUtc.secsTo(next->timeUtc);
        const QString when = secs > SkedRules::kDueBeforeSecs ? QStringLiteral("IN %1 MIN").arg((secs + 30) / 60)
                                                              : QStringLiteral("JETZT");
        m_hintLabel->setText(QStringLiteral("NÄCHSTER SKED %1 %2 · KLICK = QSY + ROTOR").arg(next->callsign, when));
    } else if (m_skeds.isEmpty()) {
        m_hintLabel->setText(QStringLiteral("KEINE SKEDS · EINTRAGEN ODER AUS KST ÜBERNEHMEN"));
    } else {
        m_hintLabel->setText(QStringLiteral("KEIN OFFENER SKED"));
    }
}

QString SkedPanel::entryCallsign() const
{
    return m_callEdit->text().trimmed().toUpper();
}

void SkedPanel::clearEntry()
{
    m_callEdit->clear();
    m_gridEdit->clear();
    m_qrgEdit->clear();
    m_timeEdit->clear();
    m_callEdit->setFocus();
}

void SkedPanel::handleRowClicked(int row)
{
    if (row < 0 || row >= m_skeds.size()) {
        return;
    }
    const Sked& sked = m_skeds.at(row);
    if (sked.state == Sked::State::Suggested) {
        emit suggestionAccepted(sked.id);
    } else if (sked.state == Sked::State::Open) {
        emit skedActivated(sked.id);
    }
}

void SkedPanel::handleAddClicked()
{
    if (entryCallsign().isEmpty()) {
        m_callEdit->setFocus();
        return;
    }
    emit addRequested(entryCallsign(), m_gridEdit->text().trimmed().toUpper(), m_qrgEdit->text().trimmed(),
                      m_timeEdit->text().trimmed());
}

void SkedPanel::keyPressEvent(QKeyEvent* event)
{
    if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) && m_table->hasFocus()) {
        const int row = m_table->currentRow();
        if (row >= 0 && row < m_skeds.size()) {
            emit deleteRequested(m_skeds.at(row).id);
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

} // namespace Contestprogramm
