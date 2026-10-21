#pragma once

// Contestprogramm-original (2026-09-14), for SettingsDialog's "Exakter
// Standort" convenience combo box (see ContestSettings::
// ownExactLatitude/ownExactLongitude). Real contest operators scout
// candidate mountaintop/parking-lot sites well ahead of a contest --
// this is a small, hand-curated picklist of ones already scouted, so
// picking one autofills the exact-location fields instead of the
// operator having to retype a remembered coordinate from a notebook or
// a previous log entry.

#include <QString>
#include <QVector>

namespace Contestprogramm {

// One field-scouted VHF/UHF contest candidate site -- exact coordinate
// and elevation for a real location the operator has visited or is
// considering. Not the same kind of value as SrtmTileLoader's own
// live elevationAt() lookup: these numbers were read off SRTM90 by
// hand ahead of time for a short list of known real places, not
// computed on demand from a downloaded tile, so no terrain module code
// depends on this list at runtime -- it only ever feeds SettingsDialog's
// combo box, which copies latitude/longitude/elevationM into the
// dialog's own fields once and is done with it.
struct KnownSite {
    QString name;
    double latitude = 0.0;
    double longitude = 0.0;
    double elevationM = 0.0;
};

// A small, hand-curated list of real field-scouted VHF/UHF contest
// candidate sites in Upper/Lower Austria -- coordinates from GPS/
// topo-map scouting, elevations from SRTM90. Expected to grow as more
// sites get scouted; a plain static QVector is intentionally all this
// is -- a fixed, code-shipped picklist, not operator-editable storage
// (an operator-editable list would be a separate, not-yet-requested
// feature, so there is no reason to over-engineer this one into
// something more than it needs to be).
const QVector<KnownSite>& knownSites();

} // namespace Contestprogramm
