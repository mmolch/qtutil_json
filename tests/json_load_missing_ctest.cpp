#include <QMetaEnum>
#include <QtTest>
#include "mmolch/qtutil_json.h"

class TestJsonLoadMissing : public QObject {
    Q_OBJECT
private slots:
    void test();
};

using namespace mmolch::qtutil;

void TestJsonLoadMissing::test()
{
    auto result = jsonLoad("tests/data/does_not_exist.json");
    if (!result) {
        qWarning() << result.error().message;
    }

    QVERIFY(!result.has_value());
    QCOMPARE(result.error().code, JsonErrorCode::FileNotFound);
}

QTEST_MAIN(TestJsonLoadMissing)
#include "json_load_missing_ctest.moc"
