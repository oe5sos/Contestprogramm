#include "data/ContestDefinition.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>

namespace Contestprogramm {

ContestDefinition ContestDefinition::loadFromFile(const QString& path, QString* errorOut)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorOut) {
            *errorOut = QStringLiteral("cannot open %1: %2").arg(path, file.errorString());
        }
        return ContestDefinition();
    }
    return loadFromJson(file.readAll(), errorOut);
}

ContestDefinition ContestDefinition::loadFromJson(const QByteArray& json, QString* errorOut)
{
    ContestDefinition def;

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (errorOut) {
            *errorOut = QStringLiteral("JSON parse error: %1").arg(parseError.errorString());
        }
        return def;
    }
    if (!doc.isObject()) {
        if (errorOut) {
            *errorOut = QStringLiteral("top-level JSON value is not an object");
        }
        return def;
    }

    const QJsonObject root = doc.object();

    if (!root.contains(QStringLiteral("id")) || !root.value(QStringLiteral("id")).isString()) {
        if (errorOut) { *errorOut = QStringLiteral("missing or non-string \"id\""); }
        return def;
    }
    def.m_id = root.value(QStringLiteral("id")).toString();

    if (!root.contains(QStringLiteral("name")) || !root.value(QStringLiteral("name")).isString()) {
        if (errorOut) { *errorOut = QStringLiteral("missing or non-string \"name\""); }
        return def;
    }
    def.m_name = root.value(QStringLiteral("name")).toString();

    if (!root.value(QStringLiteral("bands")).isArray()) {
        if (errorOut) { *errorOut = QStringLiteral("missing or non-array \"bands\""); }
        return def;
    }
    for (const QJsonValue& v : root.value(QStringLiteral("bands")).toArray()) {
        def.m_bands.append(v.toString());
    }

    if (!root.value(QStringLiteral("dupe_scope")).isArray()) {
        if (errorOut) { *errorOut = QStringLiteral("missing or non-array \"dupe_scope\""); }
        return def;
    }
    for (const QJsonValue& v : root.value(QStringLiteral("dupe_scope")).toArray()) {
        def.m_dupeScope.append(v.toString());
    }

    if (!root.value(QStringLiteral("exchange_fields")).isArray()) {
        if (errorOut) { *errorOut = QStringLiteral("missing or non-array \"exchange_fields\""); }
        return def;
    }
    for (const QJsonValue& v : root.value(QStringLiteral("exchange_fields")).toArray()) {
        if (!v.isObject()) {
            if (errorOut) { *errorOut = QStringLiteral("exchange_fields entry is not an object"); }
            return def;
        }
        const QJsonObject fieldObj = v.toObject();
        if (!fieldObj.value(QStringLiteral("key")).isString() || !fieldObj.value(QStringLiteral("label")).isString()) {
            if (errorOut) { *errorOut = QStringLiteral("exchange_fields entry missing \"key\" or \"label\""); }
            return def;
        }
        ExchangeField field;
        field.key = fieldObj.value(QStringLiteral("key")).toString();
        field.label = fieldObj.value(QStringLiteral("label")).toString();
        field.type = fieldObj.value(QStringLiteral("type")).toString();
        field.autoIncrement = fieldObj.value(QStringLiteral("auto_increment")).toBool(false);
        def.m_exchangeFields.append(field);
    }

    // Optional; defaults to "grid" (see the header comment) so neither
    // shipped contest_definitions/*.json file needs a new key.
    if (root.value(QStringLiteral("multiplier_field")).isString()) {
        def.m_multiplierField = root.value(QStringLiteral("multiplier_field")).toString();
    }
    // Optional as well; only "distance_km" and "qso_count" are known
    // (see ContestScoring.h), anything else is rejected rather than
    // silently scored as one of them.
    if (root.value(QStringLiteral("scoring")).isString()) {
        const QString scoring = root.value(QStringLiteral("scoring")).toString();
        if (scoring != QStringLiteral("distance_km") && scoring != QStringLiteral("qso_count")) {
            if (errorOut) { *errorOut = QStringLiteral("unknown \"scoring\" value \"%1\"").arg(scoring); }
            return ContestDefinition();
        }
        def.m_scoring = scoring;
    }
    if (root.value(QStringLiteral("serial_scope")).isString()) {
        const QString scope = root.value(QStringLiteral("serial_scope")).toString();
        if (scope != QStringLiteral("band") && scope != QStringLiteral("contest")) {
            if (errorOut) { *errorOut = QStringLiteral("unknown \"serial_scope\" value \"%1\"").arg(scope); }
            return ContestDefinition();
        }
        def.m_serialScope = scope;
    }
    if (root.value(QStringLiteral("cabrillo_name")).isString()) {
        def.m_cabrilloName = root.value(QStringLiteral("cabrillo_name")).toString().trimmed();
    }
    if (root.contains(QStringLiteral("schedule"))) {
        if (!root.value(QStringLiteral("schedule")).isObject()) {
            if (errorOut) { *errorOut = QStringLiteral("\"schedule\" is not an object"); }
            return ContestDefinition();
        }
        QString scheduleError;
        def.m_schedule = ContestSchedule::fromJson(root.value(QStringLiteral("schedule")).toObject(), &scheduleError);
        if (!def.m_schedule.isValid()) {
            if (errorOut) { *errorOut = scheduleError; }
            return ContestDefinition();
        }
    }
    if (root.contains(QStringLiteral("modes"))) {
        if (!root.value(QStringLiteral("modes")).isArray()) {
            if (errorOut) { *errorOut = QStringLiteral("\"modes\" is not an array"); }
            return ContestDefinition();
        }
        for (const QJsonValue& v : root.value(QStringLiteral("modes")).toArray()) {
            const QString mode = v.toString().trimmed().toUpper();
            if (!mode.isEmpty()) {
                def.m_modes.append(mode);
            }
        }
    }

    def.m_valid = true;
    return def;
}

QString ContestDefinition::overrideDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/contest_definitions_overrides");
}

QString ContestDefinition::overrideFilePath(const QString& contestId)
{
    return overrideDirectory() + QLatin1Char('/') + contestId + QStringLiteral(".json");
}

ContestDefinition::Rules ContestDefinition::rules() const
{
    Rules r;
    r.name = m_name;
    r.bands = m_bands;
    r.dupeScope = m_dupeScope;
    r.exchangeFields = m_exchangeFields;
    r.multiplierField = m_multiplierField;
    r.scoring = m_scoring;
    r.serialScope = m_serialScope;
    r.cabrilloName = m_cabrilloName;
    r.modes = m_modes;
    return r;
}

QVector<QPair<QString, QString>> ContestDefinition::scoringChoices()
{
    return {{QStringLiteral("distance_km"), QStringLiteral("Entfernung in km (UKW: km abgerundet + 1)")},
            {QStringLiteral("qso_count"), QStringLiteral("Ein Punkt je QSO")}};
}

QVector<QPair<QString, QString>> ContestDefinition::serialScopeChoices()
{
    return {{QStringLiteral("band"), QStringLiteral("Je Band wieder ab 001")},
            {QStringLiteral("contest"), QStringLiteral("Eine Folge über den ganzen Contest")}};
}

QVector<QPair<QString, QString>> ContestDefinition::multiplierChoices()
{
    return {{QStringLiteral("grid"), QStringLiteral("Locator-Großfeld (JN67)")},
            {QStringLiteral("prefix"), QStringLiteral("Präfix (WPX: OE5, DL1, W1)")},
            {QStringLiteral("dxcc"), QStringLiteral("Land (DXCC — braucht eine geladene Länderliste)")},
            {QStringLiteral("none"), QStringLiteral("Keiner")}};
}

bool ContestDefinition::validateRules(const Rules& rules, QString* errorOut)
{
    const auto fail = [errorOut](const QString& message) {
        if (errorOut) {
            *errorOut = message;
        }
        return false;
    };
    if (rules.name.trimmed().isEmpty()) {
        return fail(QStringLiteral("Der Contest braucht einen Namen."));
    }
    if (rules.bands.isEmpty()) {
        return fail(QStringLiteral("Mindestens ein Band muss angekreuzt sein."));
    }
    if (rules.exchangeFields.isEmpty()) {
        return fail(QStringLiteral("Mindestens ein Exchange-Feld wird benötigt."));
    }
    QSet<QString> seen;
    for (const ExchangeField& field : rules.exchangeFields) {
        if (field.key.trimmed().isEmpty()) {
            return fail(QStringLiteral("Ein Exchange-Feld ohne Key geht nicht."));
        }
        if (seen.contains(field.key)) {
            return fail(QStringLiteral("Feld-Key \"%1\" ist mehrfach vergeben.").arg(field.key));
        }
        seen.insert(field.key);
    }
    // Ohne Rufzeichen ist die Dupe-Prüfung keine: sie würde jedes QSO
    // auf demselben Band als Doppel melden.
    if (!rules.dupeScope.contains(QStringLiteral("callsign"))) {
        return fail(QStringLiteral("Die Dupe-Regel muss das Rufzeichen enthalten."));
    }
    for (const QString& scope : rules.dupeScope) {
        if (scope != QStringLiteral("callsign") && scope != QStringLiteral("band") && scope != QStringLiteral("mode")) {
            return fail(QStringLiteral("Unbekannter Teil der Dupe-Regel: \"%1\".").arg(scope));
        }
    }
    const auto known = [](const QVector<QPair<QString, QString>>& choices, const QString& value) {
        for (const auto& choice : choices) {
            if (choice.first == value) {
                return true;
            }
        }
        return false;
    };
    if (!known(scoringChoices(), rules.scoring)) {
        return fail(QStringLiteral("Unbekannte Wertung: \"%1\".").arg(rules.scoring));
    }
    if (!known(serialScopeChoices(), rules.serialScope)) {
        return fail(QStringLiteral("Unbekannter Nummernkreis: \"%1\".").arg(rules.serialScope));
    }
    if (!known(multiplierChoices(), rules.multiplierField)) {
        return fail(QStringLiteral("Unbekannter Multiplikator: \"%1\".").arg(rules.multiplierField));
    }
    if (errorOut) {
        errorOut->clear();
    }
    return true;
}

ContestDefinition ContestDefinition::withRules(const Rules& rules, QString* errorOut) const
{
    if (!validateRules(rules, errorOut)) {
        return ContestDefinition();
    }
    ContestDefinition copy = *this;
    copy.m_name = rules.name.trimmed();
    copy.m_bands = rules.bands;
    copy.m_dupeScope = rules.dupeScope;
    copy.m_exchangeFields = rules.exchangeFields;
    copy.m_multiplierField = rules.multiplierField;
    copy.m_scoring = rules.scoring;
    copy.m_serialScope = rules.serialScope;
    copy.m_cabrilloName = rules.cabrilloName.trimmed();
    copy.m_modes = rules.modes;
    copy.m_valid = true;
    return copy;
}

ContestDefinition ContestDefinition::fromRules(const QString& id, const Rules& rules, QString* errorOut)
{
    const QString trimmedId = id.trimmed();
    static const QRegularExpression idPattern(QStringLiteral("^[A-Z0-9_]+$"));
    if (!idPattern.match(trimmedId).hasMatch()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Die Kennung darf nur A-Z, 0-9 und _ enthalten (sie wird ein Dateiname).");
        }
        return ContestDefinition();
    }
    ContestDefinition def;
    def.m_id = trimmedId;
    return def.withRules(rules, errorOut);
}

ContestDefinition ContestDefinition::withExchangeFields(const QVector<ExchangeField>& fields) const
{
    ContestDefinition copy = *this;
    copy.m_exchangeFields = fields;
    return copy;
}

bool ContestDefinition::saveToFile(const QString& path, QString* errorOut) const
{
    QJsonArray bandsArray;
    for (const QString& band : m_bands) {
        bandsArray.append(band);
    }
    QJsonArray dupeScopeArray;
    for (const QString& scope : m_dupeScope) {
        dupeScopeArray.append(scope);
    }
    QJsonArray fieldsArray;
    for (const ExchangeField& field : m_exchangeFields) {
        QJsonObject fieldObj;
        fieldObj.insert(QStringLiteral("key"), field.key);
        fieldObj.insert(QStringLiteral("label"), field.label);
        fieldObj.insert(QStringLiteral("type"), field.type);
        if (field.autoIncrement) {
            fieldObj.insert(QStringLiteral("auto_increment"), true);
        }
        fieldsArray.append(fieldObj);
    }

    QJsonObject root;
    root.insert(QStringLiteral("id"), m_id);
    root.insert(QStringLiteral("name"), m_name);
    root.insert(QStringLiteral("bands"), bandsArray);
    root.insert(QStringLiteral("dupe_scope"), dupeScopeArray);
    root.insert(QStringLiteral("exchange_fields"), fieldsArray);
    root.insert(QStringLiteral("multiplier_field"), m_multiplierField);
    root.insert(QStringLiteral("scoring"), m_scoring);
    root.insert(QStringLiteral("serial_scope"), m_serialScope);
    // Optional keys are written only when set, so a saved override of a
    // definition without them stays identical in shape to the shipped file.
    if (!m_cabrilloName.isEmpty()) {
        root.insert(QStringLiteral("cabrillo_name"), m_cabrilloName);
    }
    if (m_schedule.isValid()) {
        root.insert(QStringLiteral("schedule"), m_schedule.toJson());
    }
    if (!m_modes.isEmpty()) {
        QJsonArray modesArray;
        for (const QString& mode : m_modes) {
            modesArray.append(mode);
        }
        root.insert(QStringLiteral("modes"), modesArray);
    }

    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        if (errorOut) {
            *errorOut = QStringLiteral("cannot create directory %1").arg(info.absolutePath());
        }
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorOut) {
            *errorOut = QStringLiteral("cannot open %1: %2").arg(path, file.errorString());
        }
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

} // namespace Contestprogramm
