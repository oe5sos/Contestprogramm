#pragma once

#include "app/ContestSettings.h"
#include "data/ContestDefinition.h"
#include "data/EdiExporter.h"

#include <QDialog>
#include <QString>
#include <QStringList>

class QLabel;
class QLineEdit;
class QSpinBox;

namespace Contestprogramm {

class ContestDatabase;

// The "Datei > EDI exportieren" window: the station paperwork an EDI
// header needs (EdiStationInfo -- who, from where, with what), the
// output folder, and a preview of which per-band files will be written.
// N1MM+ keeps these in its Station Information dialog and DXLog.net in
// its station info; here they live on the export itself, because that
// is the one moment they are needed, and they are persisted in the
// settings table so the second contest only needs a glance.
//
// Writes the files itself (one per band, Latin-1, CRLF -- see
// EdiExporter's class comment) and reports them via writtenFiles();
// MainWindow only shows the result. The station info and the folder
// are saved on a successful export, never on Cancel.
class EdiExportDialog : public QDialog {
    Q_OBJECT
public:
    EdiExportDialog(ContestDatabase& database,
                    const ContestDefinition& definition,
                    const ContestSettings& settings,
                    QWidget* parent = nullptr);

    // Where the files go; pre-filled from the last export (setting
    // "edi_export_dir"), else the Documents folder. Public so a test
    // can point it at a temporary directory.
    QString outputDirectory() const;
    void setOutputDirectory(const QString& path);

    // The station info as currently shown in the form.
    EdiStationInfo stationInfo() const;
    void setStationInfo(const EdiStationInfo& info);

    // The "Exportieren" button's action: validates, saves the station
    // info + folder, writes one file per band. Returns false (and has
    // shown a message box) when nothing was written. Public so a test
    // can drive it without a modal exec().
    bool exportNow();

    // Tests only: skip the "header fields missing" question (a modal
    // box cannot be answered headlessly).
    void setSkipSectionCheck(bool skip) { m_skipSectionCheck = skip; }

    // Full paths written by the last successful exportNow().
    QStringList writtenFiles() const { return m_writtenFiles; }

private:
    void chooseDirectory();
    void refreshPreview();

    ContestDatabase* m_database;
    ContestDefinition m_definition;
    ContestSettings m_settings;
    QStringList m_writtenFiles;
    bool m_skipSectionCheck = false;

    QLineEdit* m_sectionEdit;
    QLineEdit* m_clubEdit;
    QLineEdit* m_location1Edit;
    QLineEdit* m_location2Edit;
    QLineEdit* m_operatorsEdit;
    QLineEdit* m_nameEdit;
    QLineEdit* m_streetEdit;
    QLineEdit* m_postalCodeEdit;
    QLineEdit* m_cityEdit;
    QLineEdit* m_countryEdit;
    QLineEdit* m_phoneEdit;
    QLineEdit* m_emailEdit;
    QLineEdit* m_txEquipmentEdit;
    QSpinBox* m_powerSpin;
    QLineEdit* m_rxEquipmentEdit;
    QLineEdit* m_antennaEdit;
    QLineEdit* m_directoryEdit;
    QLabel* m_previewLabel;
};

} // namespace Contestprogramm
