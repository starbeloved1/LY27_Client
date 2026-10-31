#include "TerminalRuntime.hpp"
#include "CommandDispatcher.hpp"
#include "rm2027.pb.h"

#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

namespace {
ly27::TerminalIdentity identity(ly27::Role role, ly27::Team team = ly27::Team::Red)
{
    ly27::TerminalIdentity value;
    value.role = role; value.team = team; value.stationId = "commands-test";
    if (role != ly27::Role::Commander) value.robotId = (role == ly27::Role::Heavy ? 1 : 3) + (team == ly27::Team::Blue ? 100 : 0);
    return value;
}

struct Fixture {
    qint64 now = 1000000000;
    ly27::MqttTransport sender, observer;
    ly27::StateStore state;
    ly27::CommandDispatcher commands;
    QSignalSpy received;
    explicit Fixture(ly27::Role role, ly27::Team team = ly27::Team::Red)
        : state(identity(role, team), nullptr, [this] { return now; }),
          commands(identity(role, team), state, sender, nullptr, [this] { return now; }),
          received(&observer, &ly27::MqttTransport::messageReceived) {}

    bool start(ly27::NetworkConfig config, const QStringList &topics)
    {
        state.reset("test-session"); state.setConnected(true); commands.start("test-session");
        config.mqttClientId = "sender";
        if (!sender.start(config, {})) return false;
        config.mqttClientId = "observer";
        return observer.start(config, topics);
    }
    bool ready() const { return commands.isReady() && observer.snapshot()["state"] == "ready"; }
    bool update(const google::protobuf::Message &message)
    {
        const auto decoded = ly27::MessageCodec::decode(QString::fromStdString(message.GetDescriptor()->name()),
            QByteArray::fromStdString(message.SerializeAsString()));
        return state.apply(decoded, 1700000000000, now);
    }
    QJsonObject last() const { return commands.snapshot()["records"].toArray().last().toObject(); }
};
} // 命名空间

class CommandsTest : public QObject {
    Q_OBJECT
    QTemporaryDir directory;
    QProcess broker;
    quint16 port = 0;
    ly27::NetworkConfig network() const
    {
        ly27::NetworkConfig config;
        config.mqttHost = "127.0.0.1"; config.mqttPort = port; config.mqttClientId = "test";
        return config;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(directory.isValid());
        QTcpServer reservation;
        QVERIFY(reservation.listen(QHostAddress::LocalHost, 0));
        port = reservation.serverPort(); reservation.close();
        QFile file(directory.filePath("broker.conf"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QString("listener %1 127.0.0.1\nallow_anonymous true\npersistence false\n").arg(port).toUtf8()); file.close();
        broker.start(LY27_BROKER_PATH, {"-c", file.fileName()});
        QVERIFY(broker.waitForStarted(3000));
        QTRY_VERIFY_WITH_TIMEOUT(([&] { QTcpSocket socket; socket.connectToHost(QHostAddress::LocalHost, port); return socket.waitForConnected(30); })(), 3000);
    }

    void cleanupTestCase()
    {
        broker.terminate();
        if (!broker.waitForFinished(3000)) { broker.kill(); broker.waitForFinished(); }
    }

    void assemblyEncodingStateChecksAndSharedRateLimit()
    {
        Fixture f(ly27::Role::Heavy);
        QVERIFY(f.start(network(), {"AssemblyCommand"}));
        QTRY_VERIFY_WITH_TIMEOUT(f.ready(), 5000);
        QCOMPARE(f.commands.assembly({ly27::AssemblyOperation::Start, 2}).code, QString("state_unavailable"));
        ly27::wire::TechCoreMotionStateSync core;
        core.set_maximum_difficulty_level(1); QVERIFY(f.update(core));
        QCOMPARE(f.commands.assembly({ly27::AssemblyOperation::Start, 2}).code, QString("state_denied"));
        core.set_maximum_difficulty_level(4); QVERIFY(f.update(core));
        const auto submission = f.commands.assembly({ly27::AssemblyOperation::Start, 2});
        QVERIFY(submission.accepted);
        QCOMPARE(f.commands.assembly({ly27::AssemblyOperation::Confirm, 2}).code, QString("rate_limited"));
        QTRY_COMPARE_WITH_TIMEOUT(f.received.count(), 1, 3000);
        QCOMPARE(f.received.front()[1].toByteArray().toHex(), QByteArray("08001002"));
        ly27::wire::AssemblyCommand message;
        QVERIFY(message.ParseFromString(f.received.front()[1].toByteArray().toStdString()));
        QVERIFY(message.has_operation()); QCOMPARE(message.operation(), 0u);
        f.now += 99000000;
        QCOMPARE(f.commands.assembly({ly27::AssemblyOperation::Cancel, 2}).retryAfterMs, qint64(1));
        f.now += 1000000;
        QVERIFY(f.commands.assembly({ly27::AssemblyOperation::Cancel, 2}).accepted);
        QTRY_COMPARE_WITH_TIMEOUT(f.received.count(), 2, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(f.last()["transport"].toString(), QString("delivered"), 3000);
        QCOMPARE(f.last()["execution"].toString(), QString("not_confirmed"));
        f.sender.stop();
        f.now += 100000000;
        QCOMPARE(f.commands.assembly({ly27::AssemblyOperation::Cancel, 2}).code, QString("not_ready"));
        f.commands.stop();
        QCOMPARE(f.commands.assembly({ly27::AssemblyOperation::Cancel, 2}).code, QString("not_running"));
        f.commands.start("next-session");
        QVERIFY(f.commands.snapshot()["records"].toArray().isEmpty());
    }

    void commonFlagsQuantitiesAndNoOptimisticStateUpdate()
    {
        Fixture f(ly27::Role::Infantry);
        QVERIFY(f.start(network(), {"CommonCommand"}));
        QTRY_VERIFY_WITH_TIMEOUT(f.ready(), 5000);
        QCOMPARE(f.commands.common({ly27::CommonAction::Exchange17Ammo, 11}).code, QString("invalid_argument"));
        QCOMPARE(f.commands.common({ly27::CommonAction::Exchange42Ammo, 2}).code, QString("role_denied"));
        QCOMPARE(f.commands.common({ly27::CommonAction::RemoteHeal, {}}).code, QString("state_unavailable"));
        ly27::wire::RobotDynamicStatus dynamic;
        dynamic.set_can_remote_heal(false); dynamic.set_can_remote_ammo(true); dynamic.set_remaining_ammo(3);
        QVERIFY(f.update(dynamic));
        QCOMPARE(f.commands.common({ly27::CommonAction::RemoteHeal, {}}).code, QString("state_denied"));
        QVERIFY(f.commands.common({ly27::CommonAction::Exchange17Ammo, 20}).accepted);
        QCOMPARE(f.commands.common({ly27::CommonAction::RemoteAmmo, {}}).code, QString("rate_limited"));
        f.now += 100000000;
        QVERIFY(f.commands.common({ly27::CommonAction::RemoteAmmo, {}}).accepted);
        QTRY_COMPARE_WITH_TIMEOUT(f.received.count(), 2, 3000);
        ly27::wire::CommonCommand remote;
        QVERIFY(remote.ParseFromString(f.received.last()[1].toByteArray().toStdString()));
        QCOMPARE(remote.cmd_type(), 5u); QVERIFY(!remote.has_param());
        QCOMPARE(*f.state.self()->remainingAmmo.value, 3u);
        ly27::wire::RobotRespawnStatus respawn;
        respawn.set_can_free_respawn(true); respawn.set_can_pay_for_respawn(false);
        QVERIFY(f.update(respawn));
        f.now += 100000000;
        QVERIFY(f.commands.common({ly27::CommonAction::ConfirmRespawn, {}}).accepted);
        QCOMPARE(f.commands.common({ly27::CommonAction::BuyRespawn, {}}).code, QString("state_denied"));
        f.now += 500000000;
        QCOMPARE(f.commands.common({ly27::CommonAction::RemoteAmmo, {}}).code, QString("state_unavailable"));
    }

    void commanderZeroValuesAndServerResultAreSeparate()
    {
        Fixture f(ly27::Role::Commander);
        QVERIFY(f.start(network(), {"RobotPerformanceSelectionCommand", "SentryCtrlCommand"}));
        QTRY_VERIFY_WITH_TIMEOUT(f.ready(), 5000);
        QVERIFY(f.commands.selectSentryMode(ly27::SentryControlMode::Automatic).accepted);
        QTRY_COMPARE_WITH_TIMEOUT(f.received.count(), 1, 3000);
        QCOMPARE(f.received.front()[1].toByteArray().toHex(), QByteArray("1800"));
        QCOMPARE(f.commands.selectPerformance({ly27::ShooterPerformance::Burst, {}}).code, QString("role_denied"));
        QCOMPARE(f.commands.sentry(static_cast<ly27::SentryAction>(2)).code, QString("invalid_argument"));
        QVERIFY(f.commands.sentry(ly27::SentryAction::Move).accepted);
        QTRY_COMPARE_WITH_TIMEOUT(f.received.count(), 2, 3000);
        ly27::wire::SentryCtrlCommand command;
        QVERIFY(command.ParseFromString(f.received.last()[1].toByteArray().toStdString()));
        QCOMPARE(command.command_id(), 9u);
        QTRY_COMPARE_WITH_TIMEOUT(f.last()["transport"].toString(), QString("delivered"), 3000);
        ly27::wire::SentryCtrlResult result;
        result.set_command_id(9); result.set_result_code(0);
        const auto decoded = ly27::MessageCodec::decode("SentryCtrlResult", QByteArray::fromStdString(result.SerializeAsString()));
        f.commands.observeServerResult(decoded, f.now, true);
        QVERIFY(f.commands.snapshot()["last_server_result"].toObject().isEmpty());
        f.commands.observeServerResult(decoded, f.now, false);
        QCOMPARE(f.commands.snapshot()["last_server_result"].toObject()["correlation"].toString(), QString("unavailable_no_request_id"));
        QCOMPARE(f.last()["execution"].toString(), QString("not_confirmed"));
    }

    void runeAndDartStateChecks()
    {
        {
            Fixture f(ly27::Role::Infantry);
            QVERIFY(f.start(network(), {"RuneActivateCommand"}));
            QTRY_VERIFY_WITH_TIMEOUT(f.ready(), 5000);
            QCOMPARE(f.commands.activateRune().code, QString("state_unavailable"));
            ly27::wire::RuneStatusSync rune;
            rune.set_rune_status(2); QVERIFY(f.update(rune));
            QCOMPARE(f.commands.activateRune().code, QString("state_denied"));
            rune.set_rune_status(1); QVERIFY(f.update(rune));
            QVERIFY(f.commands.activateRune().accepted);
            QTRY_COMPARE_WITH_TIMEOUT(f.received.count(), 1, 3000);
            QCOMPARE(f.received.front()[1].toByteArray().toHex(), QByteArray("0801"));
        }
        Fixture f(ly27::Role::Commander);
        QVERIFY(f.start(network(), {"DartCommand"}));
        QTRY_VERIFY_WITH_TIMEOUT(f.ready(), 5000);
        QCOMPARE(f.commands.dart({ly27::DartTarget::Outpost, {}, {}}).code, QString("state_unavailable"));
        QVERIFY(f.commands.dart({{}, false, {}}).accepted);
        QTRY_COMPARE_WITH_TIMEOUT(f.received.count(), 1, 3000);
        QCOMPARE(f.received.front()[1].toByteArray().toHex(), QByteArray("1000"));
        ly27::wire::DartSelectTargetStatusSync gate;
        gate.set_open(0); QVERIFY(f.update(gate)); f.now += 100000000;
        QVERIFY(f.commands.dart({ly27::DartTarget::BaseFixed, {}, {}}).accepted);
        gate.set_open(1); QVERIFY(f.update(gate));
        QCOMPARE(f.commands.dart({{}, {}, true}).code, QString("state_denied"));
        gate.set_open(2); QVERIFY(f.update(gate)); f.now += 100000000;
        QVERIFY(f.commands.dart({{}, {}, true}).accepted);
        QCOMPARE(f.commands.dart({{}, false, true}).code, QString("invalid_argument"));
        QTRY_COMPARE_WITH_TIMEOUT(f.received.count(), 3, 3000);
        QCOMPARE(f.received.last()[1].toByteArray().toHex(), QByteArray("1801"));
    }

    void mapEncodingRoleRatesAndTargets()
    {
        {
            Fixture f(ly27::Role::Commander, ly27::Team::Blue);
            QVERIFY(f.start(network(), {"MapClickCmd"}));
            QTRY_VERIFY_WITH_TIMEOUT(f.ready(), 5000);
            ly27::MapRequest request;
            request.receivers = {101, 107}; request.marker = ly27::MapMarker::Custom; request.ascii = 'C';
            request.target = ly27::MapPosition{0, 1.5f};
            QVERIFY(f.commands.map(request).accepted);
            QCOMPARE(f.commands.map(request).retryAfterMs, qint64(500));
            QTRY_COMPARE_WITH_TIMEOUT(f.received.count(), 1, 3000);
            ly27::wire::MapClickCmd message;
            QVERIFY(message.ParseFromString(f.received.front()[1].toByteArray().toStdString()));
            QCOMPARE(QByteArray::fromStdString(message.robot_id()).toHex(), QByteArray("656b0000000000"));
            QCOMPARE(message.mode(), 4u); QCOMPARE(message.type(), 1u);
            QVERIFY(message.has_map_x()); QCOMPARE(message.map_x(), 0.0f);
            request.target = ly27::MapEnemy{1}; f.now += 500000000;
            QVERIFY(f.commands.map(request).accepted);
            QTRY_COMPARE_WITH_TIMEOUT(f.received.count(), 2, 3000);
            QVERIFY(message.ParseFromString(f.received.last()[1].toByteArray().toStdString()));
            QCOMPARE(message.type(), 2u); QCOMPARE(message.enemy_id(), 1u); QCOMPARE(message.map_y(), 0.0f);
            request.ascii = 'M'; QCOMPARE(f.commands.map(request).code, QString("invalid_argument"));
            request.ascii = 'C'; request.target = ly27::MapEnemy{101};
            QCOMPARE(f.commands.map(request).code, QString("invalid_argument"));
        }
        Fixture f(ly27::Role::Infantry);
        QVERIFY(f.start(network(), {"MapClickCmd"}));
        QTRY_VERIFY_WITH_TIMEOUT(f.ready(), 5000);
        ly27::MapRequest request;
        request.receivers = {3}; request.target = ly27::MapPosition{2, 3};
        QVERIFY(f.commands.map(request).accepted);
        f.now += 2999000000LL;
        QCOMPARE(f.commands.map(request).retryAfterMs, qint64(1));
        f.now += 1000000;
        QVERIFY(f.commands.map(request).accepted);
        request.receivers = {4}; QCOMPARE(f.commands.map(request).code, QString("role_denied"));
    }

    void jsonBoundaryRejectsInvalidTypesAndUnknownFields()
    {
        const QList<QByteArray> invalid = {
            R"({"command":"performance","shooter":"wrong"})",
            R"({"command":"common","action":"exchange-17","amount":11})",
            R"({"command":"common","action":"exchange-17","amount":true})",
            R"({"command":"assembly","operation":"start","difficulty":1.5})",
            R"({"command":"dart","open":0})", R"({"command":"rune","extra":1})",
            R"({"command":"map","range":"specified","marker":"attack","position":{"x":1,"y":2,"z":3}})"
        };
        for (const auto &json : invalid) {
            const auto result = ly27::parseCommandRequest(QJsonDocument::fromJson(json).object());
            QVERIFY2(!result.request && !result.error.isEmpty(), json.constData());
        }
        const auto request = ly27::parseCommandRequest({{"command", "dart"}, {"open", false}});
        QVERIFY(request.request);
        const auto dart = std::get<ly27::DartRequest>(*request.request);
        QVERIFY(dart.open.has_value()); QVERIFY(!*dart.open);
    }

    void failureAndLateAckStayWithTheirOwnSubmission()
    {
        Fixture f(ly27::Role::Infantry);
        QVERIFY(f.start(network(), {"RobotPerformanceSelectionCommand"}));
        QTRY_VERIFY_WITH_TIMEOUT(f.ready(), 5000);
        QVERIFY(f.commands.selectPerformance({ly27::ShooterPerformance::Cooling, {}}).accepted);
        const auto token = f.last()["token"].toInt();
        // 注入一次带 token 的异步失败，验证不会被随后抵达的确认覆盖。
        f.sender.publishFailed(token, "模拟传输失败");
        QCOMPARE(f.last()["transport"].toString(), QString("failed"));
        QTRY_VERIFY_WITH_TIMEOUT(f.sender.snapshot()["delivered_messages"].toInt() == 1, 3000);
        QCOMPARE(f.last()["transport"].toString(), QString("failed"));
        f.now += 100000000;
        QVERIFY(f.commands.selectPerformance({ly27::ShooterPerformance::Burst, {}}).accepted);
        f.sender.publishDelivered(token);
        QCOMPARE(f.last()["transport"].toString(), QString("queued"));
        QTRY_COMPARE_WITH_TIMEOUT(f.last()["transport"].toString(), QString("delivered"), 3000);
        QCOMPARE(f.last()["execution"].toString(), QString("not_confirmed"));
    }

    void missingAckBecomesUncertainAndDisconnectDoesNotResubmit()
    {
        // 这个模拟 Broker 只完成 CONNECT，故意不给 PUBLISH 回 PUBACK。
        QTcpServer sink;
        QVERIFY(sink.listen(QHostAddress::LocalHost, 0));
        connect(&sink, &QTcpServer::newConnection, this, [&]() {
            auto *socket = sink.nextPendingConnection();
            auto buffer = std::make_shared<QByteArray>();
            auto acknowledged = std::make_shared<bool>(false);
            connect(socket, &QTcpSocket::readyRead, socket, [socket, buffer, acknowledged]() {
                buffer->append(socket->readAll());
                if (*acknowledged || buffer->size() < 2) return;
                int remaining = 0, multiplier = 1, index = 1;
                unsigned char byte;
                do {
                    if (index >= buffer->size()) return;
                    byte = static_cast<unsigned char>((*buffer)[index++]);
                    remaining += (byte & 127) * multiplier; multiplier *= 128;
                } while (byte & 128);
                if (buffer->size() < index + remaining) return;
                *acknowledged = true; buffer->clear(); socket->write(QByteArray::fromHex("20020000"));
            });
        });
        qint64 now = 1000000000;
        ly27::MqttTransport transport;
        ly27::StateStore state(identity(ly27::Role::Commander));
        ly27::CommandDispatcher commands(identity(ly27::Role::Commander), state, transport, nullptr, [&] { return now; });
        auto config = network(); config.mqttPort = sink.serverPort(); config.mqttClientId = "blackhole";
        commands.start("ack-timeout");
        QVERIFY(transport.start(config, {}));
        QTRY_VERIFY_WITH_TIMEOUT(commands.isReady(), 3000);
        QVERIFY(commands.sentry(ly27::SentryAction::Move).accepted);
        now += qint64(ly27::CommandDispatcher::AckTimeoutMs) * 1000000;
        QTRY_COMPARE_WITH_TIMEOUT(commands.snapshot()["pending"].toInt(), 0, 2000);
        const auto record = commands.snapshot()["records"].toArray().last().toObject();
        QCOMPARE(record["transport"].toString(), QString("uncertain"));
        QCOMPARE(record["execution"].toString(), QString("not_confirmed"));
        QVERIFY(commands.sentry(ly27::SentryAction::Attack).accepted);
        transport.stop();
        QCOMPARE(commands.snapshot()["pending"].toInt(), 0);
        QCOMPARE(commands.snapshot()["records"].toArray().last().toObject()["transport"].toString(), QString("uncertain"));
    }

    void cliSendsOneOperationAndWritesTxJournal()
    {
        ly27::MqttTransport observer;
        QSignalSpy received(&observer, &ly27::MqttTransport::messageReceived);
        auto settings = network(); settings.mqttClientId = "cli-command-observer";
        QVERIFY(observer.start(settings, {"RobotPerformanceSelectionCommand"}));
        QTRY_COMPARE_WITH_TIMEOUT(observer.snapshot()["state"].toString(), QString("ready"), 3000);
        QFile example(LY27_EXAMPLE_CONFIG); QVERIFY(example.open(QIODevice::ReadOnly));
        auto config = QJsonDocument::fromJson(example.readAll()).object();
        auto net = config["network"].toObject(); net["mqtt_port"] = port; net["mqtt_client_id"] = "cli-command";
        config["network"] = net; config["logging"] = QJsonObject{{"directory", "command-logs"}, {"level", "warning"}};
        QFile configFile(directory.filePath("terminal.json")); QVERIFY(configFile.open(QIODevice::WriteOnly));
        configFile.write(QJsonDocument(config).toJson()); configFile.close();
        QFile commandFile(directory.filePath("operation.json")); QVERIFY(commandFile.open(QIODevice::WriteOnly));
        commandFile.write(R"({"command":"performance","shooter":"burst","chassis":"power"})"); commandFile.close();
        QProcess cli;
        cli.start(LY27_CLI_PATH, {"--config", configFile.fileName(), "--mqtt", "--run-for", "1200", "--command-file", commandFile.fileName()});
        QVERIFY(cli.waitForFinished(5000));
        QVERIFY2(cli.exitCode() == 0, cli.readAllStandardError().constData());
        QTRY_COMPARE_WITH_TIMEOUT(received.count(), 1, 3000);
        ly27::wire::RobotPerformanceSelectionCommand message;
        QVERIFY(message.ParseFromString(received.front()[1].toByteArray().toStdString()));
        QCOMPARE(message.shooter(), 2u); QCOMPARE(message.chassis(), 2u); QVERIFY(!message.has_sentry_control());
        const auto output = cli.readAllStandardOutput();
        QVERIFY(output.contains("command_submitted")); QVERIFY(output.contains("delivered"));
        const auto files = QDir(directory.filePath("command-logs")).entryList({"mqtt_*.jsonl"}, QDir::Files);
        QCOMPARE(files.size(), 1);
        QFile journal(directory.filePath("command-logs/" + files.front())); QVERIFY(journal.open(QIODevice::ReadOnly));
        const auto records = journal.readAll().trimmed().split('\n');
        bool txFound = false;
        for (const auto &line : records) {
            const auto record = QJsonDocument::fromJson(line).object();
            if (record["direction"] == "tx") {
                txFound = true;
                QCOMPARE(QByteArray::fromBase64(record["payload_base64"].toString().toLatin1()), received.front()[1].toByteArray());
            }
        }
        QVERIFY(txFound);

        QTcpServer reservation;
        QVERIFY(reservation.listen(QHostAddress::LocalHost, 0));
        const auto unavailablePort = reservation.serverPort(); reservation.close();
        net["mqtt_port"] = unavailablePort; config["network"] = net;
        QVERIFY(configFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        configFile.write(QJsonDocument(config).toJson()); configFile.close();
        QProcess unavailable;
        unavailable.start(LY27_CLI_PATH, {"--config", configFile.fileName(), "--mqtt", "--run-for", "250", "--command-file", commandFile.fileName()});
        QVERIFY(unavailable.waitForFinished(5000));
        QCOMPARE(unavailable.exitCode(), 5);
        QVERIFY(unavailable.readAllStandardError().contains("没有缓存"));
        QVERIFY(!unavailable.readAllStandardOutput().contains("command_submitted"));
    }
};

QTEST_GUILESS_MAIN(CommandsTest)
#include "test_commands.moc"
