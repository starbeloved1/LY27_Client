import QtQuick
import LY27.Ui 1.0

// 基地侧翼组合：基地血条、队徽与队名；前哨站仅在地图显示。
Item {
    id: root
    property bool blue: false
    property bool compact: false
    property string teamName: ""
    property int hp: -1
    readonly property string prefix: blue ? "blue" : "red"
    readonly property real logoSize: compact ? 48 : 60
    readonly property real barX: 0
    readonly property real barWidth: width
    readonly property real barY: compact ? 37 : 42
    readonly property real barHeight: compact ? 24 : 26

    UiText {
        objectName: root.prefix + "TeamName"
        x: root.barX + (root.blue ? root.logoSize * 0.62 : 0)
        y: 0
        width: root.barWidth - root.logoSize * 0.62
        height: root.compact ? 29 : 36
        text: root.teamName
        color: Theme.text
        font.pixelSize: root.compact ? Theme.body : 20
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
        healthFontSize: root.compact ? 21 : 27
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
        opacity: .84
        z: 2
    }
}
