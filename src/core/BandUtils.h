#pragma once

#include <QtGlobal>

class QString;

namespace Contestprogramm {

// IARU Region 1 amateur allocations for the two bands this program
// targets (see the plan's Kontext: "144 MHz (2m) und 432 MHz (70cm)"):
// 144-146 MHz and 430-440 MHz, widened slightly (144-148 / 420-450) to
// tolerate a rig reporting a few kHz outside the exact edge while
// sweeping the VFO. Returns an empty string when `hz` falls in neither.
//
// Extracted from ui/MainWindow.cpp (was a private static there) so
// ChatFeedModel's multiplier-boost scoring (see core/ChatImportanceScorer.h)
// can derive a SpotCandidate's band from its freqHz without depending on
// the UI layer.
QString bandLabelForFrequencyHz(qint64 hz);

} // namespace Contestprogramm
