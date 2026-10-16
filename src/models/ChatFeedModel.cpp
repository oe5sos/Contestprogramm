#include "models/ChatFeedModel.h"

#include "core/BandUtils.h"
#include "core/ChatImportanceScorer.h"
#include "core/ChatVisibilityPolicy.h"
#include "core/RecentPropagationTracker.h"
#include "data/DupeChecker.h"
#include "data/MultiplierTracker.h"
#include "ui/StyleKit.h"

#include <QColor>
#include <QHash>
#include <QDateTime>

namespace Contestprogramm {

namespace {
const QStringList kCallsignOnlyScope{QStringLiteral("callsign")};

// Wie viele Zeilen das Modell überhaupt behält.
//
// Jede ankommende Zeile baut die Sichtliste neu auf (die
// Sichtbarkeitsregel ist relativ zu allen übrigen Bewertungen, siehe
// rebuildVisibleRows()), und nach jedem geloggten QSO läuft
// refreshWorkedAndScores() mit einer Dupe-Abfrage je Eintrag darüber.
// Beides wächst mit der Liste, und ein 24-Stunden-Contest mit 30
// Zeilen je Minute sind 43 200 Zeilen.
//
// Gemessen im RELEASE, so wie das Programm läuft
// (tests/test_chat_dauerlast.cpp, Apple Silicon, 12 000 Zeilen): eine
// ankommende Zeile kostete 0,11 ms bei 1 000 und 0,84 ms bei 12 000 --
// das allein wäre zu ertragen. Das Teure ist die Auffrischung NACH
// JEDEM GELOGGTEN QSO: **346 ms** bei 12 000 Zeilen, also eine Drittel
// Sekunde Stillstand unmittelbar nach dem Return, und zwar jedesmal.
// (Im Debug-Bau sind die Zahlen anders gewichtet -- dort kostet schon
// die einzelne Zeile 12 ms bei 20 000 --, aber gefunkt wird mit dem
// Release.)
//
// 1 500 Zeilen sind bei dieser Rate knapp eine Stunde Rückschau --
// mehr braucht ein Chatfenster nicht, das ein LIVE-Bild ist und kein
// Archiv (der Chat gehört nicht zum Log, und ein Spot von vor sechs
// Stunden ist tot). Die ältesten fallen heraus.
constexpr int kMaxEntries = 1500;
} // namespace

ChatFeedModel::ChatFeedModel(GeoFilter& geoFilter, DupeChecker& dupeChecker, QObject* parent)
    : QAbstractTableModel(parent)
    , m_geoFilter(geoFilter)
    , m_dupeChecker(dupeChecker)
{
}

int ChatFeedModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_visibleIndices.size();
}

int ChatFeedModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return ColumnCount;
}

QVariant ChatFeedModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visibleIndices.size()) {
        return QVariant();
    }
    const Entry& entry = m_allEntries.at(m_visibleIndices.at(index.row()));

    if (role == DupeRole) {
        return entry.worked;
    }
    if (role == InRangeRole) {
        return entry.geo.inRange;
    }
    if (role == Qt::ForegroundRole && entry.worked) {
        // Dimmed, per the plan's "Dupes gedimmt" -- the house palette's
        // named inactive tone, not a generic Qt::gray that ignores the
        // rest of the app's colour language.
        return QVariant::fromValue(QColor(Style::kTextInactive()));
    }
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    switch (index.column()) {
    case ColumnCallsign:
        return entry.candidate.callsign;
    case ColumnGrid:
        return entry.candidate.grid;
    case ColumnDistanceKm:
        // Unknown is a dash, not a placeholder question mark --
        // HAUSSTIL rule 7, and consistent with LogTableModel's own
        // unknown-distance/-bearing cells.
        return entry.geo.distanceKnown ? QString::number(entry.geo.distanceKm, 'f', 0) : Style::unknownDash();
    case ColumnBearingDeg:
        return entry.geo.distanceKnown ? QString::number(entry.geo.bearingDeg, 'f', 0) : Style::unknownDash();
    case ColumnText:
        return entry.candidate.rawLine;
    default:
        return QVariant();
    }
}

QVariant ChatFeedModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }
    switch (section) {
    case ColumnCallsign:
        return QStringLiteral("Call");
    case ColumnGrid:
        return QStringLiteral("Grid");
    case ColumnDistanceKm:
        return QStringLiteral("km");
    case ColumnBearingDeg:
        return QStringLiteral("°");
    case ColumnText:
        return QStringLiteral("Text");
    default:
        return QVariant();
    }
}

void ChatFeedModel::setShowRawFeed(bool show)
{
    if (m_showRaw == show) {
        return;
    }
    m_showRaw = show;
    rebuildVisibleRows();
}

bool ChatFeedModel::computeWorked(const QString& callsign, qint64 freqHz,
                                  const DupeChecker::WorkedStations* vorab) const
{
    if (m_activeContestId.isEmpty()) {
        return false;
    }
    static const QStringList kCallsignBandScope{QStringLiteral("callsign"), QStringLiteral("band")};
    const QString spotBand = freqHz > 0 ? bandLabelForFrequencyHz(freqHz) : QString();
    if (!spotBand.isEmpty()) {
        return vorab ? vorab->workedOnBand(callsign, spotBand)
                     : m_dupeChecker.isDupe(callsign, spotBand, QString(), m_activeContestId,
                                            kCallsignBandScope);
    }
    if (m_contestBands.isEmpty()) {
        return vorab ? vorab->workedAtAll(callsign)
                     : m_dupeChecker.isDupe(callsign, QString(), QString(), m_activeContestId,
                                            kCallsignOnlyScope);
    }
    for (const QString& band : m_contestBands) {
        const bool gearbeitet = vorab
                                    ? vorab->workedOnBand(callsign, band)
                                    : m_dupeChecker.isDupe(callsign, band, QString(), m_activeContestId,
                                                           kCallsignBandScope);
        if (!gearbeitet) {
            return false; // still open on this band
        }
    }
    return true;
}

void ChatFeedModel::setContestBands(const QStringList& bands)
{
    if (m_contestBands == bands) {
        return;
    }
    m_contestBands = bands;
    refreshWorkedAndScores();
}

double ChatFeedModel::computeScore(const SpotCandidate& candidate, bool worked, const GeoFilter::Result& geo) const
{
    const QString band = candidate.freqHz > 0 ? bandLabelForFrequencyHz(candidate.freqHz) : QString();
    const QDateTime now = QDateTime::currentDateTimeUtc();

    bool needed = false;
    if (m_multiplierTracker && !band.isEmpty() && !candidate.grid.isEmpty()) {
        needed = m_multiplierTracker->isNeededMultiplier(band, candidate.grid, candidate.callsign);
    }

    // The plan's "Rate-Potenzial aus einem frischen eigenen QSO" --
    // needs a known bearing (geo.distanceKnown) and a resolvable band,
    // same preconditions the multiplier check above already applies for
    // its own inputs.
    bool propagationNearby = false;
    if (m_propagationTracker && !band.isEmpty() && geo.distanceKnown) {
        propagationNearby = m_propagationTracker->hasRecentOpeningNear(band, geo.bearingDeg, now);
    }

    return ChatImportanceScorer::score(worked, needed, propagationNearby, candidate.timestampUtc, now);
}

void ChatFeedModel::setActiveContest(const QString& contestId)
{
    m_activeContestId = contestId;
    // Existing entries' "worked" flag (and therefore score) was
    // computed against whatever contest was active when they arrived;
    // recompute so a contest switch does not leave stale flags around.
    refreshWorkedAndScores();
}

void ChatFeedModel::refreshWorkedAndScores()
{
    // Läuft nach JEDEM geloggten QSO (und bei Contestwechsel,
    // Korrektur, Ungültigsetzen) -- also mitten im Betrieb, im selben
    // Faden wie die Eingabezeile. Der Aufwand steckt in den
    // Datenbankabfragen: computeWorked() fragt je Eintrag, und bei
    // einer Chatzeile OHNE Frequenz sogar je Contestband einmal.
    // Gemessen mit 1 500 Einträgen: 60 ms unmittelbar nach dem Return
    // (tests/test_chat_dauerlast.cpp).
    //
    // Deshalb den Contest EINMAL lesen statt je Eintrag zu fragen:
    // DupeChecker::workedStations() gibt alle gearbeiteten Stationen
    // auf einmal zurück, mit derselben Regel (Grundrufzeichen,
    // ungültige QSOs zählen nicht). Gemessen bei 1 200 QSOs und einer
    // vollen Chatliste: 130 Einzelabfragen kosteten 60 ms je Liste,
    // die eine Abfrage kostet Bruchteile davon.
    const DupeChecker::WorkedStations gearbeitet = m_dupeChecker.workedStations(m_activeContestId);
    for (Entry& entry : m_allEntries) {
        entry.worked = computeWorked(entry.candidate.callsign, entry.candidate.freqHz, &gearbeitet);
        entry.score = computeScore(entry.candidate, entry.worked, entry.geo);
    }
    rebuildVisibleRows();
}

void ChatFeedModel::setMultiplierTracker(MultiplierTracker* tracker)
{
    m_multiplierTracker = tracker;
    for (Entry& entry : m_allEntries) {
        entry.score = computeScore(entry.candidate, entry.worked, entry.geo);
    }
    rebuildVisibleRows();
}

void ChatFeedModel::setRecentPropagationTracker(RecentPropagationTracker* tracker)
{
    m_propagationTracker = tracker;
    for (Entry& entry : m_allEntries) {
        entry.score = computeScore(entry.candidate, entry.worked, entry.geo);
    }
    rebuildVisibleRows();
}

void ChatFeedModel::setCurrentRatePerTenMinutes(double ratePerTenMinutes)
{
    // Wache wie bei den übrigen Setzern: der Neuaufbau ist ein
    // vollständiger Modell-Reset über alle Einträge, und die Rate
    // kommt nach jedem geloggten QSO herein -- meist mit demselben
    // Wert wie vorher.
    if (qFuzzyCompare(m_ratePerTenMinutes + 1.0, ratePerTenMinutes + 1.0)) {
        return;
    }
    // Und selbst eine echte Änderung ändert die Sichtliste nur, wenn
    // sie das TEMPO wechselt: ChatVisibilityPolicy::visibilityMask()
    // kennt nur die drei Stufen (ruhig / mittel / lebhaft), nicht die
    // Zahl dahinter. In einem Pile-up geht die Zahl mit jedem QSO um
    // eins hoch -- ohne diese Unterscheidung wäre das jedesmal ein
    // vollständiger Neuaufbau beider Listen.
    const ChatVisibilityPolicy::Tempo vorher = ChatVisibilityPolicy::tempoForRate(m_ratePerTenMinutes);
    const ChatVisibilityPolicy::Tempo nachher = ChatVisibilityPolicy::tempoForRate(ratePerTenMinutes);
    m_ratePerTenMinutes = ratePerTenMinutes;
    if (vorher != nachher) {
        rebuildVisibleRows();
    }
}

ChatFeedModel::FeedLine ChatFeedModel::lineAt(int index) const
{
    FeedLine zeile;
    if (index < 0 || index >= m_allEntries.size()) {
        return zeile;
    }
    const Entry& eintrag = m_allEntries.at(index);
    zeile.candidate = eintrag.candidate;
    zeile.worked = eintrag.worked;
    zeile.distanceKnown = eintrag.geo.distanceKnown;
    zeile.distanceKm = eintrag.geo.distanceKm;
    zeile.bearingDeg = eintrag.geo.bearingDeg;
    return zeile;
}

// Gehört die Frequenz dieses Spots zu einem Band, auf dem der aktive
// Contest überhaupt läuft?
//
// Martin, 2026-09-29, mit Bild: "im chatroom 144 sind
// kurzwelleneinträge." Die Zeilen kamen nicht aus dem ON4KST-Raum,
// sondern vom DX-Cluster -- der schickt, was weltweit gespottet wird,
// also auch 1,8 / 7 / 10 / 21 MHz. In einem UKW-Contest ist das nichts,
// was einen erreichen kann; es ist genau der Fall, den sein Filter
// abdecken soll ("alles was mich nicht erreicht bzw. was absolut nicht
// funktionieren kann möchte ich gefiltert haben").
//
// Ein Spot OHNE Frequenz (jede Chatzeile) bleibt drin: über den sagt
// die Frequenz nichts aus. Und ohne bekannte Contestbänder wird nichts
// aussortiert -- lieber zu viel zeigen als etwas Richtiges verschlucken.
bool ChatFeedModel::bandBelongsToContest(qint64 freqHz) const
{
    if (freqHz <= 0 || m_contestBands.isEmpty()) {
        return true;
    }
    const QString band = bandLabelForFrequencyHz(freqHz);
    if (band.isEmpty()) {
        return true; // unbekanntes Band -- nicht heimlich wegwerfen
    }
    return m_contestBands.contains(band);
}

void ChatFeedModel::rebuildVisibleRows()
{
    const QVector<int> neu = sichtbareZeilen();
    if (neu == m_visibleIndices) {
        // Dieselben Zeilen in derselben Reihenfolge -- dann hat sich
        // nur ihr INHALT geändert (Bewertung, "gearbeitet"), nicht die
        // Zusammensetzung. Ein vollständiger Reset wäre hier teuer und
        // unhöflich: die Ansicht fragt danach jede Zelle neu ab, und
        // Bildlauf und Auswahl springen. dataChanged() lässt beides
        // stehen und holt nur, was wirklich zu sehen ist.
        if (!neu.isEmpty()) {
            emit dataChanged(index(0, 0), index(neu.size() - 1, ColumnCount - 1));
        } else {
            // Keine sichtbare Zeile, über die sich etwas melden ließe --
            // und trotzdem ist etwas passiert: eine Zeile, die der
            // Filter aussortiert, steht in der Ansicht "alles zeigen"
            // des Chatpanels sehr wohl (siehe feedChanged()).
            emit feedChanged();
        }
        return;
    }
    beginResetModel();
    m_visibleIndices = neu;
    endResetModel();
}

QVector<int> ChatFeedModel::sichtbareZeilen() const
{
    QVector<int> sichtbar;

    if (m_showRaw) {
        sichtbar.reserve(m_allEntries.size());
        for (int i = 0; i < m_allEntries.size(); ++i) {
            sichtbar.append(i);
        }
        return sichtbar;
    }

    // Hard exclusion first, per the plan's Adaptive-Chat-Filterung (a):
    // out-of-range or already-worked entries never reach the
    // rate-derived score cut below, at any tempo.
    QVector<int> survivorIndices;
    QVector<double> survivorScores;
    for (int i = 0; i < m_allEntries.size(); ++i) {
        const Entry& entry = m_allEntries.at(i);
        if (entry.geo.inRange && !entry.worked && bandBelongsToContest(entry.candidate.freqHz)) {
            survivorIndices.append(i);
            survivorScores.append(entry.score);
        }
    }

    const ChatVisibilityPolicy::Tempo tempo = ChatVisibilityPolicy::tempoForRate(m_ratePerTenMinutes);
    const QVector<bool> mask = ChatVisibilityPolicy::visibilityMask(survivorScores, tempo);
    for (int i = 0; i < survivorIndices.size(); ++i) {
        if (mask.at(i)) {
            sichtbar.append(survivorIndices.at(i));
        }
    }
    return sichtbar;
}

void ChatFeedModel::addCandidate(const SpotCandidate& candidate)
{
    Entry entry;
    entry.candidate = candidate;
    entry.geo = m_geoFilter.classify(candidate);
    entry.worked = computeWorked(candidate.callsign, candidate.freqHz);
    entry.score = computeScore(candidate, entry.worked, entry.geo);

    m_allEntries.append(entry);
    if (m_allEntries.size() > kMaxEntries) {
        const int zuViel = m_allEntries.size() - kMaxEntries;
        m_allEntries.erase(m_allEntries.begin(), m_allEntries.begin() + zuViel);
    }
    // A full rebuild (reset model) rather than the previous incremental
    // beginInsertRows: at Busy tempo, ChatVisibilityPolicy's top-
    // fraction cutoff is relative to every current survivor's score, so
    // one new arrival can change which existing rows stay visible too --
    // an incremental single-row insert cannot express that. Contest-
    // scale spot volume (tens per minute, not thousands) makes the
    // full-rebuild cost a non-issue.
    rebuildVisibleRows();
}

const SpotCandidate& ChatFeedModel::candidateAt(int row) const
{
    static const SpotCandidate kEmpty;
    if (row < 0 || row >= m_visibleIndices.size()) {
        return kEmpty;
    }
    return m_allEntries.at(m_visibleIndices.at(row)).candidate;
}

double ChatFeedModel::scoreAt(int row) const
{
    if (row < 0 || row >= m_visibleIndices.size()) {
        return 0.0;
    }
    return m_allEntries.at(m_visibleIndices.at(row)).score;
}

GeoFilter::Result ChatFeedModel::geoAt(int row) const
{
    if (row < 0 || row >= m_visibleIndices.size()) {
        return GeoFilter::Result();
    }
    return m_allEntries.at(m_visibleIndices.at(row)).geo;
}

} // namespace Contestprogramm
