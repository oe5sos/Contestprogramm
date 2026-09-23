#include "ui/SettingsDialog.h"

#include "core/CallsignLocatorLookup.h"
#include "core/Maidenhead.h"
#include "core/RotorModels.h"
#include "core/terrain/SrtmTileLoader.h"

#include <algorithm>

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QSpinBox>
#include <QTimeZone>
#include <QVBoxLayout>

namespace Contestprogramm {

SettingsDialog::SettingsDialog(const ContestSettings& initial,
                               const QVector<ContestDefinition>& availableContests,
                               CallsignLocatorLookup* callsignLocatorLookup,
                               QWidget* parent)
    : QDialog(parent)
    , m_initial(initial)
    , m_availableContests(availableContests)
    , m_callsignLocatorLookup(callsignLocatorLookup)
    , m_callsignEdit(new QLineEdit(this))
    , m_gridEdit(new QLineEdit(this))
    , m_elevationSpin(new QDoubleSpinBox(this))
    , m_antennaHeightSpin(new QDoubleSpinBox(this))
    , m_gpsLatSpin(new QDoubleSpinBox(this))
    , m_gpsLonSpin(new QDoubleSpinBox(this))
    , m_gpsApplyButton(new QPushButton(QStringLiteral("Übernehmen"), this))
    , m_gpsStatusLabel(new QLabel(this))
    , m_radiusSpin(new QDoubleSpinBox(this))
    , m_contestCombo(new QComboBox(this))
    , m_contestEndSetCheck(new QCheckBox(QStringLiteral("Gesetzt"), this))
    , m_contestEndEdit(new QDateTimeEdit(this))
    , m_tciHostEdit(new QLineEdit(this))
    , m_tciPortSpin(new QSpinBox(this))
    , m_rigctldHostEdit(new QLineEdit(this))
    , m_rigctldPortSpin(new QSpinBox(this))
    , m_on4kstUserEdit(new QLineEdit(this))
    , m_on4kstPassEdit(new QLineEdit(this))
    , m_clusterHostEdit(new QLineEdit(this))
    , m_clusterPortSpin(new QSpinBox(this))
    , m_callbookProviderCombo(new QComboBox(this))
    , m_callbookUserEdit(new QLineEdit(this))
    , m_callbookPassEdit(new QLineEdit(this))
    , m_importLocatorsButton(new QPushButton(QStringLiteral("Locator-Liste importieren..."), this))
    , m_rotor1EnabledCheck(new QCheckBox(QStringLiteral("Aktiv"), this))
    , m_rotor1LabelEdit(new QLineEdit(this))
    , m_rotor1HostEdit(new QLineEdit(this))
    , m_rotor1PortSpin(new QSpinBox(this))
    , m_rotor1SecondAntennaCheck(new QCheckBox(QStringLiteral("Zweitantenne aktiv"), this))
    , m_rotor1SecondAntennaOffsetSpin(new QDoubleSpinBox(this))
    , m_rotor1ModelCombo(new QComboBox(this))
    , m_rotor1DeviceEdit(new QLineEdit(this))
    , m_rotor1BaudCombo(new QComboBox(this))
    , m_rotor2EnabledCheck(new QCheckBox(QStringLiteral("Aktiv"), this))
    , m_rotor2LabelEdit(new QLineEdit(this))
    , m_rotor2HostEdit(new QLineEdit(this))
    , m_rotor2PortSpin(new QSpinBox(this))
    , m_rotor2SecondAntennaCheck(new QCheckBox(QStringLiteral("Zweitantenne aktiv"), this))
    , m_rotor2SecondAntennaOffsetSpin(new QDoubleSpinBox(this))
    , m_rotor2ModelCombo(new QComboBox(this))
    , m_rotor2DeviceEdit(new QLineEdit(this))
    , m_rotor2BaudCombo(new QComboBox(this))
    , m_band144RotorCombo(new QComboBox(this))
    , m_band432RotorCombo(new QComboBox(this))
    , m_band1296RotorCombo(new QComboBox(this))
    , m_bandOtherRotorCombo(new QComboBox(this))
    , m_rotorDialStyleCombo(new QComboBox(this))
    , m_colorThemeCombo(new QComboBox(this))
    , m_broadcastEnabledCheck(new QCheckBox(QStringLiteral("Aktiv"), this))
    , m_broadcastHostEdit(new QLineEdit(this))
    , m_broadcastPortSpin(new QSpinBox(this))
{
    setWindowTitle(QStringLiteral("Contestprogramm - Einstellungen"));

    m_callsignEdit->setText(initial.ownCallsign);
    m_callsignEdit->setMaxLength(16);

    m_gridEdit->setText(initial.ownGrid);
    m_gridEdit->setMaxLength(6);

    m_elevationSpin->setRange(-500.0, 9000.0);
    m_elevationSpin->setSuffix(QStringLiteral(" m"));
    m_elevationSpin->setValue(initial.ownElevationM);

    m_antennaHeightSpin->setRange(0.0, 200.0);
    m_antennaHeightSpin->setSuffix(QStringLiteral(" m"));
    m_antennaHeightSpin->setValue(initial.antennaHeightM);

    // GPS-coordinates convenience row -- see applyGpsCoordinates()'s own
    // doc comment in the header. Plain signed decimal degrees (the
    // format a phone/handheld GPS unit or its Maps app shows directly),
    // not a separate N/S/E/W selector -- one fewer control, and negative
    // already reads unambiguously as South/West once the suffix names
    // the positive direction.
    m_gpsLatSpin->setRange(-90.0, 90.0);
    m_gpsLatSpin->setDecimals(6);
    m_gpsLatSpin->setSuffix(QStringLiteral(" °N"));
    m_gpsLatSpin->setToolTip(QStringLiteral(
        "Breitengrad, Dezimalgrad (negativ = südliche Breite) -- z.B. vom Handy-GPS abgelesen.\n"
        "Ganzes Koordinatenpaar (\"47.820300, 13.936400\") kann direkt eingefügt werden."));
    m_gpsLonSpin->setRange(-180.0, 180.0);
    m_gpsLonSpin->setDecimals(6);
    m_gpsLonSpin->setSuffix(QStringLiteral(" °O"));
    m_gpsLonSpin->setToolTip(QStringLiteral(
        "Längengrad, Dezimalgrad (negativ = westliche Länge) -- z.B. vom Handy-GPS abgelesen.\n"
        "Ganzes Koordinatenpaar (\"47.820300, 13.936400\") kann direkt eingefügt werden."));
    // Operator, 2026-09-13: "copy paste hat nicht funktioniert" -- this
    // dialog otherwise follows the system/German locale (comma decimal
    // separator, see e.g. m_elevationSpin's own "500,00 m" display), but
    // GPS coordinates are conventionally period-decimal everywhere (every
    // phone/Maps app copies them that way) -- QLocale::c() makes these
    // two fields accept and display "47.820300" rather than silently
    // rejecting/mangling a pasted period-decimal value.
    m_gpsLatSpin->setLocale(QLocale::c());
    m_gpsLonSpin->setLocale(QLocale::c());
    m_gpsApplyButton->setToolTip(QStringLiteral(
        "Berechnet Locator und Standorthöhe (aus echten SRTM-Geländedaten) aus den\n"
        "eingegebenen Koordinaten und trägt beide Felder oben ein."));
    m_gpsStatusLabel->setWordWrap(true);
    connect(m_gpsApplyButton, &QPushButton::clicked, this, &SettingsDialog::applyGpsCoordinates);
    // See handleGpsCoordinateTextEdited()'s own doc comment in the
    // header -- catches a pasted "lat, lon" pair (Google/Apple Maps'
    // own copy format) landing in either field. QDoubleSpinBox::lineEdit()
    // is protected (SettingsDialog isn't a subclass), so findChild() --
    // QAbstractSpinBox always parents its editor as a real QLineEdit
    // child, a standard, stable Qt idiom for reaching it from outside --
    // stands in for it here.
    if (QLineEdit* latEdit = m_gpsLatSpin->findChild<QLineEdit*>()) {
        connect(latEdit, &QLineEdit::textEdited, this, &SettingsDialog::handleGpsCoordinateTextEdited);
    }
    if (QLineEdit* lonEdit = m_gpsLonSpin->findChild<QLineEdit*>()) {
        connect(lonEdit, &QLineEdit::textEdited, this, &SettingsDialog::handleGpsCoordinateTextEdited);
    }

    m_radiusSpin->setRange(0.0, 5000.0);
    m_radiusSpin->setSuffix(QStringLiteral(" km"));
    m_radiusSpin->setValue(initial.radiusKm);

    for (const ContestDefinition& def : m_availableContests) {
        m_contestCombo->addItem(def.name(), def.id());
    }
    const int activeIdx = m_contestCombo->findData(initial.activeContestId);
    if (activeIdx >= 0) {
        m_contestCombo->setCurrentIndex(activeIdx);
    }

    // Contest-end timestamp (ui/UtcClockWidget.h's countdown) -- an
    // operator-set date+time in UTC, per ContestSettings::contestEndUtc.
    // Unset (empty string) is a genuinely different state from "midnight
    // today", so the checkbox -- not an empty/zeroed QDateTimeEdit --
    // carries that distinction; the edit itself always shows a valid
    // value (defaulting to 24h from now, a real 24h contest's duration)
    // whether or not it is currently "active".
    m_contestEndEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
    m_contestEndEdit->setCalendarPopup(true);
    m_contestEndEdit->setTimeZone(QTimeZone::utc());
    QDateTime existingEnd = QDateTime::fromString(initial.contestEndUtc, Qt::ISODate);
    if (existingEnd.isValid()) {
        existingEnd.setTimeZone(QTimeZone::utc());
    }
    const bool hasContestEnd = existingEnd.isValid();
    m_contestEndSetCheck->setChecked(hasContestEnd);
    m_contestEndEdit->setDateTime(hasContestEnd ? existingEnd : QDateTime::currentDateTimeUtc().addSecs(24 * 3600));
    m_contestEndEdit->setEnabled(hasContestEnd);
    connect(m_contestEndSetCheck, &QCheckBox::toggled, m_contestEndEdit, &QWidget::setEnabled);

    m_tciHostEdit->setText(initial.tciHost);
    m_tciPortSpin->setRange(1, 65535);
    m_tciPortSpin->setValue(initial.tciPort);

    m_rigctldHostEdit->setText(initial.rigctldHost);
    m_rigctldPortSpin->setRange(1, 65535);
    m_rigctldPortSpin->setValue(initial.rigctldPort);

    m_on4kstUserEdit->setText(initial.on4kstUsername);
    m_on4kstPassEdit->setText(initial.on4kstPassword);
    m_on4kstPassEdit->setEchoMode(QLineEdit::Password);

    m_clusterHostEdit->setText(initial.clusterHost);
    m_clusterPortSpin->setRange(1, 65535);
    m_clusterPortSpin->setValue(initial.clusterPort);

    // Callsign->grid autofill's external tier (core/
    // CallsignLocatorLookup.h) -- see ContestSettings::callbookProvider.
    // "Keine" (None) is the default, same "off unless the operator
    // explicitly configures it" posture as ON4KST/cluster above -- this
    // dials out to a third party using the operator's own account.
    m_callbookProviderCombo->addItem(QStringLiteral("Keine"), static_cast<int>(ContestSettings::CallbookProvider::None));
    m_callbookProviderCombo->addItem(QStringLiteral("QRZ.com"), static_cast<int>(ContestSettings::CallbookProvider::Qrz));
    m_callbookProviderCombo->addItem(QStringLiteral("HamQTH"), static_cast<int>(ContestSettings::CallbookProvider::HamQth));
    const int callbookProviderIdx = m_callbookProviderCombo->findData(static_cast<int>(initial.callbookProvider));
    m_callbookProviderCombo->setCurrentIndex(callbookProviderIdx >= 0 ? callbookProviderIdx : 0);

    m_callbookUserEdit->setText(initial.callbookUsername);
    m_callbookPassEdit->setText(initial.callbookPassword);
    m_callbookPassEdit->setEchoMode(QLineEdit::Password);

    // The import writes straight into ContestDatabase's imported_locators
    // table (see CallsignLocatorLookup::importCsvFile) -- immediately,
    // independent of this dialog's own OK/Cancel, same as any other
    // "does a real thing right now" button. Disabled (not hidden) when
    // no CallsignLocatorLookup was given -- see the constructor's own
    // parameter comment in the header -- so the group box still shows
    // what the button is for, just not clickable yet.
    m_importLocatorsButton->setEnabled(m_callsignLocatorLookup != nullptr);
    connect(m_importLocatorsButton, &QPushButton::clicked, this, &SettingsDialog::importLocatorCsv);

    m_rotor1EnabledCheck->setChecked(initial.rotor1Enabled);
    m_rotor1LabelEdit->setText(initial.rotor1Label);
    m_rotor1LabelEdit->setMaxLength(24);
    m_rotor1HostEdit->setText(initial.rotor1Host);
    m_rotor1PortSpin->setRange(1, 65535);
    m_rotor1PortSpin->setValue(initial.rotor1Port);
    m_rotor1SecondAntennaCheck->setChecked(initial.rotor1SecondAntennaEnabled);
    m_rotor1SecondAntennaOffsetSpin->setRange(-360.0, 360.0);
    m_rotor1SecondAntennaOffsetSpin->setSuffix(QStringLiteral(" °"));
    m_rotor1SecondAntennaOffsetSpin->setValue(initial.rotor1SecondAntennaOffsetDeg);

    // Hamlib model/device/baud for this slot's rotctld (core/
    // RotorModels.h, core/RotctldProcess.h) -- see
    // ContestSettings::rotor1HamlibModel etc. commonRotorModels()'s own
    // first entry is Yaesu GS-232A (601), the operator's confirmed
    // rotor, so a fresh install lands on it without searching the list.
    for (const RotorModel& model : commonRotorModels()) {
        m_rotor1ModelCombo->addItem(model.name, model.hamlibId);
        m_rotor1ModelCombo->setItemData(m_rotor1ModelCombo->count() - 1, model.note, Qt::ToolTipRole);
    }
    const int rotor1ModelIdx = m_rotor1ModelCombo->findData(initial.rotor1HamlibModel);
    m_rotor1ModelCombo->setCurrentIndex(rotor1ModelIdx >= 0 ? rotor1ModelIdx : 0);

    m_rotor1DeviceEdit->setText(initial.rotor1Device);
    m_rotor1DeviceEdit->setPlaceholderText(QStringLiteral("z.B. /dev/tty.usbserial-1410"));

    for (int baud : commonRotorBauds()) {
        m_rotor1BaudCombo->addItem(QString::number(baud), baud);
    }
    const int rotor1BaudIdx = m_rotor1BaudCombo->findData(initial.rotor1Baud);
    m_rotor1BaudCombo->setCurrentIndex(rotor1BaudIdx >= 0 ? rotor1BaudIdx : m_rotor1BaudCombo->findData(9600));

    m_rotor2EnabledCheck->setChecked(initial.rotor2Enabled);
    m_rotor2LabelEdit->setText(initial.rotor2Label);
    m_rotor2LabelEdit->setMaxLength(24);
    m_rotor2HostEdit->setText(initial.rotor2Host);
    m_rotor2PortSpin->setRange(1, 65535);
    m_rotor2PortSpin->setValue(initial.rotor2Port);
    m_rotor2SecondAntennaCheck->setChecked(initial.rotor2SecondAntennaEnabled);
    m_rotor2SecondAntennaOffsetSpin->setRange(-360.0, 360.0);
    m_rotor2SecondAntennaOffsetSpin->setSuffix(QStringLiteral(" °"));
    m_rotor2SecondAntennaOffsetSpin->setValue(initial.rotor2SecondAntennaOffsetDeg);

    // See the rotor1 block above -- same setup, slot 2.
    for (const RotorModel& model : commonRotorModels()) {
        m_rotor2ModelCombo->addItem(model.name, model.hamlibId);
        m_rotor2ModelCombo->setItemData(m_rotor2ModelCombo->count() - 1, model.note, Qt::ToolTipRole);
    }
    const int rotor2ModelIdx = m_rotor2ModelCombo->findData(initial.rotor2HamlibModel);
    m_rotor2ModelCombo->setCurrentIndex(rotor2ModelIdx >= 0 ? rotor2ModelIdx : 0);

    m_rotor2DeviceEdit->setText(initial.rotor2Device);
    m_rotor2DeviceEdit->setPlaceholderText(QStringLiteral("z.B. /dev/tty.usbserial-70CM"));

    for (int baud : commonRotorBauds()) {
        m_rotor2BaudCombo->addItem(QString::number(baud), baud);
    }
    const int rotor2BaudIdx = m_rotor2BaudCombo->findData(initial.rotor2Baud);
    m_rotor2BaudCombo->setCurrentIndex(rotor2BaudIdx >= 0 ? rotor2BaudIdx : m_rotor2BaudCombo->findData(9600));

    // A disabled slot's own connection/label details are moot, and it
    // cannot be picked as a band's rotor assignment either (see
    // refreshBandRotorCombos()) -- graying them out here matches this
    // codebase's established convention for "off means the rest of the
    // row cannot apply" (see e.g. WinAntenna's checkbox).
    const auto applyRotor1EnabledState = [this](bool enabled) {
        m_rotor1LabelEdit->setEnabled(enabled);
        m_rotor1HostEdit->setEnabled(enabled);
        m_rotor1PortSpin->setEnabled(enabled);
        m_rotor1SecondAntennaCheck->setEnabled(enabled);
        m_rotor1SecondAntennaOffsetSpin->setEnabled(enabled);
        m_rotor1ModelCombo->setEnabled(enabled);
        m_rotor1DeviceEdit->setEnabled(enabled);
        m_rotor1BaudCombo->setEnabled(enabled);
    };
    applyRotor1EnabledState(m_rotor1EnabledCheck->isChecked());
    connect(m_rotor1EnabledCheck, &QCheckBox::toggled, this, applyRotor1EnabledState);

    const auto applyRotor2EnabledState = [this](bool enabled) {
        m_rotor2LabelEdit->setEnabled(enabled);
        m_rotor2HostEdit->setEnabled(enabled);
        m_rotor2PortSpin->setEnabled(enabled);
        m_rotor2SecondAntennaCheck->setEnabled(enabled);
        m_rotor2SecondAntennaOffsetSpin->setEnabled(enabled);
        m_rotor2ModelCombo->setEnabled(enabled);
        m_rotor2DeviceEdit->setEnabled(enabled);
        m_rotor2BaudCombo->setEnabled(enabled);
    };
    applyRotor2EnabledState(m_rotor2EnabledCheck->isChecked());
    connect(m_rotor2EnabledCheck, &QCheckBox::toggled, this, applyRotor2EnabledState);

    // Band -> rotor-slot combos: built from the two slots' current
    // enabled/label state, then seeded from `initial`'s actual per-band
    // assignment (a combo's item list has no "previous selection" to
    // preserve yet on this first call -- see refreshBandRotorCombos()).
    refreshBandRotorCombos();
    const auto selectRotorSlot = [](QComboBox* combo, ContestSettings::RotorSlot slot) {
        const int idx = combo->findData(static_cast<int>(slot));
        combo->setCurrentIndex(idx >= 0 ? idx : 0);
    };
    selectRotorSlot(m_band144RotorCombo, initial.band144RotorSlot);
    selectRotorSlot(m_band432RotorCombo, initial.band432RotorSlot);
    selectRotorSlot(m_band1296RotorCombo, initial.band1296RotorSlot);
    selectRotorSlot(m_bandOtherRotorCombo, initial.bandOtherRotorSlot);

    // Live sync: a slot being disabled or renamed while the dialog is
    // still open must be reflected in the band combos immediately, not
    // only after the next SettingsDialog run.
    connect(m_rotor1EnabledCheck, &QCheckBox::toggled, this, &SettingsDialog::refreshBandRotorCombos);
    connect(m_rotor2EnabledCheck, &QCheckBox::toggled, this, &SettingsDialog::refreshBandRotorCombos);
    connect(m_rotor1LabelEdit, &QLineEdit::textChanged, this, &SettingsDialog::refreshBandRotorCombos);
    connect(m_rotor2LabelEdit, &QLineEdit::textChanged, this, &SettingsDialog::refreshBandRotorCombos);

    // Which of RotorWidget's four paint styles both compasses use --
    // one operator-wide combo, see ContestSettings::rotorDialStyle.
    // Names match the mockups this pass followed (Rotor-A/-B/-C.dc.html,
    // plus Design2.dc.html for the newer "Digital" style) and RotorWidget::
    // showOptionsPopup()'s own menu labels exactly, so the same style
    // reads identically in both places.
    m_rotorDialStyleCombo->addItem(QStringLiteral("Kompass (360°)"), static_cast<int>(RotorDialStyle::FullCompass));
    m_rotorDialStyleCombo->addItem(QStringLiteral("Skala (linear)"), static_cast<int>(RotorDialStyle::LinearScale));
    m_rotorDialStyleCombo->addItem(QStringLiteral("Rotor-Box (Bogen)"), static_cast<int>(RotorDialStyle::PartialArc));
    m_rotorDialStyleCombo->addItem(QStringLiteral("Digital (Zahlen)"), static_cast<int>(RotorDialStyle::Digital));
    const int rotorDialStyleIdx = m_rotorDialStyleCombo->findData(static_cast<int>(initial.rotorDialStyle));
    m_rotorDialStyleCombo->setCurrentIndex(rotorDialStyleIdx >= 0 ? rotorDialStyleIdx : 0);

    // App-wide colour theme (core/ColorTheme.h) -- built from
    // allColorThemes()/colorThemeDisplayName() rather than a hand-typed
    // list, so a future sixth theme only needs adding there, not here
    // too.
    for (ColorTheme theme : allColorThemes()) {
        m_colorThemeCombo->addItem(colorThemeDisplayName(theme), static_cast<int>(theme));
    }
    const int colorThemeIdx = m_colorThemeCombo->findData(static_cast<int>(initial.colorTheme));
    m_colorThemeCombo->setCurrentIndex(colorThemeIdx >= 0 ? colorThemeIdx : 0);

    // Off by default -- see ContestSettings::broadcastEnabled.
    m_broadcastEnabledCheck->setChecked(initial.broadcastEnabled);
    m_broadcastHostEdit->setText(initial.broadcastHost);
    m_broadcastPortSpin->setRange(1, 65535);
    m_broadcastPortSpin->setValue(initial.broadcastPort);

    auto* contestEndRow = new QWidget(this);
    auto* contestEndLayout = new QHBoxLayout(contestEndRow);
    contestEndLayout->setContentsMargins(0, 0, 0, 0);
    contestEndLayout->addWidget(m_contestEndSetCheck);
    contestEndLayout->addWidget(m_contestEndEdit);
    contestEndLayout->addStretch();

    auto* gpsRow = new QWidget(this);
    auto* gpsRowLayout = new QHBoxLayout(gpsRow);
    gpsRowLayout->setContentsMargins(0, 0, 0, 0);
    gpsRowLayout->addWidget(m_gpsLatSpin);
    gpsRowLayout->addWidget(m_gpsLonSpin);
    gpsRowLayout->addWidget(m_gpsApplyButton);
    gpsRowLayout->addStretch();

    auto* stationForm = new QFormLayout();
    stationForm->addRow(QStringLiteral("Eigenes Rufzeichen:"), m_callsignEdit);
    stationForm->addRow(QStringLiteral("Eigener Locator:"), m_gridEdit);
    stationForm->addRow(QStringLiteral("GPS-Koordinaten:"), gpsRow);
    stationForm->addRow(QString(), m_gpsStatusLabel);
    stationForm->addRow(QStringLiteral("Standorthöhe:"), m_elevationSpin);
    stationForm->addRow(QStringLiteral("Antennenhöhe:"), m_antennaHeightSpin);
    stationForm->addRow(QStringLiteral("Radius:"), m_radiusSpin);
    stationForm->addRow(QStringLiteral("Aktiver Contest:"), m_contestCombo);
    stationForm->addRow(QStringLiteral("Contest-Ende (UTC):"), contestEndRow);

    auto* stationGroup = new QGroupBox(QStringLiteral("Station"), this);
    stationGroup->setLayout(stationForm);

    // Active: RigctldClient reads this host/port to reach the contest
    // station's CAT radio (TS-590/K3 via Hamlib rigctld, default port
    // 4532 -- see ContestSettings.h).
    auto* catForm = new QFormLayout();
    catForm->addRow(QStringLiteral("rigctld-Host:"), m_rigctldHostEdit);
    catForm->addRow(QStringLiteral("rigctld-Port:"), m_rigctldPortSpin);

    auto* catGroup = new QGroupBox(QStringLiteral("CAT (rigctld)"), this);
    catGroup->setLayout(catForm);

    // Not wired up yet -- TciClient lands in a later phase if the
    // operator ends up using Longpath/SDR as the radio instead of the
    // CAT rig. ON4KST credentials are used by On4kstClient, but live
    // connection to the real server is deliberately left to the
    // operator (see AppController::applyNetworkSettings).
    auto* networkForm = new QFormLayout();
    networkForm->addRow(QStringLiteral("TCI-Host:"), m_tciHostEdit);
    networkForm->addRow(QStringLiteral("TCI-Port:"), m_tciPortSpin);
    networkForm->addRow(QStringLiteral("ON4KST-Benutzer:"), m_on4kstUserEdit);
    networkForm->addRow(QStringLiteral("ON4KST-Passwort:"), m_on4kstPassEdit);
    // DxClusterClient -- a second, independent spot source alongside
    // ON4KST (see ContestSettings::clusterHost/clusterPort). No fixed
    // public host the way ON4KST has one, so both fields are always
    // active/editable here, unlike TCI-Host/-Port above.
    networkForm->addRow(QStringLiteral("DX-Cluster-Host:"), m_clusterHostEdit);
    networkForm->addRow(QStringLiteral("DX-Cluster-Port:"), m_clusterPortSpin);

    auto* networkGroup = new QGroupBox(QStringLiteral("Netzwerk (ON4KST/Cluster/TCI, teils noch nicht aktiv)"), this);
    networkGroup->setLayout(networkForm);

    // Callsign->grid autofill: local import first (offline, see
    // CallsignLocatorLookup::importCsvFile), QRZ/HamQTH as the network
    // fallback (see ContestSettings::callbookProvider etc.).
    auto* callbookForm = new QFormLayout();
    callbookForm->addRow(QStringLiteral("Anbieter:"), m_callbookProviderCombo);
    callbookForm->addRow(QStringLiteral("Benutzername:"), m_callbookUserEdit);
    callbookForm->addRow(QStringLiteral("Passwort:"), m_callbookPassEdit);
    callbookForm->addRow(QStringLiteral("Lokale Liste:"), m_importLocatorsButton);

    auto* callbookGroup = new QGroupBox(QStringLiteral("Rufzeichen-Nachschlagen"), this);
    callbookGroup->setLayout(callbookForm);

    // Two independent, individually optional rotor slots (Kern-Welle 2,
    // generalized) -- each with its own enabled toggle, free-text
    // label, rotctld host/port, and an optional fixed-offset second
    // antenna. See ContestSettings::rotor1Enabled etc. and RotctldClient.
    auto* rotor1SecondRow = new QWidget(this);
    auto* rotor1SecondLayout = new QHBoxLayout(rotor1SecondRow);
    rotor1SecondLayout->setContentsMargins(0, 0, 0, 0);
    rotor1SecondLayout->addWidget(m_rotor1SecondAntennaCheck);
    rotor1SecondLayout->addWidget(new QLabel(QStringLiteral("Versatz:"), rotor1SecondRow));
    rotor1SecondLayout->addWidget(m_rotor1SecondAntennaOffsetSpin);
    rotor1SecondLayout->addStretch();

    auto* rotor1Form = new QFormLayout();
    rotor1Form->addRow(QStringLiteral("Aktiv:"), m_rotor1EnabledCheck);
    rotor1Form->addRow(QStringLiteral("Bezeichnung:"), m_rotor1LabelEdit);
    rotor1Form->addRow(QStringLiteral("rotctld-Host:"), m_rotor1HostEdit);
    rotor1Form->addRow(QStringLiteral("rotctld-Port:"), m_rotor1PortSpin);
    rotor1Form->addRow(QStringLiteral("Hamlib-Modell:"), m_rotor1ModelCombo);
    rotor1Form->addRow(QStringLiteral("Serielles Gerät:"), m_rotor1DeviceEdit);
    rotor1Form->addRow(QStringLiteral("Baudrate:"), m_rotor1BaudCombo);
    rotor1Form->addRow(QStringLiteral("Zweitantenne:"), rotor1SecondRow);

    auto* rotor1Group = new QGroupBox(QStringLiteral("Rotor 1"), this);
    rotor1Group->setLayout(rotor1Form);

    auto* rotor2SecondRow = new QWidget(this);
    auto* rotor2SecondLayout = new QHBoxLayout(rotor2SecondRow);
    rotor2SecondLayout->setContentsMargins(0, 0, 0, 0);
    rotor2SecondLayout->addWidget(m_rotor2SecondAntennaCheck);
    rotor2SecondLayout->addWidget(new QLabel(QStringLiteral("Versatz:"), rotor2SecondRow));
    rotor2SecondLayout->addWidget(m_rotor2SecondAntennaOffsetSpin);
    rotor2SecondLayout->addStretch();

    auto* rotor2Form = new QFormLayout();
    rotor2Form->addRow(QStringLiteral("Aktiv:"), m_rotor2EnabledCheck);
    rotor2Form->addRow(QStringLiteral("Bezeichnung:"), m_rotor2LabelEdit);
    rotor2Form->addRow(QStringLiteral("rotctld-Host:"), m_rotor2HostEdit);
    rotor2Form->addRow(QStringLiteral("rotctld-Port:"), m_rotor2PortSpin);
    rotor2Form->addRow(QStringLiteral("Hamlib-Modell:"), m_rotor2ModelCombo);
    rotor2Form->addRow(QStringLiteral("Serielles Gerät:"), m_rotor2DeviceEdit);
    rotor2Form->addRow(QStringLiteral("Baudrate:"), m_rotor2BaudCombo);
    rotor2Form->addRow(QStringLiteral("Zweitantenne:"), rotor2SecondRow);

    auto* rotor2Group = new QGroupBox(QStringLiteral("Rotor 2"), this);
    rotor2Group->setLayout(rotor2Form);

    // Explicit per-band rotor assignment -- combo items are the current
    // labels of whichever slot(s) are enabled, plus "Kein Rotor" (see
    // refreshBandRotorCombos()). "144 MHz"/"432 MHz"/"1296 MHz" name the
    // actual amateur-radio bands (fixed, independent of whatever the
    // operator names their rotors); the parenthetical nicknames are
    // purely informational.
    auto* bandRoutingForm = new QFormLayout();
    bandRoutingForm->addRow(QStringLiteral("144 MHz (2m):"), m_band144RotorCombo);
    bandRoutingForm->addRow(QStringLiteral("432 MHz (70cm):"), m_band432RotorCombo);
    bandRoutingForm->addRow(QStringLiteral("1296 MHz (23cm):"), m_band1296RotorCombo);
    // Alles übrige in einer Zeile: auf Kurzwelle hängt an einem Rotor
    // in aller Regel eine Antenne für mehrere Bänder, eine Zeile je
    // Band wäre acht Mal dieselbe Antwort.
    bandRoutingForm->addRow(QStringLiteral("Übrige Bänder (Kurzwelle …):"), m_bandOtherRotorCombo);

    auto* bandRoutingGroup = new QGroupBox(QStringLiteral("Band-Zuordnung"), this);
    bandRoutingGroup->setLayout(bandRoutingForm);

    // Rotor-display style -- one operator-wide choice for both
    // compasses (see ContestSettings::rotorDialStyle and
    // RotorWidget::setDialStyle()), not per-slot like Rotor 1/Rotor 2
    // above.
    auto* rotorDisplayForm = new QFormLayout();
    rotorDisplayForm->addRow(QStringLiteral("Darstellung:"), m_rotorDialStyleCombo);

    auto* rotorDisplayGroup = new QGroupBox(QStringLiteral("Rotor-Anzeige"), this);
    rotorDisplayGroup->setLayout(rotorDisplayForm);

    // App-wide colour theme -- see ContestSettings::colorTheme. Its own
    // group (not folded into "Rotor-Anzeige" above): this choice affects
    // the entire app, not just the rotor compasses.
    auto* appearanceForm = new QFormLayout();
    appearanceForm->addRow(QStringLiteral("Farbthema:"), m_colorThemeCombo);

    auto* appearanceGroup = new QGroupBox(QStringLiteral("Darstellung"), this);
    appearanceGroup->setLayout(appearanceForm);

    // CW F-key macros (Kern-Welle 2) -- editable templates with
    // {call}/{exchange} placeholders, see ui/CwMacroPanel.h.
    auto* cwForm = new QFormLayout();
    for (int i = 0; i < initial.cwMacros.size() && i < 6; ++i) {
        auto* edit = new QLineEdit(initial.cwMacros.at(i), this);
        m_cwMacroEdits.append(edit);
        cwForm->addRow(QStringLiteral("F%1:").arg(i + 1), edit);
    }

    auto* cwGroup = new QGroupBox(QStringLiteral("CW-Makros"), this);
    cwGroup->setLayout(cwForm);

    // UDP-Contact-Broadcast-Standard (core/BroadcastPublisher.h) --
    // same N1MM-format <contactinfo>/<RadioInfo> broadcast N1MM+/
    // DXLog.net implement, so AirScout/KST4Contest-style tools can pick
    // this program's QSOs up "for free". Off by default -- see
    // ContestSettings::broadcastEnabled.
    auto* broadcastForm = new QFormLayout();
    broadcastForm->addRow(QStringLiteral("Broadcast:"), m_broadcastEnabledCheck);
    broadcastForm->addRow(QStringLiteral("Ziel-Host:"), m_broadcastHostEdit);
    broadcastForm->addRow(QStringLiteral("Ziel-Port:"), m_broadcastPortSpin);

    auto* broadcastGroup = new QGroupBox(QStringLiteral("UDP-Broadcast (N1MM-Format)"), this);
    broadcastGroup->setLayout(broadcastForm);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Eight group boxes stacked vertically routinely exceed a laptop's
    // screen height (reported: dialog taller than the display, nothing
    // below the fold reachable, on a MacBook Air-class screen) -- a
    // QScrollArea around everything but the OK/Cancel row fixes that
    // regardless of how many groups this dialog grows to next. The
    // button row stays outside the scroll area so it is always visible.
    auto* scrollContent = new QWidget(this);
    auto* scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->addWidget(stationGroup);
    scrollLayout->addWidget(catGroup);
    scrollLayout->addWidget(networkGroup);
    scrollLayout->addWidget(callbookGroup);
    scrollLayout->addWidget(rotor1Group);
    scrollLayout->addWidget(rotor2Group);
    scrollLayout->addWidget(bandRoutingGroup);
    scrollLayout->addWidget(rotorDisplayGroup);
    scrollLayout->addWidget(appearanceGroup);
    scrollLayout->addWidget(cwGroup);
    scrollLayout->addWidget(broadcastGroup);
    scrollLayout->addStretch();

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidget(scrollContent);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(scrollArea, 1);
    layout->addWidget(buttons);

    // Open at a height that fits the screen the dialog is actually on
    // (85% of available height, capped so a very tall display does not
    // exaggerate the dialog either) rather than always sizing to fit
    // every group box at once -- the scroll area above makes anything
    // beyond that reachable.
    const QScreen* screen = this->screen();
    const int availableHeight = screen ? screen->availableGeometry().height() : 900;
    const int initialHeight = std::min(880, static_cast<int>(availableHeight * 0.85));

    // Width: `scrollContent`'s own natural sizeHint(), computed HERE
    // while it still reflects the widest group's actual QFormLayout
    // content -- setWidgetResizable(true) above makes the scroll area
    // stretch/shrink `scrollContent` to whatever width it is GIVEN from
    // this point on, so querying it later would just report back
    // whatever the dialog's own (otherwise never-set) width already
    // was. Before this fix, only height was ever passed to resize()
    // here -- width silently stayed at Qt's bare default, clipping
    // every field (operator, 2026-09-11: "das ist mir aufgefallen",
    // confirming a screenshot showing labels/fields cut off mid-word).
    // +32px covers the scroll area's own frame/margins.
    const int contentWidth = scrollContent->sizeHint().width();
    resize(contentWidth + 32, initialHeight);

    // Centered over the main window (screen, if somehow parentless) --
    // Qt does not do this for a QDialog automatically on every platform,
    // and it was not happening here in practice (operator, 2026-09-11:
    // "weiters sollte das zentriert sein", right after the width fix
    // above made the dialog's real footprint visible for the first
    // time). Centers on the FINAL width/height just set above, not
    // whatever position Qt would have otherwise placed an unsized
    // dialog at.
    if (QWidget* parent = parentWidget()) {
        move(parent->geometry().center() - QPoint(width() / 2, height() / 2));
    } else if (screen) {
        move(screen->availableGeometry().center() - QPoint(width() / 2, height() / 2));
    }
}

ContestSettings SettingsDialog::settings() const
{
    ContestSettings result = m_initial;
    result.ownCallsign = m_callsignEdit->text().trimmed().toUpper();
    result.ownGrid = m_gridEdit->text().trimmed().toUpper();
    result.ownElevationM = m_elevationSpin->value();
    result.antennaHeightM = m_antennaHeightSpin->value();
    result.radiusKm = m_radiusSpin->value();
    result.activeContestId = m_contestCombo->currentData().toString();
    result.contestEndUtc = m_contestEndSetCheck->isChecked() ? m_contestEndEdit->dateTime().toString(Qt::ISODate) : QString();
    result.tciHost = m_tciHostEdit->text().trimmed();
    result.tciPort = m_tciPortSpin->value();
    result.rigctldHost = m_rigctldHostEdit->text().trimmed();
    result.rigctldPort = m_rigctldPortSpin->value();
    result.on4kstUsername = m_on4kstUserEdit->text().trimmed();
    result.on4kstPassword = m_on4kstPassEdit->text();
    result.clusterHost = m_clusterHostEdit->text().trimmed();
    result.clusterPort = m_clusterPortSpin->value();

    result.callbookProvider = static_cast<ContestSettings::CallbookProvider>(m_callbookProviderCombo->currentData().toInt());
    result.callbookUsername = m_callbookUserEdit->text().trimmed();
    result.callbookPassword = m_callbookPassEdit->text();

    result.rotor1Enabled = m_rotor1EnabledCheck->isChecked();
    result.rotor1Label = m_rotor1LabelEdit->text().trimmed();
    result.rotor1Host = m_rotor1HostEdit->text().trimmed();
    result.rotor1Port = m_rotor1PortSpin->value();
    result.rotor1SecondAntennaEnabled = m_rotor1SecondAntennaCheck->isChecked();
    result.rotor1SecondAntennaOffsetDeg = m_rotor1SecondAntennaOffsetSpin->value();
    result.rotor1HamlibModel = m_rotor1ModelCombo->currentData().toInt();
    result.rotor1Device = m_rotor1DeviceEdit->text().trimmed();
    result.rotor1Baud = m_rotor1BaudCombo->currentData().toInt();

    result.rotor2Enabled = m_rotor2EnabledCheck->isChecked();
    result.rotor2Label = m_rotor2LabelEdit->text().trimmed();
    result.rotor2Host = m_rotor2HostEdit->text().trimmed();
    result.rotor2Port = m_rotor2PortSpin->value();
    result.rotor2SecondAntennaEnabled = m_rotor2SecondAntennaCheck->isChecked();
    result.rotor2SecondAntennaOffsetDeg = m_rotor2SecondAntennaOffsetSpin->value();
    result.rotor2HamlibModel = m_rotor2ModelCombo->currentData().toInt();
    result.rotor2Device = m_rotor2DeviceEdit->text().trimmed();
    result.rotor2Baud = m_rotor2BaudCombo->currentData().toInt();

    result.band144RotorSlot = static_cast<ContestSettings::RotorSlot>(m_band144RotorCombo->currentData().toInt());
    result.band432RotorSlot = static_cast<ContestSettings::RotorSlot>(m_band432RotorCombo->currentData().toInt());
    result.band1296RotorSlot = static_cast<ContestSettings::RotorSlot>(m_band1296RotorCombo->currentData().toInt());
    result.bandOtherRotorSlot = static_cast<ContestSettings::RotorSlot>(m_bandOtherRotorCombo->currentData().toInt());

    result.rotorDialStyle = static_cast<RotorDialStyle>(m_rotorDialStyleCombo->currentData().toInt());
    result.colorTheme = static_cast<ColorTheme>(m_colorThemeCombo->currentData().toInt());

    QStringList macros;
    for (QLineEdit* edit : m_cwMacroEdits) {
        macros << edit->text();
    }
    result.cwMacros = macros;

    result.broadcastEnabled = m_broadcastEnabledCheck->isChecked();
    result.broadcastHost = m_broadcastHostEdit->text().trimmed();
    result.broadcastPort = m_broadcastPortSpin->value();

    return result;
}

void SettingsDialog::applyGpsCoordinates()
{
    const double lat = m_gpsLatSpin->value();
    const double lon = m_gpsLonSpin->value();

    // Synchronous, no data dependency -- always succeeds immediately.
    m_gridEdit->setText(gridSquareFromLatLon(lat, lon));

    if (!m_gpsElevationLoader) {
        m_gpsElevationLoader = new SrtmTileLoader(this);
    }
    // Disconnects any handler from a previous click before wiring a new
    // one -- without this, a second click while the first was still
    // waiting on a tile fetch would stack a second handler on top of the
    // first rather than replacing it.
    disconnect(m_gpsElevationLoader, nullptr, this, nullptr);

    const auto resolveElevation = [this, lat, lon]() {
        const std::optional<double> elevation = m_gpsElevationLoader->elevationAt(lat, lon);
        if (!elevation) {
            return false;
        }
        m_elevationSpin->setValue(*elevation);
        m_gpsStatusLabel->setText(QStringLiteral("Locator %1 und Standorthöhe %2 m aus den Koordinaten übernommen "
                                                   "(echte SRTM-Geländedaten).")
                                       .arg(m_gridEdit->text())
                                       .arg(*elevation, 0, 'f', 0));
        m_gpsApplyButton->setEnabled(true);
        return true;
    };

    if (resolveElevation()) {
        return;
    }

    // Tile not in memory yet -- ensureTileAvailable() checks the local
    // disk cache first (see its own doc comment), which usually resolves
    // synchronously the moment tileLoaded() fires below; a genuine
    // network fetch (first time this exact area is ever looked up) can
    // take a few seconds.
    m_gpsApplyButton->setEnabled(false);
    m_gpsStatusLabel->setText(QStringLiteral("Locator %1 übernommen -- Standorthöhe wird aus SRTM-Geländedaten ermittelt…")
                                   .arg(m_gridEdit->text()));
    connect(m_gpsElevationLoader, &SrtmTileLoader::tileLoaded, this,
            [resolveElevation](const QString&) { resolveElevation(); });
    connect(m_gpsElevationLoader, &SrtmTileLoader::tileLoadFailed, this,
            [this](const QString&, const QString& error) {
                m_gpsStatusLabel->setText(
                    QStringLiteral("Standorthöhe konnte nicht ermittelt werden (%1) -- Locator wurde trotzdem "
                                   "übernommen, Standorthöhe bitte von Hand eintragen.")
                        .arg(error));
                m_gpsApplyButton->setEnabled(true);
            });
    m_gpsElevationLoader->ensureTileAvailable(lat, lon);
}

void SettingsDialog::handleGpsCoordinateTextEdited(const QString& text)
{
    // Pull out up to the first two period-decimal numbers appearing
    // anywhere in the text -- deliberately lenient about what surrounds
    // them (a trailing " °N"/" °O" suffix, a comma or semicolon or plain
    // space between the two, leading/trailing whitespace) rather than
    // requiring the whole string to match one exact shape, since exactly
    // how much of the spin box's own suffix is present in this signal's
    // text can vary. A single number (completely normal, ordinary typing
    // into one field) always leaves this at one match and is correctly
    // ignored -- only a genuine pasted PAIR ever reaches two.
    static const QRegularExpression numberPattern(QStringLiteral(R"(-?\d+(?:\.\d+)?)"));
    QRegularExpressionMatchIterator it = numberPattern.globalMatch(text);
    QVector<double> numbers;
    while (it.hasNext() && numbers.size() < 2) {
        numbers.append(it.next().captured(0).toDouble());
    }
    if (numbers.size() != 2) {
        return;
    }
    m_gpsLatSpin->setValue(numbers.at(0));
    m_gpsLonSpin->setValue(numbers.at(1));
}

void SettingsDialog::importLocatorCsv()
{
    if (!m_callsignLocatorLookup) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Locator-Liste importieren"), QString(),
                                                        QStringLiteral("CSV-Dateien (*.csv);;Alle Dateien (*)"));
    if (path.isEmpty()) {
        return;
    }
    QString error;
    const CallsignLocatorLookup::ImportSummary summary = m_callsignLocatorLookup->importCsvFile(path, &error);
    if (summary.imported == 0 && summary.skipped == 0 && !error.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Contestprogramm"),
                              QStringLiteral("Datei konnte nicht gelesen werden:\n%1").arg(error));
        return;
    }
    QMessageBox::information(this, QStringLiteral("Contestprogramm"),
                              QStringLiteral("%1 Einträge importiert, %2 Zeilen übersprungen.")
                                  .arg(summary.imported)
                                  .arg(summary.skipped));
}

void SettingsDialog::refreshBandRotorCombos()
{
    const auto rebuild = [this](QComboBox* combo) {
        const QVariant previous = combo->count() > 0 ? combo->currentData() : QVariant();
        combo->blockSignals(true);
        combo->clear();
        combo->addItem(QStringLiteral("Kein Rotor"), static_cast<int>(ContestSettings::RotorSlot::None));
        if (m_rotor1EnabledCheck->isChecked()) {
            combo->addItem(m_rotor1LabelEdit->text(), static_cast<int>(ContestSettings::RotorSlot::Slot1));
        }
        if (m_rotor2EnabledCheck->isChecked()) {
            combo->addItem(m_rotor2LabelEdit->text(), static_cast<int>(ContestSettings::RotorSlot::Slot2));
        }
        const int idx = previous.isValid() ? combo->findData(previous) : -1;
        combo->setCurrentIndex(idx >= 0 ? idx : 0);
        combo->blockSignals(false);
    };
    rebuild(m_band144RotorCombo);
    rebuild(m_band432RotorCombo);
    rebuild(m_band1296RotorCombo);
    rebuild(m_bandOtherRotorCombo);
}

} // namespace Contestprogramm
