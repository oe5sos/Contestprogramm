#pragma once

// =================================================================
// src/core/RotorModels.h  (Contestprogramm)
// =================================================================
//
// Contestprogramm-original -- not a Thetis port, so none of the GPL
// attribution machinery in Longpath/NereusSDR's CLAUDE.md applies
// here. Ported verbatim (model list, notes and baud-rate list
// unchanged) from Longpath/NereusSDR's own src/core/RotorModels.h,
// which is itself NereusSDR-original -- see that file's own header.
// Only the namespace changed, `Longpath` -> `Contestprogramm`.
//
// The rotator controllers an operator is likely to own, by name, with
// the Hamlib model number each one needs.
//
// Hamlib supports around sixty. Offering all sixty is not help — most
// of the list is telescope mounts and controllers nobody in this hobby
// has seen, and a long list of unfamiliar names is harder to answer
// than a short one. So this is curated, ordered by how common the
// controller is, and there is a free field beside it for the numbers
// that are not here.
//
// Two controllers worth naming because they are asked about and both
// work through an emulation rather than a driver of their own:
//
//   ERC (Easy-Rotor-Control, DF9GR) — has its own Hamlib driver, 404.
//       It also emulates GS-232A/B and DCU-1, so if 404 misbehaves the
//       emulations are a fallback rather than a dead end.
//
//   ARCO (microHAM) — no Hamlib driver of its own, but it speaks
//       Yaesu GS-232A, DCU-1/Rotor-EZ and SPID over LAN, USB and
//       RS-232. GS-232A is the one to pick.
//
// Model numbers verified against the Hamlib 4.7.1 rotator list
// (github.com/Hamlib/Hamlib/wiki/Supported-Rotators, read 2026-08-07
// in NereusSDR/Longpath).
//
// Contestprogramm's own default is Yaesu GS-232A (601) -- the
// operator confirmed the contest station's rotor will be Yaesu
// (2026-09-11) -- see RotorModel::hamlibId's use as
// ContestSettings::rotor1HamlibModel / rotor2HamlibModel's default.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-09-11 — Ported from Longpath/NereusSDR's RotorModels.h for
//                 Contestprogramm (operator confirmed the station's
//                 rotor will be Yaesu, so this list's existing
//                 GS-232A-first ordering already matches). AI-assisted
//                 via Anthropic Claude Code, operator Ralph Martin
//                 Fischer.
// =================================================================

#include <QString>
#include <QVector>

namespace Contestprogramm {

struct RotorModel {
    int     hamlibId;
    QString name;
    // Shown under the picker. Says what the entry actually is, because
    // "GS-232A" means nothing to someone holding an ARCO box.
    QString note;
};

// Ordered by how likely an operator is to own one, not alphabetically.
inline QVector<RotorModel> commonRotorModels()
{
    return {
        {601,  QStringLiteral("Yaesu GS-232A"),
               QStringLiteral("Also the setting for microHAM ARCO and "
                              "ARCO Junior, and for ERC in GS-232A mode. "
                              "For an ARCO on the network, enter its "
                              "address and GS-232A port as the serial "
                              "port, e.g. 192.168.1.50:4001 — Hamlib "
                              "then talks TCP instead of a serial line")},
        {603,  QStringLiteral("Yaesu GS-232B"),
               QStringLiteral("G-800DXA, G-1000DXA, G-2800DXA with the "
                              "B-series controller")},
        {404,  QStringLiteral("ERC — Easy-Rotor-Control (DF9GR)"),
               QStringLiteral("ERC Mini DX, ERC-DUO, ERC-M. Its own "
                              "Hamlib driver; the GS-232 entries above "
                              "also work if this one gives trouble")},
        {403,  QStringLiteral("Hy-Gain DCU-1 / DCU-1X"),
               QStringLiteral("Also ARCO in DCU-1 mode, and Idiom Press "
                              "Rotor-EZ boards")},
        {401,  QStringLiteral("Idiom Press Rotor-EZ"),
               QStringLiteral("Hy-Gain Ham-IV / T2X with a Rotor-EZ board")},
        {405,  QStringLiteral("Green Heron RT-21"),
               QStringLiteral("RT-21, RT-21D, RT-990")},
        {901,  QStringLiteral("SPID Rot2Prog"),
               QStringLiteral("Azimuth and elevation")},
        {902,  QStringLiteral("SPID Rot1Prog"),
               QStringLiteral("Azimuth only. Also ARCO in SPID mode")},
        {903,  QStringLiteral("SPID MD-01 / MD-02"),
               QStringLiteral("In ROT2 mode")},
        {1001, QStringLiteral("M2 RC2800"),
               QStringLiteral("M2 Antenna Systems / Orion")},
        {1701, QStringLiteral("Prosistel D azimuth"),
               QStringLiteral("Combi-Track is model 1703")},
        {1101, QStringLiteral("EA4TX ARS RCI azimuth + elevation"),
               QStringLiteral("Azimuth only is model 1102")},
        {606,  QStringLiteral("Yaesu / Kenpro GS-232"),
               QStringLiteral("The original, neither A nor B")},
        {605,  QStringLiteral("Yaesu / Kenpro GS-23"),
               QStringLiteral("Also GS-232 on older controllers")},
        {602,  QStringLiteral("GS-232 generic"),
               QStringLiteral("Home-built and Arduino controllers "
                              "speaking GS-232, e.g. K3NG")},
        {1501, QStringLiteral("Ether6 (network)"),
               QStringLiteral("Connects over Ethernet rather than a "
                              "serial port")},
        {1,    QStringLiteral("Hamlib dummy"),
               QStringLiteral("A rotator that does not exist. For "
                              "checking the link works without a mast "
                              "turning")},
    };
}

// Baud rates worth offering. Most controllers ship at 9600; the ERC
// family and some GS-232 boxes are configurable.
inline QVector<int> commonRotorBauds()
{
    return {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
}

} // namespace Contestprogramm
