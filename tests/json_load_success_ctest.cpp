#include <QtTest>
#include "mmolch/qtutil_json.h"

using namespace mmolch::qtutil;

class TestJsonLoadSuccess : public QObject {
    Q_OBJECT
private slots:
    void test();
};

void TestJsonLoadSuccess::test()
{
    auto result = jsonLoad("tests/data/valid_base.json");
    QVERIFY(result.has_value());
    QCOMPARE(result->value("name").toString(), QString("BaseConfig"));
}

QTEST_MAIN(TestJsonLoadSuccess)
#include "json_load_success_ctest.moc"
