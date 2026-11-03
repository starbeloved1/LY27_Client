import QtQuick

Rectangle {
    id: root
    objectName: "matchHeader"
    property var match: ({})
    readonly property bool narrow: width < 1050
    readonly property real hudScale: narrow ? 1 : width / 1776
    readonly property int total: match.totalRounds > 0 ? match.totalRounds : 3
    readonly property real hudHeight: narrow ? 232 : 130 * hudScale
    height: hudHeight + resourcePanel.height
    color: "#080c10"

    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#28333d" }
    // 宽屏整体按同一比例缩放，队徽、侧翼和中间面板保持原来的衔接关系。
    Item {
        id: hud
        width: root.narrow ? root.width : 1776
        height: root.narrow ? 232 : 130
        scale: root.hudScale
        transformOrigin: Item.TopLeft
        Item {
            id: center
            objectName: "matchCenterPanel"
            width: root.narrow ? 360 : 414
            height: width * 106 / 414
            anchors.horizontalCenter: parent.horizontalCenter
            Image {
                anchors.fill: parent
                source: "images/top_mid/top_middle_background_v2.png"
                fillMode: Image.PreserveAspectFit
            }
            Text {
                objectName: "matchRound"
                x: parent.width * 0.34; y: parent.height * 0.045
                width: parent.width * 0.32; height: 18
                text: "BO" + root.total + " · 第 " + (root.match.currentRound > 0 ? root.match.currentRound : "—") + " 局"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: "#e0eaf2"; font.pixelSize: 13
            }
            Text {
                objectName: "matchRedScore"
                x: parent.width * 0.125; y: parent.height * 0.32
                width: parent.width * 0.15; height: parent.height * 0.4
                text: root.match.redScore >= 0 ? root.match.redScore : "—"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: "#ff2738"; font.pixelSize: 29; font.bold: true
            }
            Text {
                objectName: "matchTimer"
                x: parent.width * 0.335; y: parent.height * 0.40
                width: parent.width * 0.33; height: parent.height * 0.43
                text: root.match.timer || "--:--"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: root.match.paused ? "#ffda7e" : "#ffffff"
                font.pixelSize: 30; font.family: "Arial"; font.bold: true
            }
            Text {
                objectName: "matchBlueScore"
                x: parent.width * 0.725; y: parent.height * 0.32
                width: parent.width * 0.15; height: parent.height * 0.4
                text: root.match.blueScore >= 0 ? root.match.blueScore : "—"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: "#178cff"; font.pixelSize: 29; font.bold: true
            }
        }
        Text {
            objectName: "matchStage"
            anchors.horizontalCenter: center.horizontalCenter
            anchors.top: center.bottom
            anchors.topMargin: 4
            width: center.width; height: 16
            text: root.match.stage || "等待比赛数据"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: root.match.paused ? "#ffda7e" : "#a9c0d3"
            font.pixelSize: 12
        }
        Repeater {
            model: 2
            delegate: BaseHealthWing {
                required property int index
                blue: index === 1
                compact: root.narrow
                width: root.narrow ? (hud.width - 96) / 2 : (hud.width - center.width - 48) / 2
                height: root.narrow ? 104 : 120
                x: blue ? (root.narrow ? hud.width / 2 + 36 : center.x + center.width + 12) : 12
                y: root.narrow ? 124 : 0
                teamName: blue ? (root.match.blueTeam || "蓝方队伍") : (root.match.redTeam || "红方队伍")
                hp: blue ? (root.match.blueBaseHp ?? -1) : (root.match.redBaseHp ?? -1)
                baseState: blue ? (root.match.blueBaseState ?? -1) : (root.match.redBaseState ?? -1)
                statusText: blue ? (root.match.blueBaseStatusText || "状态未知") : (root.match.redBaseStatusText || "状态未知")
                outpostHp: blue ? (root.match.bOutpostHp ?? -1) : (root.match.rOutpostHp ?? -1)
            }
        }
        Row {
            anchors.right: parent.right
            anchors.rightMargin: 12
            y: 2
            spacing: 5
            Rectangle {
                width: 7; height: 7; radius: 3.5
                anchors.verticalCenter: parent.verticalCenter
                color: root.match.fresh ? "#38f17e" : "#65717b"
            }
            Text {
                text: "DATA"
                color: root.match.fresh ? "#b2f1cc" : "#8b99a5"
                font.pixelSize: root.narrow ? 10 : 12
                font.bold: true
            }
        }
    }
    MatchResources {
        id: resourcePanel
        anchors.left: parent.left
        anchors.right: parent.right
        y: root.hudHeight
        match: root.match
    }
}
