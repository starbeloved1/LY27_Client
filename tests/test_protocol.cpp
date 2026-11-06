#include "MessageCodec.hpp"
#include "rm2027.pb.h"

#include <QJsonArray>
#include <QSet>
#include <QtTest>

class ProtocolTest : public QObject {
    Q_OBJECT
private slots:
    void manifestAndDescriptors()
    {
        const auto entries = ly27::MessageCodec::manifest()["messages"].toArray();
        QCOMPARE(entries.size(), 32);
        const auto topics = ly27::MessageCodec::inboundTopics();
        QCOMPARE(topics.size(), 23);
        QCOMPARE(QSet<QString>(topics.begin(), topics.end()).size(), 23);
        int provisional = 0;
        for (const auto &entry : entries) {
            const auto info = entry.toObject();
            const auto topic = info["topic"].toString();
            QCOMPARE(info["schema_status"].toString(), QString("defined"));
            if (info["schema_origin"] == "local_provisional") {
                QCOMPARE(topic, QString("RadarInfoToClient"));
                ++provisional;
            }
            const auto name = "ly27.wire." + info["message"].toString();
            const auto *descriptor = google::protobuf::DescriptorPool::generated_pool()
                ->FindMessageTypeByName(name.toStdString());
            QVERIFY2(descriptor, qPrintable(topic));
            for (int i = 0; i < descriptor->field_count(); ++i) {
                const auto *field = descriptor->field(i);
                QVERIFY(field->is_repeated() || field->has_presence());
            }
            if (info["direction"] == "inbound") {
                const auto decoded = ly27::MessageCodec::decode(topic, {});
                // 官方标量均为 optional，空消息合法，但不代表业务数据已经有效。
                if (topic == "RadarInfoToClient") QCOMPARE(decoded.status, QString("invalid_payload"));
                else QVERIFY2(decoded.isParsed(), qPrintable(topic + ": " + decoded.error));
            }
        }
        QCOMPARE(provisional, 1);
    }

    void knownWirePayloads()
    {
        // 第 5 字段为阶段 4，第 6 字段为剩余时间 300 秒。
        const auto decoded = ly27::MessageCodec::decode("GameStatus", QByteArray::fromHex("280430ac02"));
        QVERIFY(decoded.isParsed());
        QCOMPARE(decoded.fields["current_stage"].toInt(), 4);
        QCOMPARE(decoded.fields["stage_countdown_sec"].toInt(), 300);
        const auto *status = dynamic_cast<const ly27::wire::GameStatus *>(decoded.message.get());
        QVERIFY(status);
        QVERIFY(status->has_current_stage());
        QVERIFY(!status->has_game_result());

        // 装配命令显式发送 operation=0，避免零值字段被错误省略。
        ly27::wire::AssemblyCommand command;
        command.set_operation(0);
        command.set_difficulty(2);
        QString error;
        QCOMPARE(ly27::MessageCodec::encode("AssemblyCommand", command, error).toHex(), QByteArray("08001002"));
        QVERIFY(error.isEmpty());

        // 2027 新增双方控制度字段号分别为 15 和 16。
        const auto global = ly27::MessageCodec::decode("GlobalUnitStatus", QByteArray::fromHex("780a800114"));
        QVERIFY(global.isParsed());
        const auto *units = dynamic_cast<const ly27::wire::GlobalUnitStatus *>(global.message.get());
        QVERIFY(units);
        QCOMPARE(units->total_cp_ally(), 10u);
        QCOMPARE(units->total_cp_enemy(), 20u);
        QCOMPARE(ly27::wire::RobotDynamicStatus::descriptor()->FindFieldByNumber(4)->name(), std::string("reserved"));
    }

    void rawBytesAndLimits()
    {
        ly27::wire::CustomByteBlock block;
        const std::string bytes("\0\xff\0", 3);
        block.set_data(bytes);
        const auto result = ly27::MessageCodec::decode("CustomByteBlock", QByteArray::fromStdString(block.SerializeAsString()));
        QVERIFY(result.isParsed());
        QCOMPARE(QByteArray::fromBase64(result.fields["data"].toString().toLatin1()), QByteArray(bytes.data(), 3));
        block.set_data(std::string(301, 'x'));
        QCOMPARE(ly27::MessageCodec::decode("CustomByteBlock", QByteArray::fromStdString(block.SerializeAsString())).status,
                 QString("invalid_payload"));
        ly27::wire::CustomControl control;
        QString error;
        control.set_data(std::string(60, 'x'));
        ly27::MessageCodec::encode("CustomControl", control, error);
        QVERIFY(error.isEmpty());
        control.set_data(std::string(61, 'x'));
        ly27::MessageCodec::encode("CustomControl", control, error);
        QVERIFY(!error.isEmpty());
    }

    void provisionalRadarWireAndValidation()
    {
        // 固定线格式：外层字段 1、单体字段 x=1/y=2/标识=3，防止两端共同改错后互相自洽。
        const auto bytes = QByteArray::fromHex("0a06080710091800").repeated(8);
        const auto decoded = ly27::MessageCodec::decode("RadarInfoToClient", bytes);
        QVERIFY(decoded.isParsed());
        const auto *radar = dynamic_cast<const ly27::wire::RadarInfoToClient *>(decoded.message.get());
        QVERIFY(radar);
        QCOMPARE(radar->robot_info_size(), 8);
        QCOMPARE(radar->robot_info(0).target_pos_x(), quint32(7));
        QCOMPARE(radar->robot_info(0).target_pos_y(), quint32(9));
        QCOMPARE(QByteArray::fromStdString(radar->SerializeAsString()), bytes);
        auto wrong = *radar;
        wrong.mutable_robot_info()->RemoveLast();
        QCOMPARE(ly27::MessageCodec::decode("RadarInfoToClient", QByteArray::fromStdString(wrong.SerializeAsString())).status,
                 QString("invalid_payload"));
        wrong = *radar;
        wrong.mutable_robot_info(0)->clear_target_pos_y();
        QCOMPARE(ly27::MessageCodec::decode("RadarInfoToClient", QByteArray::fromStdString(wrong.SerializeAsString())).status,
                 QString("invalid_payload"));
        wrong = *radar;
        wrong.mutable_robot_info(0)->set_is_high_light(3);
        QCOMPARE(ly27::MessageCodec::decode("RadarInfoToClient", QByteArray::fromStdString(wrong.SerializeAsString())).status,
                 QString("invalid_payload"));
    }

    void invalidMessages()
    {
        QCOMPARE(ly27::MessageCodec::decode("GameStatus", QByteArray::fromHex("80")).status, QString("malformed"));
        QCOMPARE(ly27::MessageCodec::decode("Other", {}).status, QString("unknown_topic"));
        QCOMPARE(ly27::MessageCodec::decode("AssemblyCommand", {}).status, QString("wrong_direction"));
        QCOMPARE(ly27::MessageCodec::decode("GameStatus", QByteArray(1024 * 1024 + 1, 'x')).status, QString("oversized"));
        const auto radar = ly27::MessageCodec::decode("RadarInfoToClient", QByteArray::fromHex("0a03080102"));
        QCOMPARE(radar.status, QString("malformed"));
        QVERIFY(!radar.message);
        QVERIFY(radar.fields.isEmpty());
        ly27::wire::GameStatus wrong;
        QString error;
        ly27::MessageCodec::encode("AssemblyCommand", wrong, error);
        QVERIFY(!error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(ProtocolTest)
#include "test_protocol.moc"
