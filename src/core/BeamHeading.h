#pragma once

// =================================================================
// src/core/BeamHeading.h  (Contestprogramm)
// =================================================================
//
// Contestprogramm-original. Ported faithfully (no maths changed) from
// Longpath/NereusSDR's src/core/BeamHeading.h -- itself NereusSDR-
// original, not a Thetis port, so no GPL attribution chain applies
// here beyond this porting note.
//
// Which way to point, and how far the rotor has to travel to get there.
//
// -- Short path and long path -----------------------------------------
//
// Every bearing computed from Maidenhead grids (core/Maidenhead.h) is
// the short path -- the great circle the signal takes if nothing is in
// the way. On VHF/UHF, tropo ducting and scatter openings sometimes
// favour the long path (short path + 180 degrees); longPath() exists so
// nobody has to add 180 in their head mid-pileup.
//
// -- End stops, which are the part people get wrong ---------------------
//
// A rotor is not a compass. Most have a mechanical stop somewhere --
// commonly at north or at south -- and cannot pass through it. Asking a
// north-stop rotor to go from 350 to 10 degrees is a twenty-degree move
// if it can wrap and a three-hundred-and-forty-degree move if it
// cannot.
//
// plan() computes the travel against the given stop and reports how far
// the rotor will actually turn. Stop::None (no known mechanical limit)
// is Contestprogramm's current default everywhere this is called --
// the operator's actual stop positions are not yet configured anywhere
// in ContestSettings, so nothing here invents one.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-09-09 — Created in C++20/Qt6, ported faithfully from
//                 Longpath/NereusSDR's BeamHeading.h/.cpp. Namespace
//                 changed Longpath::BeamHeading -> Contestprogramm::
//                 BeamHeading; no maths changed. AI-assisted via
//                 Anthropic Claude Code, operator Ralph Martin Fischer.
// =================================================================
//   2026-09-22 — longPathDistanceKm() ergänzt (Contestprogramm-eigen,
//                 nicht aus dem Vorbild portiert).

#include <QString>

namespace Contestprogramm::BeamHeading {

// Normalise any angle to 0..360.
double wrap360(double deg);

// The long path for a given short-path bearing.
double longPath(double shortPathDeg);

// Wie weit es auf dem langen Weg ist: einmal um die Erde, minus dem
// kurzen Weg. Der Umfang kommt aus demselben Erdradius, mit dem das
// ganze Programm rechnet (6371 km, siehe core/Maidenhead.cpp) --
// 40 030 km. Eine Entfernung, die größer als der halbe Umfang ist
// (rechnerisch möglich, in echten Daten nicht), gibt 0 statt eines
// negativen Wertes.
double longPathDistanceKm(double shortPathKm);

// Where a rotor cannot turn through.
enum class Stop {
    None  = 0,   // continuous rotation, 0 and 360 are the same place
    North = 1,   // cannot pass 0 degrees -- the common case
    South = 2,   // cannot pass 180 degrees
};

struct Move {
    bool    reachable{false};
    double  targetDeg{0.0};
    // Degrees the rotor will actually turn. Signed: negative is
    // counter-clockwise. This is the number that says whether a move is
    // twenty degrees or three hundred and forty.
    double  travelDeg{0.0};
    QString note;      // why it is unreachable, or what is unusual
};

// Plan a move from `fromDeg` to `toDeg` for a rotor with `stop`.
//
// With Stop::None the shorter of the two directions wins. With a stop,
// the rotor is confined to one continuous span and there is only one
// route -- which may be the long way round, and the returned travel
// says so rather than hiding it.
Move plan(double fromDeg, double toDeg, Stop stop);

// A sentence for the operator, naming the travel and warning when a
// move is a long one. Empty when there is nothing worth saying.
QString advice(const Move& m);

} // namespace Contestprogramm::BeamHeading
