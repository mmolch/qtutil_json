#include <QtTest>
#include "mmolch/qtutil_json.h"
#include "mmolch/qtutil_json_pointer.h"

using namespace mmolch::qtutil;

class TestJsonValidateFailure : public QObject {
    Q_OBJECT
private slots:
    void test();
};

void TestJsonValidateFailure::test()
{
    QFile f("tests/data/invalid_schema.json");
    QVERIFY(f.open(QIODevice::ReadOnly));
    QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();

    QFile s("tests/data/schema.json");
    QVERIFY(s.open(QIODevice::ReadOnly));
    QJsonObject schema = QJsonDocument::fromJson(s.readAll()).object();

    auto result = jsonValidate(obj, schema);
    if (!result) {
        for (const auto &error : result.error()) {
            qInfo().noquote() << JsonPointer::create(error.pointer).value().resolve(obj);
            qInfo().noquote() << error.pointer << error.message;
        }
    }

    QVERIFY(!result);
    QCOMPARE(result.error().size(), 6);
}

QTEST_MAIN(TestJsonValidateFailure)
#include "json_validate_failure_ctest.moc"
