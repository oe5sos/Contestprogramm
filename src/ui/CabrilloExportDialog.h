#pragma once

#include "data/CabrilloExporter.h"

#include <QDialog>

class QComboBox;
class QLineEdit;

namespace Contestprogramm {

// Was vor dem Einreichen in den Cabrillo-Kopf gehört und kein QSO
// beantworten kann: Bedienerklasse, Hilfe durch Cluster/Skimmer,
// Leistung, Sender, Stationsart, dazu Club und E-Mail. Bis 2026-09-22
// stand das als feste Vorgabe im Code ("SINGLE-OP", "LOW", "FIXED") --
// falsch angegeben landet ein Log in der falschen Wertungsklasse.
//
// Die Auswahl wird in der Einstellungstabelle der Datenbank gemerkt
// (CabrilloCategories::load/save), also einmal angegeben und nicht vor
// jedem Export neu. Band und Betriebsart fragt dieser Dialog nicht --
// die weiß das Log selbst.
class CabrilloExportDialog : public QDialog {
    Q_OBJECT

public:
    CabrilloExportDialog(const CabrilloCategories& initial, QWidget* parent = nullptr);

    CabrilloCategories categories() const;

private:
    QComboBox* m_operator;
    QComboBox* m_assisted;
    QComboBox* m_power;
    QComboBox* m_transmitter;
    QComboBox* m_station;
    QLineEdit* m_club;
    QLineEdit* m_email;
};

} // namespace Contestprogramm
