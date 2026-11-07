#include "DesktopSession.hpp"
#include "MapWindow.hpp"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QQmlComponent>
#include <QQuickItem>
#include <QTemporaryDir>
#include <QUdpSocket>
#include <QtEndian>
#include <QtTest>
#include <memory>

namespace {
QQuickItem *item(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name) return root;
    for (auto *child : root->childItems()) if (auto *found = item(child, name)) return found;
    return nullptr;
}
QByteArray firstAccessUnit()
{
    QFile file(LY27_VIDEO_FIXTURE);
    if (!file.open(QIODevice::ReadOnly)) return {};
    const auto bytes = file.readAll();
    qsizetype first = -1;
    for (qsizetype i = 0; i + 5 < bytes.size(); ++i) {
        if (bytes.mid(i, 4) != QByteArray::fromHex("00000001") ||
            ((static_cast<unsigned char>(bytes[i + 4]) >> 1) & 0x3f) != 35) continue;
        if (first >= 0) return bytes.mid(first, i - first);
        first = i;
    }
    return {};
}
bool sendFrame(QUdpSocket &sender, quint16 port, const QByteArray &bytes, quint16 frameId = 0)
{
    // 使用真实 HEVC 访问单元和官方分片格式，倒序发送以覆盖重组链路。
    for (qsizetype i = (bytes.size() + 1391) / 1392; i-- > 0;) {
        QByteArray packet(8, '\0');
        qToBigEndian<quint16>(frameId, packet.data());
        qToBigEndian<quint16>(i, packet.data() + 2);
        qToBigEndian<quint32>(bytes.size(), packet.data() + 4);
        packet += bytes.mid(i * 1392, 1392);
        if (sender.writeDatagram(packet, QHostAddress::LocalHost, port) != packet.size()) return false;
    }
    return true;
}
}

class DesktopVideoTest : public QObject {
    Q_OBJECT
private slots:
    void hotkeysRespectHoldRepeatEditingAndFocus()
    {
        ly27::MapWindow window;
        QCOMPARE(window.status(), QQuickView::Ready);
        window.show(); QTest::qWait(30);
        QVERIFY(!window.videoVisible()); QVERIFY(!window.settingsVisible());
        QTest::keyPress(&window, Qt::Key_Space); QVERIFY(window.settingsVisible());
        auto *settings = item(window.rootObject(), "terminalSettings"); QVERIFY(settings);
        QVERIFY(settings->property("opened").toBool());
        QKeyEvent autoRelease(QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier, " ", true);
        QCoreApplication::sendEvent(&window, &autoRelease); QVERIFY(window.settingsVisible());
        QKeyEvent autoPress(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, " ", true);
        QCoreApplication::sendEvent(&window, &autoPress); QVERIFY(window.settingsVisible());
        QTest::keyRelease(&window, Qt::Key_Space); QVERIFY(!window.settingsVisible());
        QTest::keyPress(&window, Qt::Key_P); QVERIFY(window.videoVisible());
        QVERIFY(!item(window.rootObject(), "settingsButton")->isVisible());
        QTest::keyPress(&window, Qt::Key_Space); QVERIFY(window.settingsVisible());
        QVERIFY(item(window.rootObject(), "terminalSettingsPanel")->isVisible());
        QTest::keyRelease(&window, Qt::Key_Space); QVERIFY(!window.settingsVisible());
        QKeyEvent repeatP(QEvent::KeyPress, Qt::Key_P, Qt::NoModifier, "p", true);
        QCoreApplication::sendEvent(&window, &repeatP); QVERIFY(window.videoVisible());
        QTest::keyRelease(&window, Qt::Key_P);
        QTest::keyClick(&window, Qt::Key_P); QVERIFY(!window.videoVisible());
        QTest::keyClick(&window, Qt::Key_P, Qt::ControlModifier); QVERIFY(!window.videoVisible());
        QTest::keyClick(&window, Qt::Key_Space, Qt::ControlModifier); QVERIFY(!window.settingsVisible());
        QTest::keyPress(&window, Qt::Key_Space); QVERIFY(window.settingsVisible());
        QEvent lostFocus(QEvent::WindowDeactivate); QCoreApplication::sendEvent(&window, &lostFocus);
        QVERIFY(!window.settingsVisible()); QTest::keyRelease(&window, Qt::Key_Space);
        QTest::keyPress(&window, Qt::Key_Space); QVERIFY(window.settingsVisible());
        window.hide(); QVERIFY(!window.settingsVisible()); window.show();

        QQmlComponent component(window.engine());
        component.setData("import QtQuick; TextInput { width: 100; height: 30 }", QUrl());
        std::unique_ptr<QObject> owner(component.create()); QVERIFY(owner);
        auto *input = qobject_cast<QQuickItem *>(owner.get()); QVERIFY(input);
        input->setParentItem(window.rootObject()); input->forceActiveFocus();
        QCOMPARE(window.activeFocusItem(), input);
        QTest::keyClick(&window, Qt::Key_Space); QVERIFY(!window.settingsVisible());
        QTest::keyClick(&window, Qt::Key_P); QVERIFY(!window.videoVisible());
        QCOMPARE(input->property("text").toString(), QString(" p"));
        window.setSource(QUrl());
    }
    void udpDecodesRendersExpiresAndClearsAcrossIdentity()
    {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        QUdpSocket reservation; QVERIFY(reservation.bind(QHostAddress::LocalHost, 0));
        const quint16 port = reservation.localPort(); reservation.close();
        ly27::TerminalConfig c;
        c.sourcePath = directory.filePath("terminal.json");
        c.identity = {ly27::Role::Infantry, ly27::Team::Red, "red-infantry-3", 3};
        c.network.mqttHost = "127.0.0.1"; c.network.mqttClientId = "desktop-video-test";
        c.network.videoPort = port; c.logDirectory = directory.path();
        ly27::TerminalRuntime runtime(c); QVERIFY(runtime.start(false, true));
        ly27::DesktopSession session(runtime, "红", "蓝");
        ly27::MapWindow window(session.viewModel(), nullptr, &session);
        QCOMPARE(window.status(), QQuickView::Ready); window.show(); QTest::qWait(30);
        auto *root = window.rootObject(); auto *map = item(root, "battlefieldImage");
        auto *surface = item(root, "videoSurface"); auto *status = item(root, "videoStatus");
        auto *markers = item(root, "mapMarkerLayer");
        QVERIFY(map); QVERIFY(surface); QVERIFY(status); QVERIFY(markers);
        const QString originalSession = runtime.snapshot().value("session_id").toString();
        auto *area = item(root, "mapArea"); QVERIFY(area);
        const QSizeF mapSize = area->size();
        QTest::keyClick(&window, Qt::Key_P);
        QVERIFY(surface->isVisible()); QVERIFY(!map->isVisible()); QVERIFY(!markers->isVisible());
        QCOMPARE(surface->mapRectToScene(QRectF(QPointF(), surface->size())), QRectF(0, 0, window.width(), window.height()));
        for (const auto &name : {"matchHeader", "eventPanel", "battleAnalysisPanel"})
            QVERIFY(!item(root, name)->isVisible());
        QVERIFY(status->isVisible()); QCOMPARE(session.videoStatus(), QString("等待 UDP 图传"));

        const auto encoded = firstAccessUnit(); QVERIFY(!encoded.isEmpty());
        QUdpSocket sender; QVERIFY(sendFrame(sender, port, encoded));
        QTRY_VERIFY_WITH_TIMEOUT(session.videoAvailable(), 3000);
        QVERIFY(!status->isVisible());
        const auto decoded = session.videoFrame(); QVERIFY(!decoded.isNull());
        QCOMPARE(surface->property("frame").value<QImage>(), decoded);
        QCOMPARE(session.videoAspect(), qreal(decoded.width()) / decoded.height());
        QCOMPARE(area->size(), mapSize);
        QTest::qWait(30);
        const QImage screenshot = window.grabWindow(); QVERIFY(!screenshot.isNull());
        // 同时检查屏幕实际像素，防止只接通图像属性却没有绘制。
        const QSizeF fitted = QSizeF(decoded.size()).scaled(surface->size(), Qt::KeepAspectRatio);
        const QPointF position = surface->mapToScene(QPointF((surface->width() - fitted.width()) / 2 + fitted.width() * .67,
                                                            (surface->height() - fitted.height()) / 2 + fitted.height() * .7));
        const QColor shown = screenshot.pixelColor(qRound(position.x() * screenshot.devicePixelRatio()),
                                                  qRound(position.y() * screenshot.devicePixelRatio()));
        const QColor expected = decoded.pixelColor(int(decoded.width() * .67), int(decoded.height() * .7));
        QVERIFY2(qAbs(shown.red() - expected.red()) < 15 && qAbs(shown.green() - expected.green()) < 15 &&
                 qAbs(shown.blue() - expected.blue()) < 15, qPrintable(QString("屏幕 %1，解码 %2").arg(shown.name(), expected.name())));
        QDir().mkpath("test-artifacts"); QVERIFY(screenshot.save("test-artifacts/desktop-video.png"));
        QTest::keyClick(&window, Qt::Key_P);
        QVERIFY(map->isVisible()); QVERIFY(markers->isVisible()); QVERIFY(!surface->isVisible());
        for (const auto &name : {"matchHeader", "eventPanel", "battleAnalysisPanel"})
            QVERIFY(item(root, name)->isVisible());
        QCOMPARE(area->size(), mapSize);
        QVERIFY(qAbs(area->width() / area->height() - 1662.0 / 946) < .0001);
        QCOMPARE(runtime.snapshot().value("session_id").toString(), originalSession);
        QTest::keyClick(&window, Qt::Key_P);
        QTRY_VERIFY_WITH_TIMEOUT(!session.videoAvailable(), 1500);
        QVERIFY(status->isVisible()); QCOMPARE(session.videoStatus(), QString("图传信号中断"));
        QVERIFY(surface->property("frame").value<QImage>().isNull());
        QVERIFY(sendFrame(sender, port, encoded, 1));
        QTRY_VERIFY_WITH_TIMEOUT(session.videoAvailable(), 1000);
        QTest::qWait(20);
        // 新身份建立独立视频会话，不能继承旧图像。
        QVERIFY(session.selectIdentity("heavy", "blue"));
        QVERIFY(!session.videoAvailable()); QCOMPARE(session.videoStatus(), QString("等待 UDP 图传"));
        QVERIFY(runtime.snapshot().value("session_id").toString() != originalSession);
        QVERIFY(sendFrame(sender, port, encoded)); QTRY_VERIFY_WITH_TIMEOUT(session.videoAvailable(), 3000);
        runtime.stop(); QVERIFY(!session.videoAvailable()); QVERIFY(status->isVisible());
        window.setSource(QUrl());
    }
};

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv); DesktopVideoTest test; return QTest::qExec(&test, argc, argv);
}
#include "test_desktop_video.moc"
