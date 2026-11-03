import QtQuick

// 移植 RM26 的完整基地侧翼组合：前哨卡片、双层血条、队徽、队名与状态行。
Item {
    id: root
    property bool blue: false
    property bool compact: false
    property string teamName: ""
    property int hp: -1
    property int baseState: -1
    property string statusText: "状态未知"
    property int outpostHp: -1
    readonly property string prefix: blue ? "blue" : "red"
    readonly property real logoSize: compact ? 64 : 92
    readonly property real outpostWidth: compact ? 92 : 140
    readonly property real gap: compact ? 10 : 20
    readonly property real barX: blue ? 0 : outpostWidth + gap
    readonly property real barWidth: width - outpostWidth - gap
    readonly property real barY: compact ? 29 : 34
    readonly property real barHeight: compact ? 30 : 40
    readonly property bool invincible: baseState === 0 && hp !== 0

    Text {
        objectName: root.prefix + "TeamName"
        x: root.barX + (root.blue ? root.logoSize * 0.62 : 0)
        y: 0
        width: root.barWidth - root.logoSize * 0.62
        height: root.compact ? 25 : 29
        text: root.teamName
        color: "#ffffff"
        font.pixelSize: root.compact ? 15 : 25
        font.bold: true
        horizontalAlignment: root.blue ? Text.AlignLeft : Text.AlignRight
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    BaseHealthBar {
        id: bar
        objectName: root.prefix + "BaseHealth"
        x: root.barX; y: root.barY
        width: root.barWidth; height: root.barHeight
        currentHealth: root.hp
        isBlue: root.blue
        invincible: root.invincible
        healthTextInset: root.logoSize * 0.62
        healthFontSize: root.compact ? 20 : 31
    }
    Image {
        id: logo
        objectName: root.prefix + "TeamLogo"
        width: root.logoSize; height: width
        x: root.blue ? bar.x - width / 2 : bar.x + bar.width - width / 2
        y: bar.y + (bar.height - height) / 2
        source: "images/top_mid/" + root.prefix + "_team_logo.png"
        fillMode: Image.PreserveAspectFit
        smooth: true; mipmap: true
        z: 2
    }
    Item {
        id: status
        objectName: root.prefix + "BaseStatus"
        x: bar.x + (root.blue ? 0 : 12)
        y: bar.y + bar.height + 8
        width: bar.width - 12
        height: 22
        Row {
            anchors.left: root.blue ? undefined : parent.left
            anchors.right: root.blue ? parent.right : undefined
            height: parent.height
            spacing: root.compact ? 3 : 6
            layoutDirection: root.blue ? Qt.RightToLeft : Qt.LeftToRight
            Image {
                objectName: root.prefix + "DefenseIcon"
                width: root.compact ? 16 : 22; height: width
                visible: root.invincible
                source: "images/buffs_new/defence_100.png"
                fillMode: Image.PreserveAspectFit
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                objectName: root.prefix + "BaseStatusText"
                text: root.statusText
                color: root.baseState < 0 ? "#8b99a5" : "#f3f6f8"
                font.pixelSize: root.compact ? 10 : 14; font.bold: true
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: "基地状态"
                visible: !root.compact
                color: "#b2b9bf"; font.pixelSize: 13
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
    OutpostStatusBadge {
        id: outpost
        objectName: root.prefix + "OutpostHealth"
        x: root.blue ? root.width - width : 0
        y: root.compact ? 10 : 14
        width: root.outpostWidth
        height: width * 52 / 92
        isBlue: root.blue
        hp: root.outpostHp
    }
    Text {
        x: outpost.x; y: outpost.y + outpost.height
        width: outpost.width; height: root.compact ? 14 : 16
        text: root.blue ? "B 前哨站" : "R 前哨站"
        horizontalAlignment: Text.AlignHCenter
        color: "#8d9ba7"; font.pixelSize: root.compact ? 10 : 12
    }
}
