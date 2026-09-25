#include "data/ReadinessCheck.h"

#include "core/Maidenhead.h"

#include <cmath>

namespace Contestprogramm {

namespace {

const char* const kGroupStation = "Station";
const char* const kGroupContest = "Contest";
const char* const kGroupLinks = "Verbindungen";
const char* const kGroupData = "Daten";

QString stamp(const QDateTime& utc)
{
    static const char* const kDays[] = {"Mo", "Di", "Mi", "Do", "Fr", "Sa", "So"};
    return QStringLiteral("%1 %2 UTC")
        .arg(QLatin1String(kDays[utc.date().dayOfWeek() - 1]), utc.toString(QStringLiteral("dd.MM. HH:mm")));
}

QString linkText(LinkState state)
{
    switch (state) {
    case LinkState::NotConfigured: return QStringLiteral("nicht eingerichtet");
    case LinkState::Connecting: return QStringLiteral("verbindet…");
    case LinkState::Connected: return QStringLiteral("verbunden");
    case LinkState::Disconnected: return QStringLiteral("getrennt");
    }
    return QString();
}

} // namespace

QString describeSpan(qint64 seconds)
{
    const qint64 s = std::llabs(seconds);
    const qint64 days = s / 86400;
    const qint64 hours = (s % 86400) / 3600;
    const qint64 minutes = (s % 3600) / 60;
    if (days > 0) {
        return QStringLiteral("%1 T %2 h").arg(days).arg(hours);
    }
    if (hours > 0) {
        return QStringLiteral("%1 h %2 min").arg(hours).arg(minutes);
    }
    return QStringLiteral("%1 min").arg(minutes);
}

QString ReadinessResult::summaryText() const
{
    QStringList parts;
    if (errors > 0) {
        parts << QStringLiteral("%1 Fehler").arg(errors);
    }
    if (warnings > 0) {
        parts << QStringLiteral("%1 %2").arg(warnings).arg(warnings == 1 ? QStringLiteral("Warnung") : QStringLiteral("Warnungen"));
    }
    if (hints > 0) {
        parts << QStringLiteral("%1 %2").arg(hints).arg(hints == 1 ? QStringLiteral("Hinweis") : QStringLiteral("Hinweise"));
    }
    const QString counts = parts.isEmpty() ? QStringLiteral("alles in Ordnung") : parts.join(QStringLiteral(", "));
    return ready() ? QStringLiteral("Bereit — %1.").arg(counts) : QStringLiteral("Nicht bereit — %1.").arg(counts);
}

ReadinessResult checkReadiness(const ReadinessContext& ctx)
{
    ReadinessResult result;
    const auto add = [&result](ReadinessItem::Level level, const char* group, const QString& title,
                               const QString& detail, const QString& code) {
        result.items.append({level, QString::fromLatin1(group), title, detail, code});
        switch (level) {
        case ReadinessItem::Level::Error: ++result.errors; break;
        case ReadinessItem::Level::Warning: ++result.warnings; break;
        case ReadinessItem::Level::Hint: ++result.hints; break;
        case ReadinessItem::Level::Ok: break;
        }
    };
    using Level = ReadinessItem::Level;

    // ---- Station -------------------------------------------------------
    const QString call = ctx.ownCallsign.trimmed().toUpper();
    if (call.isEmpty()) {
        add(Level::Error, kGroupStation, QStringLiteral("Rufzeichen"),
            QStringLiteral("Kein eigenes Rufzeichen eingetragen (Einstellungen › Station)."), QStringLiteral("own_call"));
    } else {
        add(Level::Ok, kGroupStation, QStringLiteral("Rufzeichen"), call, QStringLiteral("own_call"));
    }

    const QString grid = ctx.ownGrid.trimmed().toUpper();
    if (!isValidGridSquare(grid)) {
        add(Level::Error, kGroupStation, QStringLiteral("Locator"),
            grid.isEmpty() ? QStringLiteral("Kein eigener Locator — ohne ihn keine Entfernungen, keine Punkte.")
                           : QStringLiteral("„%1“ ist kein gültiger Locator.").arg(grid),
            QStringLiteral("own_grid"));
    } else if (!isFullLocator(grid)) {
        // IARU R1 GC 2023, 1.9.1: der vollstaendige, sechsstellige
        // Locator gehoert in den Austausch; mit "JN67" gibt es weder
        // brauchbare Entfernungen noch ein gueltiges PWWLo.
        add(Level::Error, kGroupStation, QStringLiteral("Locator"),
            QStringLiteral("„%1“ ist nur vierstellig — der Contest verlangt den sechsstelligen Locator.").arg(grid),
            QStringLiteral("own_grid"));
    } else if (ctx.useExactOwnLocation) {
        if (ctx.ownExactLatitude == 0.0 && ctx.ownExactLongitude == 0.0) {
            add(Level::Warning, kGroupStation, QStringLiteral("Locator"),
                QStringLiteral("%1 — exakter Standort aktiviert, aber ohne Koordinaten.").arg(grid),
                QStringLiteral("own_grid"));
        } else {
            const QString exactGrid = gridSquareFromLatLon(ctx.ownExactLatitude, ctx.ownExactLongitude).toUpper();
            if (exactGrid.left(6) != grid.left(6)) {
                add(Level::Warning, kGroupStation, QStringLiteral("Locator"),
                    QStringLiteral("%1 im Log, aber der exakte Standort (%2 / %3) liegt in %4.")
                        .arg(grid)
                        .arg(ctx.ownExactLatitude, 0, 'f', 5)
                        .arg(ctx.ownExactLongitude, 0, 'f', 5)
                        .arg(exactGrid),
                    QStringLiteral("own_grid"));
            } else {
                add(Level::Ok, kGroupStation, QStringLiteral("Locator"),
                    QStringLiteral("%1, exakt %2 / %3").arg(grid).arg(ctx.ownExactLatitude, 0, 'f', 5).arg(ctx.ownExactLongitude, 0, 'f', 5),
                    QStringLiteral("own_grid"));
            }
        }
    } else {
        add(Level::Ok, kGroupStation, QStringLiteral("Locator"), grid, QStringLiteral("own_grid"));
    }

    if (ctx.ownElevationM <= 0.0) {
        add(Level::Hint, kGroupStation, QStringLiteral("Standorthöhe"),
            QStringLiteral("Keine Höhe eingetragen — Horizont und Abschattung rechnen mit 0 m."),
            QStringLiteral("own_elevation"));
    } else {
        add(Level::Ok, kGroupStation, QStringLiteral("Standorthöhe"),
            QStringLiteral("%1 m, Antenne %2 m darüber").arg(qRound(ctx.ownElevationM)).arg(qRound(ctx.antennaHeightM)),
            QStringLiteral("own_elevation"));
    }

    // The clock: every QSO time comes from it, and the adjudication
    // cross-checks times between logs.
    if (!ctx.clockChecked) {
        add(Level::Hint, kGroupStation, QStringLiteral("Uhrzeit"), QStringLiteral("Wird gegen einen Zeitserver geprüft…"),
            QStringLiteral("clock"));
    } else if (!ctx.clockReachable) {
        add(Level::Hint, kGroupStation, QStringLiteral("Uhrzeit"),
            QStringLiteral("Nicht prüfbar (kein Internet) — Systemuhr vor dem Start von Hand vergleichen."),
            QStringLiteral("clock"));
    } else {
        const qint64 off = ctx.clockOffsetSecs;
        const QString direction = off > 0 ? QStringLiteral("vor") : QStringLiteral("nach");
        if (std::llabs(off) <= 5) {
            add(Level::Ok, kGroupStation, QStringLiteral("Uhrzeit"),
                QStringLiteral("Stimmt (%1%2 s gegen %3)").arg(off > 0 ? QStringLiteral("+") : QString()).arg(off).arg(ctx.clockSource),
                QStringLiteral("clock"));
        } else if (std::llabs(off) <= 60) {
            add(Level::Warning, kGroupStation, QStringLiteral("Uhrzeit"),
                QStringLiteral("Geht %1 s %2 (gegen %3) — Systemuhr stellen, jede QSO-Zeit hängt daran.")
                    .arg(std::llabs(off)).arg(direction, ctx.clockSource),
                QStringLiteral("clock"));
        } else {
            add(Level::Error, kGroupStation, QStringLiteral("Uhrzeit"),
                QStringLiteral("Geht %1 %2 (gegen %3) — Systemuhr stellen, sonst stimmt keine QSO-Zeit.")
                    .arg(describeSpan(off), direction, ctx.clockSource),
                QStringLiteral("clock"));
        }
    }

    // ---- Contest -------------------------------------------------------
    if (!ctx.contestFound) {
        add(Level::Error, kGroupContest, QStringLiteral("Contest"),
            QStringLiteral("Kein aktiver Contest — Datei › Contest wählen."), QStringLiteral("contest"));
    } else {
        add(Level::Ok, kGroupContest, QStringLiteral("Contest"),
            ctx.contestBands.isEmpty() ? ctx.contestName
                                       : QStringLiteral("%1 — %2").arg(ctx.contestName, ctx.contestBands.join(QStringLiteral(" / "))),
            QStringLiteral("contest"));

        if (!ctx.window.isValid()) {
            add(Level::Hint, kGroupContest, QStringLiteral("Zeitfenster"),
                QStringLiteral("Kein Zeitplan hinterlegt — Countdown und Zeitprüfung bleiben aus."),
                QStringLiteral("window"));
        } else if (ctx.nowUtc < ctx.window.startUtc) {
            add(Level::Ok, kGroupContest, QStringLiteral("Zeitfenster"),
                QStringLiteral("Start in %1 — %2").arg(describeSpan(ctx.nowUtc.secsTo(ctx.window.startUtc)), ctx.window.describe()),
                QStringLiteral("window"));
        } else if (ctx.nowUtc < ctx.window.endUtc) {
            add(Level::Ok, kGroupContest, QStringLiteral("Zeitfenster"),
                QStringLiteral("Läuft, noch %1 — bis %2").arg(describeSpan(ctx.nowUtc.secsTo(ctx.window.endUtc)), stamp(ctx.window.endUtc)),
                QStringLiteral("window"));
        } else {
            add(Level::Warning, kGroupContest, QStringLiteral("Zeitfenster"),
                QStringLiteral("Vorbei seit %1 (%2) — ist der richtige Contest aktiv?")
                    .arg(describeSpan(ctx.window.endUtc.secsTo(ctx.nowUtc)), ctx.window.describe()),
                QStringLiteral("window"));
        }

        const bool beforeStart = ctx.window.isValid() && ctx.nowUtc < ctx.window.startUtc;
        // "nächste Nummer 001" / "nächste Nummer 432: 012 · 1296: 003"
        QStringList serials;
        for (const auto& entry : ctx.nextSerials) {
            const QString number = QStringLiteral("%1").arg(entry.second, 3, 10, QLatin1Char('0'));
            serials << (entry.first.isEmpty() ? number : QStringLiteral("%1: %2").arg(entry.first, number));
        }
        const QString nextNumber = serials.isEmpty() ? QStringLiteral("%1").arg(ctx.qsoCount + 1, 3, 10, QLatin1Char('0'))
                                                     : serials.join(QStringLiteral(" · "));
        if (ctx.qsoCount > 0 && beforeStart) {
            add(Level::Warning, kGroupContest, QStringLiteral("Log"),
                QStringLiteral("%1 QSOs im Log vor dem Start — Testeinträge? Datei › Neues Log beginnen (altes bleibt im Archiv), dann beginnt die Nummer bei 001.")
                    .arg(ctx.qsoCount),
                QStringLiteral("log"));
        } else if (ctx.qsoCount == 0) {
            add(Level::Ok, kGroupContest, QStringLiteral("Log"), QStringLiteral("Leer, nächste Nummer 001"),
                QStringLiteral("log"));
        } else {
            add(Level::Ok, kGroupContest, QStringLiteral("Log"),
                QStringLiteral("%1 QSOs, nächste Nummer %2").arg(ctx.qsoCount).arg(nextNumber), QStringLiteral("log"));
        }
    }

    // The transverter: a contest band above 1 GHz is reached through
    // one on most stations, and the switch decides what the rig's IF
    // means.
    {
        bool microwaveBand = false;
        for (const QString& band : ctx.contestBands) {
            microwaveBand = microwaveBand || band.toLongLong() >= 1000;
        }
        if (ctx.transverterConfigured && ctx.transverterActive) {
            add(Level::Ok, kGroupContest, QStringLiteral("Transverter"),
                QStringLiteral("%1 — an, das Funkgerät zeigt die ZF").arg(ctx.transverterText), QStringLiteral("transverter"));
        } else if (ctx.transverterConfigured) {
            add(Level::Hint, kGroupContest, QStringLiteral("Transverter"),
                QStringLiteral("%1 eingerichtet, aber aus — Schalter in der Kopfzeile, sobald er dran ist.")
                    .arg(ctx.transverterText),
                QStringLiteral("transverter"));
        } else if (microwaveBand) {
            add(Level::Hint, kGroupContest, QStringLiteral("Transverter"),
                QStringLiteral("Keiner eingerichtet — ein Band über 1 GHz landet sonst unter der ZF des Funkgeräts (Datei › Transverter…)."),
                QStringLiteral("transverter"));
        }
    }

    // ---- Links ---------------------------------------------------------
    if (ctx.catTarget.isEmpty()) {
        add(Level::Hint, kGroupLinks, QStringLiteral("CAT"),
            QStringLiteral("Kein rigctld eingerichtet — Frequenz und Mode kommen von Hand."), QStringLiteral("cat"));
    } else if (ctx.cat == LinkState::Connected) {
        add(Level::Ok, kGroupLinks, QStringLiteral("CAT"), QStringLiteral("rigctld %1 verbunden").arg(ctx.catTarget),
            QStringLiteral("cat"));
    } else {
        add(Level::Warning, kGroupLinks, QStringLiteral("CAT"),
            QStringLiteral("rigctld %1: %2 — läuft rigctld? Band und Mode müssten sonst von Hand stimmen.")
                .arg(ctx.catTarget, linkText(ctx.cat)),
            QStringLiteral("cat"));
    }

    const auto rotor = [&](const ReadinessContext::Rotor& r, int slot) {
        const QString title = QStringLiteral("Rotor %1%2").arg(slot).arg(r.label.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(r.label));
        const QString code = QStringLiteral("rotor%1").arg(slot);
        if (!r.enabled) {
            add(Level::Hint, kGroupLinks, title, QStringLiteral("Abgeschaltet."), code);
            return;
        }
        if (r.link == LinkState::Connected) {
            add(Level::Ok, kGroupLinks, title, QStringLiteral("rotctld %1 verbunden").arg(r.target), code);
            return;
        }
        if (!r.rotctldError.isEmpty()) {
            add(Level::Error, kGroupLinks, title,
                QStringLiteral("rotctld %1 konnte nicht gestartet werden: %2").arg(r.target, r.rotctldError.section(QLatin1Char('\n'), 0, 0)),
                code);
            return;
        }
        add(Level::Warning, kGroupLinks, title,
            QStringLiteral("rotctld %1: %2 — Rotorsteuerung eingeschaltet, rotctld gestartet?").arg(r.target, linkText(r.link)), code);
    };
    rotor(ctx.rotor1, 1);
    rotor(ctx.rotor2, 2);

    if (!ctx.on4kstConfigured) {
        add(Level::Hint, kGroupLinks, QStringLiteral("ON4KST"),
            QStringLiteral("Kein Zugang eingetragen — Chat und Spots bleiben aus."), QStringLiteral("on4kst"));
    } else if (ctx.on4kst == LinkState::Connected) {
        add(Level::Ok, kGroupLinks, QStringLiteral("ON4KST"), QStringLiteral("Angemeldet."), QStringLiteral("on4kst"));
    } else {
        add(Level::Warning, kGroupLinks, QStringLiteral("ON4KST"),
            QStringLiteral("%1 — Internet? Zugangsdaten?").arg(linkText(ctx.on4kst)), QStringLiteral("on4kst"));
    }

    if (ctx.clusterTarget.isEmpty()) {
        add(Level::Hint, kGroupLinks, QStringLiteral("DX-Cluster"), QStringLiteral("Keiner eingetragen."),
            QStringLiteral("cluster"));
    } else if (ctx.cluster == LinkState::Connected) {
        add(Level::Ok, kGroupLinks, QStringLiteral("DX-Cluster"), QStringLiteral("%1 verbunden").arg(ctx.clusterTarget),
            QStringLiteral("cluster"));
    } else {
        add(Level::Warning, kGroupLinks, QStringLiteral("DX-Cluster"),
            QStringLiteral("%1: %2").arg(ctx.clusterTarget, linkText(ctx.cluster)), QStringLiteral("cluster"));
    }

    // ---- Data ----------------------------------------------------------
    if (ctx.backupDirectory.isEmpty() || !ctx.backupDirectoryWritable) {
        add(Level::Error, kGroupData, QStringLiteral("Sicherung"),
            ctx.backupDirectory.isEmpty() ? QStringLiteral("Kein Sicherungsordner.")
                                          : QStringLiteral("Sicherungsordner nicht beschreibbar: %1").arg(ctx.backupDirectory),
            QStringLiteral("backup"));
    } else if (!ctx.lastBackupUtc.isValid()) {
        add(Level::Hint, kGroupData, QStringLiteral("Sicherung"),
            QStringLiteral("Noch keine Sicherung geschrieben (kommt jede Minute nach %1, sobald ein QSO im Log ist).").arg(ctx.backupDirectory),
            QStringLiteral("backup"));
    } else {
        add(Level::Ok, kGroupData, QStringLiteral("Sicherung"),
            QStringLiteral("Zuletzt %1, nach %2").arg(stamp(ctx.lastBackupUtc), ctx.backupDirectory), QStringLiteral("backup"));
    }

    // The second copy -- a stick that is unplugged is the one case
    // worth a warning before the start.
    if (!ctx.mirrorDirectory.isEmpty()) {
        if (ctx.mirrorWritable) {
            add(Level::Ok, kGroupData, QStringLiteral("Zweite Sicherung"), QStringLiteral("Auch nach %1").arg(ctx.mirrorDirectory),
                QStringLiteral("mirror"));
        } else {
            add(Level::Warning, kGroupData, QStringLiteral("Zweite Sicherung"),
                QStringLiteral("%1 nicht erreichbar — Stick eingesteckt? (Datei › Zweiter Sicherungsordner…)").arg(ctx.mirrorDirectory),
                QStringLiteral("mirror"));
        }
    } else {
        add(Level::Hint, kGroupData, QStringLiteral("Zweite Sicherung"),
            QStringLiteral("Keine — ein USB-Stick oder Cloud-Ordner unter Datei › Zweiter Sicherungsordner… überlebt den Laptop."),
            QStringLiteral("mirror"));
    }

    if (ctx.terrainLoadedForOwnLocation) {
        add(Level::Ok, kGroupData, QStringLiteral("Geländedaten"), QStringLiteral("Für den Standort geladen."),
            QStringLiteral("terrain"));
    } else {
        add(Level::Warning, kGroupData, QStringLiteral("Geländedaten"),
            QStringLiteral("Keine Höhendaten für den Standort — Horizont und Abschattung fehlen (Internet beim ersten Start nötig)."),
            QStringLiteral("terrain"));
    }

    if (ctx.importedLocators > 0) {
        add(Level::Ok, kGroupData, QStringLiteral("Locator-Liste"),
            QStringLiteral("%1 Rufzeichen mit Locator").arg(ctx.importedLocators), QStringLiteral("locators"));
    } else {
        add(Level::Hint, kGroupData, QStringLiteral("Locator-Liste"),
            QStringLiteral("Keine alten Logs übernommen — Locator-Vorschläge nur aus diesem Log (Datei › Locator aus alten Logs übernehmen)."),
            QStringLiteral("locators"));
    }

    // Die Länderliste nur dort, wo sie zählt: bei einem Contest ohne
    // getauschten Locator oder mit Länder-Multiplikator. Bei einem
    // UKW-Contest hat sie nichts zu sagen und steht deshalb auch nicht
    // in der Liste.
    if (ctx.countryListNeeded) {
        if (ctx.countryEntries > 0) {
            add(Level::Ok, kGroupData, QStringLiteral("Länderliste"),
                QStringLiteral("%1 Gebiete").arg(ctx.countryEntries), QStringLiteral("countries"));
        } else {
            add(Level::Warning, kGroupData, QStringLiteral("Länderliste"),
                QStringLiteral("Keine geladen — ohne sie bleibt eine Station ohne Land, ohne Punkt auf der "
                               "Karte und ohne Richtung für den Rotor (Datei › Länderliste laden)."),
                QStringLiteral("countries"));
        }
    }

    return result;
}

} // namespace Contestprogramm
