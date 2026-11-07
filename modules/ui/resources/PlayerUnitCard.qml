// 改编自 RM26 RobotStatusItem（MIT），单位仅为 27 的 1/3/4/7。
/**
 * @file RobotStatusItem.qml
 * @brief 单个机器人状态组件 (用于顶部栏列表)
 * @details 使用 docs/sprites 的顶部机器人卡片风格素材：
 *   - 底板 + 机器人头像 + 编号贴片 + 底部血条
 */

import QtQuick 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    width: 82
    height: 72
    implicitWidth: 82
    implicitHeight: 72

    // 固定卡片尺寸，避免被布局压缩
    Layout.preferredWidth: 82
    Layout.preferredHeight: 72
    Layout.minimumWidth: 82
    Layout.maximumWidth: 82
    Layout.minimumHeight: 72
    Layout.maximumHeight: 72

    property var robotInfo: ({})
    property int robotId: 1
    property bool isBlue: false
    property string avatarSource: ""
    readonly property int normalizedRobotId: robotId % 100
    readonly property string teamPrefix: isBlue ? "blue" : "red"
    readonly property string cardBackgroundSource: "qrc:/ly27/ui/images/top_robots/" + teamPrefix + "_teammate_bg.png"
    readonly property string hpBarSource: "qrc:/ly27/ui/images/top_robots/" + teamPrefix + "_tab_bar.png"
    readonly property int levelValue: robotInfo.level ?? -1
    readonly property int safeLevelValue: Math.max(1,Math.min(10,levelValue))
    readonly property string levelIconSource: "qrc:/ly27/ui/images/top_robots/levels/tab_level_" + safeLevelValue + ".png"
    readonly property int currentHP: robotInfo.hp ?? -1
    readonly property int maxHP: robotInfo.maxHp ?? -1
    readonly property bool isDead: currentHP === 0
    readonly property bool isConnected: currentHP >= 0
    readonly property real cardOpacity: isConnected ? (isDead ? .65 : 1) : .56
    readonly property real avatarOpacity: isConnected ? (isDead ? .45 : 1) : .25
    // 卡片底板
    Image {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        width: 82
        height: 44
        source: root.cardBackgroundSource
        fillMode: Image.PreserveAspectFit
        opacity: root.cardOpacity
    }

    // 机器人头像
    Image {
        id: icon
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        width: 62
        height: 50
        source: root.avatarSource
        fillMode: Image.PreserveAspectFit
        opacity: root.avatarOpacity
        visible: root.isConnected
    }

    // 统一数字样式，避免 1-7 大小不一致
    Text {
        anchors.left: root.isBlue ? undefined : parent.left
        anchors.right: root.isBlue ? parent.right : undefined
        anchors.leftMargin: -2
        anchors.rightMargin: 2
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        text: root.normalizedRobotId
        color: root.isConnected ? "#F5F7FA" : "#7D848C"
        font.pixelSize: 15
        font.bold: true
        style: Text.Outline
        styleColor: "#101214"
    }

    // 等级角标（右下角）
    Image {
        anchors.left: root.isBlue ? parent.left : undefined
        anchors.right: root.isBlue ? undefined : parent.right
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        width: 14
        height: 12
        source: root.levelIconSource
        fillMode: Image.PreserveAspectFit
        visible: !root.isDead && root.isConnected && root.levelValue >= 0
    }

    Text {
        anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter
        text: root.currentHP >= 0 ? root.currentHP : "—"
        color: "white"; font.pixelSize: 10; style: Text.Outline; styleColor: "black"
    }
    // 底部血条
    Item {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 2
        anchors.left: root.isBlue ? parent.left : undefined
        anchors.right: root.isBlue ? undefined : parent.right
        anchors.leftMargin: 1
        anchors.rightMargin: 1
        width: 59
        height: 6

        Image {
            anchors.fill: parent
            source: root.hpBarSource
            fillMode: Image.Stretch
            opacity: root.isDead ? 0.35 : (root.isConnected ? 0.95 : 0.45)
        }

        Rectangle {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: root.maxHP > 0 && root.currentHP >= 0 ? parent.width * Math.max(0, Math.min(1, root.currentHP / root.maxHP)) : 0
            height: 3
            radius: 1.5
            color: (!root.isConnected || root.isDead) ? "#777777" : (root.isBlue ? "#2DB5FF" : "#FF2C44")
            opacity: root.isDead ? 0.45 : (root.isConnected ? 0.95 : 0.0)
        }
    }
}
