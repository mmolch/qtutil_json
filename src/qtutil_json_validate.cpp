#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <cmath>

#include "mmolch/qtutil_json_error.h"
#include "mmolch/qtutil_json.h"

namespace mmolch::qtutil {

namespace {

// ---------------------------------------------------------------------------
// JSON POINTER HELPERS
// ---------------------------------------------------------------------------

static QString escapeJsonPointer(QString key)
{
    // Important: ~ must be replaced first so we don't double-escape the ~ in ~1
    key.replace(QLatin1String("~"), QLatin1String("~0"));
    key.replace(QLatin1String("/"), QLatin1String("~1"));
    return key;
}

// ---------------------------------------------------------------------------
// Validation context
// ---------------------------------------------------------------------------

struct ValidationContext {
    QJsonObject rootSchema;
    QList<JsonValidationError>* errors = nullptr;
    QSet<QString> visitedRefs = QSet<QString>();
    JsonValidationOptions options = JsonValidationOption::None;
};

static void addError(ValidationContext& ctx,
                     QString pointer,
                     QString message)
{
    if (!ctx.errors) return;
    ctx.errors->append(JsonValidationError{std::move(pointer), std::move(message)});
}

static QString jsonTypeName(const QJsonValue& v)
{
    switch (v.type()) {
    case QJsonValue::Null:   return QStringLiteral("null");
    case QJsonValue::Bool:   return QStringLiteral("boolean");
    case QJsonValue::Double: return QStringLiteral("number");
    case QJsonValue::String: return QStringLiteral("string");
    case QJsonValue::Array:  return QStringLiteral("array");
    case QJsonValue::Object: return QStringLiteral("object");
    default:                 return QStringLiteral("undefined");
    }
}

static bool isInteger(const QJsonValue& v)
{
    if (!v.isDouble())
        return false;

    double d = v.toDouble();
    return std::floor(d) == d;
}

// ---------------------------------------------------------------------------
// TYPE VALIDATION
// ---------------------------------------------------------------------------

static bool validateType(ValidationContext& ctx,
                         const QString& pointer,
                         const QJsonValue& instance,
                         const QJsonValue& typeKeyword)
{
    bool ok = true;

    if (typeKeyword.isString()) {
        const QString expected = typeKeyword.toString();
        const QString actual = jsonTypeName(instance);

        if (expected == QStringLiteral("integer")) {
            if (!isInteger(instance)) {
                addError(ctx, pointer,
                         QStringLiteral("Expected type 'integer' but got '%1'")
                             .arg(actual));
                ok = false;
            }
        } else if (expected != actual) {
            addError(ctx, pointer,
                     QStringLiteral("Expected type '%1' but got '%2'")
                         .arg(expected, actual));
            ok = false;
        }
    }
    else if (typeKeyword.isArray()) {
        const QString actual = jsonTypeName(instance);
        bool allowed = false;

        for (const QJsonValue& v : typeKeyword.toArray()) {
            if (!v.isString())
                continue;

            const QString t = v.toString();
            if (t == QStringLiteral("integer") && isInteger(instance)) {
                allowed = true;
                break;
            }
            if (t == actual) {
                allowed = true;
                break;
            }
        }

        if (!allowed) {
            addError(ctx, pointer,
                     QStringLiteral("Type '%1' is not allowed").arg(actual));
            ok = false;
        }
    }

    return ok;
}

// ---------------------------------------------------------------------------
// ENUM
// ---------------------------------------------------------------------------

static bool validateEnum(ValidationContext& ctx,
                         const QString& pointer,
                         const QJsonValue& instance,
                         const QJsonValue& enumKeyword)
{
    if (!enumKeyword.isArray())
        return true;

    for (const QJsonValue& v : enumKeyword.toArray()) {
        if (v == instance)
            return true;
    }

    addError(ctx, pointer, QStringLiteral("Value is not in enum"));
    return false;
}

// ---------------------------------------------------------------------------
// CONST
// ---------------------------------------------------------------------------

static bool validateConst(ValidationContext& ctx,
                          const QString& pointer,
                          const QJsonValue& instance,
                          const QJsonValue& constKeyword)
{
    if (instance != constKeyword) {
        addError(ctx, pointer, QStringLiteral("Value does not match const"));
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// NUMERIC
// ---------------------------------------------------------------------------

static bool validateNumeric(ValidationContext& ctx,
                            const QString& pointer,
                            const QJsonValue& instance,
                            const QJsonObject& schema)
{
    if (!instance.isDouble())
        return true;

    bool ok = true;
    const double value = instance.toDouble();

    auto check = [&](const char* key, auto pred, const QString& msg) {
        if (schema.contains(QString::fromLatin1(key))) {
            double limit = schema.value(QString::fromLatin1(key)).toDouble();
            if (!pred(value, limit)) {
                addError(ctx, pointer, msg.arg(value).arg(limit));
                ok = false;
            }
        }
    };

    check("minimum", [](double v, double m){ return v >= m; },
          QStringLiteral("Value %1 is less than minimum %2"));
    check("maximum", [](double v, double m){ return v <= m; },
          QStringLiteral("Value %1 is greater than maximum %2"));
    check("exclusiveMinimum", [](double v, double m){ return v > m; },
          QStringLiteral("Value %1 is not greater than exclusiveMinimum %2"));
    check("exclusiveMaximum", [](double v, double m){ return v < m; },
          QStringLiteral("Value %1 is not less than exclusiveMaximum %2"));

    if (schema.contains(QStringLiteral("multipleOf"))) {
        double m = schema.value(QStringLiteral("multipleOf")).toDouble();
        if (m != 0.0) {
            double q = value / m;
            if (std::fabs(q - std::round(q)) > 1e-12) {
                addError(ctx, pointer,
                         QStringLiteral("Value %1 is not a multipleOf %2")
                             .arg(value).arg(m));
                ok = false;
            }
        }
    }

    return ok;
}

// ---------------------------------------------------------------------------
// STRING
// ---------------------------------------------------------------------------

static bool validateString(ValidationContext& ctx,
                           const QString& pointer,
                           const QJsonValue& instance,
                           const QJsonObject& schema)
{
    if (!instance.isString())
        return true;

    bool ok = true;
    const QString s = instance.toString();
    const int len = s.size();

    if (schema.contains(QStringLiteral("minLength"))) {
        int minLen = schema.value(QStringLiteral("minLength")).toInt();
        if (len < minLen) {
            addError(ctx, pointer,
                     QStringLiteral("String length %1 is less than minLength %2")
                         .arg(len).arg(minLen));
            ok = false;
        }
    }

    if (schema.contains(QStringLiteral("maxLength"))) {
        int maxLen = schema.value(QStringLiteral("maxLength")).toInt();
        if (len > maxLen) {
            addError(ctx, pointer,
                     QStringLiteral("String length %1 is greater than maxLength %2")
                         .arg(len).arg(maxLen));
            ok = false;
        }
    }

    if (schema.contains(QStringLiteral("pattern"))) {
        QString pattern = schema.value(QStringLiteral("pattern")).toString();
        QRegularExpression re(pattern);

        if (!re.isValid()) {
            addError(ctx, pointer,
                     QStringLiteral("Invalid regex pattern in schema: %1")
                         .arg(pattern));
            ok = false;
        } else if (!re.match(s).hasMatch()) {
            addError(ctx, pointer,
                     QStringLiteral("String does not match pattern '%1'")
                         .arg(pattern));
            ok = false;
        }
    }

    return ok;
}

// ---------------------------------------------------------------------------
// FORWARD DECLARATIONS
// ---------------------------------------------------------------------------

static bool validateSchema(ValidationContext& ctx,
                           const QString& pointer,
                           const QJsonValue& instance,
                           const QJsonObject& schema);

static bool validateArray(ValidationContext& ctx,
                          const QString& pointer,
                          const QJsonValue& instance,
                          const QJsonObject& schema);

static bool validateObject(ValidationContext& ctx,
                           const QString& pointer,
                           const QJsonValue& instance,
                           const QJsonObject& schema);

// ---------------------------------------------------------------------------
// ARRAY
// ---------------------------------------------------------------------------

static bool validateArray(ValidationContext& ctx,
                          const QString& pointer,
                          const QJsonValue& instance,
                          const QJsonObject& schema)
{
    if (!instance.isArray())
        return true;

    bool ok = true;
    QJsonArray arr = instance.toArray();
    const int size = arr.size();

    if (!ctx.options.testFlag(JsonValidationOption::IgnoreMinConstraints)) {
        if (schema.contains(QStringLiteral("minItems"))) {
            int minItems = schema.value(QStringLiteral("minItems")).toInt();
            if (size < minItems) {
                addError(ctx, pointer,
                         QStringLiteral("Array size %1 is less than minItems %2")
                             .arg(size).arg(minItems));
                ok = false;
            }
        }
    }

    if (schema.contains(QStringLiteral("maxItems"))) {
        int maxItems = schema.value(QStringLiteral("maxItems")).toInt();
        if (size > maxItems) {
            addError(ctx, pointer,
                     QStringLiteral("Array size %1 is greater than maxItems %2")
                         .arg(size).arg(maxItems));
            ok = false;
        }
    }

    if (schema.contains(QStringLiteral("uniqueItems")) &&
        schema.value(QStringLiteral("uniqueItems")).toBool())
    {
        QSet<QByteArray> seen;
        for (int i = 0; i < size; ++i) {
            QJsonArray wrapper;
            wrapper.append(arr.at(i));
            QByteArray ba = QJsonDocument(wrapper).toJson(QJsonDocument::Compact);

            if (seen.contains(ba)) {
                addError(ctx, pointer,
                         QStringLiteral("Array items are not unique"));
                ok = false;
            }
            seen.insert(ba);
        }
    }

    if (schema.contains(QStringLiteral("items"))) {
        QJsonValue items = schema.value(QStringLiteral("items"));
        if (items.isObject()) {
            QJsonObject itemSchema = items.toObject();
            for (int i = 0; i < size; ++i) {
                QString childPointer = pointer + "/" + QString::number(i);
                if (!validateSchema(ctx, childPointer, arr.at(i), itemSchema))
                    ok = false;
            }
        }
        else if (items.isArray()) {
            QJsonArray tuple = items.toArray();
            int i = 0;

            for (; i < size && i < tuple.size(); ++i) {
                if (tuple.at(i).isObject()) {
                    QString childPointer = pointer + "/" + QString::number(i);
                    if (!validateSchema(ctx, childPointer, arr.at(i),
                                        tuple.at(i).toObject()))
                        ok = false;
                }
            }

            if (size > tuple.size() &&
                schema.contains(QStringLiteral("additionalItems")))
            {
                QJsonValue addItems = schema.value(QStringLiteral("additionalItems"));
                if (addItems.isBool() && !addItems.toBool()) {
                    addError(ctx, pointer,
                             QStringLiteral("Additional items are not allowed"));
                    ok = false;
                }
                else if (addItems.isObject()) {
                    QJsonObject addSchema = addItems.toObject();
                    for (; i < size; ++i) {
                        QString childPointer = pointer + "/" + QString::number(i);
                        if (!validateSchema(ctx, childPointer, arr.at(i), addSchema))
                            ok = false;
                    }
                }
            }
        }
    }

    return ok;
}

// ---------------------------------------------------------------------------
// OBJECT
// ---------------------------------------------------------------------------

static bool validateObject(ValidationContext& ctx,
                           const QString& pointer,
                           const QJsonValue& instance,
                           const QJsonObject& schema)
{
    if (!instance.isObject())
        return true;

    bool ok = true;
    QJsonObject obj = instance.toObject();

    if (!ctx.options.testFlag(JsonValidationOption::IgnoreMinConstraints)) {
        if (schema.contains(QStringLiteral("minProperties"))) {
            int minProps = schema.value(QStringLiteral("minProperties")).toInt();
            if (obj.size() < minProps) {
                addError(ctx, pointer,
                         QStringLiteral("Object has %1 properties, less than minProperties %2")
                             .arg(obj.size()).arg(minProps));
                ok = false;
            }
        }
    }

    if (schema.contains(QStringLiteral("maxProperties"))) {
        int maxProps = schema.value(QStringLiteral("maxProperties")).toInt();
        if (obj.size() > maxProps) {
            addError(ctx, pointer,
                     QStringLiteral("Object has %1 properties, greater than maxProperties %2")
                         .arg(obj.size()).arg(maxProps));
            ok = false;
        }
    }

    // required
    if (!ctx.options.testFlag(JsonValidationOption::IgnoreRequired)) {
        if (schema.contains(QStringLiteral("required"))) {
            QJsonArray req = schema.value(QStringLiteral("required")).toArray();
            for (const QJsonValue& v : std::as_const(req)) {
                if (!v.isString()) continue;
                QString key = v.toString();
                if (!obj.contains(key)) {
                    addError(ctx, pointer,
                             QStringLiteral("Missing required property '%1'").arg(key));
                    ok = false;
                }
            }
        }
    }

    // properties
    QSet<QString> knownProps;
    if (schema.contains(QStringLiteral("properties"))) {
        QJsonObject props = schema.value(QStringLiteral("properties")).toObject();
        for (auto it = props.begin(); it != props.end(); ++it) {
            knownProps.insert(it.key());
            if (obj.contains(it.key()) && it.value().isObject()) {
                QString childPointer = pointer + "/" + escapeJsonPointer(it.key());
                if (!validateSchema(ctx, childPointer,
                                    obj.value(it.key()),
                                    it.value().toObject()))
                    ok = false;
            }
        }
    }

    // additionalProperties
    if (schema.contains(QStringLiteral("additionalProperties"))) {
        QJsonValue addProps = schema.value(QStringLiteral("additionalProperties"));
        if (addProps.isBool() && !addProps.toBool()) {
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                if (!knownProps.contains(it.key())) {
                    addError(ctx, pointer,
                             QStringLiteral("Additional property '%1' is not allowed")
                                 .arg(it.key()));
                    ok = false;
                }
            }
        }
        else if (addProps.isObject()) {
            QJsonObject addSchema = addProps.toObject();
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                if (!knownProps.contains(it.key())) {
                    QString childPointer = pointer + "/" + escapeJsonPointer(it.key());
                    if (!validateSchema(ctx, childPointer, it.value(), addSchema))
                        ok = false;
                }
            }
        }
    }

    return ok;
}

// ---------------------------------------------------------------------------
// LOGICAL COMBINERS
// ---------------------------------------------------------------------------

static bool validateLogicalCombiner(ValidationContext& ctx,
                                    const QString& pointer,
                                    const QJsonValue& instance,
                                    const QJsonObject& schema)
{
    bool ok = true;

    // allOf
    if (schema.contains(QStringLiteral("allOf"))) {
        QJsonArray arr = schema.value(QStringLiteral("allOf")).toArray();
        for (const QJsonValue& sub : std::as_const(arr)) {
            if (sub.isObject()) {
                if (!validateSchema(ctx, pointer, instance, sub.toObject()))
                    ok = false;
            }
        }
    }

    // anyOf
    if (schema.contains(QStringLiteral("anyOf"))) {
        QJsonArray arr = schema.value(QStringLiteral("anyOf")).toArray();
        bool matched = false;

        for (const QJsonValue& sub : std::as_const(arr)) {
            if (!sub.isObject())
                continue;

            QList<JsonValidationError> local;
            ValidationContext localCtx{ctx.rootSchema, &local};

            if (validateSchema(localCtx, pointer, instance, sub.toObject())) {
                matched = true;
                break;
            }
        }

        if (!matched) {
            addError(ctx, pointer,
                     QStringLiteral("Value does not match anyOf subschemas"));
            ok = false;
        }
    }

    // oneOf
    if (schema.contains(QStringLiteral("oneOf"))) {
        QJsonArray arr = schema.value(QStringLiteral("oneOf")).toArray();
        int count = 0;

        for (const QJsonValue& sub : std::as_const(arr)) {
            if (!sub.isObject())
                continue;

            QList<JsonValidationError> local;
            ValidationContext localCtx{ctx.rootSchema, &local};

            if (validateSchema(localCtx, pointer, instance, sub.toObject()))
                ++count;
        }

        if (count != 1) {
            addError(ctx, pointer,
                     QStringLiteral("Value must match exactly one of oneOf subschemas (matched %1)")
                         .arg(count));
            ok = false;
        }
    }

    // not
    if (schema.contains(QStringLiteral("not"))) {
        QJsonObject sub = schema.value(QStringLiteral("not")).toObject();
        QList<JsonValidationError> local;
        ValidationContext localCtx{ctx.rootSchema, &local};

        if (validateSchema(localCtx, pointer, instance, sub)) {
            addError(ctx, pointer,
                     QStringLiteral("Value must not match 'not' subschema"));
            ok = false;
        }
    }

    return ok;
}

// ---------------------------------------------------------------------------
// $ref RESOLUTION
// ---------------------------------------------------------------------------

static bool resolveRef(const ValidationContext& ctx,
                       const QString& ref,
                       QJsonObject& out)
{
    if (!ref.startsWith('#'))
        return false;

    QString pointer = ref.mid(1);
    if (pointer.isEmpty()) {
        out = ctx.rootSchema;
        return true;
    }

    if (!pointer.startsWith('/'))
        return false;

    QJsonValue current = ctx.rootSchema;
    pointer = pointer.mid(1);
    QStringList tokens = pointer.split('/');

    for (QString token : std::as_const(tokens)) {
        // RFC 6901: unescape ~1 to / and ~0 to ~
        token.replace("~1", "/");
        token.replace("~0", "~");

        if (!current.isObject())
            return false;

        QJsonObject obj = current.toObject();
        if (!obj.contains(token))
            return false;

        current = obj.value(token);
    }

    if (!current.isObject())
        return false;

    out = current.toObject();
    return true;
}

// ---------------------------------------------------------------------------
// MAIN VALIDATION DISPATCH
// ---------------------------------------------------------------------------

bool validateSchema(ValidationContext &ctx,
                    const QString &pointer,
                    const QJsonValue &instance,
                    const QJsonObject &schema)
{
    bool ok = true;

    // $ref (simple local)
    if (schema.contains(QStringLiteral("$ref"))) {
        QString ref = schema.value(QStringLiteral("$ref")).toString();
        if (ctx.visitedRefs.contains(ref)) {
            // Cycle detected, handle gracefully
            return false;
        }
        ctx.visitedRefs.insert(ref);
        QJsonObject target;

        if (!resolveRef(ctx, ref, target)) {
            addError(ctx, pointer,
                     QStringLiteral("Unable to resolve $ref '%1'").arg(ref));
            ok = false;
        }
        if (!validateSchema(ctx, pointer, instance, target)) {
            ok = false;
        }
        ctx.visitedRefs.remove(ref);
    };

    // type
    if (schema.contains(QStringLiteral("type"))) {
        if (!validateType(ctx, pointer, instance, schema.value(QStringLiteral("type"))))
            ok = false;
    }

    // enum
    if (schema.contains(QStringLiteral("enum"))) {
        if (!validateEnum(ctx, pointer, instance, schema.value(QStringLiteral("enum"))))
            ok = false;
    }

    // const
    if (schema.contains(QStringLiteral("const"))) {
        if (!validateConst(ctx, pointer, instance, schema.value(QStringLiteral("const"))))
            ok = false;
    }

    // numeric constraints
    if (!validateNumeric(ctx, pointer, instance, schema))
        ok = false;

    // string constraints
    if (!validateString(ctx, pointer, instance, schema))
        ok = false;

    // array constraints
    if (!validateArray(ctx, pointer, instance, schema))
        ok = false;

    // object constraints
    if (!validateObject(ctx, pointer, instance, schema))
        ok = false;

    // logical combiners
    if (!validateLogicalCombiner(ctx, pointer, instance, schema))
        ok = false;

    return ok;
}

} // namespace

JsonValidationStatus jsonValidate(const QJsonObject &object,
                                  const QJsonObject &schema,
                                  JsonValidationOptions options)
{
    QList<JsonValidationError> errors;
    ValidationContext ctx{schema, &errors, QSet<QString>(), options};
    const QString rootPointer;

    if (!validateSchema(ctx, rootPointer, object, schema)) {
        return std::unexpected(std::move(errors));
    }

    return {};
}

} // namespace mmolch::qtutil
