#pragma once

#include "qtutil_json_error.h"

#include <QStringList>
#include <QJsonObject>

namespace mmolch::qtutil {


// JSON VALIDATE ======================================================================================================

enum class JsonValidationOption : uint8_t {
    None                 = 0,
    IgnoreRequired       = 1 << 0, /**< Do not evaluate "required" properties */
    IgnoreMinConstraints = 1 << 1  /**< Do not evaluate "minProperties" and "minItems" */
};
Q_DECLARE_FLAGS(JsonValidationOptions, JsonValidationOption)
Q_DECLARE_OPERATORS_FOR_FLAGS(JsonValidationOptions)

static constexpr JsonValidationOptions DefaultValidationOptions = JsonValidationOption::None;

/**
 * @brief Validate a JSON object against a JSON Schema (Draft‑7‑like subset).
 *
 * @param object The JSON instance to validate.
 * @param schema The schema to validate against.
 * @return `JsonValidationResult` containing either success or a list of validation errors.
 */
JsonValidationStatus jsonValidate(const QJsonObject &object,
                                  const QJsonObject &schema,
                                  JsonValidationOptions options = DefaultValidationOptions);


// JSON DIFF ==========================================================================================================


enum struct JsonDiffOption : uint8_t {
    None         = 0,
    Recursive    = 1 << 0, /**< Recurse into subobjects */
    ExplicitNull = 1 << 1  /**< Sets keys to null to indicate deleted keys */
};
Q_DECLARE_FLAGS(JsonDiffOptions, JsonDiffOption)
Q_DECLARE_OPERATORS_FOR_FLAGS(JsonDiffOptions)

static constexpr JsonDiffOptions DefaultDiffOptions = JsonDiffOption::Recursive |
                                                      JsonDiffOption::ExplicitNull;

/**
 * @brief Compute the difference between two JSON objects.
 *
 * @param merged The "final" object.
 * @param base   The "original" object.
 * @param options Diff behavior flags.
 * @return A `QJsonObject` describing the differences.
 */
QJsonObject jsonDiff(const QJsonObject &merged,
                     const QJsonObject &base,
                     JsonDiffOptions options = DefaultDiffOptions);


// JSON OPTIONS & CONFIGURATION =======================================================================================


enum class JsonLoadOption : uint8_t {
    None            = 0,
    SkipNonExisting = 1 << 0  /**< silently skip non-existing files during load operations */
};
Q_DECLARE_FLAGS(JsonLoadOptions, JsonLoadOption)
Q_DECLARE_OPERATORS_FOR_FLAGS(JsonLoadOptions)

static constexpr JsonLoadOptions DefaultLoadOptions = JsonLoadOption::None;

enum class JsonMergeOption : uint8_t {
    None         = 0,
    Recursive    = 1 << 0,  /**< merge nested objects */
    OverrideNull = 1 << 1   /**< allow null in overrides to delete keys */
};
Q_DECLARE_FLAGS(JsonMergeOptions, JsonMergeOption)
Q_DECLARE_OPERATORS_FOR_FLAGS(JsonMergeOptions)

static constexpr JsonMergeOptions DefaultMergeOptions = JsonMergeOption::Recursive |
                                                        JsonMergeOption::OverrideNull;

enum class JsonValidationMode {
    None,                   /**< Do not validate at all */
    PerFile,                /**< Fail fast: validate each file immediately after loading */
    FinalResult,            /**< Merge everything first, then validate the final merged object */
    Both,                   /**< Validate each file upon load, merge them, then validate the final object */
    PartialPerFileAndFinal  /**< Loose validation on files, strict validation on the merged result */
};

static const JsonValidationMode DefaultValidationMode = JsonValidationMode::FinalResult;

/**
 * @brief Configuration struct defining the behavior of the JSON processing pipeline.
 */
struct JsonPipelineOptions {
    JsonLoadOptions loadOptions = DefaultLoadOptions;
    JsonMergeOptions mergeOptions = DefaultMergeOptions;
    JsonValidationMode validationMode = DefaultValidationMode;
};


// JSON MERGE =========================================================================================================


/**
 * @brief Merge @p overrides into @p base in place.
 *
 * If a schema is provided, merge behavior per key is determined by the `"mergeStrategy"` field
 * inside `schema.properties.<key>`. If no strategy is found, or if `schema` is `nullptr`,
 * it falls back to the behavior defined by @p options.
 *
 * @param base The object to modify.
 * @param overrides The object providing overriding values.
 * @param schema Optional schema fragment containing per-property merge strategies.
 * @param options Fallback merge behavior flags.
 * @return Returns a JsonError if an unknown strategy is found in the schema.
 */
JsonStatus jsonMergeInplace(QJsonObject &base,
                            const QJsonObject &overrides,
                            const QJsonObject *schema = nullptr,
                            JsonMergeOptions options = DefaultMergeOptions);

/**
 * @brief Return a merged copy of @p base and @p overrides.
 *
 * This is a convenience wrapper around `json_merge_inplace()`. See that function for detailed behavior.
 */
JsonObjectResult jsonMerge(QJsonObject base,
                           const QJsonObject &overrides,
                           const QJsonObject *schema = nullptr,
                           JsonMergeOptions options = DefaultMergeOptions);


// JSON LOAD & PROCESS ================================================================================================


/**
 * @brief Load a JSON file from disk and return its top-level object.
 *
 * @param path Filesystem path to the JSON file.
 * @return `JsonLoadResult` containing either the parsed object or a `JsonError`.
 */
JsonObjectResult jsonLoad(const QString &path);

/**
 * @brief High-level pipeline to load, merge, and validate multiple JSON files.
 *
 * Behavior:
 * - Iterates through @p files and loads them via `json_load()`.
 * - Handles missing files based on `options.loadOptions`.
 * - Merges loaded files sequentially using `json_merge_inplace()`.
 * - Validates against @p schema based strictly on `options.validationMode`.
 *
 * @param files List of JSON file paths.
 * @param schema Optional JSON schema. Required if `validationMode` is not `None`, or if schema merge strategies are needed.
 * @param options Configuration struct dictating load, merge, and validation behavior.
 * @return `JsonProcessResult` containing the final processed object or an error trace.
 */
JsonProcessResult jsonLoadAndProcess(const QStringList &files,
                                     const QJsonObject *schema = nullptr,
                                     JsonPipelineOptions options = {});

} // namespace mmolch::qtutil
