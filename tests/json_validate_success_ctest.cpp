#include <QtTest>
#include "mmolch/qtutil_json.h"

using namespace mmolch::qtutil;

class TestJsonValidateSuccess : public QObject {
    Q_OBJECT
private slots:
    void test();
};

void TestJsonValidateSuccess::test()
{
    QFile f("tests/data/valid_base.json");
    QVERIFY(f.open(QIODevice::ReadOnly));
    QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();

    QFile s("tests/data/schema.json");
    QVERIFY(s.open(QIODevice::ReadOnly));
    QJsonObject schema = QJsonDocument::fromJson(s.readAll()).object();

    auto result = jsonValidate(obj, schema);
    QVERIFY(result);
}

QTEST_MAIN(TestJsonValidateSuccess)
#include "json_validate_success_ctest.moc"
