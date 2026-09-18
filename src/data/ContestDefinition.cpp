#include "data/ContestDefinition.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
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
