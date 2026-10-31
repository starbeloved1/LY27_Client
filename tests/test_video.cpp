#include "VideoAssembler.hpp"
#include "HevcDecoder.hpp"
#include "VideoReceiver.hpp"
#include "TerminalRuntime.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QUdpSocket>
#include <QtEndian>
#include <QtTest>

namespace {
QByteArray packet(quint16 frame, quint16 index, quint32 total, const QByteArray &data)
{
    QByteArray result(8, '\0');
    qToBigEndian(frame, result.data());
    qToBigEndian(index, result.data() + 2);
    qToBigEndian(total, result.data() + 4);
    return result + data;
}

QList<QByteArray> accessUnits()
{
    QFile file(LY27_VIDEO_FIXTURE);
    if (!file.open(QIODevice::ReadOnly)) return {};
    const auto bytes = file.readAll();
    QList<qsizetype> starts;
    for (qsizetype i = 0; i + 5 < bytes.size(); ++i) {
        // 测试码流明确使用四字节起始码的 AUD 分隔访问单元。
        if (bytes.mid(i, 4) == QByteArray::fromHex("00000001")
            && ((static_cast<unsigned char>(bytes[i + 4]) >> 1) & 0x3f) == 35) starts.append(i);
    }
    starts.append(bytes.size());
    QList<QByteArray> result;
    for (qsizetype i = 0; i + 1 < starts.size(); ++i)
        result.append(bytes.mid(starts[i], starts[i + 1] - starts[i]));
    return result;
}

void sendFrame(QUdpSocket &sender, quint16 port, quint16 id, const QByteArray &bytes)
{
    // 倒序发送，覆盖真实 UDP 乱序到达时的重组。
    const auto fragments = (bytes.size() + 1391) / 1392;
    for (qsizetype i = fragments; i-- > 0;)
        sender.writeDatagram(packet(id, static_cast<quint16>(i), static_cast<quint32>(bytes.size()),
            bytes.mid(i * 1392, 1392)), QHostAddress::LocalHost, port);
}
}

class VideoTest : public QObject {
    Q_OBJECT
private slots:
    void reassemblyAndWireValidation()
    {
        ly27::VideoAssembler assembler;
        QVERIFY(!assembler.push(packet(65535, 1, 6, "def"), 0, 100));
        QVERIFY(!assembler.push(packet(65535, 1, 6, "def"), 1, 101));
        auto frame = assembler.push(packet(65535, 0, 6, "abc"), 2, 102);
        QVERIFY(frame); QCOMPARE(frame->bytes, QByteArray("abcdef"));
        QCOMPARE(frame->receivedUtcMs, qint64(100));
        QVERIFY(frame->discontinuity);
        frame = assembler.push(packet(0, 1, 1, "x"), 3, 103);
        QVERIFY(frame); QVERIFY(!frame->discontinuity);
        QVERIFY(!assembler.push(packet(65535, 0, 1, "x"), 4, 104));
        QVERIFY(!assembler.push(packet(1, 0, 2, "a"), 5, 105));
        QVERIFY(!assembler.push(packet(1, 2, 2, "b"), 6, 106));
        QVERIFY(!assembler.push(QByteArray(1401, 'x'), 7, 107));
        QVERIFY(!assembler.push(packet(2, 0, ly27::VideoAssembler::MaxFrameBytes + 1, "x"), 8, 108));
        QVERIFY(!assembler.push(packet(2, 4096, 1, "x"), 9, 109));
        QCOMPARE(assembler.snapshot()["invalid_datagrams"].toInteger(), qint64(4));
        QCOMPARE(assembler.snapshot()["assembled_frames"].toInteger(), qint64(2));
    }

    void conflictsExpiryAndMemoryBounds()
    {
        ly27::VideoAssembler assembler;
        QVERIFY(!assembler.push(packet(1, 0, 6, "abc"), 0, 0));
        QVERIFY(!assembler.push(packet(1, 0, 6, "xyz"), 1, 1));
        QCOMPARE(assembler.snapshot()["pending_frames"].toInt(), 0);
        QVERIFY(!assembler.push(packet(2, 0, 6, "abc"), 2, 2));
        QVERIFY(!assembler.push(packet(2, 1, 7, "defg"), 3, 3));
        QCOMPARE(assembler.snapshot()["pending_frames"].toInt(), 0);
        for (int i = 0; i < 20; ++i)
            QVERIFY(!assembler.push(packet(static_cast<quint16>(i + 10), 0, 2, "a"), 10 + i, 10 + i));
        QCOMPARE(assembler.snapshot()["pending_frames"].toInt(), ly27::VideoAssembler::MaxPendingFrames);
        assembler.expire(200);
        QCOMPARE(assembler.snapshot()["pending_frames"].toInt(), 0);
        QCOMPARE(assembler.snapshot()["buffered_bytes"].toInt(), 0);
        auto frame = assembler.push(packet(100, 0, 1, "a"), 300, 300);
        QVERIFY(frame);
        QVERIFY(!assembler.push(packet(1, 0, 1, "b"), 301, 301));
        frame = assembler.push(packet(1, 0, 1, "b"), 1300, 1300);
        QVERIFY(frame); QVERIFY(frame->discontinuity);
    }

    void aggregateBufferLimit()
    {
        ly27::VideoAssembler assembler;
        for (int frame = 0; frame < 8; ++frame) {
            int remaining = ly27::VideoAssembler::MaxBufferedBytes / 8;
            quint16 index = 0;
            while (remaining > 0) {
                const int bytes = qMin(remaining, 1392);
                QVERIFY(!assembler.push(packet(static_cast<quint16>(frame), index++,
                    ly27::VideoAssembler::MaxFrameBytes, QByteArray(bytes, 'x')), 1, 1));
                remaining -= bytes;
            }
        }
        QCOMPARE(assembler.snapshot()["buffered_bytes"].toInt(), ly27::VideoAssembler::MaxBufferedBytes);
        QVERIFY(!assembler.push(packet(7, 2000, ly27::VideoAssembler::MaxFrameBytes, "x"), 1, 1));
        QVERIFY(assembler.snapshot()["buffered_bytes"].toInt() < ly27::VideoAssembler::MaxBufferedBytes);
        QCOMPARE(assembler.snapshot()["dropped_frames"].toInteger(), qint64(1));
    }

    void decodeAndRecoverAtKeyframe()
    {
        const auto units = accessUnits();
        QCOMPARE(units.size(), 6);
        ly27::HevcDecoder decoder;
        QString error;
        QVERIFY2(decoder.start(error), qPrintable(error));
        auto decode = [&](int index, bool gap = false) {
            ly27::EncodedVideoFrame frame;
            frame.frameId = static_cast<quint16>(index);
            frame.bytes = units[index];
            frame.receivedUtcMs = index + 100;
            frame.receivedMonotonicMs = index;
            frame.discontinuity = gap;
            return decoder.decode(frame, error);
        };
        auto images = decode(0);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(images.size(), 1);
        QCOMPARE(images[0].image.size(), QSize(96, 64));
        QCOMPARE(images[0].frameId, quint16(0));
        QCOMPARE(images[0].receivedUtcMs, qint64(100));
        const auto preserved = images[0].image.copy();
        QVERIFY(decode(2, true).isEmpty());
        QVERIFY(decoder.waitingForKeyframe());
        images = decode(3);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(images.size(), 1);
        QCOMPARE(images[0].frameId, quint16(3));
        QCOMPARE(decode(4).size(), 1);
        decoder.reset();
        QVERIFY(decoder.waitingForKeyframe());
        QVERIFY(decode(5).isEmpty());
        images = decode(0);
        QCOMPARE(images.size(), 1);
        QCOMPARE(images[0].image, preserved);
        // 带 IRAP 头但无有效参数/图像的损坏数据不得产生画面，随后仍能恢复。
        ly27::EncodedVideoFrame broken;
        broken.bytes = QByteArray::fromHex("000000012601ff");
        broken.discontinuity = true;
        QVERIFY(decoder.decode(broken, error).isEmpty());
        images = decode(0, true);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(images.size(), 1);
        QVERIFY(images[0].image.pixelColor(10, 10) != images[0].image.pixelColor(80, 50));
    }

    void udpLifecycleSourceAndStale()
    {
        ly27::VideoReceiver receiver;
        QString error;
        QVERIFY2(receiver.start(0, "first", error), qPrintable(error));
        const auto port = static_cast<quint16>(receiver.snapshot()["port"].toInt());
        QVERIFY(port);
        QCOMPARE(receiver.snapshot()["state"].toString(), QString("waiting"));
        QVERIFY(!receiver.latestFrame());
        QUdpSocket sender, foreign;
        const auto units = accessUnits();
        sendFrame(sender, port, 65535, units[0]);
        QTRY_VERIFY_WITH_TIMEOUT(receiver.latestFrame().has_value(), 2000);
        QCOMPARE(receiver.latestFrame()->sessionId, QString("first"));
        sendFrame(foreign, port, 0, units[0]);
        QTRY_VERIFY(receiver.snapshot()["ignored_source_datagrams"].toInteger() > 0);
        sendFrame(sender, port, 0, units[1]);
        QTRY_VERIFY(receiver.latestFrame() && receiver.latestFrame()->frameId == 0);
        QTRY_COMPARE_WITH_TIMEOUT(receiver.snapshot()["state"].toString(), QString("stale"), 1500);
        QVERIFY(!receiver.latestFrame());
        // 停流后同一会话内发送端可重新从小帧号和另一 UDP 源端口开始。
        QTest::qWait(650);
        sendFrame(foreign, port, 0, units[0]);
        QTRY_VERIFY_WITH_TIMEOUT(receiver.latestFrame().has_value(), 2000);
        receiver.stop();
        QVERIFY(!receiver.latestFrame());
        QCOMPARE(receiver.snapshot()["state"].toString(), QString("stopped"));
        QVERIFY(receiver.start(port, "second", error));
        QVERIFY(!receiver.latestFrame());
        sendFrame(sender, port, 0, units[0]);
        QTRY_VERIFY(receiver.latestFrame().has_value());
        QCOMPARE(receiver.latestFrame()->sessionId, QString("second"));
        receiver.stop();
        receiver.stop();
        QUdpSocket blocker;
        QVERIFY(blocker.bind(QHostAddress::LocalHost, port, QUdpSocket::DontShareAddress));
        QVERIFY(!receiver.start(port, "blocked", error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(receiver.snapshot()["state"].toString(), QString("error"));
        blocker.close();
        QVERIFY(receiver.start(port, "third", error));
        receiver.stop();
    }

    void pinnedSourceAndRuntimeFailure()
    {
        QString error;
        ly27::VideoReceiver receiver;
        QVERIFY(receiver.start(0, "pinned", error, QHostAddress::LocalHost, QHostAddress("127.0.0.2")));
        QUdpSocket sender;
        sendFrame(sender, static_cast<quint16>(receiver.snapshot()["port"].toInt()), 0, accessUnits()[0]);
        QTRY_VERIFY(receiver.snapshot()["ignored_source_datagrams"].toInteger() > 0);
        QVERIFY(!receiver.latestFrame());
        receiver.stop();
        QUdpSocket blocker;
        QVERIFY(blocker.bind(QHostAddress::LocalHost, 0));
        ly27::TerminalConfig config;
        config.network.videoPort = blocker.localPort();
        ly27::TerminalRuntime runtime(config);
        QVERIFY(!runtime.start(false, true));
        QVERIFY(!runtime.isRunning());
        blocker.close();
        QVERIFY(runtime.start(false, true));
        runtime.stop();
    }

    void runtimeAndCliExport()
    {
        QUdpSocket reservation;
        QVERIFY(reservation.bind(QHostAddress::LocalHost, 0));
        const auto port = reservation.localPort(); reservation.close();
        ly27::TerminalConfig config;
        config.network.videoPort = port;
        ly27::TerminalRuntime runtime(config);
        QVERIFY(runtime.start(false, true));
        QUdpSocket sender;
        const auto units = accessUnits();
        sendFrame(sender, port, 0, units[0]);
        QTRY_VERIFY(runtime.video().latestFrame().has_value());
        runtime.stop();
        QCOMPARE(runtime.snapshot()["video"].toObject()["state"].toString(), QString("stopped"));
        QVERIFY(runtime.start(false, true));
        QVERIFY(!runtime.video().latestFrame());
        runtime.stop();
        QVERIFY(runtime.start());
        QCOMPARE(runtime.video().snapshot()["state"].toString(), QString("disabled"));
        QVERIFY(runtime.video().snapshot()["session_id"].toString().isEmpty());
        QVERIFY(!runtime.video().snapshot().contains("decoded_frames"));
        runtime.stop();

        QTemporaryDir directory;
        QFile example(LY27_EXAMPLE_CONFIG); QVERIFY(example.open(QIODevice::ReadOnly));
        auto json = QJsonDocument::fromJson(example.readAll()).object();
        auto network = json["network"].toObject(); network["video_port"] = port; json["network"] = network;
        json["logging"] = QJsonObject{{"directory", "logs"}, {"level", "info"}};
        QFile file(directory.filePath("terminal.json")); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(json).toJson()); file.close();
        QProcess process;
        auto env = QProcessEnvironment::systemEnvironment(); env.remove("DISPLAY"); env.remove("WAYLAND_DISPLAY");
        process.setProcessEnvironment(env);
        process.start(LY27_CLI_PATH, {"--config", file.fileName(), "--video", "--run-for", "1200",
            "--save-frame", directory.filePath("frame.png"), "--status-at-exit"});
        QVERIFY(process.waitForStarted());
        quint16 id = 0;
        QTimer timer;
        connect(&timer, &QTimer::timeout, this, [&] { sendFrame(sender, port, id++, units[0]); });
        timer.start(40);
        QTRY_COMPARE_WITH_TIMEOUT(process.state(), QProcess::NotRunning, 5000);
        timer.stop();
        QCOMPARE(process.exitCode(), 0);
        QCOMPARE(QImage(directory.filePath("frame.png")).size(), QSize(96, 64));
        const auto status = QJsonDocument::fromJson(process.readAllStandardOutput()).object();
        QCOMPARE(status["video"].toObject()["state"].toString(), QString("stopped"));
        QVERIFY(status["video"].toObject()["decoded_frames"].toInteger() > 0);
        process.start(LY27_CLI_PATH, {"--config", file.fileName(), "--video", "--run-for", "100",
            "--save-frame", directory.filePath("missing.png")});
        QVERIFY(process.waitForFinished(3000));
        QCOMPARE(process.exitCode(), 6);
        QVERIFY(!QFile::exists(directory.filePath("missing.png")));
    }
};
QTEST_GUILESS_MAIN(VideoTest)
#include "test_video.moc"
