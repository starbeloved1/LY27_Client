import QtQuick
import LY27.Ui 1.0

// 沿用 RM26 TopInfoBar 的中央比赛条、侧翼基地/前哨与两排单位卡片，适配 27 的四台地面机器人。
Item {
    id: root
    property var viewModel: null
    readonly property var match: viewModel ? viewModel.match : ({})
    readonly property var units: viewModel ? viewModel.playerHud.units : ({})
    width: 1920; height: 166
    MatchHeader {
        objectName: "playerMatchHeader"
        anchors.left: parent.left; anchors.right: parent.right
        match: root.match
        color: "transparent"
    }
    Repeater {
        model: [false, true]
        delegate: Item {
            required property bool modelData
            readonly property bool blue: modelData
            readonly property string owner: (blue ? root.match.bOutpostOwner : root.match.rOutpostOwner) || "unknown"
            x: blue ? root.width - width - 12 : 12
            y: 28; width: 128; height: 70
            OutpostStatusBadge {
                anchors.fill: parent
                isBlue: parent.owner === "blue"
                hp: (parent.blue ? root.match.bOutpostHp : root.match.rOutpostHp) ?? -1
                opacity: parent.owner === "unknown" ? .45 : 1
            }
        }
    }
    Repeater {
        model: [false, true]
        delegate: Row {
            required property bool modelData
            readonly property bool blue: modelData
            x: blue ? 1218 : 350
            y: 86; spacing: 8
            Repeater {
                model: [1, 3, 4, 7]
                delegate: PlayerUnitCard {
                    required property int modelData
                    objectName: "playerUnit" + robotId
                    width: 82; height: 62
                    robotId: modelData + (parent.blue ? 100 : 0)
                    isBlue: parent.blue
                    robotInfo: root.units[String(robotId)] || ({})
                    avatarSource: "qrc:/ly27/ui/images/top_robots/" + (isBlue ? "blue" : "red") +
                        (modelData === 7 ? "_guard_avatar.png" : "_teammate_avatar_" + (modelData === 1 ? "hero" : "soldier") + ".png")
                }
            }
        }
    }
    UiText {
        objectName: "playerEconomy"
        anchors.horizontalCenter: parent.horizontalCenter
        y: 112
        text: "我方经济  " + (root.match.allyEconomy >= 0 ? root.match.allyEconomy : "—")
        color: Theme.text; font.pixelSize: 17; font.bold: true
        style: Text.Outline; styleColor: Theme.ink
    }
}
