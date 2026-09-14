#pragma once

#include "app/ContestSettings.h"
#include "data/ContestDefinition.h"

#include <QDialog>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDateTimeEdit;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace Contestprogramm {

class CallsignLocatorLookup;
class SrtmTileLoader;

// Minimal first-run / preferences dialog: own callsign, own grid, site
// elevation, antenna height, active contest, a radius field (now
// active -- GeoFilter uses it to classify ON4KST candidates), and a
// "Contest-Ende (UTC)" date+time field for the top-bar countdown (ui/
// UtcClockWidget.h) -- see ContestSettings::contestEndUtc; the "Gesetzt"
// checkbox next to it distinguishes "unset" (the countdown shows a dash)
// from any particular timestamp, including midnight. The "CAT
// (rigctld)" group is wired up (RigctldClient reads host/port from
// here). TCI host/port and ON4KST credentials remain plain fields in a
// "connected later" group -- TciClient is not built in this phase (the
// classic-transceiver CAT path via rigctld is the primary radio source
// per the plan), and ON4KST live-server verification is a deliberate
// separate manual step for the operator.
//
// Kern-Welle 2 adds three more groups: "Rotor 1"/"Rotor 2" (each slot's
// enabled toggle, free-text label, rotctld host/port, an optional
// fixed-offset second antenna, and -- ported from Longpath/NereusSDR's
// rotor-comfort feature, 2026-09-11 -- a Hamlib model combo (populated
// from core/RotorModels.h's commonRotorModels(), Yaesu GS-232A
// preselected per the operator's own confirmation), a serial-device
// text field, and a baud-rate combo (commonRotorBauds()); these three
// are only consulted if/when this program is wired up to start rotctld
// itself for that slot (core/RotctldProcess.h) -- an operator who runs
// rotctld their own way is unaffected, see that class's own comment.
// See ContestSettings::rotor1Enabled etc. and RotctldClient; disabling
// a slot grays out the rest of its group),
// "Band-Zuordnung" (which slot -- by its current label, or "Kein Rotor"
// -- handles 144/432/1296 MHz; a disabled slot cannot be selected, see
// refreshBandRotorCombos()), a "Rotor-Anzeige" combo (m_rotorDialStyleCombo
// -- which of RotorWidget's four paint styles both compasses use, see
// ContestSettings::rotorDialStyle; one operator-wide choice, not
// per-slot like the two groups above), and "CW-Makros" (one QLineEdit
// per ContestSettings::cwMacros entry, consumed by ui/CwMacroPanel.h).
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    // `callsignLocatorLookup` backs the "Locator-Liste importieren..."
    // button (core/CallsignLocatorLookup.h) -- optional (defaults to
    // nullptr) so existing call sites/tests that only care about the
    // other fields keep compiling unchanged; the import button is simply
    // disabled when null (see the constructor).
    SettingsDialog(const ContestSettings& initial,
                   const QVector<ContestDefinition>& availableContests,
                   CallsignLocatorLookup* callsignLocatorLookup = nullptr,
                   QWidget* parent = nullptr);

    ContestSettings settings() const;

private:
    // "Locator-Liste importieren..." button handler -- opens a
    // QFileDialog for a CSV file, imports it via
    // m_callsignLocatorLookup->importCsvFile(), and reports the
    // imported/skipped counts via QMessageBox. A no-op (button is
    // disabled, so this is never actually reachable) when
    // m_callsignLocatorLookup is null.
    void importLocatorCsv();

    // Rebuilds the three band->rotor combo boxes' item lists from the
    // current rotor1/rotor2 enabled-checkbox + label-edit state
    // (connected to both, live, so toggling a slot off or renaming it
    // updates the combos immediately) -- an item only exists for a slot
    // that is currently checked as enabled, so a disabled slot can never
    // be selected as a band's rotor assignment. Preserves each combo's
    // current selection across the rebuild where the selected slot is
    // still available; falls back to "Kein Rotor" when the previously
    // selected slot just became disabled.
    void refreshBandRotorCombos();

    // "GPS-Koordinaten" row's "Übernehmen" button -- operator, 2026-09-13:
    // "ggf. sollte man die genauen standortdaten, ev, auch über gps daten
    // eingeben können", prompted by the terrain-wash investigation
    // finding a mismatched (guessed) elevation. Computes the 6-character
    // locator from m_gpsLatSpin/m_gpsLonSpin via
    // Maidenhead::gridSquareFromLatLon() and writes it into m_gridEdit
    // immediately (synchronous, no data dependency); then looks up the
    // real SRTM elevation at that same point via m_gpsElevationLoader
    // and writes it into m_elevationSpin once available -- synchronously
    // if the covering tile is already disk-cached, otherwise after its
    // tileLoaded()/tileLoadFailed() signal (see SrtmTileLoader::
    // ensureTileAvailable()'s own doc comment). Does NOT touch
    // m_antennaHeightSpin (mast height above ground is not something
    // SRTM or a lat/lon pair can tell us) or persist the raw lat/lon
    // itself anywhere -- only the two fields ContestSettings already has
    // a place for (ownGrid, ownElevationM) are affected, so this is a
    // one-time convenience action, not new persisted state.
    void applyGpsCoordinates();

    // Operator, 2026-09-13: "copy paste hat nicht funktioniert" --
    // pasting a coordinate copied from a phone/Maps app failed for two
    // separate reasons, both fixed here: (1) m_gpsLatSpin/m_gpsLonSpin
    // now use QLocale::c() (see the constructor) so a period-decimal
    // paste like "47.820300" -- the universal GPS convention, and what
    // every phone/Maps app actually copies -- is accepted at all,
    // rather than being silently rejected/mangled by this dialog's
    // otherwise-German locale (comma decimal separator); (2) copying a
    // location from Google/Apple Maps hands you BOTH values as one
    // string ("47.820300, 13.936400"), which cannot land correctly in
    // either single spin box on its own -- this handler, connected to
    // both fields' own QLineEdit::textEdited (real user input,
    // including paste; never fires for this method's own setValue()
    // calls), detects that combined-pair shape regardless of which of
    // the two fields it was pasted into and fills both from it.
    void handleGpsCoordinateTextEdited(const QString& text);

    ContestSettings m_initial;
    QVector<ContestDefinition> m_availableContests;
    CallsignLocatorLookup* m_callsignLocatorLookup = nullptr;

    QLineEdit* m_callsignEdit;
    QLineEdit* m_gridEdit;
    QDoubleSpinBox* m_elevationSpin;
    QDoubleSpinBox* m_antennaHeightSpin;
    // GPS-coordinates convenience row -- see applyGpsCoordinates()'s own
    // doc comment. m_gpsElevationLoader is a dialog-owned SrtmTileLoader,
    // independent of AppController::terrainDataManager()'s own instance
    // (SettingsDialog has no access to that higher-layer object) -- both
    // read/write the SAME on-disk tile cache
    // (QStandardPaths::AppDataLocation + "/srtm/", keyed by
    // applicationName/organizationName, not per-instance), so this one
    // still benefits from tiles the terrain feature already fetched, and
    // vice versa. Created lazily (see applyGpsCoordinates()), stays null
    // until the operator actually clicks the button once.
    QDoubleSpinBox* m_gpsLatSpin;
    QDoubleSpinBox* m_gpsLonSpin;
    QPushButton* m_gpsApplyButton;
    QLabel* m_gpsStatusLabel;
    SrtmTileLoader* m_gpsElevationLoader = nullptr;
    QDoubleSpinBox* m_radiusSpin;
    QComboBox* m_contestCombo;

    // Contest-end timestamp for the top-bar countdown (ui/
    // UtcClockWidget.h) -- see ContestSettings::contestEndUtc.
    // m_contestEndSetCheck grays out (not auto-unchecks) m_contestEndEdit
    // when unset, same "off means the rest of the row cannot apply"
    // convention as the rotor-enabled checkboxes below.
    QCheckBox* m_contestEndSetCheck;
    QDateTimeEdit* m_contestEndEdit;
    QLineEdit* m_tciHostEdit;
    QSpinBox* m_tciPortSpin;
    QLineEdit* m_rigctldHostEdit;
    QSpinBox* m_rigctldPortSpin;
    QLineEdit* m_on4kstUserEdit;
    QLineEdit* m_on4kstPassEdit;
    QLineEdit* m_clusterHostEdit;
    QSpinBox* m_clusterPortSpin;

    // Callsign->grid autofill's external tier (core/
    // CallsignLocatorLookup.h) -- provider choice + credentials, and the
    // one-time CSV import button for the offline imported_locators
    // table. See ContestSettings::callbookProvider etc.
    QComboBox* m_callbookProviderCombo;
    QLineEdit* m_callbookUserEdit;
    QLineEdit* m_callbookPassEdit;
    QPushButton* m_importLocatorsButton;

    // Rotor slot 1 (Kern-Welle 2, generalized) -- see
    // ContestSettings::rotor1Enabled etc.
    QCheckBox* m_rotor1EnabledCheck;
    QLineEdit* m_rotor1LabelEdit;
    QLineEdit* m_rotor1HostEdit;
    QSpinBox* m_rotor1PortSpin;
    QCheckBox* m_rotor1SecondAntennaCheck;
    QDoubleSpinBox* m_rotor1SecondAntennaOffsetSpin;
    // Hamlib rotctld model/device/baud (core/RotorModels.h,
    // core/RotctldProcess.h) -- see ContestSettings::rotor1HamlibModel
    // etc.
    QComboBox* m_rotor1ModelCombo;
    QLineEdit* m_rotor1DeviceEdit;
    QComboBox* m_rotor1BaudCombo;

    // Rotor slot 2 -- see ContestSettings::rotor2Enabled etc.
    QCheckBox* m_rotor2EnabledCheck;
    QLineEdit* m_rotor2LabelEdit;
    QLineEdit* m_rotor2HostEdit;
    QSpinBox* m_rotor2PortSpin;
    QCheckBox* m_rotor2SecondAntennaCheck;
    QDoubleSpinBox* m_rotor2SecondAntennaOffsetSpin;
    // See m_rotor1ModelCombo etc. above -- same fields, slot 2.
    QComboBox* m_rotor2ModelCombo;
    QLineEdit* m_rotor2DeviceEdit;
    QComboBox* m_rotor2BaudCombo;

    // Band -> rotor-slot assignment, see ContestSettings::
    // band144RotorSlot etc. and refreshBandRotorCombos().
    QComboBox* m_band144RotorCombo;
    QComboBox* m_band432RotorCombo;
    QComboBox* m_band1296RotorCombo;

    // Which RotorWidget paint style both compasses use (core/
    // RotorDialStyle.h) -- one operator-wide combo, not per-slot, see
    // ContestSettings::rotorDialStyle.
    QComboBox* m_rotorDialStyleCombo;

    // Which of the five selectable colour palettes the whole app uses
    // (core/ColorTheme.h) -- see ContestSettings::colorTheme. Its own
    // small "Darstellung" group, not folded into "Rotor-Anzeige" above:
    // this one is app-wide, not rotor-specific.
    QComboBox* m_colorThemeCombo;

    // CW F-key macros (Kern-Welle 2) -- see ContestSettings::cwMacros.
    QVector<QLineEdit*> m_cwMacroEdits;

    // UDP-Contact-Broadcast-Standard (core/BroadcastPublisher.h) --
    // see ContestSettings::broadcastEnabled etc. Off by default, so the
    // checkbox starts unchecked unless the operator already turned it
    // on in a previous session.
    QCheckBox* m_broadcastEnabledCheck;
    QLineEdit* m_broadcastHostEdit;
    QSpinBox* m_broadcastPortSpin;
};

} // namespace Contestprogramm
