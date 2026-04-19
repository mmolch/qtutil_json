#include <format>

#include <QJsonArray>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>

#include "mmolch/qtutil_json.h"

namespace mmolch::qtutil {

// ---------------------------------------------------------------------------
// JSON DIFF
// ---------------------------------------------------------------------------

QJsonObject jsonDiff(const QJsonObject &merged, const QJsonObject &base, JsonDiffOptions options)
{
    const bool recursive = options.testFlag(JsonDiffOption::Recursive);
    const bool explicitNull = options.testFlag(JsonDiffOption::ExplicitNull);

    QJsonObject diff;

    for (auto it = merged.begin(); it != merged.end(); ++it) {
        const QString &key = it.key();
        const QJsonValue &mergedValue = it.value();
        const QJsonValue baseValue = base.value(key);

        if (!base.contains(key)) {
            diff.insert(key, mergedValue);
            continue;
        }

        if (recursive && mergedValue.isObject() && baseValue.isObject()) {
            auto child = jsonDiff(mergedValue.toObject(),
                                   baseValue.toObject(),
                                   options);
            if (!child.isEmpty())
                diff.insert(key, child);
        } else if (mergedValue != baseValue) {
            diff.insert(key, mergedValue);
        }
    }

    if (explicitNull) {
        for (auto it = base.begin(); it != base.end(); ++it) {
            const QString &key = it.key();
            if (!merged.contains(key))
                diff.insert(key, QJsonValue::Null);
        }
    }

    return diff;
}

// ---------------------------------------------------------------------------
// JSON MERGE
// ---------------------------------------------------------------------------

namespace {

// Forward declaration for recursive merging
JsonStatus jsonMergeEngine(QJsonObject &base,
                           const QJsonObject &overrides,
                           const QJsonObject *schemaNode,
                           JsonMergeOptions options);

// ================================================================
// Strategy helpers
// ================================================================

void strategyReplace(QJsonObject &base,
                     const QString &key,
                     const QJsonValue &overrideValue)
{
    base.insert(key, overrideValue);
}

JsonStatus strategyDeep(QJsonObject &base,
                        const QString &key,
                        const QJsonValue &overrideValue,
                        const QJsonObject *childSchema,
                        JsonMergeOptions options,
                        bool &applied)
{
    applied = false;
    if (!overrideValue.isObject() || !base.contains(key) || !base.value(key).isObject()) {
        return {}; // Not an error, just can't apply deep merge
    }

    QJsonObject childBase = base.value(key).toObject();
    auto result = jsonMergeEngine(childBase, overrideValue.toObject(), childSchema, options);

    if (!result) {
        return result; // Bubble up schema/merge errors
    }

    base.insert(key, childBase);
    applied = true;
    return {};
}

bool strategyAppendUnique(QJsonObject &base,
                          const QString &key,
                          const QJsonValue &overrideValue)
{
    if (!overrideValue.isArray())
        return false;

    QJsonArray result = base.value(key).toArray();
    const QJsonArray incoming = overrideValue.toArray();

    for (const QJsonValue &v : incoming) {
        if (!result.contains(v))
            result.append(v);
    }

    base.insert(key, result);
    return true;
}

// ================================================================
// Main merge engine
// ================================================================

JsonStatus jsonMergeEngine(QJsonObject &base,
                             const QJsonObject &overrides,
                             const QJsonObject *schemaNode,
                             JsonMergeOptions options)
{
    const bool fallbackRecursive = options.testFlag(JsonMergeOption::Recursive);
    const bool fallbackOverrideNull = options.testFlag(JsonMergeOption::OverrideNull);

    QJsonObject props;
    if (schemaNode) {
        props = schemaNode->value(QStringLiteral("properties")).toObject();
    }

    for (auto it = overrides.begin(); it != overrides.end(); ++it) {
        const QString &key = it.key();
        const QJsonValue &overrideValue = it.value();

        QJsonObject childSchema;
        bool schemaHasStrategy = false;
        QString mergeStrategy;

        if (schemaNode) {
            childSchema = props.value(key).toObject();
            schemaHasStrategy = childSchema.contains(QStringLiteral("mergeStrategy"));
            mergeStrategy = childSchema.value(QStringLiteral("mergeStrategy")).toString();
        }

        // ------------------------------------------------------------
        // NULL HANDLING — schema-first, fallback to OverrideNull
        // ------------------------------------------------------------
        if (overrideValue.isNull()) {
            if (schemaHasStrategy || fallbackOverrideNull) {
                base.remove(key); // Actually delete the key from the object
                continue;
            }
            continue; // Ignore null if OverrideNull is not set
        }

        // ------------------------------------------------------------
        // SCHEMA-DEFINED STRATEGIES
        // ------------------------------------------------------------
        if (schemaHasStrategy) {

            if (mergeStrategy == QStringLiteral("replace")) {
                strategyReplace(base, key, overrideValue);
                continue;
            }

            if (mergeStrategy == QStringLiteral("deep")) {
                bool applied = false;
                auto status = strategyDeep(base, key, overrideValue, childSchema.isEmpty() ? nullptr : &childSchema, options, applied);
                if (!status) {
                    return status; // Bubble up nested errors
                }
                if (!applied) {
                    strategyReplace(base, key, overrideValue); // fallback to replace on type mismatch
                }
                continue;
            }

            if (mergeStrategy == QStringLiteral("appendUnique")) {
                if (!strategyAppendUnique(base, key, overrideValue)) {
                    strategyReplace(base, key, overrideValue); // fallback to replace
                }
                continue;
            }

            // Unknown strategy → return an explicit error instead of silent fallback
            return std::unexpected(JsonError{
                JsonErrorCode::MergeError,
                QStringLiteral("Unknown merge strategy '%1' found for key '%2'").arg(mergeStrategy, key)
            });
        }

        // ------------------------------------------------------------
        // FALLBACK BEHAVIOR (no schema strategy)
        // ------------------------------------------------------------

        // Fallback deep merge
        if (fallbackRecursive && overrideValue.isObject() && base.contains(key) && base.value(key).isObject()) {
            bool applied = false;
            auto status = strategyDeep(base, key, overrideValue, nullptr, options, applied);
            if (!status) return status;
            continue;
        }

        // Fallback: simple replace
        strategyReplace(base, key, overrideValue);
    }

    return {};
}

} // namespace

JsonStatus jsonMergeInplace(QJsonObject &base,
                            const QJsonObject &overrides,
                            const QJsonObject *schema,
                            JsonMergeOptions options)
{
    return jsonMergeEngine(base, overrides, schema, options);
}

JsonObjectResult jsonMerge(QJsonObject base,
                           const QJsonObject &overrides,
                           const QJsonObject *schema,
                           JsonMergeOptions options)
{
    auto status = jsonMergeEngine(base, overrides, schema, options);
    if (!status) {
        return std::unexpected(status.error());
    }
    return base;
}

// ---------------------------------------------------------------------------
// JSON LOAD
// ---------------------------------------------------------------------------

namespace {

// Helper: convert byte offset -> line/column
void offsetToLineColumn(const QByteArray &data,
                        int offset,
                        int &line,
                        int &column)
{
    line = 1;
    column = 1;

    if (offset < 0 || offset > data.size())
        return;

    for (int i = 0; i < offset; ++i) {
        const char c = data.at(i);
        if (c == '\n') {
            ++line;
            column = 1;
        } else {
            ++column;
        }
    }
}
} // namespace

JsonObjectResult jsonLoad(const QString &path)
{
    QFile file(path);
    if (!file.exists()) {
        return std::unexpected(JsonError{
            JsonErrorCode::FileNotFound,
            QStringLiteral("File does not exist: %1").arg(path)
        });
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::unexpected(JsonError{
            JsonErrorCode::OpenFailed,
            QStringLiteral("Cannot open file %1: %2").arg(path, file.errorString())
        });
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        int line = -1;
        int column = -1;
        offsetToLineColumn(data, static_cast<int>(parseError.offset), line, column);
        return std::unexpected(JsonError{
            JsonErrorCode::ParseError,
            QString::fromStdString(std::format(
                "Parse error in {}: {} (line {}, column {})",
                path.toStdString(), parseError.errorString().toStdString(), line, column))
        });
    }

    if (!doc.isObject()) {
        return std::unexpected(JsonError{
            JsonErrorCode::TopLevelNotObject,
            QStringLiteral("Top-level JSON is not an object in %1").arg(path)
        });
    }

    return doc.object();
}

// ---------------------------------------------------------------------------
// JSON PIPELINE
// ---------------------------------------------------------------------------

namespace {

// Helper to translate the broad Mode into specific Option flags
JsonValidationOptions getValOptions(JsonValidationMode mode) {
    if (mode == JsonValidationMode::Partial) {
        return JsonValidationOption::IgnoreRequired | JsonValidationOption::IgnoreMinConstraints;
    }
    return JsonValidationOption::None;
}

} // namespace

// ---------------------------------------------------------------------------
// IN-MEMORY PIPELINE
// ---------------------------------------------------------------------------

JsonProcessResult jsonProcess(const QList<QJsonObject> &objects,
                              const QJsonObject *schema,
                              JsonProcessOptions options)
{
    if (objects.isEmpty()) return QJsonObject{};

    QJsonObject result;
    bool isFirstObject = true;

    for (int i = 0; i < objects.size(); ++i) {
        const QJsonObject &currentObj = objects.at(i);

        // 1. Input Validation
        if (schema && options.inputValidationMode != JsonValidationMode::None) {
            auto valResult = jsonValidate(currentObj, *schema, getValOptions(options.inputValidationMode));
            if (!valResult) {
                return std::unexpected(JsonProcessError{
                    JsonErrorCode::SchemaViolation,
                    QStringLiteral("Validation failed at input index %1").arg(i),
                    std::move(valResult.error())
                });
            }
        }

        // 2. Merge
        if (isFirstObject) {
            result = currentObj;
            isFirstObject = false;
        } else {
            auto mergeStatus = jsonMergeInplace(result, currentObj, schema, options.mergeOptions);
            if (!mergeStatus) {
                return std::unexpected(JsonProcessError{
                    mergeStatus.error().code,
                    QStringLiteral("Merge failed at input index %1: %2").arg(QString("%1").arg(i), mergeStatus.error().message)
                });
            }
        }
    }

    // 3. Output Validation
    if (schema && options.outputValidationMode != JsonValidationMode::None) {
        auto valResult = jsonValidate(result, *schema, getValOptions(options.outputValidationMode));
        if (!valResult) {
            return std::unexpected(JsonProcessError{
                JsonErrorCode::SchemaViolation,
                QStringLiteral("Validation failed on final merged object."),
                std::move(valResult.error())
            });
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// I/O PIPELINE
// ---------------------------------------------------------------------------

JsonProcessResult jsonLoadAndProcess(const QStringList &files,
                                     const QJsonObject *schema,
                                     JsonLoadAndProcessOptions options)
{
    QJsonObject result;
    bool isFirstFile = true;
    const bool skipNonExisting = options.loadOptions.testFlag(JsonLoadOption::SkipNonExisting);

    for (const QString &file : files) {
        // 1. Load File
        auto loadResult = jsonLoad(file);
        if (!loadResult) {
            if (skipNonExisting && loadResult.error().code == JsonErrorCode::FileNotFound) {
                continue; // Skip silently
            }
            return std::unexpected(JsonProcessError{loadResult.error().code, loadResult.error().message});
        }

        QJsonObject currentObj = loadResult.value();

        // 2. Input Validation (Fail Fast!)
        if (schema && options.processOptions.inputValidationMode != JsonValidationMode::None) {
            auto valResult = jsonValidate(currentObj, *schema, getValOptions(options.processOptions.inputValidationMode));
            if (!valResult) {
                return std::unexpected(JsonProcessError{
                    JsonErrorCode::SchemaViolation,
                    QStringLiteral("Validation failed for file: '%1'").arg(file),
                    std::move(valResult.error())
                });
            }
        }

        // 3. Merge (Fail Fast!)
        if (isFirstFile) {
            result = std::move(currentObj);
            isFirstFile = false;
        } else {
            auto mergeStatus = jsonMergeInplace(result, currentObj, schema, options.processOptions.mergeOptions);
            if (!mergeStatus) {
                return std::unexpected(JsonProcessError{
                    mergeStatus.error().code,
                    QStringLiteral("Merge failed at file '%1': %2").arg(file, mergeStatus.error().message)
                });
            }
        }
    }

    // 4. Output Validation
    if (schema && options.processOptions.outputValidationMode != JsonValidationMode::None) {
        auto valResult = jsonValidate(result, *schema, getValOptions(options.processOptions.outputValidationMode));
        if (!valResult) {
            return std::unexpected(JsonProcessError{
                JsonErrorCode::SchemaViolation,
                QStringLiteral("Validation failed on final merged object."),
                std::move(valResult.error())
            });
        }
    }

    return result;
}

} // namespace mmolch::qtutil
