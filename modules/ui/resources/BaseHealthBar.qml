/**
 * @file BaseHealthBar.qml
 * @brief 基地血量条 QML 组件
 * @details 按官方样式实现:
 *          - 沿用 RM26 素材，填充长度统一表示当前血量占上限的比例
 *          - 扣血追血段
 *          - 低血量呼吸暗化
 */

import QtQuick 2.15
import LY27.Ui 1.0

Item {
    id: root

    property int currentHealth: -1
    property int maxHealth: 10000
    property bool isBlue: false
    property bool showHealthText: true

    readonly property int safeMaxHealth: Math.max(1, maxHealth)
    readonly property int clampedHealth: Math.max(0, Math.min(safeMaxHealth, currentHealth))

    readonly property real healthRatio: clampedHealth / safeMaxHealth
    readonly property bool lowHealth: healthRatio > 0 && healthRatio < 0.2

    // 方向: 红方剩余血量贴近右侧校徽(右锚定), 蓝方剩余血量贴近左侧校徽(左锚定)
    readonly property real totalFillWidth: width * healthRatio

    // 资源映射
    readonly property string depletedSource: isBlue ? "images/blue_base_blood/blue_base_blood_bottom.png" : "images/red_base_blood/red_base_blood_bottom.png"
    readonly property string frameSource: isBlue ? "images/blue_base_blood/blue_base_blood_middle.png" : "images/red_base_blood/red_base_blood_middle.png"
    readonly property string lightSource: isBlue ? "images/blue_base_blood/blue_base_blood_top.png" : "images/red_base_blood/red_base_blood_top.png"
    readonly property string borderSource: isBlue ? "images/blue_base_blood/blue_base_blood_border.png" : "images/red_base_blood/red_base_blood_border.png"
    readonly property string glowSource: isBlue ? "images/blue_base_blood/blue_base_blood_light.png" : "images/red_base_blood/red_base_blood_light.png"
    readonly property string lightTrailSource: isBlue ? "images/blue_base_blood/blue_base_blood_top_full.png" : "images/red_base_blood/red_base_blood_top_full.png"

    // 扣血追血段: 用 trailHealth 记录动画前值
    property real trailHealth: clampedHealth
    property real lowHealthPulse: 0

    readonly property real trailClampedHealth: Math.max(0, Math.min(safeMaxHealth, trailHealth))
    readonly property real trailWidth: width * trailClampedHealth / safeMaxHealth
    property real healthTextInset: Math.min(48, width * 0.15)
    property real healthFontSize: Math.min(22, height * 0.74)

    readonly property real trailDelta: Math.max(0, trailWidth - totalFillWidth)
    readonly property real trailX: isBlue ? totalFillWidth : (width - trailWidth)

    onClampedHealthChanged: {
        if (currentHealth >= 0 && clampedHealth < trailHealth) {
            trailAnim.stop()
            trailAnim.from = trailHealth
            trailAnim.to = clampedHealth
            trailAnim.start()
        } else {
            trailAnim.stop()
            trailHealth = clampedHealth
        }
    }

    Component.onCompleted: {
        trailHealth = clampedHealth
    }

    NumberAnimation {
        id: trailAnim
        target: root
        property: "trailHealth"
        duration: Theme.numericMotion
        easing.type: Easing.OutCubic
    }

    SequentialAnimation {
        running: root.lowHealth
        loops: Animation.Infinite
        NumberAnimation { target: root; property: "lowHealthPulse"; to: 1.0; duration: 700; easing.type: Easing.InOutSine }
        NumberAnimation { target: root; property: "lowHealthPulse"; to: 0.0; duration: 700; easing.type: Easing.InOutSine }
    }


    // 缺失血量底图
    Image {
        anchors.fill: parent
        source: root.depletedSource
        fillMode: Image.Stretch
    }

    // 深色素材是完整血槽，始终保留满宽，不参与剩余血量裁切。
    Image {
        objectName: "baseHealthFrame"
        anchors.fill: parent
        source: root.frameSource
        fillMode: Image.Stretch
    }

    // 只有亮色填充代表剩余血量，红蓝两侧均按总血量比例裁切。
    Item {
        objectName: "baseHealthFill"
        width: root.totalFillWidth
        height: root.height
        clip: true
        anchors.left: root.isBlue ? parent.left : undefined
        anchors.right: root.isBlue ? undefined : parent.right

        Image {
            width: root.width
            height: root.height
            source: root.lightSource
            fillMode: Image.Stretch
            anchors.left: root.isBlue ? parent.left : undefined
            anchors.right: root.isBlue ? undefined : parent.right
        }
    }

    // 扣血后的短暂追血段与主血条使用相同的总血量刻度。
    Item {
        objectName: "baseHealthTrail"
        visible: root.currentHealth >= 0 && root.trailDelta > 0.5
        x: root.trailX
        width: root.trailDelta
        height: root.height
        clip: true
        opacity: 0.75

        Image {
            anchors.fill: parent
            source: root.lightTrailSource
            fillMode: Image.Stretch
        }
    }

    // 低血量段暗化 + 呼吸
    Item {
        visible: root.lowHealth && root.totalFillWidth > 0.5
        width: root.totalFillWidth
        height: root.height
        clip: true
        x: root.isBlue ? 0 : (root.width - width)
        opacity: 0.2 + root.lowHealthPulse * 0.25

        Rectangle {
            anchors.fill: parent
            color: "#77000000"
        }
    }

    // 边框和微光
    Image {
        anchors.fill: parent
        source: root.borderSource
        fillMode: Image.Stretch
        opacity: 0.95
    }

    Image {
        anchors.fill: parent
        source: root.glowSource
        fillMode: Image.Stretch
        opacity: 0.28
    }


    // 满槽参照刻度不随剩余血量裁切，半血对应第五格。
    Repeater {
        model: 9
        Rectangle {
            required property int index
            x: (index + 1) * root.width / 10
            y: root.height * .25; width: 1; height: root.height * .5
            color: Theme.ink; opacity: .32
        }
    }

    // 血量数字
    UiText {
        visible: root.showHealthText
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: root.isBlue ? parent.left : undefined
        anchors.right: root.isBlue ? undefined : parent.right
        anchors.leftMargin: root.healthTextInset
        anchors.rightMargin: root.healthTextInset
        objectName: "baseHealthValue"
        text: root.currentHealth >= 0 ? root.currentHealth.toString() : "—"
        color: root.currentHealth >= 0 ? Theme.text : Theme.unknown
        font.family: Theme.numberFont
        font.pixelSize: root.healthFontSize
        font.bold: true
        style: Text.Outline
        styleColor: "#000000"
    }
}
