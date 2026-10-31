#include "TerminalConfig.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QtTest>

class ConfigTest : public QObject {
    Q_OBJECT
private:
    QJsonObject example() const
    {
        QFile file(LY27_EXAMPLE_CONFIG);
        if (!file.open(QIODevice::ReadOnly)) return {};
        return QJsonDocument::fromJson(file.readAll()).object();
    }

private slots:
    void exampleAndRelativePaths()
    {
        const auto result = ly27::loadConfig(LY27_EXAMPLE_CONFIG);
        QVERIFY2(result.config.has_value(), qPrintable(result.error));
        QCOMPARE(result.config->identity.role, ly27::Role::Infantry);
        QCOMPARE(*result.config->identity.robotId, 3);
        QCOMPARE(result.config->network.mqttPort, quint16(3333));
        const auto expected = QFileInfo(LY27_EXAMPLE_CONFIG).absoluteDir().absoluteFilePath("../logs");
        QCOMPARE(result.config->logDirectory, QDir::cleanPath(expected));
    }

    void legacyVideoDefaults()
    {
        auto root = example();
        auto network = root["network"].toObject();
        network.remove("video_bind_address");
        network.remove("video_source_address");
        root["network"] = network;
        const auto result = ly27::parseConfig(QJsonDocument(root).toJson(), LY27_EXAMPLE_CONFIG);
        QVERIFY2(result.config.has_value(), qPrintable(result.error));
        QCOMPARE(result.config->network.videoBindAddress, QString("127.0.0.1"));
        QVERIFY(result.config->network.videoSourceAddress.isEmpty());
    }

    void identities_data()
    {
        QTest::addColumn<QString>("role");
        QTest::addColumn<QString>("team");
        QTest::addColumn<int>("robotId");
        QTest::newRow("red-heavy") << "heavy" << "red" << 1;
        QTest::newRow("blue-heavy") << "heavy" << "blue" << 101;
        QTest::newRow("red-infantry-3") << "infantry" << "red" << 3;
        QTest::newRow("red-infantry-4") << "infantry" << "red" << 4;
        QTest::newRow("blue-infantry-3") << "infantry" << "blue" << 103;
        QTest::newRow("blue-infantry-4") << "infantry" << "blue" << 104;
        QTest::newRow("red-commander") << "commander" << "red" << 0;
        QTest::newRow("blue-commander") << "commander" << "blue" << 0;
    }

    void identities()
    {
        QFETCH(QString, role);
        QFETCH(QString, team);
        QFETCH(int, robotId);
        auto root = example();
        auto identity = root["identity"].toObject();
        identity["role"] = role;
        identity["team"] = team;
        identity["robot_id"] = robotId ? QJsonValue(robotId) : QJsonValue(QJsonValue::Null);
        root["identity"] = identity;
        auto network = root["network"].toObject();
        // 服务器身份显式配置，不根据角色自动推断。
        network["mqtt_client_id"] = "";
        root["network"] = network;
        const auto result = ly27::parseConfig(QJsonDocument(root).toJson(), LY27_EXAMPLE_CONFIG);
        QVERIFY2(result.config.has_value(), qPrintable(result.error));
        QCOMPARE(result.config->identity.robotId.has_value(), robotId != 0);
        QVERIFY(result.config->network.mqttClientId.isEmpty());
    }

    void rejectInvalidFields_data()
    {
        QTest::addColumn<QString>("section");
        QTest::addColumn<QString>("field");
        QTest::addColumn<QJsonValue>("value");
        QTest::newRow("unsupported-schema") << "" << "schema_version" << QJsonValue(2);
        QTest::newRow("unknown-root-field") << "" << "netwrok" << QJsonValue(QJsonObject());
        QTest::newRow("unknown-nested-field") << "network" << "mqtt_prt" << QJsonValue(3333);
        QTest::newRow("missing-field") << "network" << "mqtt_port" << QJsonValue(QJsonValue::Undefined);
        QTest::newRow("wrong-object-type") << "" << "network" << QJsonValue("localhost");
        QTest::newRow("unknown-role") << "identity" << "role" << QJsonValue("engineer");
        QTest::newRow("unknown-team") << "identity" << "team" << QJsonValue("green");
        QTest::newRow("empty-station") << "identity" << "station_id" << QJsonValue("  ");
        QTest::newRow("wrong-robot-role") << "identity" << "robot_id" << QJsonValue(1);
        QTest::newRow("wrong-team") << "identity" << "robot_id" << QJsonValue(103);
        QTest::newRow("missing-ground-robot") << "identity" << "robot_id" << QJsonValue(QJsonValue::Null);
        QTest::newRow("fractional-robot") << "identity" << "robot_id" << QJsonValue(3.5);
        QTest::newRow("zero-port") << "network" << "mqtt_port" << QJsonValue(0);
        QTest::newRow("large-port") << "network" << "video_port" << QJsonValue(65536);
        QTest::newRow("string-port") << "network" << "mqtt_port" << QJsonValue("3333");
        QTest::newRow("fractional-port") << "network" << "mqtt_port" << QJsonValue(3333.5);
        QTest::newRow("empty-host") << "network" << "mqtt_host" << QJsonValue(" ");
        QTest::newRow("wrong-client-type") << "network" << "mqtt_client_id" << QJsonValue(3);
        QTest::newRow("video-bind-type") << "network" << "video_bind_address" << QJsonValue(12);
        QTest::newRow("video-bind-hostname") << "network" << "video_bind_address" << QJsonValue("localhost");
        QTest::newRow("video-source-broadcast") << "network" << "video_source_address" << QJsonValue("255.255.255.255");
        QTest::newRow("video-source-any") << "network" << "video_source_address" << QJsonValue("0.0.0.0");
        QTest::newRow("video-source-multicast") << "network" << "video_source_address" << QJsonValue("239.0.0.1");
        QTest::newRow("empty-log-directory") << "logging" << "directory" << QJsonValue("");
        QTest::newRow("unknown-log-level") << "logging" << "level" << QJsonValue("verbose");
    }

    void rejectInvalidFields()
    {
        QFETCH(QString, section);
        QFETCH(QString, field);
        QFETCH(QJsonValue, value);
        auto root = example();
        if (section.isEmpty()) root[field] = value;
        else {
            auto object = root[section].toObject();
            object[field] = value;
            root[section] = object;
        }
        const auto result = ly27::parseConfig(QJsonDocument(root).toJson(), LY27_EXAMPLE_CONFIG);
        QVERIFY(!result.config.has_value());
        QVERIFY2(result.error.contains(field), qPrintable(result.error));
    }

    void commanderCannotMasqueradeAsGroundRobot()
    {
        auto root = example();
        auto identity = root["identity"].toObject();
        identity["role"] = "commander";
        root["identity"] = identity;
        const auto result = ly27::parseConfig(QJsonDocument(root).toJson(), LY27_EXAMPLE_CONFIG);
        QVERIFY(!result.config);
        QVERIFY(result.error.contains("must be null"));
    }

    void malformedDocuments()
    {
        for (const auto &bytes : {QByteArray("{"), QByteArray("[]"), QByteArray("{}")}) {
            const auto result = ly27::parseConfig(bytes, LY27_EXAMPLE_CONFIG);
            QVERIFY(!result.config);
            QVERIFY(!result.error.isEmpty());
        }
        QVERIFY(!ly27::loadConfig("/path/that/does/not/exist/terminal.json").config);
    }
};

QTEST_GUILESS_MAIN(ConfigTest)
#include "test_config.moc"
