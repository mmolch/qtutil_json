#include <QtTest/QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "mmolch/qtutil_json.h"
#include "mmolch/qtutil_json_error.h"

using namespace mmolch::qtutil;

static QJsonObject parseObject(const QByteArray &json)
{
    QJsonDocument d = QJsonDocument::fromJson(json);
    return d.object();
}

class TestJsonValidate : public QObject
{
    Q_OBJECT

private slots:
    void testTypeValidation();
    void testStringConstraints();
    void testArrayAndItems();
    void testObjectConstraints();
    void testLogicalCombiners();
    void testRefResolutionAndCycle();
    void testPatternInvalidRegex();
    void testIntegerVsNumber();
    void testMultipleErrorsReported();
};

// -----------------------------
// Helpers for assertions
// -----------------------------
static QStringList errorPointers(const QList<JsonValidationError> &errs)
{
    QStringList out;
    for (const auto &e : errs) out << e.pointer;
    return out;
}

static QStringList errorMessages(const QList<JsonValidationError> &errs)
{
    QStringList out;
    for (const auto &e : errs) out << e.message;
    return out;
}

// -----------------------------
// Tests
// -----------------------------

void TestJsonValidate::testTypeValidation()
{
    QJsonObject schema = parseObject(R"({
        "type": "object",
        "properties": {
            "name": { "type": "string" },
            "age":  { "type": "integer" },
            "active": { "type": "boolean" }
        },
        "required": ["name","age"]
    })");

    QJsonObject good = parseObject(R"({"name":"Alice","age":30,"active":true})");
    auto resGood = jsonValidate(good, schema);
    QVERIFY(resGood.has_value());

    QJsonObject bad = parseObject(R"({"name":123,"age":30.5})");
    auto resBad = jsonValidate(bad, schema);
    QVERIFY(!resBad.has_value());
    auto errs = resBad.error();
    QVERIFY(!errs.isEmpty());
    // Expect at least a type error for /name and integer mismatch for /age
    QVERIFY(errorMessages(errs).join(" ").contains("Expected type 'string'") ||
            errorMessages(errs).join(" ").contains("integer"));
}

void TestJsonValidate::testStringConstraints()
{
    QJsonObject schema = parseObject(R"({
        "type": "string",
        "minLength": 2,
        "maxLength": 4,
        "pattern": "^[a-z]+$"
    })");

    // valid
    auto res1 = jsonValidate(QJsonObject{{"s", QJsonValue("abc")}},
                             parseObject(R"({
                                "type":"object",
                                "properties": { "s": { "type":"string", "minLength":2, "maxLength":4, "pattern":"^[a-z]+$" } }
                             })"));
    QVERIFY(res1.has_value());

    // too short
    auto res2 = jsonValidate(QJsonObject{{"s", QJsonValue("a")}},
                             parseObject(R"({
                                "type":"object",
                                "properties": { "s": { "type":"string", "minLength":2 } }
                             })"));
    QVERIFY(!res2.has_value());
    QVERIFY(errorMessages(res2.error()).join(" ").contains("minLength"));

    // pattern mismatch
    auto res3 = jsonValidate(QJsonObject{{"s", QJsonValue("Abc")}},
                             parseObject(R"({
                                "type":"object",
                                "properties": { "s": { "type":"string", "pattern":"^[a-z]+$" } }
                             })"));
    QVERIFY(!res3.has_value());
    QVERIFY(errorMessages(res3.error()).join(" ").contains("pattern"));
}

void TestJsonValidate::testArrayAndItems()
{
    // items as single schema
    auto schema1 = parseObject(R"({
        "type":"array",
        "items": { "type":"integer" },
        "minItems": 1,
        "uniqueItems": true
    })");

    auto ok = QJsonArray{1,2,3};
    QJsonObject inst1; inst1.insert("a", ok);
    auto res1 = jsonValidate(inst1, parseObject(R"({
        "type":"object",
        "properties": { "a": { "type":"array", "items": { "type":"integer" }, "uniqueItems": true } }
    })"));
    QVERIFY(res1.has_value());

    // duplicate items
    auto res2 = jsonValidate(QJsonObject{{"a", QJsonArray{1,1}}}, parseObject(R"({
        "type":"object",
        "properties": { "a": { "type":"array", "uniqueItems": true } }
    })"));
    QVERIFY(!res2.has_value());
    QVERIFY(errorMessages(res2.error()).join(" ").contains("unique"));
}

void TestJsonValidate::testObjectConstraints()
{
    auto schema = parseObject(R"({
        "type":"object",
        "properties": {
            "id": { "type":"integer" },
            "name": { "type":"string" }
        },
        "required": ["id"],
        "additionalProperties": false,
        "minProperties": 1
    })");

    // missing required
    auto res1 = jsonValidate(QJsonObject{{"name","x"}}, schema);
    QVERIFY(!res1.has_value());
    QVERIFY(errorMessages(res1.error()).join(" ").contains("Missing required"));

    // additional property
    auto res2 = jsonValidate(QJsonObject{{"id",1},{"extra",2}}, schema);
    QVERIFY(!res2.has_value());
    QVERIFY(errorMessages(res2.error()).join(" ").contains("Additional property"));
}

void TestJsonValidate::testLogicalCombiners()
{
    // allOf
    auto schemaAll = parseObject(R"({
        "allOf": [
            { "type":"number", "minimum": 0 },
            { "multipleOf": 2 }
        ]
    })");

    auto res1 = jsonValidate(QJsonObject{{"v", 4}}, parseObject(R"({
        "type":"object",
        "properties": { "v": { "allOf":[ { "type":"number", "minimum":0 }, { "multipleOf":2 } ] } }
    })"));
    QVERIFY(res1.has_value());

    auto res2 = jsonValidate(QJsonObject{{"v", 3}}, parseObject(R"({
        "type":"object",
        "properties": { "v": { "allOf":[ { "type":"number", "minimum":0 }, { "multipleOf":2 } ] } }
    })"));
    QVERIFY(!res2.has_value());

    // anyOf / oneOf / not
    auto schemaAny = parseObject(R"({
        "anyOf": [
            { "type":"string" },
            { "type":"integer" }
        ]
    })");

    auto rAny = jsonValidate(QJsonObject{{"x", "s"}}, parseObject(R"({
        "type":"object",
        "properties": { "x": { "anyOf":[ { "type":"string" }, { "type":"integer" } ] } }
    })"));
    QVERIFY(rAny.has_value());

    auto schemaNot = parseObject(R"({ "not": { "type":"string" } })");
    auto rNot = jsonValidate(QJsonObject{{"x", "s"}}, parseObject(R"({
        "type":"object",
        "properties": { "x": { "not": { "type":"string" } } }
    })"));
    QVERIFY(!rNot.has_value());
}

void TestJsonValidate::testRefResolutionAndCycle()
{
    // simple local $ref
    auto schema = parseObject(R"({
        "definitions": {
            "person": {
                "type":"object",
                "properties": {
                    "name": { "type":"string" }
                },
                "required": ["name"]
            }
        },
        "type":"object",
        "properties": {
            "p": { "$ref": "#/definitions/person" }
        }
    })");

    auto ok = jsonValidate(QJsonObject{{"p", QJsonObject{{"name","A"}}}}, schema);
    QVERIFY(ok.has_value());

    auto bad = jsonValidate(QJsonObject{{"p", QJsonObject{{}}}}, schema);
    QVERIFY(!bad.has_value());
    QVERIFY(errorMessages(bad.error()).join(" ").contains("Missing required"));

    // cycle detection: $ref to itself should produce an error (or at least not crash)
    auto cyc = parseObject(R"({
        "definitions": {
            "a": { "$ref": "#/definitions/a" }
        },
        "type":"object",
        "properties": { "x": { "$ref": "#/definitions/a" } }
    })");

    auto r = jsonValidate(QJsonObject{{"x", QJsonObject{}}}, cyc);
    QVERIFY(!r.has_value());
    QVERIFY(errorMessages(r.error()).join(" ").contains("Unable to resolve") ||
            !r.has_value()); // accept either resolution-failure or graceful cycle handling
}

void TestJsonValidate::testPatternInvalidRegex()
{
    // invalid regex in schema should produce a validation error (schema error)
    auto schema = parseObject(R"({
        "type":"object",
        "properties": {
            "s": { "type":"string", "pattern": "[" }
        }
    })");

    auto r = jsonValidate(QJsonObject{{"s","abc"}}, schema);
    QVERIFY(!r.has_value());
    QVERIFY(errorMessages(r.error()).join(" ").contains("Invalid regex"));
}

void TestJsonValidate::testIntegerVsNumber()
{
    auto schema = parseObject(R"({
        "type":"object",
        "properties": {
            "i": { "type": "integer" },
            "n": { "type": "number" }
        }
    })");

    auto r1 = jsonValidate(QJsonObject{{"i", 3}, {"n", 3.14}}, schema);
    QVERIFY(r1.has_value());

    // integer expected but provided 3.5
    auto r2 = jsonValidate(QJsonObject{{"i", 3.5}}, schema);
    QVERIFY(!r2.has_value());
    QVERIFY(errorMessages(r2.error()).join(" ").contains("integer"));
}

void TestJsonValidate::testMultipleErrorsReported()
{
    auto schema = parseObject(R"({
        "type":"object",
        "properties": {
            "a": { "type":"string", "minLength": 3 },
            "b": { "type":"integer" }
        },
        "required": ["a","b"]
    })");

    auto inst = parseObject(R"({"a":"x"})"); // missing b and a too short
    auto r = jsonValidate(inst, schema);
    QVERIFY(!r.has_value());
    auto errs = r.error();
    QVERIFY(errs.size() >= 2);
    QVERIFY(errorMessages(errs).join(" ").contains("minLength"));
    QVERIFY(errorMessages(errs).join(" ").contains("Missing required"));
}

QTEST_MAIN(TestJsonValidate)
#include "json_validate_ctest.moc"
