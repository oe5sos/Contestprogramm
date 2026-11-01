#pragma once

// =================================================================
// src/core/RigModels.h  (Contestprogramm)
// =================================================================
//
// The transceivers an operator is likely to put behind rigctld, by
// name, with the Hamlib model number each one needs -- the rig-side
// counterpart to core/RotorModels.h.
//
// Two differences to the rotor list, both forced by the subject:
//
//   * Hamlib knows around three hundred radios, not sixty, and the
//     short curated list below cannot stand in for that. So the full
//     list is read from the installed Hamlib at runtime
//     (rigModelsFromHamlib(), "rigctl -l") and offered BELOW the
//     curated ones. The operator sees the handful that matter first
//     and still has every radio Hamlib supports.
//
//   * The numbers are NOT written down from the web. They move between
//     Hamlib releases -- three sources consulted on 2026-10-15 gave
//     three different answers for the TS-590 -- so the curated list
//     carries them only as a fallback for a machine with no Hamlib
//     installed, and anything read from the local rigctl wins.
//
// Every number below is checked against the installed Hamlib at
// startup (see the picker in ui/SettingsDialog.cpp): an entry this
// Hamlib does not know is dropped rather than offered. Read from
// Hamlib 4.7.2 on the contest machine, 2026-10-15.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-10-16 — Created for RigctldProcess, so the TS-590 the
//                 station is getting can be picked by name.
//                 AI-assisted via Anthropic Claude Code, operator
//                 Ralph Martin Fischer.
// =================================================================

#include <QString>
#include <QVector>

namespace Contestprogramm {

struct RigModel {
    int     hamlibId = 0;
    // "Kenwood TS-590SG" -- für die kuratierten Einträge GENAU der
    // Name, den das installierte Hamlib für diese Nummer führt
    // (Hersteller + Modell), bei zwei Geräten in einer Zeile der des
    // ersten vor dem " / ". tests/test_rigctld_supervision.cpp prüft
    // das: sonst steht irgendwann ein vertrauter Name vor einer
    // Nummer, die ein ganz anderes Gerät meint -- genau so war FT-710
    // kurzzeitig 1042 statt 1049 (2026-10-16).
    QString name;
    // Shown under the picker. Says what the entry actually is, or what
    // to watch out for. Empty for the ones read from Hamlib itself.
    QString note;
};

// The short list: the radios usually found driving a VHF/UHF
// transverter, so the common case is two clicks away. NOT a decision
// about which radio this station will use -- that is open (Martin,
// 2026-10-16: "bin mir nicht sicher, welches contestradio ich mit
// transverter benutzen werde"), and every radio the installed Hamlib
// supports is in the picker below these anyway.
//
// Also the fallback when Hamlib is not installed yet.
inline QVector<RigModel> commonRigModels()
{
    return {
        {2029, QStringLiteral("Elecraft K3"),
               QStringLiteral("Hat eigene Transverter-Anschlüsse und einen Transverter-Versatz "
                              "im Gerät -- der verbreitetste ZF-Treiber auf UKW")},
        {2043, QStringLiteral("Elecraft K3S"),
               QStringLiteral("Eigene Hamlib-Nummer, nicht die des K3")},
        {2047, QStringLiteral("Elecraft K4"),
               QStringLiteral("K4, K4D und K4HD")},
        {2037, QStringLiteral("Kenwood TS-590SG"),
               QStringLiteral("Baudrate muss zu Menü Nr. 68 passen, PTT dort auf CAT")},
        {2031, QStringLiteral("Kenwood TS-590S"),
               QStringLiteral("Die Fassung vor der SG. Baudrate in Menü Nr. 62")},
        {2014, QStringLiteral("Kenwood TS-2000"),
               QStringLiteral("Hat 2 m und 70 cm schon eingebaut -- dann ohne Transverter")},
        {3073, QStringLiteral("Icom IC-7300"),
               QStringLiteral("Über die eingebaute USB-Schnittstelle")},
        {3085, QStringLiteral("Icom IC-705"),
               QStringLiteral("Beliebt als kleiner Transverter-Treiber")},
        {1035, QStringLiteral("Yaesu FT-991 / FT-991A"),
               QStringLiteral("Hat 2 m und 70 cm schon eingebaut")},
        {1049, QStringLiteral("Yaesu FT-710"),
               QStringLiteral("Kurzwelle und 6 m, als ZF hinter dem Transverter")},
        {2,    QStringLiteral("Hamlib NET rigctl"),
               QStringLiteral("Ein rigctld, das anderswo läuft -- dann hier keinen Anschluss "
                              "eintragen, nur Host und Port")},
        {1,    QStringLiteral("Hamlib Dummy"),
               QStringLiteral("Ein Funkgerät, das es nicht gibt. Zum Prüfen, ob die Strecke "
                              "steht, ohne dass etwas sendet")},
    };
}

// Baud rates worth offering. What matters is not which one is "right"
// but that it matches the radio's own menu -- every one of these is a
// factory default on some radio.
inline QVector<int> commonRigBauds()
{
    return {4800, 9600, 19200, 38400, 57600, 115200};
}

// Every radio the INSTALLED Hamlib supports, read from "rigctl -l".
// Empty when rigctl cannot be found or does not answer -- the caller
// then has commonRigModels() and nothing is lost but the long tail.
//
// Deliberately a function that shells out rather than a table in the
// source: a table would be wrong the moment Hamlib is updated, and
// wrong in the quiet way where the radio simply does not answer.
QVector<RigModel> rigModelsFromHamlib();

} // namespace Contestprogramm
