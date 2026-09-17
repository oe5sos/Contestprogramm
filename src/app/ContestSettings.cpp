#include "app/ContestSettings.h"

#include "data/ContestDatabase.h"

#include <QJsonArray>
#include <QJsonDocument>

namespace Contestprogramm {

namespace {

// cwMacros is a QStringList; the settings table only stores plain
// strings per key, so it round-trips through a compact JSON array --
// robust against any punctuation a CW macro template might contain
// (unlike joining on a fixed separator character).
QString stringListToJson(const QStringList& list)
{
    return QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(list)).toJson(QJsonDocument::Compact));
}

QStringList stringListFromJson(const QString& json, const QStringList& fallback)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isArray()) {
        return fallback;
    }
    QStringList result;
    for (const QJsonValue& value : doc.array()) {
        result << value.toString();
    }
    return result;
}

QString boolToString(bool value)
{
    return value ? QStringLiteral("1") : QStringLiteral("0");
}

bool boolFromString(const QString& value, bool fallback)
{
    if (value == QStringLiteral("1")) { return true; }
    if (value == QStringLiteral("0")) { return false; }
    return fallback;
}

QString operatingModeToString(ContestSettings::OperatingMode mode)
{
    return mode == ContestSettings::OperatingMode::Run
        ? QStringLiteral("run")
        : QStringLiteral("search_and_pounce");
}

ContestSettings::OperatingMode operatingModeFromString(const QString& value, ContestSettings::OperatingMode fallback)
{
    if (value == QStringLiteral("run")) {
        return ContestSettings::OperatingMode::Run;
    }
    if (value == QStringLiteral("search_and_pounce")) {
        return ContestSettings::OperatingMode::SearchAndPounce;
    }
    return fallback;
}

QString rotorSlotToString(ContestSettings::RotorSlot slot)
{
    switch (slot) {
    case ContestSettings::RotorSlot::Slot1: return QStringLiteral("slot1");
    case ContestSettings::RotorSlot::Slot2: return QStringLiteral("slot2");
    case ContestSettings::RotorSlot::None:  return QStringLiteral("none");
    }
    return QStringLiteral("none");
}

ContestSettings::RotorSlot rotorSlotFromString(const QString& value, ContestSettings::RotorSlot fallback)
{
    if (value == QStringLiteral("slot1")) {
        return ContestSettings::RotorSlot::Slot1;
    }
    if (value == QStringLiteral("slot2")) {
        return ContestSettings::RotorSlot::Slot2;
    }
    if (value == QStringLiteral("none")) {
        return ContestSettings::RotorSlot::None;
    }
    return fallback;
}

QString rotorDialStyleToString(RotorDialStyle style)
{
    switch (style) {
    case RotorDialStyle::LinearScale: return QStringLiteral("linear_scale");
    case RotorDialStyle::PartialArc:  return QStringLiteral("partial_arc");
    case RotorDialStyle::Digital:     return QStringLiteral("digital");
    case RotorDialStyle::FullCompass: return QStringLiteral("full_compass");
    }
    return QStringLiteral("full_compass");
}

RotorDialStyle rotorDialStyleFromString(const QString& value, RotorDialStyle fallback)
{
    if (value == QStringLiteral("linear_scale")) {
        return RotorDialStyle::LinearScale;
    }
    if (value == QStringLiteral("partial_arc")) {
        return RotorDialStyle::PartialArc;
    }
    if (value == QStringLiteral("digital")) {
        return RotorDialStyle::Digital;
    }
    if (value == QStringLiteral("full_compass")) {
        return RotorDialStyle::FullCompass;
    }
    return fallback;
}

QString logViewModeToString(ContestSettings::LogViewMode mode)
{
    switch (mode) {
    case ContestSettings::LogViewMode::DxLogFullColumns: return QStringLiteral("dxlog_full_columns");
    case ContestSettings::LogViewMode::Compact:           return QStringLiteral("compact");
    }
    return QStringLiteral("compact");
}

ContestSettings::LogViewMode logViewModeFromString(const QString& value, ContestSettings::LogViewMode fallback)
{
    if (value == QStringLiteral("dxlog_full_columns")) {
        return ContestSettings::LogViewMode::DxLogFullColumns;
    }
    if (value == QStringLiteral("compact")) {
        return ContestSettings::LogViewMode::Compact;
    }
    return fallback;
}

QString logEntryRowPositionToString(ContestSettings::LogEntryRowPosition position)
{
    switch (position) {
    case ContestSettings::LogEntryRowPosition::Bottom: return QStringLiteral("bottom");
    case ContestSettings::LogEntryRowPosition::Top:    return QStringLiteral("top");
    }
    return QStringLiteral("top");
}

ContestSettings::LogEntryRowPosition logEntryRowPositionFromString(const QString& value,
                                                                     ContestSettings::LogEntryRowPosition fallback)
{
    if (value == QStringLiteral("bottom")) {
        return ContestSettings::LogEntryRowPosition::Bottom;
    }
    if (value == QStringLiteral("top")) {
        return ContestSettings::LogEntryRowPosition::Top;
    }
    return fallback;
}

QString callbookProviderToString(ContestSettings::CallbookProvider provider)
{
    switch (provider) {
    case ContestSettings::CallbookProvider::Qrz:    return QStringLiteral("qrz");
    case ContestSettings::CallbookProvider::HamQth: return QStringLiteral("hamqth");
    case ContestSettings::CallbookProvider::None:   return QStringLiteral("none");
    }
    return QStringLiteral("none");
}

ContestSettings::CallbookProvider callbookProviderFromString(const QString& value, ContestSettings::CallbookProvider fallback)
{
    if (value == QStringLiteral("qrz")) {
        return ContestSettings::CallbookProvider::Qrz;
    }
    if (value == QStringLiteral("hamqth")) {
        return ContestSettings::CallbookProvider::HamQth;
    }
    if (value == QStringLiteral("none")) {
        return ContestSettings::CallbookProvider::None;
    }
    return fallback;
}

} // namespace

void ContestSettings::loadFrom(const ContestDatabase& database)
{
    ownCallsign = database.settingValue(QStringLiteral("own_callsign"), ownCallsign);
    ownGrid = database.settingValue(QStringLiteral("own_grid"), ownGrid);
    ownElevationM = database.settingValue(QStringLiteral("own_elevation_m"), QString::number(ownElevationM)).toDouble();
    antennaHeightM = database.settingValue(QStringLiteral("antenna_height_m"), QString::number(antennaHeightM)).toDouble();
    tciHost = database.settingValue(QStringLiteral("tci_host"), tciHost);
    tciPort = database.settingValue(QStringLiteral("tci_port"), QString::number(tciPort)).toInt();
    rigctldHost = database.settingValue(QStringLiteral("rigctld_host"), rigctldHost);
    rigctldPort = database.settingValue(QStringLiteral("rigctld_port"), QString::number(rigctldPort)).toInt();
    on4kstUsername = database.settingValue(QStringLiteral("on4kst_username"), on4kstUsername);
    on4kstPassword = database.settingValue(QStringLiteral("on4kst_password"), on4kstPassword);
    clusterHost = database.settingValue(QStringLiteral("cluster_host"), clusterHost);
    clusterPort = database.settingValue(QStringLiteral("cluster_port"), QString::number(clusterPort)).toInt();
    radiusKm = database.settingValue(QStringLiteral("radius_km"), QString::number(radiusKm)).toDouble();
    activeContestId = database.settingValue(QStringLiteral("active_contest_id"), activeContestId);
    operatingMode = operatingModeFromString(
        database.settingValue(QStringLiteral("operating_mode"), operatingModeToString(operatingMode)), operatingMode);

    rotor1Enabled = boolFromString(
        database.settingValue(QStringLiteral("rotor1_enabled"), boolToString(rotor1Enabled)), rotor1Enabled);
    rotor1Label = database.settingValue(QStringLiteral("rotor1_label"), rotor1Label);
    rotor1Host = database.settingValue(QStringLiteral("rotor1_host"), rotor1Host);
    rotor1Port = database.settingValue(QStringLiteral("rotor1_port"), QString::number(rotor1Port)).toInt();
    rotor1SecondAntennaEnabled = boolFromString(
        database.settingValue(QStringLiteral("rotor1_second_antenna_enabled"), boolToString(rotor1SecondAntennaEnabled)),
        rotor1SecondAntennaEnabled);
    rotor1SecondAntennaOffsetDeg = database.settingValue(
        QStringLiteral("rotor1_second_antenna_offset_deg"), QString::number(rotor1SecondAntennaOffsetDeg)).toDouble();
    rotor1HamlibModel = database.settingValue(
        QStringLiteral("rotor1_hamlib_model"), QString::number(rotor1HamlibModel)).toInt();
    rotor1Device = database.settingValue(QStringLiteral("rotor1_device"), rotor1Device);
    rotor1Baud = database.settingValue(QStringLiteral("rotor1_baud"), QString::number(rotor1Baud)).toInt();

    rotor2Enabled = boolFromString(
        database.settingValue(QStringLiteral("rotor2_enabled"), boolToString(rotor2Enabled)), rotor2Enabled);
    rotor2Label = database.settingValue(QStringLiteral("rotor2_label"), rotor2Label);
    rotor2Host = database.settingValue(QStringLiteral("rotor2_host"), rotor2Host);
    rotor2Port = database.settingValue(QStringLiteral("rotor2_port"), QString::number(rotor2Port)).toInt();
    rotor2SecondAntennaEnabled = boolFromString(
        database.settingValue(QStringLiteral("rotor2_second_antenna_enabled"), boolToString(rotor2SecondAntennaEnabled)),
        rotor2SecondAntennaEnabled);
    rotor2SecondAntennaOffsetDeg = database.settingValue(
        QStringLiteral("rotor2_second_antenna_offset_deg"), QString::number(rotor2SecondAntennaOffsetDeg)).toDouble();
    rotor2HamlibModel = database.settingValue(
        QStringLiteral("rotor2_hamlib_model"), QString::number(rotor2HamlibModel)).toInt();
    rotor2Device = database.settingValue(QStringLiteral("rotor2_device"), rotor2Device);
    rotor2Baud = database.settingValue(QStringLiteral("rotor2_baud"), QString::number(rotor2Baud)).toInt();

    band144RotorSlot = rotorSlotFromString(
        database.settingValue(QStringLiteral("band_144_rotor_slot"), rotorSlotToString(band144RotorSlot)), band144RotorSlot);
    band432RotorSlot = rotorSlotFromString(
        database.settingValue(QStringLiteral("band_432_rotor_slot"), rotorSlotToString(band432RotorSlot)), band432RotorSlot);
    band1296RotorSlot = rotorSlotFromString(
        database.settingValue(QStringLiteral("band_1296_rotor_slot"), rotorSlotToString(band1296RotorSlot)), band1296RotorSlot);

    rotorDialStyle = rotorDialStyleFromString(
        database.settingValue(QStringLiteral("rotor_dial_style"), rotorDialStyleToString(rotorDialStyle)), rotorDialStyle);

    colorTheme = colorThemeFromStorageKey(
        database.settingValue(QStringLiteral("color_theme"), colorThemeStorageKey(colorTheme)));

    logViewMode = logViewModeFromString(
        database.settingValue(QStringLiteral("log_view_mode"), logViewModeToString(logViewMode)), logViewMode);

    logEntryRowPosition = logEntryRowPositionFromString(
        database.settingValue(QStringLiteral("log_entry_row_position"), logEntryRowPositionToString(logEntryRowPosition)),
        logEntryRowPosition);

    cwMacros = stringListFromJson(
        database.settingValue(QStringLiteral("cw_macros"), stringListToJson(cwMacros)), cwMacros);

    cwMacroPanelVisible = boolFromString(
        database.settingValue(QStringLiteral("cw_macro_panel_visible"), boolToString(cwMacroPanelVisible)),
        cwMacroPanelVisible);
    esmEnabled = boolFromString(database.settingValue(QStringLiteral("esm_enabled"), boolToString(esmEnabled)), esmEnabled);
    esmCq = database.settingValue(QStringLiteral("esm_cq"), esmCq);
    esmRunExchange = database.settingValue(QStringLiteral("esm_run_exchange"), esmRunExchange);
    esmTu = database.settingValue(QStringLiteral("esm_tu"), esmTu);
    esmMyCall = database.settingValue(QStringLiteral("esm_my_call"), esmMyCall);
    esmSpExchange = database.settingValue(QStringLiteral("esm_sp_exchange"), esmSpExchange);

    contestEndUtc = database.settingValue(QStringLiteral("contest_end_utc"), contestEndUtc);
    countdownVisible = boolFromString(
        database.settingValue(QStringLiteral("countdown_visible"), boolToString(countdownVisible)), countdownVisible);

    broadcastEnabled = boolFromString(
        database.settingValue(QStringLiteral("broadcast_enabled"), boolToString(broadcastEnabled)), broadcastEnabled);
    broadcastHost = database.settingValue(QStringLiteral("broadcast_host"), broadcastHost);
    broadcastPort = database.settingValue(QStringLiteral("broadcast_port"), QString::number(broadcastPort)).toInt();

    callbookProvider = callbookProviderFromString(
        database.settingValue(QStringLiteral("callbook_provider"), callbookProviderToString(callbookProvider)), callbookProvider);
    callbookUsername = database.settingValue(QStringLiteral("callbook_username"), callbookUsername);
    callbookPassword = database.settingValue(QStringLiteral("callbook_password"), callbookPassword);
}

void ContestSettings::saveTo(ContestDatabase& database) const
{
    database.setSettingValue(QStringLiteral("own_callsign"), ownCallsign);
    database.setSettingValue(QStringLiteral("own_grid"), ownGrid);
    database.setSettingValue(QStringLiteral("own_elevation_m"), QString::number(ownElevationM));
    database.setSettingValue(QStringLiteral("antenna_height_m"), QString::number(antennaHeightM));
    database.setSettingValue(QStringLiteral("tci_host"), tciHost);
    database.setSettingValue(QStringLiteral("tci_port"), QString::number(tciPort));
    database.setSettingValue(QStringLiteral("rigctld_host"), rigctldHost);
    database.setSettingValue(QStringLiteral("rigctld_port"), QString::number(rigctldPort));
    database.setSettingValue(QStringLiteral("on4kst_username"), on4kstUsername);
    database.setSettingValue(QStringLiteral("on4kst_password"), on4kstPassword);
    database.setSettingValue(QStringLiteral("cluster_host"), clusterHost);
    database.setSettingValue(QStringLiteral("cluster_port"), QString::number(clusterPort));
    database.setSettingValue(QStringLiteral("radius_km"), QString::number(radiusKm));
    database.setSettingValue(QStringLiteral("active_contest_id"), activeContestId);
    database.setSettingValue(QStringLiteral("operating_mode"), operatingModeToString(operatingMode));

    database.setSettingValue(QStringLiteral("rotor1_enabled"), boolToString(rotor1Enabled));
    database.setSettingValue(QStringLiteral("rotor1_label"), rotor1Label);
    database.setSettingValue(QStringLiteral("rotor1_host"), rotor1Host);
    database.setSettingValue(QStringLiteral("rotor1_port"), QString::number(rotor1Port));
    database.setSettingValue(QStringLiteral("rotor1_second_antenna_enabled"), boolToString(rotor1SecondAntennaEnabled));
    database.setSettingValue(QStringLiteral("rotor1_second_antenna_offset_deg"), QString::number(rotor1SecondAntennaOffsetDeg));
    database.setSettingValue(QStringLiteral("rotor1_hamlib_model"), QString::number(rotor1HamlibModel));
    database.setSettingValue(QStringLiteral("rotor1_device"), rotor1Device);
    database.setSettingValue(QStringLiteral("rotor1_baud"), QString::number(rotor1Baud));

    database.setSettingValue(QStringLiteral("rotor2_enabled"), boolToString(rotor2Enabled));
    database.setSettingValue(QStringLiteral("rotor2_label"), rotor2Label);
    database.setSettingValue(QStringLiteral("rotor2_host"), rotor2Host);
    database.setSettingValue(QStringLiteral("rotor2_port"), QString::number(rotor2Port));
    database.setSettingValue(QStringLiteral("rotor2_second_antenna_enabled"), boolToString(rotor2SecondAntennaEnabled));
    database.setSettingValue(QStringLiteral("rotor2_second_antenna_offset_deg"), QString::number(rotor2SecondAntennaOffsetDeg));
    database.setSettingValue(QStringLiteral("rotor2_hamlib_model"), QString::number(rotor2HamlibModel));
    database.setSettingValue(QStringLiteral("rotor2_device"), rotor2Device);
    database.setSettingValue(QStringLiteral("rotor2_baud"), QString::number(rotor2Baud));

    database.setSettingValue(QStringLiteral("band_144_rotor_slot"), rotorSlotToString(band144RotorSlot));
    database.setSettingValue(QStringLiteral("band_432_rotor_slot"), rotorSlotToString(band432RotorSlot));
    database.setSettingValue(QStringLiteral("band_1296_rotor_slot"), rotorSlotToString(band1296RotorSlot));

    database.setSettingValue(QStringLiteral("rotor_dial_style"), rotorDialStyleToString(rotorDialStyle));

    database.setSettingValue(QStringLiteral("color_theme"), colorThemeStorageKey(colorTheme));

    database.setSettingValue(QStringLiteral("log_view_mode"), logViewModeToString(logViewMode));

    database.setSettingValue(QStringLiteral("log_entry_row_position"), logEntryRowPositionToString(logEntryRowPosition));

    database.setSettingValue(QStringLiteral("cw_macros"), stringListToJson(cwMacros));

    database.setSettingValue(QStringLiteral("cw_macro_panel_visible"), boolToString(cwMacroPanelVisible));
    database.setSettingValue(QStringLiteral("esm_enabled"), boolToString(esmEnabled));
    database.setSettingValue(QStringLiteral("esm_cq"), esmCq);
    database.setSettingValue(QStringLiteral("esm_run_exchange"), esmRunExchange);
    database.setSettingValue(QStringLiteral("esm_tu"), esmTu);
    database.setSettingValue(QStringLiteral("esm_my_call"), esmMyCall);
    database.setSettingValue(QStringLiteral("esm_sp_exchange"), esmSpExchange);

    database.setSettingValue(QStringLiteral("contest_end_utc"), contestEndUtc);
    database.setSettingValue(QStringLiteral("countdown_visible"), boolToString(countdownVisible));

    database.setSettingValue(QStringLiteral("broadcast_enabled"), boolToString(broadcastEnabled));
    database.setSettingValue(QStringLiteral("broadcast_host"), broadcastHost);
    database.setSettingValue(QStringLiteral("broadcast_port"), QString::number(broadcastPort));

    database.setSettingValue(QStringLiteral("callbook_provider"), callbookProviderToString(callbookProvider));
    database.setSettingValue(QStringLiteral("callbook_username"), callbookUsername);
    database.setSettingValue(QStringLiteral("callbook_password"), callbookPassword);
}

} // namespace Contestprogramm
