#include <QtTest>
#include "mmolch/qtutil_json.h"

using namespace mmolch::qtutil;

class TestJsonLoadParseError : public QObject {
    Q_OBJECT
private slots:
    void test();
};

void TestJsonLoadParseError::test()
{
    auto result = jsonLoad("tests/data/invalid_json.json");
    QVERIFY(!result.has_value());
    QCOMPARE(result.error().code, JsonErrorCode::ParseError);
}

QTEST_MAIN(TestJsonLoadParseError)
#include "json_load_parse_error_ctest.moc"
