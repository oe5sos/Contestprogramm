#include "core/terrain/KnownSites.h"

namespace Contestprogramm {

const QVector<KnownSite>& knownSites()
{
    // Values as scouted/verified for this task (2026-09-14) -- do not
    // hand-edit these without re-checking against a real source, see
    // this list's own test (test_terrain.cpp's
    // knownSitesReturnsTheNineSeededEntries()), which exists specifically
    // to catch an accidental future edit of these numbers.
    static const QVector<KnownSite> sites = {
        {QStringLiteral("Feuerkogelhaus (Ebensee)"), 47.815814, 13.721506, 1587.0},
        {QStringLiteral("Parkplatz Loser (Loserhütte)"), 47.656225, 13.783518, 1495.0},
        {QStringLiteral("Parkplatz Nebelstein"), 48.670667, 14.774931, 961.0},
        {QStringLiteral("Parkplatz Ameisberg (Atzesberg/Rohrbach)"), 48.554431, 13.832186, 930.0},
        {QStringLiteral("Parkplatz Jauerling"), 48.339768, 15.338003, 965.0},
        {QStringLiteral("Gasthaus Schwarz am Hongar (Kasten 32, 4861 Aurach am Hongar)"), 47.921518, 13.685333, 877.0},
        {QStringLiteral("Grünberg Gasthaus"), 47.898289, 13.821580, 980.0},
        {QStringLiteral("Grünberg, 80 m östlich vom Haus"), 47.898289, 13.822653, 969.0},
        {QStringLiteral("Hinteralm / Traisner Hütte (JN77TX, Lilienfeld)"), 47.971922, 15.610548, 1293.0},
        {QStringLiteral("Parkplatz Dreisesselalm (Dreisesselberg, Böhmerwald)"), 48.793062, 13.772974, 917.0},
    };
    return sites;
}

} // namespace Contestprogramm
