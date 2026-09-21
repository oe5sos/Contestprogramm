#pragma once

#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace Contestprogramm {

class ContestDatabase;

// A transverter between the CAT rig and the antenna: the rig shows its
// IF (144.300 MHz), the signal is on 1296.300 MHz. rigctld can only
// report the IF, so without this the program would log every 1296 QSO
// on 144 (or 432) and route the rotor by the wrong band. Operator,
// 2026-09-21: "es wird einen transverter geben, welchen weiß ich noch
// nicht" -- hence a generic IF band → RF band pair with the offset
// derived from the band bases (1296 − 144 = 1152 MHz, the LO nearly
// every 23 cm transverter uses), editable for the odd one.
//
// `enabled` is the operator's switch (a checkbox in the top row): with
// a 432 IF the rig's 432.300 MHz means 432 or 1296 depending on what
// is plugged in, and only the operator knows. Persisted as settings-
// table keys (transverter_*) -- see load()/save().
struct TransverterSetup {
    bool enabled = false;
    QString ifBand; // what the rig shows, e.g. "144"
    QString rfBand; // what is on the antenna, e.g. "1296"
    qint64 offsetHz = 0; // RF = IF + offset

    bool configured() const { return !ifBand.isEmpty() && !rfBand.isEmpty() && offsetHz != 0; }
    bool active() const { return enabled && configured(); }

    // Rig frequency → the frequency on the air, when active and the rig
    // sits in the IF band; unchanged otherwise.
    qint64 rfFrequencyHz(qint64 rigHz) const;
    // The frequency on the air → what to send the rig, when active and
    // the target lies in the RF band; unchanged otherwise.
    qint64 rigFrequencyHz(qint64 rfHz) const;

    // "144 → 1296 (+1152 MHz)"; empty when not configured.
    QString describe() const;

    // The usual LO: the RF band's base minus the IF band's base; 0 for
    // an unknown pair.
    static qint64 defaultOffsetHz(const QString& ifBand, const QString& rfBand);

    static TransverterSetup load(const ContestDatabase& database);
    void save(ContestDatabase& database) const;
};

} // namespace Contestprogramm
