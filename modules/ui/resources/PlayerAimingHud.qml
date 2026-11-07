// 准星与射击信息改编自 RM26 CentralAimingHUD（MIT），只读 27 协议字段。
pragma ComponentBehavior: Bound

import QtQuick
import LY27.Ui 1.0

/**
 * @file CentralAimingHUD.qml
 * @brief 中央瞄准 HUD - 包含准心、热量环、射击信息
 * @details 准心始终位于屏幕几何中心，射击信息面板挂载在准心右侧
 */
Item {
    id: root
    width: 600
    height: 600

    property var robot: ({})
    readonly property real currentHeat: robot.currentHeat ?? -1
    readonly property real heatLimit: robot.heatLimit ?? -1
    readonly property real heatRatioRaw: currentHeat >= 0 && heatLimit > 0 ? currentHeat / heatLimit : 0
    readonly property real heatRatio: Math.max(0, Math.min(1, heatRatioRaw))
    readonly property bool isOverheated: heatLimit > 0 && currentHeat >= heatLimit
    readonly property color heatColor: isOverheated ? Theme.danger : heatRatio >= .5 ? Theme.tactical : Theme.text
    readonly property real muzzleVelocity: robot.muzzleVelocity ?? -1
    readonly property real allowedAmmoRaw: robot.remainingAmmo ?? -1
    readonly property color ammoColor: allowedAmmoRaw < 0 ? Theme.unknown : allowedAmmoRaw === 0 ? Theme.danger : Theme.text
    // --- 视觉中心点 (固定在组件正中央) ---
    Item {
        id: visualCenter
        anchors.centerIn: parent
        width: 1
        height: 1

        // --- 1. 外圈背景 (超大淡灰圆环) ---
        Image {
            id: outerRing
            anchors.centerIn: parent
            width: 560
            height: 560
            source: "qrc:/ly27/ui/images/crosshair/crosshair_bg_single.png"
            fillMode: Image.PreserveAspectFit
            opacity: 0.35
        }

        // --- 横向基准线 ---
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: heatRingContainer.left
            anchors.rightMargin: 4
            width: 178
            height: 1
            color: "#596067"
            opacity: 0.55
        }
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: heatRingContainer.right
            anchors.leftMargin: 4
            width: 178
            height: 1
            color: "#596067"
            opacity: 0.55
        }

        // --- 2. 热量环 (紧贴准星的小圈) ---
        Item {
            id: heatRingContainer
            anchors.centerIn: parent
            width: 116
            height: 116

            // 热量环背景
            Image {
                id: heatRingBg
                anchors.fill: parent
                source: "qrc:/ly27/ui/images/crosshair/crosshair_slid_circle_bg.png"
                fillMode: Image.PreserveAspectFit
                opacity: 0.92
            }

            Canvas {
                id: heatProgressCanvas
                anchors.fill: parent
                antialiasing: true

                onPaint: {
                    var context = getContext("2d");
                    context.clearRect(0, 0, width, height);

                    var displayRatio = root.isOverheated ? 1.0 : root.heatRatio;
                    if (displayRatio <= 0) {
                        return;
                    }

                    var lineWidth = 8;
                    var radius = Math.max(0, Math.min(width, height) * 0.5 - lineWidth * 0.7);
                    var centerX = width * 0.5;
                    var centerY = height * 0.5;
                    var startAngle = -Math.PI * 0.5;
                    var endAngle = startAngle + Math.PI * 2 * displayRatio;

                    context.beginPath();
                    context.lineWidth = lineWidth;
                    context.lineCap = "round";
                    context.strokeStyle = root.heatColor;
                    context.shadowColor = root.heatColor;
                    context.shadowBlur = root.isOverheated ? 18 : 10;
                    context.arc(centerX, centerY, radius, startAngle, endAngle, false);
                    context.stroke();
                }

                Component.onCompleted: requestPaint()
            }

            Rectangle {
                anchors.centerIn: parent
                width: 94
                height: 94
                radius: 47
                color: "transparent"
                border.width: 1
                border.color: "#4D545B"
                opacity: 0.7
            }

            Text {
                id: overheatWarningText
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.top
                anchors.bottomMargin: 10
                objectName: "playerOverheatWarning"
                text: "射击热量已达上限"
                visible: root.isOverheated
                color: "#FF4747"
                font.pixelSize: 18
                font.bold: true
                font.family: "PingFang SC"
                opacity: 0.92

                SequentialAnimation {
                    running: root.isOverheated
                    loops: Animation.Infinite
                    NumberAnimation {
                        target: overheatWarningText
                        property: "opacity"
                        to: 0.35
                        duration: 280
                    }
                    NumberAnimation {
                        target: overheatWarningText
                        property: "opacity"
                        to: 0.92
                        duration: 280
                    }
                }
            }

        }

        // --- 3. 准心 (中心十字) ---
        Image {
            id: crosshair
            anchors.centerIn: parent
            width: 36
            height: 36
            source: "qrc:/ly27/ui/images/crosshair/crosshair_middle.png"
            fillMode: Image.PreserveAspectFit
            opacity: 1.0
        }
    }

    UiText {
        objectName: "playerHeatText"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter; anchors.verticalCenterOffset: 80
        text: (root.currentHeat >= 0 ? root.currentHeat : "—") + " / " + (root.heatLimit > 0 ? root.heatLimit : "—")
        color: root.heatColor; style: Text.Outline; styleColor: Theme.ink
        numeric: true
    }
    onHeatRatioChanged: heatProgressCanvas.requestPaint()
    onHeatColorChanged: heatProgressCanvas.requestPaint()
    onIsOverheatedChanged: heatProgressCanvas.requestPaint()

    // --- 4. 射击信息面板 (固定挂载到最大外环右侧) ---
    Item {
        id: shootingInfo
        anchors.left: visualCenter.right
        anchors.leftMargin: 286
        anchors.verticalCenter: visualCenter.verticalCenter
        anchors.verticalCenterOffset: -8
        width: 146
        height: 184

        // 透明容器 - 无背景无边框
        Item {
            width: 146
            height: 178

            Column {
                anchors.fill: parent
                anchors.leftMargin: 17
                anchors.rightMargin: 10
                anchors.topMargin: 14
                anchors.bottomMargin: 11
                spacing: 10

                Column {
                    spacing: 2
                    Text {
                        text: "射击初速度"
                        color: "#8E959C"
                        font.pixelSize:10
                        font.family: "PingFang SC"
                    }
                    Text {
                        objectName: "playerMuzzleVelocity"
                        text: root.muzzleVelocity >= 0 ? root.muzzleVelocity.toFixed(1) : "—"
                        color: root.muzzleVelocity >= 0 ? Theme.text : Theme.unknown
                        font.pixelSize: 28
                        font.family: "PingFang SC"
                        font.bold: true
                    }
                }

                Column {
                    spacing: 2
                    Text {
                        text: "允许发弹量"
                        color: "#8E959C"
                        font.pixelSize:10
                        font.family: "PingFang SC"
                    }
                    Text {
                        objectName: "playerAllowedAmmo"
                        text: root.allowedAmmoRaw >= 0 ? String(root.allowedAmmoRaw) : "—"
                        color: root.ammoColor
                        font.pixelSize: 28
                        font.family: "PingFang SC"
                        font.bold: true
                    }

                }
            }
        }
    }

}
