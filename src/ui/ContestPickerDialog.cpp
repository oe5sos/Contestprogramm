#include "ui/ContestPickerDialog.h"

#include "core/Maidenhead.h"
#include "data/ContestSchedule.h"
#include "ui/PanelHeaderBar.h"
#include "ui/StyleKit.h"

#include <QAbstractItemView>
#include <QDateTime>
#include <QBrush>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace Contestprogramm {

namespace {
// ── „Nächster Termin" ist jetzt die ERSTE Spalte ─────────────────────
//
// Betreiber am 2026-10-23: „toll wäre es, immer die conteste der
// nächsten 4 wochen zu sehen, sodass man nicht lange suchen muss."
//
// Also nicht nur eine Spalte dazu, sondern die Reihenfolge danach:
// was bald läuft, steht oben. Die Liste ist damit eine Antwort auf
// „was ist als nächstes", nicht mehr eine alphabetische Ablage.
constexpr int kColWann = 0;
constexpr int kColName = 1;
constexpr int kColId = 2;
constexpr int kColBands = 3;
constexpr int kColExchange = 4;

/// Was „die nächsten vier Wochen" heißt.
constexpr qint64 kVorschauSekunden = 28LL * 24 * 3600;

// "Serial, Grid" style summary -- enough to tell two contests with
// similar names apart at a glance, same purpose ContestRulesEditor's own
// field table serves in more detail.
QString exchangeFieldSummary(const ContestDefinition& def)
{
    QStringList labels;
    for (const ContestDefinition::ExchangeField& field : def.exchangeFields()) {
        labels << field.label;
    }
    return labels.join(QStringLiteral(", "));
}
// ── „in 2 Tagen", „läuft gerade", „Sa 24.10. 00:00" ──────────────────
//
// Zwei Angaben in einer Zelle: WANN er anfängt und WIE LANG es noch
// hin ist. Das Datum allein zwingt zum Kopfrechnen, die Frist allein
// sagt nicht, ob es ein Samstag ist.
QString terminText(const ContestWindow& fenster, const QDateTime& jetzt)
{
    if (!fenster.isValid()) {
        return QStringLiteral("—");
    }
    if (fenster.contains(jetzt)) {
        return QStringLiteral("läuft gerade");
    }
    const qint64 sek = jetzt.secsTo(fenster.startUtc);
    QString frist;
    if (sek < 3600) {
        frist = QStringLiteral("in %1 min").arg(sek / 60);
    } else if (sek < 48 * 3600) {
        frist = QStringLiteral("in %1 h").arg(sek / 3600);
    } else if (sek < 28LL * 24 * 3600) {
        frist = QStringLiteral("in %1 Tagen").arg(sek / (24 * 3600));
    } else {
        // Ab einem Monat ist die Zahl der Tage keine Hilfe mehr.
        frist = QStringLiteral("in %1 Mon.").arg(sek / (30LL * 24 * 3600));
    }
    return QStringLiteral("%1  (%2)")
        .arg(fenster.startUtc.toString(QStringLiteral("dd.MM. HH:mm")), frist);
}

} // namespace

ContestPickerDialog::ContestPickerDialog(const QVector<ContestDefinition>& availableContests,
                                          const QString& initialContestId,
                                          QWidget* parent)
    : QDialog(parent)
    , m_availableContests(availableContests)
    , m_table(new QTableWidget(this))
{
    setWindowTitle(QStringLiteral("Contestprogramm - Contest wählen"));
    setModal(true);

    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("Nächster Termin"), QStringLiteral("Name"),
         QStringLiteral("ID"), QStringLiteral("Bänder"), QStringLiteral("Exchange")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setFont(Style::capsFont(m_table->horizontalHeader()->font()));
    m_table->setFont(Style::monoFont(m_table->font(), Style::kFontSmall));
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    // The name is what one reads; the id and the bands are short.
    m_table->setColumnWidth(kColWann, 210);
    m_table->setColumnWidth(kColName, 250);
    m_table->setColumnWidth(kColId, 150);
    m_table->setColumnWidth(kColBands, 90);

    // ── Nach dem nächsten Termin sortieren ──────────────────────────
    //
    // Die Liste kam bisher in der Reihenfolge der Regeldateien. Mit
    // den Kurzwellencontesten sind das dreizehn Zeilen, und dann ist
    // „welcher ist am Wochenende" nicht mehr abzulesen.
    //
    // m_availableContests bleibt unberührt -- selectedContest() liest
    // über diesen Index zurück, nicht über die Zeilennummer.
    const QDateTime jetzt = QDateTime::currentDateTimeUtc();
    struct Zeile {
        int index;
        ContestWindow fenster;
        qint64 bisDahin;   // Sekunden; max für „kein Termin bekannt"
    };
    QVector<Zeile> zeilen;
    zeilen.reserve(m_availableContests.size());
    for (int i = 0; i < m_availableContests.size(); ++i) {
        Zeile z{i, {}, std::numeric_limits<qint64>::max()};
        const ContestSchedule plan = m_availableContests.at(i).schedule();
        if (plan.isValid()) {
            z.fenster = plan.windowUpcoming(jetzt);
            if (z.fenster.isValid()) {
                // Läuft er gerade, ist er der dringendste von allen.
                z.bisDahin = z.fenster.contains(jetzt)
                                 ? -1 : jetzt.secsTo(z.fenster.startUtc);
            }
        }
        zeilen.append(z);
    }
    std::stable_sort(zeilen.begin(), zeilen.end(),
                     [](const Zeile& a, const Zeile& b) {
        return a.bisDahin < b.bisDahin;
    });

    int preselectRow = -1;
    int ersterBalder = -1;
    m_table->setRowCount(zeilen.size());
    m_zeileZuIndex.clear();
    for (int row = 0; row < zeilen.size(); ++row) {
        const Zeile& z = zeilen.at(row);
        const ContestDefinition& def = m_availableContests.at(z.index);
        m_zeileZuIndex.append(z.index);

        auto* wann = new QTableWidgetItem(terminText(z.fenster, jetzt));
        const bool bald = z.bisDahin <= kVorschauSekunden;
        if (bald) {
            // Bernstein für „das steht an" -- dieselbe Rolle wie überall
            // sonst im Haus; Rot bleibt der Warnung.
            wann->setForeground(QBrush(QColor(Style::kAmberText())));
            QFont f = m_table->font();
            f.setBold(true);
            wann->setFont(f);
            if (ersterBalder < 0) { ersterBalder = row; }
        }
        if (z.fenster.isValid()) {
            wann->setToolTip(z.fenster.describe());
        }
        m_table->setItem(row, kColWann, wann);
        m_table->setItem(row, kColName, new QTableWidgetItem(def.name()));
        m_table->setItem(row, kColId, new QTableWidgetItem(def.id()));
        m_table->setItem(row, kColBands, new QTableWidgetItem(def.bands().join(QStringLiteral(" / "))));
        m_table->setItem(row, kColExchange, new QTableWidgetItem(exchangeFieldSummary(def)));
        if (def.id() == initialContestId) {
            preselectRow = row;
        }
    }
    // Ohne vorgegebenen Contest der nächste anstehende -- das ist das
    // „automatisch vorgeschlagen" aus dem Wunsch. Gibt es in den vier
    // Wochen keinen, bleibt es bei der ersten Zeile.
    if (preselectRow < 0) {
        preselectRow = ersterBalder >= 0 ? ersterBalder
                                         : (zeilen.isEmpty() ? -1 : 0);
    }
    if (preselectRow >= 0) {
        m_table->selectRow(preselectRow);
    }

    // Double-click a row commits it immediately, matching the OK
    // button -- a plain list-picker convention, not something ContestRulesEditor's
    // table (an editable grid, not a picker) needs.
    // -- unless the locator below is not valid yet; then it only selects.
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this] {
        if (m_okButton && m_okButton->isEnabled()) {
            accept();
        }
    });

    // The own locator, asked every time -- see the class comment.
    auto* gridCaption = new QLabel(QStringLiteral("Eigener Locator für diesen Contest:"), this);
    m_gridEdit = new QLineEdit(this);
    m_gridEdit->setObjectName(QStringLiteral("contestPickerGridEdit"));
    m_gridEdit->setPlaceholderText(QStringLiteral("z.B. JN67UT"));
    m_gridEdit->setMaxLength(8);
    m_gridEdit->setFont(Style::monoFont(m_gridEdit->font(), Style::kFontBody, QFont::DemiBold));
    m_gridEdit->setFixedWidth(120);
    m_exactGridButton = new QPushButton(this);
    m_exactGridButton->setObjectName(QStringLiteral("contestPickerExactGridButton"));
    m_exactGridButton->hide();
    connect(m_exactGridButton, &QPushButton::clicked, this, [this] { m_gridEdit->setText(m_exactLocationGrid); });
    auto* gridHint = new QLabel(
        QStringLiteral("Der Locator des Standorts, von dem aus gefunkt wird — er geht mit jedem Exchange raus."), this);
    gridHint->setWordWrap(true);
    gridHint->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextSecondary()));
    auto* gridRow = new QHBoxLayout();
    gridRow->addWidget(gridCaption);
    gridRow->addWidget(m_gridEdit);
    gridRow->addWidget(m_exactGridButton);
    gridRow->addStretch();
    connect(m_gridEdit, &QLineEdit::textChanged, this, [this] { refreshGridState(); });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okButton = buttons->button(QDialogButtonBox::Ok);
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Abbrechen"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Real top-level (dialog) window chrome, same posture as
    // ContestRulesEditor/MultiplierWindow: no rounded-panel wrapper
    // around the whole dialog (a QSS border-radius on a top-level
    // widget's own rect leaves square corner gaps), only the header bar
    // gets house-style chrome.
    auto* header = new PanelHeaderBar(QStringLiteral("Contest wählen"), this);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(header);

    auto* bodyLayout = new QVBoxLayout();
    bodyLayout->setContentsMargins(10, 10, 10, 10);
    bodyLayout->addWidget(m_table, 1);
    bodyLayout->addLayout(gridRow);
    bodyLayout->addWidget(gridHint);
    bodyLayout->addWidget(buttons);
    layout->addLayout(bodyLayout, 1);

    resize(600, 420);
    refreshGridState();
}

void ContestPickerDialog::setOwnGrid(const QString& currentGrid, const QString& exactLocationGrid)
{
    m_exactLocationGrid = exactLocationGrid.trimmed().toUpper();
    m_gridEdit->setText(currentGrid.trimmed().toUpper());
    m_gridEdit->selectAll();
    m_gridEdit->setFocus();
    refreshGridState();
}

QString ContestPickerDialog::ownGrid() const
{
    return m_gridEdit->text().trimmed().toUpper();
}

void ContestPickerDialog::refreshGridState()
{
    const QString grid = ownGrid();
    const bool valid = isValidGridSquare(grid);
    if (m_okButton) {
        m_okButton->setEnabled(valid && !m_availableContests.isEmpty());
    }
    m_gridEdit->setStyleSheet(valid ? QString() : QStringLiteral("QLineEdit { border: 1px solid %1; }").arg(Style::kAmberWarn()));
    // The settings' exact position in another square than the one
    // typed: offer it -- the usual mistake is the home locator left in
    // place at a portable site.
    const bool offerExact = !m_exactLocationGrid.isEmpty() && isValidGridSquare(m_exactLocationGrid)
        && m_exactLocationGrid.left(6) != grid.left(6);
    m_exactGridButton->setVisible(offerExact);
    m_exactGridButton->setText(QStringLiteral("Exakter Standort: %1 übernehmen").arg(m_exactLocationGrid));
}

QString ContestPickerDialog::selectedContestId() const
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_zeileZuIndex.size()) {
        return QString();
    }
    const int index = m_zeileZuIndex.at(row);
    if (index < 0 || index >= m_availableContests.size()) {
        return QString();
    }
    return m_availableContests.at(index).id();
}

} // namespace Contestprogramm
