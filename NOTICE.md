# NOTICE

Contestprogramm is © 2026 Martin Fischer, OE5SOS, and contributors,
licensed as a whole under the GNU General Public License v3.0 or later —
see `LICENSE`. This file documents third-party attribution independently
of that choice.

## Maidenhead locator geometry (`src/core/Maidenhead.h` / `.cpp`)

The grid-square ↔ latitude/longitude conversion, Haversine great-circle
distance, and initial-bearing functions in `src/core/Maidenhead.cpp` are
ported from:

- **freedv-gui** — `src/gui/dialogs/freedv_reporter.cpp:2312-2410`
  (`calculateDistance_`, `calculateLatLonFromGridSquare_`,
  `calculateBearingInDegrees_`, `DegreesToRadians_`, `RadiansToDegrees_`),
  commit `@77e793a`. <https://github.com/drowe67/freedv-gui>

  freedv-gui carries an **LGPL-2.1-or-later** root license
  (`freedv-gui/COPYING`). The specific `freedv_reporter.cpp` file has no
  per-file copyright header, so the project root license applies.
  Copyright the freedv-gui contributors / FreeDV project.

This port was taken by way of an intermediate stop: **Longpath**
(a fork of NereusSDR, © J.J. Boyd / KG4VCF; itself a C++/Qt6 port of
Thetis), whose `src/core/Maidenhead.h` / the implementation formerly
embedded in `src/models/FreeDVStationModel.cpp` hoisted the same
freedv-gui-derived functions into standalone, reusable form on
2026-08-07, with no change to the math. Contestprogramm's copy is taken
from that already-hoisted, unmodified form — see
`~/Longpath/NereusSDR/src/core/Maidenhead.h` and
`~/Longpath/NereusSDR/src/models/FreeDVStationModel.cpp` for the
intermediate copy this was taken from.

The two source files retain the LGPL-2.1-or-later SPDX identifier and a
verbatim copy of this attribution in their own header comments; see
those files for the exact upstream line ranges and translation notes.
