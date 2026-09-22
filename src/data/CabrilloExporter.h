#pragma once

#include <QString>
#include <QStringList>

namespace Contestprogramm {

class ContestDatabase;
class ContestDefinition;
struct ContestSettings;

// Die Kategorien im Cabrillo-Kopf, die kein QSO beantworten kann --
// mit welcher Leistung, allein oder zu mehreren, mit oder ohne Hilfe
// von Cluster und Skimmer. Der Robot liest sie, und falsch angegeben
// landet ein Log in der falschen Wertungsklasse. Bis 2026-09-22 stand
// hier eine feste Vorgabe im Code; jetzt fragt ui/CabrilloExportDialog
// danach und merkt sich die Antwort in der Einstellungstabelle.
//
// CATEGORY-BAND / CATEGORY-MODE stehen bewusst nicht hier: die kann
// das Log selbst beantworten und tut es auch (unten aus den geloggten
// QSOs abgeleitet).
struct CabrilloCategories {
    QString operatorCategory = QStringLiteral("SINGLE-OP");
    QString assisted = QStringLiteral("NON-ASSISTED");
    QString power = QStringLiteral("LOW");
    QString transmitter = QStringLiteral("ONE");
    QString station = QStringLiteral("FIXED");
    QString club;   // leer: die Zeile entfällt
    QString email;  // leer: die Zeile entfällt

    // Die zuletzt gewählten Angaben, aus der Einstellungstabelle der
    // Datenbank (`cabrillo_*`-Schlüssel) -- eine Station gibt sie
    // einmal an und nicht vor jedem Export neu.
    static CabrilloCategories load(const ContestDatabase& database);
    void save(ContestDatabase& database) const;

    // Was die Oberfläche anbietet, in dieser Reihenfolge.
    static QStringList operatorChoices();
    static QStringList assistedChoices();
    static QStringList powerChoices();
    static QStringList transmitterChoices();
    static QStringList stationChoices();
};

// Builds a Cabrillo v3.0 log for one contest, per the project plan's
// "Cabrillo-Export (Phase 1)" section. Reads `qsos WHERE contest_id=?`,
// builds the header from ContestSettings + ContestDefinition +
// CabrilloCategories. CATEGORY-BAND / CATEGORY-MODE are derived from
// the logged QSOs (single value if the log is single-band/single-mode,
// else "ALL" / "MIXED").
class CabrilloExporter {
public:
    explicit CabrilloExporter(ContestDatabase& database);

    QString exportContest(const QString& contestId,
                          const ContestDefinition& definition,
                          const ContestSettings& settings,
                          const CabrilloCategories& categories = CabrilloCategories()) const;

private:
    ContestDatabase* m_database;
};

} // namespace Contestprogramm
