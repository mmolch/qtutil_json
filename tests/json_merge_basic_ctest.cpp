#include <QtTest>
#include "mmolch/qtutil_json.h"

using namespace mmolch::qtutil;

class TestJsonMergeBasic : public QObject {
    Q_OBJECT
private slots:
    void test();
};

void TestJsonMergeBasic::test()
{
    QFile f1("tests/data/valid_base.json");
    QFile f2("tests/data/valid_override.json");
    QVERIFY(f1.open(QIODevice::ReadOnly));
    QVERIFY(f2.open(QIODevice::ReadOnly));

    QJsonObject base = QJsonDocument::fromJson(f1.readAll()).object();
    QJsonObject override = QJsonDocument::fromJson(f2.readAll()).object();

    const auto merged = jsonMerge(base, override);
    QCOMPARE(merged.value().value("version").toInt(), 2);
    QVERIFY(merged.value().value("features").toObject().contains("extra"));
}

QTEST_MAIN(TestJsonMergeBasic)
#include "json_merge_basic_ctest.moc"
