#include "ui/ChatPanelWidget.h"

#include "core/SpotCandidate.h"
#include "models/ChatFeedModel.h"
#include "ui/StyleKit.h"

#include <QAbstractTableModel>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

namespace Contestprogramm {

namespace {

constexpr int kColumnWidths[ChatPanelWidget::ColumnCount] = {56, 44, 92, 74, 420};

} // namespace

// Beide Quellen in einer Liste, nach Zeit geordnet, älteste oben --
// dieselbe Leserichtung wie im Log und wie in jedem Chatfenster.
//
// Kein QSortFilterProxyModel und kein QConcatenateTablesProxyModel: die
// beiden ChatFeedModels bauen ihre sichtbaren Zeilen bei jedem
// Neubewerten komplett neu auf (rebuildVisibleRows), und ein Proxy
// darüber müsste jedes Mal mitsortieren. Eine eigene, flache Liste aus
// Zeigern auf (Modell, Zeile) ist hier weniger Mechanik und leichter zu
// lesen.
class MergedChatModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit MergedChatModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    void setFeeds(ChatFeedModel* onKst, ChatFeedModel* cluster)
    {
        for (ChatFeedModel* alt : {m_onKst, m_cluster}) {
            if (alt) {
                disconnect(alt, nullptr, this, nullptr);
            }
        }
        m_onKst = onKst;
        m_cluster = cluster;
        for (ChatFeedModel* neu : {m_onKst, m_cluster}) {
            if (!neu) {
                continue;
            }
            connect(neu, &QAbstractItemModel::modelReset, this, &MergedChatModel::rebuild);
            connect(neu, &QAbstractItemModel::rowsInserted, this, &MergedChatModel::rebuild);
            connect(neu, &QAbstractItemModel::dataChanged, this, &MergedChatModel::rebuild);
        }
        rebuild();
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : m_rows.size();
    }
    int columnCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : ChatPanelWidget::ColumnCount;
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
            return QVariant();
        }
        const Zeile& zeile = m_rows.at(index.row());

        if (role == Qt::DisplayRole) {
            switch (index.column()) {
            case ChatPanelWidget::ColumnTime:
                return zeile.linie.candidate.timestampUtc.toString(QStringLiteral("HH:mm"));
            case ChatPanelWidget::ColumnSource:
                return zeile.ausKst ? QStringLiteral("KST") : QStringLiteral("CLU");
            case ChatPanelWidget::ColumnCall:
                return zeile.linie.candidate.callsign;
            case ChatPanelWidget::ColumnDistance:
                // Nur wenn in der Zeile ein Locator stand -- sonst ein
                // Strich, keine erfundene Zahl (HAUSSTIL-Regel 7).
                if (!zeile.linie.distanceKnown) {
                    return Style::unknownDash();
                }
                return QStringLiteral("%1 · %2")
                    .arg(QString::number(qRound(zeile.linie.distanceKm)),
                          QString::number(qRound(zeile.linie.bearingDeg)));
            case ChatPanelWidget::ColumnText:
                return zeile.linie.candidate.rawLine;
            default:
                return QVariant();
            }
        }
        if (role == Qt::ForegroundRole) {
            // Schon gearbeitet: gedämpft. Das Auge soll an den offenen
            // Stationen hängenbleiben.
            if (zeile.linie.worked) {
                return QVariant::fromValue(QColor(Style::kTextInactive()));
            }
            if (index.column() == ChatPanelWidget::ColumnCall) {
                return QVariant::fromValue(QColor(Style::kTextPrimary()));
            }
            return QVariant::fromValue(QColor(Style::kTextSecondary()));
        }
        return QVariant();
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
            return QAbstractTableModel::headerData(section, orientation, role);
        }
        switch (section) {
        case ChatPanelWidget::ColumnTime: return QStringLiteral("Zeit");
        case ChatPanelWidget::ColumnSource: return QStringLiteral("Quelle");
        case ChatPanelWidget::ColumnCall: return QStringLiteral("Call");
        case ChatPanelWidget::ColumnDistance: return QStringLiteral("km · °");
        case ChatPanelWidget::ColumnText: return QStringLiteral("Text");
        default: return QVariant();
        }
    }

    // Rufzeichen, Locator und Frequenz einer Zeile -- für den
    // Doppelklick.
    bool spotForRow(int row, QString* callsign, QString* grid, qint64* freqHz) const
    {
        if (row < 0 || row >= m_rows.size()) {
            return false;
        }
        const SpotCandidate& kandidat = m_rows.at(row).linie.candidate;
        if (callsign) {
            *callsign = kandidat.callsign;
        }
        if (grid) {
            *grid = kandidat.grid;
        }
        if (freqHz) {
            *freqHz = kandidat.freqHz;
        }
        return !kandidat.callsign.isEmpty();
    }

public slots:
    void rebuild()
    {
        beginResetModel();
        m_rows.clear();
        const auto sammle = [this](ChatFeedModel* modell, bool ausKst) {
            if (!modell) {
                return;
            }
            // lineCount()/lineAt(), NICHT rowCount(): der Chat zeigt
            // alles, was hereinkam. rowCount() gäbe nur die Zeilen, die
            // der Filter für die Vorschläge durchlässt -- siehe
            // ChatFeedModel::lineAt()'s eigenen Kommentar.
            for (int i = 0; i < modell->lineCount(); ++i) {
                Zeile zeile;
                zeile.ausKst = ausKst;
                zeile.linie = modell->lineAt(i);
                m_rows.append(zeile);
            }
        };
        sammle(m_onKst, true);
        sammle(m_cluster, false);
        std::stable_sort(m_rows.begin(), m_rows.end(), [](const Zeile& a, const Zeile& b) {
            return a.linie.candidate.timestampUtc < b.linie.candidate.timestampUtc;
        });
        endResetModel();
    }

private:
    struct Zeile {
        bool ausKst = true;
        ChatFeedModel::FeedLine linie;
    };

    ChatFeedModel* m_onKst = nullptr;
    ChatFeedModel* m_cluster = nullptr;
    QVector<Zeile> m_rows;
};

ChatPanelWidget::ChatPanelWidget(QWidget* parent)
    : QWidget(parent)
    , m_model(new MergedChatModel(this))
    , m_table(new QTableView(this))
    , m_input(new QLineEdit(this))
    , m_sendButton(new QPushButton(QStringLiteral("Senden"), this))
    , m_status(new QLabel(QString(), this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_status->setObjectName(QLatin1String(kStatusObjectName));
    m_status->setFont(Style::capsFont(m_status->font()));
    m_status->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(Style::kTextScale()));
    layout->addWidget(m_status);

    m_table->setObjectName(QLatin1String(kTableObjectName));
    m_table->setModel(m_model);
    m_table->horizontalHeader()->hide();
    m_table->verticalHeader()->hide();
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setShowGrid(false);
    m_table->setFont(Style::monoFont(m_table->font(), Style::kFontBody));
    m_table->verticalHeader()->setDefaultSectionSize(QFontMetrics(m_table->font()).height() + 8);
    for (int spalte = 0; spalte < ColumnCount; ++spalte) {
        m_table->setColumnWidth(spalte, kColumnWidths[spalte]);
    }
    m_table->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(m_table, 1);

    // Neueste Zeile im Blick behalten -- ein Chat, der nicht mitläuft,
    // ist keiner.
    connect(m_model, &QAbstractItemModel::modelReset, this, [this]() { m_table->scrollToBottom(); });

    connect(m_table, &QTableView::doubleClicked, this, [this](const QModelIndex& index) {
        QString callsign;
        QString grid;
        qint64 freqHz = 0;
        if (m_model->spotForRow(index.row(), &callsign, &grid, &freqHz) && !callsign.isEmpty()) {
            emit candidateActivated(callsign, grid, freqHz);
        }
    });

    auto* eingabeZeile = new QHBoxLayout;
    eingabeZeile->setContentsMargins(0, 0, 0, 0);
    eingabeZeile->setSpacing(6);
    m_input->setObjectName(QLatin1String(kInputObjectName));
    m_input->setPlaceholderText(QStringLiteral("Nachricht an ON4KST ..."));
    eingabeZeile->addWidget(m_input, 1);
    m_sendButton->setObjectName(QLatin1String(kSendButtonObjectName));
    eingabeZeile->addWidget(m_sendButton);
    layout->addLayout(eingabeZeile);

    connect(m_input, &QLineEdit::returnPressed, this, &ChatPanelWidget::sendCurrentInput);
    connect(m_sendButton, &QPushButton::clicked, this, &ChatPanelWidget::sendCurrentInput);
}

void ChatPanelWidget::setFeedModels(ChatFeedModel* onKst, ChatFeedModel* cluster)
{
    m_model->setFeeds(onKst, cluster);
}

void ChatPanelWidget::setConnectionStatus(const QString& text)
{
    m_status->setText(text);
}

void ChatPanelWidget::sendCurrentInput()
{
    const QString text = m_input->text().trimmed();
    if (text.isEmpty()) {
        return;
    }
    emit messageSubmitted(text);
    m_input->clear();
}

} // namespace Contestprogramm

#include "ChatPanelWidget.moc"
