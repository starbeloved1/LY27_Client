#include "TerminalRuntime.hpp"
#include "MessageCodec.hpp"
#include "MqttTransport.hpp"
#include "rm2027.pb.h"

#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

class MqttTest : public QObject {
    Q_OBJECT
private:
    QTemporaryDir directory;
    QProcess broker;
    quint16 port = 0;
    QString brokerConfig;

    bool brokerReady()
    {
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, port);
        return socket.waitForConnected(50);
    }

    bool startBroker()
    {
        broker.start(LY27_BROKER_PATH, {"-c", brokerConfig});
        return broker.waitForStarted(3000);
    }

    ly27::NetworkConfig network(const QString &id) const
    {
        ly27::NetworkConfig config;
        config.mqttHost = "127.0.0.1";
        config.mqttPort = port;
        config.mqttClientId = id;
        return config;
    }

    bool ready(const ly27::MqttTransport &transport) const
    {
        return transport.snapshot()["state"] == "ready";
    }

private slots:
    void initTestCase()
    {
        QVERIFY(directory.isValid());
        QTcpServer reservation;
        QVERIFY(reservation.listen(QHostAddress::LocalHost, 0));
        port = reservation.serverPort();
        reservation.close();
        brokerConfig = directory.filePath("mosquitto.conf");
        QFile config(brokerConfig);
        QVERIFY(config.open(QIODevice::WriteOnly));
        config.write(QString("listener %1 127.0.0.1\nallow_anonymous true\npersistence false\n").arg(port).toUtf8());
        config.close();
        QVERIFY(startBroker());
        QTRY_VERIFY_WITH_TIMEOUT(brokerReady(), 5000);
    }

    void cleanupTestCase()
    {
        broker.terminate();
        if (!broker.waitForFinished(3000)) { broker.kill(); broker.waitForFinished(); }
    }

    void subscribePublishAndDirection()
    {
        ly27::MqttTransport subscriber, publisher;
        QSignalSpy received(&subscriber, &ly27::MqttTransport::messageReceived);
        QSignalSpy acknowledgments(&publisher, &ly27::MqttTransport::publishDelivered);
        QVERIFY(subscriber.start(network("subscriber"), ly27::MessageCodec::inboundTopics()));
        QVERIFY(publisher.start(network("publisher"), {}));
        QTRY_VERIFY_WITH_TIMEOUT(ready(subscriber) && ready(publisher), 5000);
        QCOMPARE(subscriber.snapshot()["subscribed_topics"].toInt(), 23);
        const auto bytes = QByteArray::fromHex("280430ac02");
        QVERIFY(publisher.publish("GameStatus", bytes));
        QTRY_COMPARE_WITH_TIMEOUT(received.count(), 1, 3000);
        QCOMPARE(received.front()[0].toString(), QString("GameStatus"));
        QCOMPARE(received.front()[1].toByteArray(), bytes);
        QVERIFY(received.front()[2].toLongLong() > 0);
        QVERIFY(received.front()[3].toLongLong() > 0);
        QVERIFY(!received.front()[5].toBool());
        QTRY_COMPARE_WITH_TIMEOUT(acknowledgments.count(), 1, 3000);
        QVERIFY(publisher.publish("AssemblyCommand", QByteArray::fromHex("08001002")));
        QTRY_COMPARE_WITH_TIMEOUT(acknowledgments.count(), 2, 3000);
        // 官方出站指令不在接收端订阅列表内。
        QTest::qWait(80);
        QCOMPARE(received.count(), 1);
        publisher.stop();
        QVERIFY(!publisher.publish("GameStatus", bytes));
    }

    void runtimeRecordsParsedMalformedAndOpaquePayloads()
    {
        ly27::TerminalConfig config;
        config.identity.stationId = "integration";
        config.identity.robotId = 3;
        config.network = network("runtime");
        config.logDirectory = directory.filePath("runtime-logs");
        ly27::TerminalRuntime runtime(config);
        QSignalSpy observed(&runtime, &ly27::TerminalRuntime::messageObserved);
        ly27::MqttTransport publisher;
        QVERIFY(runtime.start(true));
        QVERIFY(publisher.start(network("runtime-peer"), {}));
        QTRY_VERIFY_WITH_TIMEOUT(runtime.snapshot()["modules"].toObject()["mqtt"] == "ready" && ready(publisher), 5000);
        QVERIFY(publisher.publish("GameStatus", QByteArray::fromHex("280430ac02")));
        QVERIFY(publisher.publish("GameStatus", QByteArray::fromHex("80")));
        const auto radarBytes = QByteArray::fromHex("0a03080102");
        QVERIFY(publisher.publish("RadarInfoToClient", radarBytes));
        QTRY_COMPARE_WITH_TIMEOUT(observed.count(), 3, 3000);
        const auto status = runtime.snapshot()["protocol"].toObject();
        QCOMPARE(status["parsed"].toInt(), 1);
        QCOMPARE(status["rejected"].toInt(), 1);
        QCOMPARE(status["unresolved"].toInt(), 1);
        QVERIFY(runtime.stateStore().topic("GameStatus").isFresh());
        QCOMPARE(runtime.stateStore().topic("GameStatus").fields["stage_countdown_sec"].toInt(), 300);
        const auto match = runtime.stateStore().matchState();
        QCOMPARE(*match.stage.value, ly27::GameStage::Battle);
        QCOMPARE(*match.remainingSeconds.value, 300);
        QVERIFY(match.stage.isFresh());
        QCOMPARE(runtime.stateStore().topic("RadarInfoToClient").validity, QString("unknown"));
        runtime.stop();
        QCOMPARE(runtime.stateStore().topic("GameStatus").validity, QString("disconnected"));
        QCOMPARE(runtime.stateStore().matchState().stage.source.validity, ly27::StateValidity::Disconnected);
        QFile journal(status["journal"].toString());
        QVERIFY(journal.open(QIODevice::ReadOnly));
        const auto lines = journal.readAll().trimmed().split('\n');
        QCOMPARE(lines.size(), 3);
        const auto last = QJsonDocument::fromJson(lines.last()).object();
        QCOMPARE(last["status"].toString(), QString("unresolved_schema"));
        QCOMPARE(QByteArray::fromBase64(last["payload_base64"].toString().toLatin1()), radarBytes);
        QCOMPARE(last["session_id"].toString(), runtime.snapshot()["session_id"].toString());
        const auto oldSession = runtime.snapshot()["session_id"].toString();
        QVERIFY(runtime.start(true));
        QVERIFY(runtime.snapshot()["session_id"].toString() != oldSession);
        QCOMPARE(runtime.snapshot()["protocol"].toObject()["received"].toInt(), 0);
        QCOMPARE(runtime.stateStore().topic("GameStatus").validity, QString("unknown"));
        QVERIFY(!runtime.stateStore().matchState().stage.value);
        runtime.stop();
    }

    void reconnectAndResubscribe()
    {
        ly27::TerminalConfig config;
        config.identity.stationId = "reconnect";
        config.identity.robotId = 3;
        config.network = network("reconnect-runtime");
        config.logDirectory = directory.filePath("reconnect-logs");
        ly27::TerminalRuntime runtime(config);
        QVERIFY(runtime.start(true));
        ly27::MqttTransport subscriber;
        QSignalSpy messages(&subscriber, &ly27::MqttTransport::messageReceived);
        QVERIFY(subscriber.start(network("reconnect-subscriber"), {"GameStatus"}));
        QTRY_VERIFY_WITH_TIMEOUT(ready(subscriber) && runtime.snapshot()["modules"].toObject()["mqtt"] == "ready", 5000);
        {
            ly27::MqttTransport publisher;
            QVERIFY(publisher.start(network("before-reconnect-peer"), {}));
            QTRY_VERIFY_WITH_TIMEOUT(ready(publisher), 5000);
            QVERIFY(publisher.publish("GameStatus", QByteArray::fromHex("2804")));
            QTRY_VERIFY_WITH_TIMEOUT(runtime.stateStore().topic("GameStatus").isFresh(), 3000);
            QTRY_COMPARE_WITH_TIMEOUT(messages.count(), 1, 3000);
        }
        broker.kill();
        QVERIFY(broker.waitForFinished(3000));
        QTRY_VERIFY_WITH_TIMEOUT(!ready(subscriber), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(runtime.stateStore().topic("GameStatus").validity, QString("disconnected"), 5000);
        QVERIFY(startBroker());
        QTRY_VERIFY_WITH_TIMEOUT(brokerReady(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(ready(subscriber) && runtime.snapshot()["modules"].toObject()["mqtt"] == "ready", 12000);
        QCOMPARE(runtime.stateStore().topic("GameStatus").reason, QString("previous_connection"));
        QVERIFY(subscriber.snapshot()["successful_connections"].toInt() >= 2);
        QCOMPARE(subscriber.snapshot()["subscribed_topics"].toInt(), 1);
        ly27::MqttTransport publisher;
        QVERIFY(publisher.start(network("reconnect-peer"), {}));
        QTRY_VERIFY_WITH_TIMEOUT(ready(publisher), 5000);
        QVERIFY(publisher.publish("GameStatus", QByteArray::fromHex("2804")));
        QTRY_COMPARE_WITH_TIMEOUT(messages.count(), 2, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(runtime.stateStore().topic("GameStatus").isFresh(), 3000);
    }

    void unavailableBrokerKeepsEventLoopResponsive()
    {
        QTcpServer reservation;
        QVERIFY(reservation.listen(QHostAddress::LocalHost, 0));
        const auto unavailablePort = reservation.serverPort();
        reservation.close();
        auto config = network("unavailable");
        config.mqttPort = unavailablePort;
        ly27::MqttTransport transport;
        int ticks = 0;
        QTimer timer;
        connect(&timer, &QTimer::timeout, this, [&ticks]() { ++ticks; });
        timer.start(10);
        QVERIFY(transport.start(config, {"GameStatus"}));
        QTRY_VERIFY_WITH_TIMEOUT(ticks >= 10, 2000);
        QTRY_VERIFY_WITH_TIMEOUT(!transport.snapshot()["last_error"].toString().isEmpty(), 5000);
        transport.stop();
        QCOMPARE(transport.snapshot()["state"].toString(), QString("stopped"));
    }

    void stopAndRestartRejectOldCallbacks()
    {
        ly27::MqttTransport transport;
        for (int i = 0; i < 5; ++i) {
            QVERIFY(transport.start(network("restart-" + QString::number(i)), {"GameStatus"}));
            transport.stop();
        }
        QCoreApplication::processEvents();
        QCOMPARE(transport.snapshot()["state"].toString(), QString("stopped"));
        QVERIFY(transport.start(network("restart-final"), {"Event"}));
        QTRY_VERIFY_WITH_TIMEOUT(ready(transport), 5000);
        QCOMPARE(transport.snapshot()["successful_connections"].toInt(), 1);
        QCOMPARE(transport.snapshot()["subscribed_topics"].toInt(), 1);
    }

    void rejectInvalidTransportRequests()
    {
        ly27::MqttTransport transport;
        auto config = network("");
        QVERIFY(!transport.start(config, {"GameStatus"}));
        QCOMPARE(transport.snapshot()["state"].toString(), QString("error"));
        config.mqttClientId = QString::fromUtf8("bad\0id", 6);
        QVERIFY(!transport.start(config, {"GameStatus"}));
        config.mqttClientId = "valid";
        QVERIFY(!transport.start(config, {"#"}));
        QVERIFY(transport.start(config, {}));
        QTRY_VERIFY_WITH_TIMEOUT(ready(transport), 5000);
        QVERIFY(!transport.publish("bad/+", {}));
        QVERIFY(!transport.publish("GameStatus", {}, 2));
        QVERIFY(!transport.publish("GameStatus", QByteArray(1024 * 1024 + 1, 'x')));
    }

    void cliReceivesOverRealBroker()
    {
        QFile example(LY27_EXAMPLE_CONFIG);
        QVERIFY(example.open(QIODevice::ReadOnly));
        auto config = QJsonDocument::fromJson(example.readAll()).object();
        auto settings = config["network"].toObject();
        settings["mqtt_port"] = port;
        settings["mqtt_client_id"] = "cli-integration";
        config["network"] = settings;
        config["logging"] = QJsonObject{{"directory", "cli-logs"}, {"level", "info"}};
        QFile file(directory.filePath("cli.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(config).toJson()); file.close();
        QProcess cli;
        cli.start(LY27_CLI_PATH, {"--config", file.fileName(), "--mqtt", "--watch", "--run-for", "1600"});
        QVERIFY(cli.waitForStarted(3000));
        QByteArray diagnostics;
        QElapsedTimer deadline; deadline.start();
        while (!diagnostics.contains("mqtt.state=ready") && deadline.elapsed() < 5000) {
            cli.waitForReadyRead(50);
            diagnostics += cli.readAllStandardError();
        }
        QVERIFY2(diagnostics.contains("mqtt.state=ready"), diagnostics.constData());
        ly27::MqttTransport publisher;
        QVERIFY(publisher.start(network("cli-peer"), {}));
        QTRY_VERIFY_WITH_TIMEOUT(ready(publisher), 3000);
        QVERIFY(publisher.publish("GameStatus", QByteArray::fromHex("280430ac02")));
        QTRY_VERIFY_WITH_TIMEOUT(publisher.snapshot()["delivered_messages"].toInt() > 0, 2000);
        QVERIFY(cli.waitForFinished(5000));
        QCOMPARE(cli.exitCode(), 0);
        const auto output = cli.readAllStandardOutput().trimmed();
        const auto observation = QJsonDocument::fromJson(output).object();
        QCOMPARE(observation["topic"].toString(), QString("GameStatus"));
        QCOMPARE(observation["status"].toString(), QString("parsed"));
        QCOMPARE(observation["fields"].toObject()["stage_countdown_sec"].toInt(), 300);
    }
};

QTEST_GUILESS_MAIN(MqttTest)
#include "test_mqtt.moc"
