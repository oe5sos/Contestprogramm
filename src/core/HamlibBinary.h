#pragma once

// =================================================================
// src/core/HamlibBinary.h  (Contestprogramm)
// =================================================================
//
// Finding a Hamlib program on this machine -- rotctld, rigctld,
// rigctl. Hoisted out of RotctldProcess::findBinary() when
// RigctldProcess arrived and needed exactly the same search; the
// search itself is unchanged.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-10-16 — Hoisted from core/RotctldProcess.cpp so rigctld and
//                 rigctl can be found the same way. AI-assisted via
//                 Anthropic Claude Code, operator Ralph Martin
//                 Fischer.
// =================================================================

#include <QString>

namespace Contestprogramm {

// Where `name` lives, or empty if it cannot be found.
//
// PATH alone is not enough: a GUI application launched from Finder
// inherits a PATH without /opt/homebrew/bin, so Hamlib installed with
// brew is invisible to it while being perfectly present in the
// operator's terminal. That discrepancy would read as "the software
// cannot find something I can see", which is worse than a plain
// absence -- so the places package managers actually use are named
// explicitly after PATH has had its say.
QString findHamlibBinary(const QString& name);

} // namespace Contestprogramm
