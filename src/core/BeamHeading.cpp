// =================================================================
// src/core/BeamHeading.cpp  (Contestprogramm)
// =================================================================
//
// Contestprogramm-original -- ported faithfully from
// Longpath/NereusSDR's BeamHeading.cpp. See BeamHeading.h.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-09-09 — Created in C++20/Qt6, ported faithfully from
//                 Longpath/NereusSDR's BeamHeading.cpp; no maths
//                 changed. AI-assisted via Anthropic Claude Code,
//                 operator Ralph Martin Fischer.
//   2026-09-22 — longPathDistanceKm() ergänzt (Contestprogramm-eigen,
//                 nicht aus dem Vorbild portiert): auf Kurzwelle ist
//                 die Entfernung auf dem langen Weg eine echte Frage.
// =================================================================

#include "core/BeamHeading.h"

#include <algorithm>
#include <cmath>

namespace Contestprogramm::BeamHeading {

double wrap360(double deg)
{
    double d = std::fmod(deg, 360.0);
    if (d < 0.0) { d += 360.0; }
    return d;
}

double longPath(double shortPathDeg)
{
    return wrap360(shortPathDeg + 180.0);
}

double longPathDistanceKm(double shortPathKm)
{
    constexpr double kEarthCircumferenceKm = 2.0 * M_PI * 6371.0;
    return std::max(0.0, kEarthCircumferenceKm - shortPathKm);
}

Move plan(double fromDeg, double toDeg, Stop stop)
{
    Move m;
    m.targetDeg = wrap360(toDeg);
    const double from = wrap360(fromDeg);

    if (stop == Stop::None) {
        // Free to rotate: take whichever direction is shorter. The
        // signed difference wrapped into -180..180 IS that direction.
        double d = m.targetDeg - from;
        if (d > 180.0)  { d -= 360.0; }
        if (d < -180.0) { d += 360.0; }
        m.travelDeg = d;
        m.reachable = true;
        return m;
    }

    // With a stop, the rotor lives on one continuous span and cannot
    // cross the boundary. Rotate both angles so the stop sits at 0, and
    // then the only route is the plain difference -- no wrapping,
    // because wrapping is exactly what the stop forbids.
    const double stopAt = (stop == Stop::North) ? 0.0 : 180.0;
    const double a = wrap360(from        - stopAt);
    const double b = wrap360(m.targetDeg - stopAt);

    m.travelDeg = b - a;
    m.reachable = true;

    // A stop does not usually forbid a heading, only a route -- a rotor
    // with a north stop still reaches 350 and 10 degrees, just never
    // directly between them. So nothing here is unreachable; it is only
    // sometimes a very long way round, and the travel says which.
    if (std::abs(m.travelDeg) > 270.0) {
        m.note = QStringLiteral(
            "The rotor cannot turn through its end stop, so this is the "
            "long way round.");
    }
    return m;
}

QString advice(const Move& m)
{
    if (!m.reachable) { return m.note; }

    const double t = std::abs(m.travelDeg);
    if (t < 2.0) {
        return QStringLiteral("Already pointing there.");
    }

    QString s = QStringLiteral("%1° %2")
                    .arg(t, 0, 'f', 0)
                    .arg(m.travelDeg >= 0.0 ? QStringLiteral("clockwise")
                                            : QStringLiteral("anticlockwise"));
    if (!m.note.isEmpty()) { s += QStringLiteral(" — ") + m.note; }
    else if (t > 180.0) {
        s += QStringLiteral(" — a long move; give it time before you "
                            "call.");
    }
    return s;
}

} // namespace Contestprogramm::BeamHeading
