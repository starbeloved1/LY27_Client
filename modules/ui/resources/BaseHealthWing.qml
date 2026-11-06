import QtQuick

// 基地侧翼组合：基地血条、队徽、队名与状态行；前哨站仅在地图显示。
Item {
    id: root
    property bool blue: false
    property bool compact: false
    property string teamName: ""
    property int hp: -1
    property int baseState: -1
    property string statusText: "状态未知"
    readonly property string prefix: blue ? "blue" : "red"
    readonly property real logoSize: compact ? 64 : 76
    readonly property real barX: 0
    readonly property real barWidth: width
    readonly property real barY: compact ? 29 : 28
    readonly property real barHeight: compact ? 30 : 34

    Text {
        objectName: root.prefix + "TeamName"
        x: root.barX + (root.blue ? root.logoSize * 0.62 : 0)
        y: 0
        width: root.barWidth - root.logoSize * 0.62
        height: 25
        text: root.teamName
        color: "#ffffff"
        font.pixelSize: root.compact ? 15 : 22
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
        healthTextInset: root.logoSize * 0.62
        healthFontSize: root.compact ? 20 : 27
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
        visible: root.statusText.length > 0
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
}
