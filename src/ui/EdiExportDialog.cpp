#include "ui/EdiExportDialog.h"

#include "core/Maidenhead.h"
#include "data/ContestDatabase.h"
#include "data/QsoRecord.h"
#include "ui/StyleKit.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace Contestprogramm {

namespace {

const QString kExportDirKey = QStringLiteral("edi_export_dir");

// macOS's QFormLayout default keeps every field at its size hint,
// which leaves a 140-px line edit next to a 300-px placeholder; every
// form here wants its fields to take the group's full width instead.
QFormLayout* makeForm()
{
    auto* form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    return form;
}

QLineEdit* makeEdit(QWidget* parent, const QString& placeholder = QString())
{
    auto* edit = new QLineEdit(parent);
    if (!placeholder.isEmpty()) {
        edit->setPlaceholderText(placeholder);
    }
    return edit;
}

} // namespace

EdiExportDialog::EdiExportDialog(ContestDatabase& database,
                                 const ContestDefinition& definition,
                                 const ContestSettings& settings,
                                 QWidget* parent)
    : QDialog(parent)
    , m_database(&database)
    , m_definition(definition)
    , m_settings(settings)
    , m_sectionEdit(makeEdit(this, QStringLiteral("z.B. SINGLE, MULTI, 6H -- wie in der Ausschreibung")))
    , m_clubEdit(makeEdit(this))
    , m_location1Edit(makeEdit(this, QStringLiteral("z.B. Feuerkogel, 1592 m")))
    , m_location2Edit(makeEdit(this))
    , m_operatorsEdit(makeEdit(this, QStringLiteral("nur Multi-Op: OE5XXX;OE5YYY")))
    , m_nameEdit(makeEdit(this))
    , m_streetEdit(makeEdit(this))
    , m_postalCodeEdit(makeEdit(this))
    , m_cityEdit(makeEdit(this))
    , m_countryEdit(makeEdit(this))
    , m_phoneEdit(makeEdit(this))
    , m_emailEdit(makeEdit(this))
    , m_txEquipmentEdit(makeEdit(this, QStringLiteral("z.B. Kenwood TS-590 + PA")))
    , m_powerSpin(new QSpinBox(this))
    , m_rxEquipmentEdit(makeEdit(this))
    , m_antennaEdit(makeEdit(this, QStringLiteral("z.B. 2 x 12 el. Yagi")))
    , m_directoryEdit(makeEdit(this))
    , m_previewLabel(new QLabel(this))
{
    setWindowTitle(QStringLiteral("Contestprogramm - EDI exportieren (REG1TEST)"));
    setModal(true);

    m_powerSpin->setRange(0, 99999);
    m_powerSpin->setSuffix(QStringLiteral(" W"));
    m_powerSpin->setSpecialValueText(QStringLiteral("-"));

    EdiStationInfo stored;
    stored.loadFrom(*m_database);
    setStationInfo(stored);

    const QString storedDir = m_database->settingValue(kExportDirKey);
    setOutputDirectory(storedDir.isEmpty()
                           ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                           : storedDir);

    auto* entryForm = makeForm();
    entryForm->addRow(QStringLiteral("Klasse/Sektion (PSect):"), m_sectionEdit);
    entryForm->addRow(QStringLiteral("Standort (PAdr1):"), m_location1Edit);
    entryForm->addRow(QStringLiteral("Standort, Zeile 2:"), m_location2Edit);
    entryForm->addRow(QStringLiteral("Club:"), m_clubEdit);
    entryForm->addRow(QStringLiteral("Operatoren (MOpe1):"), m_operatorsEdit);
    auto* entryGroup = new QGroupBox(QStringLiteral("Teilnahme"), this);
    entryGroup->setLayout(entryForm);

    auto* responsibleForm = makeForm();
    responsibleForm->addRow(QStringLiteral("Name:"), m_nameEdit);
    responsibleForm->addRow(QStringLiteral("Straße:"), m_streetEdit);
    responsibleForm->addRow(QStringLiteral("PLZ:"), m_postalCodeEdit);
    responsibleForm->addRow(QStringLiteral("Ort:"), m_cityEdit);
    responsibleForm->addRow(QStringLiteral("Land:"), m_countryEdit);
    responsibleForm->addRow(QStringLiteral("Telefon:"), m_phoneEdit);
    responsibleForm->addRow(QStringLiteral("E-Mail:"), m_emailEdit);
    auto* responsibleGroup = new QGroupBox(QStringLiteral("Verantwortlicher Operator"), this);
    responsibleGroup->setLayout(responsibleForm);

    auto* equipmentForm = makeForm();
    equipmentForm->addRow(QStringLiteral("Sender:"), m_txEquipmentEdit);
    equipmentForm->addRow(QStringLiteral("Leistung:"), m_powerSpin);
    equipmentForm->addRow(QStringLiteral("Empfänger:"), m_rxEquipmentEdit);
    equipmentForm->addRow(QStringLiteral("Antenne:"), m_antennaEdit);
    auto* heightLabel = new QLabel(QStringLiteral("%1 m über Grund, %2 m über NN (aus den Einstellungen)")
                                       .arg(m_settings.antennaHeightM, 0, 'f', 0)
                                       .arg(m_settings.ownElevationM, 0, 'f', 0),
                                   this);
    heightLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextSecondary()));
    equipmentForm->addRow(QStringLiteral("Antennenhöhe:"), heightLabel);
    auto* equipmentGroup = new QGroupBox(QStringLiteral("Station"), this);
    equipmentGroup->setLayout(equipmentForm);

    auto* chooseButton = new QPushButton(QStringLiteral("Ordner..."), this);
    connect(chooseButton, &QPushButton::clicked, this, &EdiExportDialog::chooseDirectory);
    auto* dirRow = new QHBoxLayout();
    dirRow->setContentsMargins(0, 0, 0, 0);
    dirRow->addWidget(m_directoryEdit, 1);
    dirRow->addWidget(chooseButton);
    auto* outputForm = makeForm();
    outputForm->addRow(QStringLiteral("Zielordner:"), dirRow);
    m_previewLabel->setWordWrap(true);
    m_previewLabel->setFont(Style::monoFont(m_previewLabel->font(), Style::kFontSmall));
    auto* outputLayout = new QVBoxLayout();
    outputLayout->addLayout(outputForm);
    outputLayout->addWidget(m_previewLabel);
    auto* outputGroup = new QGroupBox(QStringLiteral("Ausgabe -- eine Datei je Band"), this);
    outputGroup->setLayout(outputLayout);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    QPushButton* exportButton = buttons->addButton(QStringLiteral("Exportieren"), QDialogButtonBox::AcceptRole);
    exportButton->setDefault(true);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(exportButton, &QPushButton::clicked, this, [this]() {
        if (exportNow()) {
            accept();
        }
    });

    auto* columns = new QHBoxLayout();
    auto* leftColumn = new QVBoxLayout();
    leftColumn->addWidget(entryGroup);
    leftColumn->addWidget(equipmentGroup);
    leftColumn->addStretch();
    auto* rightColumn = new QVBoxLayout();
    rightColumn->addWidget(responsibleGroup);
    rightColumn->addStretch();
    columns->addLayout(leftColumn, 1);
    columns->addLayout(rightColumn, 1);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(columns);
    layout->addWidget(outputGroup);
    layout->addWidget(buttons);

    refreshPreview();
}

QString EdiExportDialog::outputDirectory() const
{
    return m_directoryEdit->text().trimmed();
}

void EdiExportDialog::setOutputDirectory(const QString& path)
{
    m_directoryEdit->setText(path);
}

EdiStationInfo EdiExportDialog::stationInfo() const
{
    EdiStationInfo info;
    info.section = m_sectionEdit->text();
    info.club = m_clubEdit->text();
    info.locationLine1 = m_location1Edit->text();
    info.locationLine2 = m_location2Edit->text();
    info.operators = m_operatorsEdit->text();
    info.name = m_nameEdit->text();
    info.street = m_streetEdit->text();
    info.postalCode = m_postalCodeEdit->text();
    info.city = m_cityEdit->text();
    info.country = m_countryEdit->text();
    info.phone = m_phoneEdit->text();
    info.email = m_emailEdit->text();
    info.txEquipment = m_txEquipmentEdit->text();
    info.powerWatts = m_powerSpin->value();
    info.rxEquipment = m_rxEquipmentEdit->text();
    info.antenna = m_antennaEdit->text();
    return info;
}

void EdiExportDialog::setStationInfo(const EdiStationInfo& info)
{
    m_sectionEdit->setText(info.section);
    m_clubEdit->setText(info.club);
    m_location1Edit->setText(info.locationLine1);
    m_location2Edit->setText(info.locationLine2);
    m_operatorsEdit->setText(info.operators);
    m_nameEdit->setText(info.name);
    m_streetEdit->setText(info.street);
    m_postalCodeEdit->setText(info.postalCode);
    m_cityEdit->setText(info.city);
    m_countryEdit->setText(info.country);
    m_phoneEdit->setText(info.phone);
    m_emailEdit->setText(info.email);
    m_txEquipmentEdit->setText(info.txEquipment);
    m_powerSpin->setValue(info.powerWatts);
    m_rxEquipmentEdit->setText(info.rxEquipment);
    m_antennaEdit->setText(info.antenna);
}

void EdiExportDialog::chooseDirectory()
{
    const QString chosen = QFileDialog::getExistingDirectory(this, QStringLiteral("Zielordner für EDI-Dateien"),
                                                             outputDirectory());
    if (!chosen.isEmpty()) {
        setOutputDirectory(chosen);
    }
}

void EdiExportDialog::refreshPreview()
{
    QMap<QString, int> countByBand;
    for (const QsoRecord& record : m_database->qsosForContest(m_settings.activeContestId)) {
        if (!record.isInvalid) {
            countByBand[record.band] += 1;
        }
    }
    EdiExporter exporter(*m_database);
    const QStringList bands = exporter.bandsWithQsos(m_settings.activeContestId, m_definition);
    if (bands.isEmpty()) {
        m_previewLabel->setText(QStringLiteral("(keine QSOs im aktiven Contest)"));
        return;
    }
    QStringList parts;
    for (const QString& band : bands) {
        parts << QStringLiteral("%1  (%2 QSOs)")
                     .arg(EdiExporter::suggestedFileName(m_settings.ownCallsign, band))
                     .arg(countByBand.value(band));
    }
    m_previewLabel->setText(parts.join(QLatin1Char('\n')));
}

bool EdiExportDialog::exportNow()
{
    m_writtenFiles.clear();

    if (m_settings.ownCallsign.trimmed().isEmpty() || !isValidGridSquare(m_settings.ownGrid)) {
        QMessageBox::warning(this, QStringLiteral("Contestprogramm"),
                             QStringLiteral("Eigenes Rufzeichen und Locator müssen in den Einstellungen "
                                            "gesetzt sein -- ohne PCall/PWWLo ist ein EDI-Log wertlos."));
        return false;
    }

    EdiExporter exporter(*m_database);
    const QStringList bands = exporter.bandsWithQsos(m_settings.activeContestId, m_definition);
    if (bands.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Contestprogramm"),
                             QStringLiteral("Keine QSOs im aktiven Contest -- nichts zu exportieren."));
        return false;
    }

    const QString directory = outputDirectory();
    if (directory.isEmpty() || !QDir().mkpath(directory)) {
        QMessageBox::warning(this, QStringLiteral("Contestprogramm"),
                             QStringLiteral("Zielordner konnte nicht angelegt werden:\n%1").arg(directory));
        return false;
    }

    const EdiStationInfo station = stationInfo();
    // The one header field the robots actually reject a log for:
    // without a section the entry cannot be placed in a category.
    if (station.section.trimmed().isEmpty() && !m_skipSectionCheck) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Contestprogramm"),
            QStringLiteral("Klasse/Sektion (PSect) ist leer -- die Auswertung braucht sie, um das Log "
                           "einer Kategorie zuzuordnen.\nTrotzdem exportieren?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return false;
        }
    }
    QStringList written;
    for (const QString& band : bands) {
        const QString text = exporter.exportBand(m_settings.activeContestId, band, m_definition, m_settings, station);
        const QString path = QDir(directory).filePath(EdiExporter::suggestedFileName(m_settings.ownCallsign, band));
        QFile file(path);
        // No QIODevice::Text: the exporter already terminates every line
        // with CRLF, and Text mode would double the CR on Windows.
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QMessageBox::warning(this, QStringLiteral("Contestprogramm"),
                                 QStringLiteral("Datei konnte nicht geschrieben werden:\n%1\n%2")
                                     .arg(path, file.errorString()));
            return false;
        }
        file.write(text.toLatin1());
        file.close();
        written << path;
    }

    station.saveTo(*m_database);
    m_database->setSettingValue(kExportDirKey, directory);
    m_writtenFiles = written;
    return true;
}

} // namespace Contestprogramm
