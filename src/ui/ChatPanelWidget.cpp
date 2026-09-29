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

    // Eine zusammengeführte Zeile: woher sie kam und was drinsteht.
    struct Zeile {
        bool ausKst = true;
        ChatFeedModel::FeedLine linie;
    };

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
                // Der lesbare Text, nicht die Protokollzeile -- siehe
                // SpotCandidate::message. Fällt auf rawLine zurück,
                // falls eine Quelle ihn (noch) nicht setzt.
                return zeile.linie.candidate.message.isEmpty() ? zeile.linie.candidate.rawLine
                                                                : zeile.linie.candidate.message;
            default:
                return QVariant();
            }
        }
        if (role == Qt::ForegroundRole) {
            // Was mich betrifft, zuerst -- auch wenn die Station schon
            // gearbeitet ist: wer mich anspricht, ist wichtiger als die
            // Frage, ob ich ihn schon im Log habe.
            if (betrifftMich(zeile)) {
                return QVariant::fromValue(QColor(Style::kMentionMagenta()));
            }
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
    // Ob ALLE Zeilen gezeigt werden oder nur die gefilterten.
    void setOwnCallsign(const QString& callsign)
    {
        const QString neu = callsign.trimmed().toUpper();
        if (m_ownCallsign == neu) {
            return;
        }
        m_ownCallsign = neu;
        if (!m_rows.isEmpty()) {
            emit dataChanged(index(0, 0), index(m_rows.size() - 1, ChatPanelWidget::ColumnCount - 1),
                             {Qt::ForegroundRole});
        }
    }

    QString ownCallsign() const { return m_ownCallsign; }

    // Betrifft mich diese Zeile? Wenn mein Rufzeichen darin steht --
    // als eigenes Wort, nicht als Teil eines anderen Rufzeichens:
    // "OE5SOS" darf nicht in "OE5SOSX" anschlagen. ON4KST schreibt die
    // Anrede mitten in den Text ("OE5SOS de DL1ABC ..."), deshalb wird
    // der ganze Text durchsucht und nicht nur ein Empfängerfeld.
    bool betrifftMich(const Zeile& zeile) const
    {
        if (m_ownCallsign.isEmpty()) {
            return false;
        }
        if (zeile.linie.candidate.callsign.trimmed().compare(m_ownCallsign, Qt::CaseInsensitive) == 0) {
            return true;
        }
        const QString text = zeile.linie.candidate.message.isEmpty() ? zeile.linie.candidate.rawLine
                                                                      : zeile.linie.candidate.message;
        int ab = 0;
        while (true) {
            const int pos = text.indexOf(m_ownCallsign, ab, Qt::CaseInsensitive);
            if (pos < 0) {
                return false;
            }
            const bool linksFrei = pos == 0 || !(text.at(pos - 1).isLetterOrNumber() || text.at(pos - 1) == QLatin1Char('/'));
            const int nach = pos + m_ownCallsign.size();
            const bool rechtsFrei = nach >= text.size()
                                     || !(text.at(nach).isLetterOrNumber() || text.at(nach) == QLatin1Char('/'));
            if (linksFrei && rechtsFrei) {
                return true;
            }
            ab = pos + 1;
        }
    }

    void setShowAll(bool alle)
    {
        if (m_alleZeigen == alle) {
            return;
        }
        m_alleZeigen = alle;
        rebuild();
    }
    bool showsAll() const { return m_alleZeigen; }
    // Wie viele Zeilen insgesamt hereinkamen -- für den Hinweis "x von y".
    int totalCount() const
    {
        int summe = 0;
        for (ChatFeedModel* modell : {m_onKst, m_cluster}) {
            if (modell) {
                summe += modell->lineCount();
            }
        }
        return summe;
    }

    void rebuild()
    {
        beginResetModel();
        m_rows.clear();
        const auto sammle = [this](ChatFeedModel* modell, bool ausKst) {
            if (!modell) {
                return;
            }
            // Standardmäßig nur die gefilterten Zeilen. Martin,
            // 2026-09-28: "alles was mich nicht erreicht bzw. was
            // absolut nicht funktionieren kann möchte ich gefiltert
            // haben um nicht 1000 unnötige chat zu sehen." Das Filtern
            // ist also gewollt -- es soll nur nicht UNSICHTBAR
            // geschehen: die Kopfzeile sagt, wie viele Zeilen gerade
            // stehen und wie viele hereinkamen, und über den ⚙ lässt
            // sich alles zeigen.
            if (!m_alleZeigen) {
                for (int r = 0; r < modell->rowCount(); ++r) {
                    Zeile zeile;
                    zeile.ausKst = ausKst;
                    // candidateAt() geht über die SICHTBAREN Zeilen;
                    // die Entfernung dazu holt lineAt() nicht, also aus
                    // dem Modell lesen.
                    zeile.linie.candidate = modell->candidateAt(r);
                    zeile.linie.worked =
                        modell->data(modell->index(r, ChatFeedModel::ColumnCallsign), ChatFeedModel::DupeRole)
                            .toBool();
                    const QString km = modell->data(modell->index(r, ChatFeedModel::ColumnDistanceKm)).toString();
                    zeile.linie.distanceKnown = !km.isEmpty() && km != Style::unknownDash();
                    zeile.linie.distanceKm = km.toDouble();
                    zeile.linie.bearingDeg =
                        modell->data(modell->index(r, ChatFeedModel::ColumnBearingDeg)).toString().toDouble();
                    m_rows.append(zeile);
                }
                return;
            }
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
    QString m_ownCallsign;
    ChatFeedModel* m_onKst = nullptr;
    ChatFeedModel* m_cluster = nullptr;
    QVector<Zeile> m_rows;
    bool m_alleZeigen = false;
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
    connect(m_model, &QAbstractItemModel::modelReset, this, [this]() {
        m_table->scrollToBottom();
        updateStatusLine();
    });

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

void ChatPanelWidget::setOwnCallsign(const QString& callsign)
{
    if (m_model) {
        m_model->setOwnCallsign(callsign);
    }
}

QString ChatPanelWidget::ownCallsign() const
{
    return m_model ? m_model->ownCallsign() : QString();
}

void ChatPanelWidget::setConnectionStatus(const QString& text)
{
    m_connectionText = text;
    updateStatusLine();
}

void ChatPanelWidget::setShowAll(bool showAll)
{
    m_model->setShowAll(showAll);
    updateStatusLine();
}

bool ChatPanelWidget::showsAll() const
{
    return m_model->showsAll();
}

// "ON4KST: verbunden · 12 von 87 Zeilen (gefiltert)" -- damit sichtbar
// ist, DASS gefiltert wird. Das Filtern selbst ist gewollt, siehe
// setShowAll().
void ChatPanelWidget::updateStatusLine()
{
    const int gezeigt = m_model->rowCount();
    const int gesamt = m_model->totalCount();
    QString text = m_connectionText;
    if (!text.isEmpty()) {
        text += QStringLiteral(" · ");
    }
    if (m_model->showsAll() || gezeigt == gesamt) {
        text += QStringLiteral("%1 Zeilen").arg(gesamt);
    } else {
        text += QStringLiteral("%1 von %2 Zeilen (gefiltert)").arg(gezeigt).arg(gesamt);
    }
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
