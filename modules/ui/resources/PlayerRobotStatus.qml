// 头像、经验环、血条和缓冲条移植自 RM26 LeftBottomPanel（MIT）；27 数据不复用旧等级/兵种逻辑。
import QtQuick
import LY27.Ui 1.0
Item {
    id: root
    property var robot: ({})
    property var buffs: []
    // 与伤害面板共用模块展示数据，各字段的新鲜度由 C++ 独立检查。
    property var modules: []
    width: 460; height: 200
    readonly property int robotId: robot.robotId ?? 0
    readonly property int displayRobotId: robotId % 100
    readonly property bool isRed: !robot.blue
    readonly property real currentHP: robot.currentHP ?? -1
    readonly property real maxHP: robot.maxHP ?? -1
    readonly property int robotLevel: robot.level ?? -1
    readonly property real bufferEnergy: robot.bufferEnergy ?? -1
    readonly property real maxBufferEnergy: robot.maxBufferEnergy ?? -1
    readonly property real currentExp: robot.currentExp ?? -1
    readonly property real maxExp: robot.maxExp ?? -1
    readonly property bool isOutOfCombat: !!robot.isOutOfCombat
    readonly property string outOfCombatStatus: robot.outOfCombatKnown ? (isOutOfCombat ? "已脱战" : "交战中") : "脱战状态未知"
    readonly property string chassisStatus: robot.maxPower >= 0 ? "功率上限 " + robot.maxPower + " W" : "功率上限 —"
    readonly property bool chassisOverPowerCut: false
    readonly property real statsLeftMargin: root.width * -.10
    readonly property real statsVerticalOffset: root.height * .08
    readonly property real hpBarTopMargin: root.height * .18
    readonly property real healthRatio: currentHP >= 0 && maxHP > 0 ? Math.max(0,Math.min(1,currentHP/maxHP)) : 0
    readonly property real bufferEnergyRatio: bufferEnergy >= 0 && maxBufferEnergy > 0 ? Math.max(0,Math.min(1,bufferEnergy/maxBufferEnergy)) : 0
    readonly property real expRatio: currentExp >= 0 && maxExp > 0 ? Math.max(0,Math.min(1,currentExp/maxExp)) : 0
    readonly property string teamPrefix: isRed ? "red" : "blue"
    readonly property int safeRobotLevel: Math.max(1,Math.min(10,robotLevel))
    readonly property string levelIconPrimarySource: "qrc:/ly27/ui/images/my_robot/" + teamPrefix + "_statusbar_level_" + safeRobotLevel + ".png"
    readonly property string levelIconFallbackSource: "qrc:/ly27/ui/images/top_robots/levels/tab_level_" + safeRobotLevel + ".png"
    // 1 号为重装，沿用旧 hero 头像作为素材；3/4 均为步兵，绝不能将 4 号视为空中机器人。
    readonly property string avatarSource: "qrc:/ly27/ui/images/my_robot/" + teamPrefix + "_statusbar_avatar_" + (displayRobotId === 1 ? "hero" : "infantry") + ".png"
    Item {
        id: robotStatusArea
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.height * 1.00 //230

        // 2.1 头像区域 (放大 + 紧凑)
        Item {
            id: avatarArea
            width: root.height * 0.80
            height: root.height * 0.80
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: root.width * 0.01 //10

            // 经验值圆弧
            Canvas {
                anchors.fill: parent
                property real progress: root.expRatio
                onProgressChanged: requestPaint()
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                onPaint: {
                    var ctx = getContext("2d");
                    ctx.reset();
                    ctx.clearRect(0, 0, width, height);
                    var centerX = width / 2;
                    var centerY = height / 2;
                    var radius = width / 2 - root.height * 0.10;

                    ctx.beginPath();
                    ctx.arc(centerX, centerY, radius, Math.PI * 0.75, Math.PI * 1.18, false);
                    ctx.lineWidth = root.height * 0.04;
                    ctx.strokeStyle = "#40000000";
                    ctx.stroke();

                    if (progress > 0) {
                        var end = Math.PI * 0.75 + (Math.PI * 0.43 * progress);
                        ctx.beginPath();
                        ctx.arc(centerX, centerY, radius, Math.PI * 0.75, end, false);
                        ctx.lineWidth = root.height * 0.03;
                        ctx.strokeStyle = "#FFD700";
                        ctx.stroke();
                    }
                }
            }

            // 机器人头像
            Image {
                anchors.centerIn: parent
                width: root.height * 0.60
                height: root.height * 0.60
                source: root.avatarSource
                fillMode: Image.PreserveAspectFit
            }

            // 等级图标
            Image {
                id: levelIconPrimary
                visible: root.robotLevel >= 0
                width: root.height * 0.20
                height: root.height * 0.20
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.leftMargin: root.width * 0.04
                anchors.topMargin: root.height * 0.10
                source: root.levelIconPrimarySource
                fillMode: Image.PreserveAspectFit
            }

            // 资源回退：如果 my_robot 等级图标缺失，则使用 top_robots 等级图标
            Image {
                width: root.height * 0.20
                height: root.height * 0.20
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.leftMargin: root.width * 0.03
                anchors.topMargin: root.height * 0.08
                source: root.levelIconFallbackSource
                fillMode: Image.PreserveAspectFit
                visible: root.robotLevel >= 0 && levelIconPrimary.status === Image.Error
            }

            // ID
            Item {
                width: root.height * 0.23
                height: root.height * 0.23
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.leftMargin: root.width * 0.10
                anchors.bottomMargin: root.height * 0.09
                Image{
                    anchors.fill: parent
                    source: "qrc:/ly27/ui/images/my_robot/" + root.teamPrefix + "_statusbar_id_border.png"
                    fillMode: Image.PreserveAspectFit
                }

                Text {
                    anchors.centerIn: parent
                    text: root.displayRobotId
                    color: "white"
                    font.bold: true
                    font.pixelSize: root.height * 0.10
                }
            }

        }

        // 2.2 数据条 (紧贴头像)
        Item {
            id: statsArea
            anchors.left: avatarArea.right
            anchors.leftMargin: root.statsLeftMargin
            anchors.right: parent.right
            anchors.verticalCenter: avatarArea.verticalCenter
            anchors.verticalCenterOffset: root.statsVerticalOffset
            height: root.height * 0.50
            z: -1 // 置于头像下方

            BuffRow {
                anchors.left: hpBar.left; anchors.leftMargin: 14
                anchors.bottom: hpBar.top; anchors.bottomMargin: 7
                buffs: root.buffs; tooltipAbove: true
            }
            // 血条：参照官方客户端比例缩小
            Item {
                id: hpBar
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.topMargin: root.hpBarTopMargin
                anchors.leftMargin: root.width * -0.01

                width: root.width * 0.51
                height: root.height * 0.15

                // 脱战提示 (血量条右方)
                Item {
                    anchors.left: parent.right
                    anchors.leftMargin: root.width * 0.01
                    anchors.verticalCenter: parent.verticalCenter
                    width: root.width * 0.22
                    height: root.height * 0.14

                    Row {
                        spacing: root.width * 0.0043
                        anchors.verticalCenter: parent.verticalCenter

                        // 图标
                        Image {
                            visible: root.robot.outOfCombatKnown ?? false
                            width: root.height * 0.14
                            height: root.height * 0.14
                            source: "qrc:/ly27/ui/images/my_robot/" + (root.isOutOfCombat ? "outofcombat_icon.png" : "combat_icon.png")
                            fillMode: Image.PreserveAspectFit
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        // 文字
                        Text {
                            text: root.outOfCombatStatus
                            color: !root.robot.outOfCombatKnown ? Theme.unknown : root.isOutOfCombat ? Theme.positive : Theme.tactical
                            font.pixelSize: root.height * 0.07
                            font.bold: true
                            style: Text.Outline
                            styleColor: "black"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }

                // 血条边框背景
                Image {
                    id: bgImage
                    anchors.fill: parent
                    source: "qrc:/ly27/ui/images/my_robot/"  + root.teamPrefix + "_statusbar_blood_background.png"
                    fillMode: Image.Stretch
                }
                // 血量增益背景


                // 血量填充层（裁剪显示）
                Item {
                    id: fillContainer
                    anchors.top: parent.top
                    anchors.topMargin: root.height * -0.03
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    // 宽度根据血量比例计算
                    width: parent.width * root.healthRatio  // 使用限制后的血量比例
                    clip: true

                    Image {
                        width: hpBar.width + root.width * 0.01    // 填充层宽度与血条宽度一致
                        height: hpBar.height + root.height * 0.05
                        anchors.left: parent.left
                        anchors.top: parent.top
                        source: "qrc:/ly27/ui/images/my_robot/" + root.teamPrefix + "_statusbar_blood_slid.png"
                        fillMode: Image.Stretch
                    }
                }

                


                // HP 数值文本
                Text {
                    anchors.centerIn: parent
                    objectName: "playerHealthText"
                    text: (root.currentHP >= 0 ? root.currentHP : "—") + " / " + (root.maxHP > 0 ? root.maxHP : "—")
                    color: "white"
                    font.pixelSize: root.height * 0.10
                    font.bold: true
                    style: Text.Outline
                    styleColor: "black"
                }

            }

            // 能量条宽度不超过血条
            Item {
                id: energyBarsContainer
                anchors.left: hpBar.left
                anchors.top: hpBar.bottom
                anchors.topMargin: root.height * 0.01
                width: hpBar.width               // 宽度与血条一致
                height: root.height * 0.10

                 // 缓冲能量条
                Item {
                    id: bufferBar
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.topMargin: root.height * 0.01
                    anchors.leftMargin: root.width * -0.03

                    width: energyBarsContainer.width*0.80
                    height: root.height * 0.03
                    z: -1 // 置于头像下方

                    // 边框背景
                    Image {
                        id: bgImage1
                        anchors.fill: parent
                        source: "qrc:/ly27/ui/images/my_robot/statusbar_energy_background.png"
                        fillMode: Image.Stretch
                    }

                    // 填充层（裁剪显示）
                    Item {
                        id: fillContainer1
                        anchors.top: parent.top
                        anchors.topMargin: root.height * -0.03
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        // 宽度根据血量比例计算
                        width: parent.width * root.bufferEnergyRatio
                        clip: true

                        Image {
                            width: bufferBar.width + root.width * 0.01    // 填充层宽度与血条宽度一致
                            height: bufferBar.height + root.height * 0.05
                            anchors.left: parent.left
                            anchors.top: parent.top
                            source: "qrc:/ly27/ui/images/my_robot/statusbar_energy.png"
                            fillMode: Image.Stretch
                        }
                    }

                }
                UiText {
                    objectName: "playerBufferText"
                    anchors.top: bufferBar.bottom; anchors.topMargin: 5
                    anchors.left: bufferBar.left
                    text: "缓冲 " + (root.bufferEnergy >= 0 ? root.bufferEnergy : "—") + " / " + (root.maxBufferEnergy > 0 ? root.maxBufferEnergy : "—")
                    color: Theme.secondary; font.pixelSize: 10
                    style: Text.Outline; styleColor: Theme.ink
                }
                // 功率文本
                Text {
                    id: powerText
                    anchors.top: parent.top
                    anchors.left: bufferBar.right
                    anchors.leftMargin: root.width * 0.01
                    text: root.chassisStatus
                    color: root.chassisOverPowerCut ? "red" : "white"   //超限断电显示红色，底盘功率增益显示白色
                    font.pixelSize: root.height * 0.04
                    font.bold: true
                }
                // 底盘能量条 (青色)

            }
        }

        // 位置、五列两行、灯和文字尺寸直接沿用 RM26 LeftBottomPanel。
        Item {
            id: moduleStatusArea
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: statsArea.bottom
            anchors.leftMargin: root.width * .08
            anchors.rightMargin: root.width * .03
            height: root.height * .20
            // 保留图传、RFID、UWB、17mm、42mm 的旧位置，其余槽位放 27 实际模块。
            readonly property var moduleKeys: ["mainController", "powerManager", "capacitor", "videoTransmission", "rfid",
                "armor", "uwb", "lightStrip", "smallShooter", "bigShooter"]
            readonly property var moduleNames: ["主控", "电源", "电容", "图传", "RFID识别",
                "装甲", "UWB", "灯条", "17mm", "42mm"]
            Grid {
                objectName: "playerModuleGrid"
                anchors.fill: parent
                columns: 5
                spacing: root.width * .0043
                Repeater {
                    model: moduleStatusArea.moduleKeys
                    delegate: Item {
                        id: moduleItem
                        required property string modelData
                        required property int index
                        readonly property var module: root.modules.find(function(value) { return value.key === moduleItem.modelData }) ?? ({state: -1, stateText: "未知"})
                        objectName: "playerModule_" + modelData
                        readonly property int moduleState: module.state
                        readonly property color stateColor: moduleState === 1 ? Theme.positive
                            : moduleState === 0 ? Theme.danger : moduleState === 2 ? Theme.tactical : Theme.unknown
                        readonly property string stateText: module.stateText
                        width: root.width * .13
                        height: root.height * .10
                        Image {
                            objectName: "playerModuleLamp_" + moduleItem.modelData
                            width: root.width * .03; height: root.height * .09
                            anchors.left: parent.left
                            visible: moduleItem.moduleState >= 0
                            source: visible ? "qrc:/ly27/ui/images/my_robot/statusbar_signallamp_"
                                + (moduleItem.moduleState === 1 ? "green" : moduleItem.moduleState === 0 ? "red" : "yellow")
                                + "_single.png" : ""
                            fillMode: Image.Stretch
                        }
                        Rectangle {
                            x: 5; y: 3; width: 4; height: 12; radius: 2; rotation: 24
                            visible: moduleItem.moduleState < 0
                            color: Theme.unknown
                        }
                        Text {
                            objectName: "playerModuleLabel_" + moduleItem.modelData
                            anchors.left: parent.left
                            anchors.leftMargin: root.width * .03
                            anchors.verticalCenter: parent.verticalCenter
                            text: moduleStatusArea.moduleNames[moduleItem.index]
                            color: moduleItem.moduleState < 0 ? Theme.unknown : "white"
                            font.pixelSize: root.height * .06
                            style: Text.Outline; styleColor: "black"
                        }
                    }
                }
            }
        }
    }
}
