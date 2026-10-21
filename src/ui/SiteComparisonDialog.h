#pragma once

#include "app/ContestSettings.h"

#include <QDialog>
#include <QVector>

class QComboBox;
class QDoubleSpinBox;
class QLabel;

namespace Contestprogramm {

class SiteHorizonManager;
class HorizonCompassWidget;
struct HorizonSector;

// Standalone real-terrain site-scouting/comparison view --
// Contestprogramm-original (2026-09-14). Built after the operator
// picked "Jauerling" from SettingsDialog's m_knownSitesCombo (the
// "Exakter Standort" feature) and saw NO change in RotorWidget's/
// MapWidget's terrain wash -- not a bug: that wash is
// TerrainDataManager::sectorSweep()'s own fixed-kSectorSweepDistanceKm
// (50km) near-field contest-time safety check, and Jauerling's real
// terrain -- independently verified this session with actual SRTM
// elevation-angle math accounting for earth curvature + standard 4/3-
// radius refraction (see core/terrain/HorizonSweep.h) -- is genuinely
// open for the first ~50km in almost every direction; the only real
// obstruction is the Alps at 65-100km to the south/south-southwest,
// which that 50km sweep structurally cannot see. This dialog answers
// the different question the operator actually wanted ("einen eigenen
// Punkt, wo man nur die Standorte miteinander vergleicht, unabhängig
// von Karte/Verbindung"): comparing up to two candidate sites' real
// terrain openness side by side out to core/terrain/HorizonSweep.h's
// own long reach (~180km), entirely independent of the live map/rotor
// panels and of whichever origin TerrainDataManager currently has
// configured for actual contest use -- picking a site here never
// changes AppController's own terrain origin, unlike SettingsDialog's
// "Exakter Standort" row.
//
// Reads core/terrain/KnownSites.h's own scouted-site list (the same one
// SettingsDialog::m_knownSitesCombo already reads) for both slot
// pickers, and owns a private SiteHorizonManager -- see that class's
// own doc comment for why it deliberately owns a SEPARATE
// SrtmTileLoader rather than reaching into AppController::
// terrainDataManager()'s instance.
class SiteComparisonDialog : public QDialog {
    Q_OBJECT

public:
    // `settings` seeds Slot A with the operator's OWN current station
    // (its exact-location fields if useExactOwnLocation is set and
    // actually filled in, otherwise its grid centroid -- the same
    // "which origin wins" choice AppController::applyNetworkSettings()
    // already makes when feeding TerrainDataManager) and both slots'
    // antenna-height spin boxes with its antennaHeightM, so opening
    // this dialog right after wondering "is my CURRENT site the
    // problem" immediately shows that comparison, with Slot B free to
    // pick whichever candidate the operator wants to check it against
    // (e.g. Jauerling). Slot B starts at "Kein Vergleich" -- there is
    // no sensible second default before the operator has said which
    // candidate they actually want compared.
    explicit SiteComparisonDialog(const ContestSettings& settings, QWidget* parent = nullptr);
    ~SiteComparisonDialog() override;

private:
    // One slot's whole set of widgets -- combo picker, manual
    // coordinate/elevation/antenna-height spin boxes, its compass, and
    // its plain-text summary label. Two instances (m_slotA/m_slotB)
    // rather than a QVector<Slot> of two: the constructor treats them
    // asymmetrically (Slot A's own default selection, see above), so
    // named members read more clearly here than an indexed pair would.
    struct Slot {
        QComboBox* combo = nullptr;
        QDoubleSpinBox* latSpin = nullptr;
        QDoubleSpinBox* lonSpin = nullptr;
        QDoubleSpinBox* elevationSpin = nullptr;
        QDoubleSpinBox* antennaHeightSpin = nullptr;
        HorizonCompassWidget* compass = nullptr;
        QLabel* summaryLabel = nullptr;
    };

    // Builds one slot's whole group box (title + combo + coordinate
    // fields + compass + summary label) -- identical structure for A
    // and B, only the combo's initial selection differs (set by the
    // constructor after this returns), so this is written once and
    // called twice rather than duplicated inline.
    QWidget* buildSlotGroup(const QString& title, Slot& slot);

    // `slot`'s combo activated() handler -- mirrors SettingsDialog::
    // applyKnownSiteSelection()'s own "copy the chosen KnownSite's lat/
    // lon/elevation into the visible fields, but leave them editable
    // afterwards" behaviour, generalized to this dialog's own four-way
    // item convention (built identically for both slots by
    // buildSlotGroup(), so either slot can hold any of these): "Kein
    // Vergleich" (clears the slot, no fields to fill), "Eigener
    // Standort" (copies the CURRENT settings() origin, not a
    // KnownSites.h entry -- offered in both combos, not just Slot A's,
    // even though only Slot A defaults to it), "-- Manuell --" (leaves
    // whatever is already in the coordinate fields untouched), and
    // every entry from there on is a real KnownSites.h site.
    void applyComboSelection(Slot& slot, int index);

    // Recomputes `slot`'s sweep via m_horizonManager and repaints its
    // compass + summary label -- connected to every one of that slot's
    // coordinate/elevation/antenna-height spin boxes' valueChanged
    // (operator edits a field by hand) AND to
    // SiteHorizonManager::sweepChanged for a matching origin (a
    // background tile fetch just resolved more of that slot's
    // bearings), so a slot's compass updates live from either cause
    // without SiteComparisonDialog needing to tell which one just
    // happened.
    void refreshSlot(Slot& slot);

    // True once the operator has picked something other than "Kein
    // Vergleich" for this slot -- combo index alone is not enough to
    // ask this cheaply from refreshSlot()'s many call sites, so it is
    // tracked directly.
    bool isSlotActive(const Slot& slot) const;

    // Builds the "Offen: 310°-130° (NW über N bis SO). Nächstes
    // Hindernis: 190°, ca. 80km." style summary sentence from one
    // sweep's sectors -- see this method's own body comment on the
    // "longest contiguous Clear run, plus the single worst Blocked/
    // Marginal sector" approach. Returns a "wird geladen"-style
    // sentence instead while any bearing is still Unknown, and a plain
    // "keine Geländedaten" sentence for a fully-Unknown sweep (e.g. no
    // network at all and nothing in the local tile cache yet).
    static QString summarize(const QVector<HorizonSector>& sectors);
    // A compact "NW" / "SO" / ... German 8-point compass label for one
    // bearing in degrees -- summarize()'s own small helper, not useful
    // outside it.
    static QString compassLabel(int bearingDeg);

    ContestSettings m_settings;
    SiteHorizonManager* m_horizonManager;
    Slot m_slotA;
    Slot m_slotB;
    bool m_slotAActive = true;
    bool m_slotBActive = false;
};

} // namespace Contestprogramm
