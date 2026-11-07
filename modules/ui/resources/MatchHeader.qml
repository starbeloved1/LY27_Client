import QtQuick
import LY27.Ui 1.0

Rectangle {
    id: root
    objectName: "matchHeader"
    property var match: ({})
    readonly property bool narrow: width < 1050
    readonly property real hudScale: narrow ? 1 : width / 1776
    readonly property int total: match.totalRounds > 0 ? match.totalRounds : 3
    readonly property real hudHeight: narrow ? 188 : 114 * hudScale
    height: hudHeight
    color: Theme.background

    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.divider }
    TechPanel {
        anchors.fill: parent; anchors.margins: 4
        cut: 6
        fill: Theme.panel; line: Theme.divider
    }
    // 宽屏整体按同一比例缩放，队徽、侧翼和中间面板保持原来的衔接关系。
    Item {
        id: hud
        width: root.narrow ? root.width : 1776
        height: root.narrow ? 188 : 114
        scale: root.hudScale
        transformOrigin: Item.TopLeft
        Item {
            id: center
            objectName: "matchCenterPanel"
            width: root.narrow ? 300 : 360
            height: width * 106 / 414
            anchors.horizontalCenter: parent.horizontalCenter
            y: 2
            // 文字和装饰图共用素材的 414×106 坐标，避免字号与背景各自缩放。
            Item {
                width: 414; height: 106
                scale: center.width / width
                transformOrigin: Item.TopLeft
                Image {
                    anchors.fill: parent
                    source: "images/top_mid/top_middle_background_v2.png"
                    fillMode: Image.PreserveAspectFit
                }
                UiText {
                    objectName: "matchRound"
                    x: 150; y: 5; width: 114; height: 17
                    text: "BO" + root.total + " · 第 " + (root.match.currentRound > 0 ? root.match.currentRound : "—") + " 局"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: Theme.secondary; font.pixelSize: 11
                }
                UiText {
                    objectName: "matchRedScore"
                    x: 60; y: 30; width: 60; height: 46
                    text: root.match.redScore >= 0 ? root.match.redScore : "—"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: root.match.redScore >= 0 ? Theme.teamRed : Theme.unknown
                    numeric: true; font.pixelSize: 36; font.bold: true; font.letterSpacing: -.5
                }
                UiText {
                    objectName: "matchTimer"
                    x: 145; y: 43; width: 124; height: 46
                    text: root.match.timer || "--:--"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: root.match.paused ? Theme.tactical : Theme.text
                    numeric: true; font.pixelSize: 38; font.bold: true; font.letterSpacing: -.5
                }
                UiText {
                    objectName: "matchBlueScore"
                    x: 294; y: 30; width: 60; height: 46
                    text: root.match.blueScore >= 0 ? root.match.blueScore : "—"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: root.match.blueScore >= 0 ? Theme.teamBlue : Theme.unknown
                    numeric: true; font.pixelSize: 36; font.bold: true; font.letterSpacing: -.5
                }
            }
        }
        UiText {
            objectName: "matchStage"
            anchors.horizontalCenter: center.horizontalCenter
            anchors.top: center.bottom
            anchors.topMargin: 0
            width: center.width; height: 18
            text: root.match.stage || "等待比赛数据"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: root.match.paused ? Theme.tactical : Theme.secondary
            font.pixelSize: 12
        }
        Repeater {
            model: 2
            delegate: BaseHealthWing {
                required property int index
                blue: index === 1
                compact: root.narrow
                width: root.narrow ? (hud.width - 96) / 2 : (hud.width - center.width - 48) / 2
                height: root.narrow ? 92 : 104
                x: blue ? (root.narrow ? hud.width / 2 + 36 : center.x + center.width + 12) : 12
                y: root.narrow ? 100 : 0
                teamName: blue ? (root.match.blueTeam || "蓝方队伍") : (root.match.redTeam || "红方队伍")
                hp: blue ? (root.match.blueBaseHp ?? -1) : (root.match.redBaseHp ?? -1)
            }
        }
    }
}
