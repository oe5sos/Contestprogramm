#pragma once

#include "core/SpotCandidate.h"

#include <QDateTime>
#include <QHash>
#include <QString>
#include <QVector>

namespace Contestprogramm {

// The spots behind the bandmap (ui/BandmapWidget.h): every ON4KST/
// cluster candidate that carried a frequency, newest per callsign,
// dropped again after maxAgeMinutes -- the same bookkeeping N1MM+'s
// and DXLog.net's bandmaps do before they draw anything. Pure data,
// no widgets: spotsForBand() is what the widget paints, and the
// `worked` flag on each entry is filled in by the caller (MainWindow
// asks DupeChecker), not guessed here.
struct BandmapSpot {
    QString callsign;
    QString grid;
    qint64 freqHz = 0;
    QDateTime timestampUtc;
    QString source;
    bool worked = false;
    // Würde diese Station auf diesem Band einen neuen Multiplikator
    // bringen? Wie `worked` vom Aufrufer gefüllt (MainWindow fragt den
    // MultiplierTracker), nicht hier geraten -- N1MM und DXLog heben
    // genau diese Spots hervor, weil sie mehr zählen als ein QSO.
    bool neededMultiplier = false;
};

class BandmapModel {
public:
    static constexpr int kDefaultMaxAgeMinutes = 30;

    // A candidate without a frequency (a chat line) is ignored -- it
    // has no place on a frequency axis. A call already on the map is
    // moved to the new frequency/time rather than listed twice.
    void addSpot(const SpotCandidate& candidate);

    void setMaxAgeMinutes(int minutes) { m_maxAgeMinutes = minutes; }
    int maxAgeMinutes() const { return m_maxAgeMinutes; }

    // Spots on `band` (QsoRecord::band value, via bandLabelForFrequencyHz)
    // still younger than the age limit at `nowUtc`, lowest frequency
    // first. Expired spots are dropped from the model on the way.
    QVector<BandmapSpot> spotsForBand(const QString& band, const QDateTime& nowUtc);

    int count() const { return m_spots.size(); }
    void clear() { m_spots.clear(); }

private:
    QHash<QString, BandmapSpot> m_spots; // keyed by callsign
    int m_maxAgeMinutes = kDefaultMaxAgeMinutes;
};

} // namespace Contestprogramm
