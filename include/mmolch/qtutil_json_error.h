#pragma once

#include <expected>
#include <QJsonObject>
#include <QString>
#include <QList>

namespace mmolch::qtutil {
    Q_NAMESPACE

enum JsonErrorCode {
    None = 0,
    FileNotFound,
    OpenFailed,
    ParseError,
    TopLevelNotObject,
    SchemaViolation,
    MergeError,
    RefResolutionFailed,
    InternalError
};
Q_ENUM_NS(JsonErrorCode)

struct JsonError
{
    explicit JsonError(JsonErrorCode code = JsonErrorCode::None,
                       QString message = QString())
        : code{code}
        , message{std::move(message)}
    {}
    JsonErrorCode code;
    QString message;
};

struct JsonValidationError
{
    explicit JsonValidationError(QString pointer, QString message)
        : pointer{std::move(pointer)}
        , message{std::move(message)}
    {}
    QString pointer;
    QString message;
};

struct JsonProcessError
{
    explicit JsonProcessError(JsonErrorCode code = JsonErrorCode::None,
                              QString message = QString(),
                              QList<JsonValidationError> validationErrors = QList<JsonValidationError>{})
        : code{code}
        , message{std::move(message)}
        , validationErrors{std::move(validationErrors)}
    {}
    JsonErrorCode code;
    QString message;
    QList<JsonValidationError> validationErrors;
};

/** @brief Success or a single operational JsonError. */
using JsonStatus           = std::expected<void, JsonError>;

/** @brief A QJsonObject or a single operational JsonError. */
using JsonObjectResult     = std::expected<QJsonObject, JsonError>;

/** @brief Success or a list of Schema validation violations. */
using JsonValidationStatus = std::expected<void, QList<JsonValidationError>>;

/** @brief A QJsonObject or a complex error trace (Load/Merge/Validation). */
using JsonProcessResult    = std::expected<QJsonObject, JsonProcessError>;

} // namespace mmolch::qtutil