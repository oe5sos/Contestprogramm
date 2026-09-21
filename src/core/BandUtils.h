#pragma once

#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace Contestprogramm {

// IARU Region 1 amateur allocations for the bands a VHF/UHF/microwave
// contest can carry, widened slightly at the edges to tolerate a rig
// reporting a few kHz outside the exact allocation while sweeping the
// VFO. Returns an empty string when `hz` falls in none.
//
// Extracted from ui/MainWindow.cpp (was a private static there) so
// ChatFeedModel's multiplier-boost scoring (see core/ChatImportanceScorer.h)
// can derive a SpotCandidate's band from its freqHz without depending on
// the UI layer. Since 2026-09-21 also 28/50/70 MHz and the microwave
// bands up to 10 GHz -- the transverter setup (core/Transverter.h)
// converts between an IF band the rig shows and the band on the
// antenna, and both ends need a range and a base frequency.
QString bandLabelForFrequencyHz(qint64 hz);

// The bands bandLabelForFrequencyHz() knows, lowest first.
QStringList knownBands();

// The band's range as this program recognises it (false for an unknown
// label), and the band's base -- its lower edge as contest bands are
// named: 144 000 000 for "144", 1 296 000 000 for "1296".
bool bandRangeHz(const QString& band, qint64& lowHz, qint64& highHz);
qint64 bandBaseHz(const QString& band);

} // namespace Contestprogramm
