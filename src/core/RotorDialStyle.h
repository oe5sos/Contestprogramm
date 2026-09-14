#pragma once

namespace Contestprogramm {

// Which of RotorWidget's four paint styles to use for both rotor
// compasses at once -- one operator-wide ContestSettings::rotorDialStyle
// setting, not per-rotor (see SettingsDialog's rotor-display group and
// MainWindow::applyRotorWidgetSettings()), so the two side-by-side
// compasses always match.
//
// Lives in its own core/ header, not nested inside app/ContestSettings.h
// or declared in ui/RotorWidget.h, so both can use the same
// unqualified `RotorDialStyle` type without either layer including the
// other (this codebase's app/core/data layer never includes from ui/,
// see e.g. core/BeamHeading.h's Stop enum for the same reasoning).
enum class RotorDialStyle {
    // Today's full 360-degree compass rose (N/E/S/W + degree ticks) --
    // RotorWidget's only rendering before this pass, per the
    // Rotor-A.dc.html mockup. The default.
    FullCompass,
    // A horizontal azimuth scale/slider, per the Rotor-B.dc.html
    // mockup -- more compact, no circular dial at all.
    LinearScale,
    // A ~300-degree arc with a fixed gap centred on south representing
    // a mechanical stop / cable-wrap zone, per the Rotor-C.dc.html
    // mockup. The gap is a static visual convention of this style only
    // -- see RotorWidget.cpp's drawArcGapMarkers() comment -- not live
    // BeamHeading::Stop or per-rotor ContestSettings data, neither of
    // which exists yet.
    PartialArc,
    // A small, secondary compass ring (roughly half the other styles'
    // radius, moved to the top of the panel) with the AKTUELL/ZIEL/
    // ENTFERNUNG readout as the dominant element instead: large glowing
    // monospace digits inside a "black glass" panel, per HAUSSTIL's
    // already-documented "Grosse Zahlen liegen in schwarzem Glas" rule
    // (~/Longpath/NereusSDR/docs/design/HAUSSTIL.md), applied to the
    // rotor for the first time. Ground truth: Design2.dc.html
    // ("Option 2 -- Digital", build2.py's glass() helper).
    //
    // Martin, 2026-09-11, after a long back-and-forth this session
    // culminating in three genuinely distinct graphical mockups shown
    // on the design canvas: "bitte baue alle 3 also optionen ein, die
    // ich jederzeit ändern kann" -- Digital is the one of those three
    // that is a genuinely new paint path (RotorWidget::
    // paintDigitalDial()/drawDigitalReadout()). The other two mockups
    // ("Klassisch"/"Bogen-Präzision") are refinements of FullCompass/
    // PartialArc above rather than separate enum values -- see
    // RotorWidget.cpp's drawNeedle() (needle-width match for Main.
    // dc.html) and its PartialArc-only dotted-second-antenna branch
    // (Design3.dc.html) for what "refinement" means concretely here.
    Digital,
};

} // namespace Contestprogramm
