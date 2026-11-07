import QtQuick 2.15
import LY27.Ui 1.0

Item {
    id: root
    width: 92
    height: 52
    clip: true

    readonly property real contentScale: Math.min(width / 92, height / 52)

    property bool isBlue: false
    property int hp: -1
    property int maxHp: 1000
    readonly property bool destroyed: hp === 0
    readonly property real hpRatio: Math.max(0.0, Math.min(1.0, hp / Math.max(1, maxHp)))
    readonly property string teamPrefix: isBlue ? "blue" : "red"
    readonly property string cardBackgroundSource: "images/top_robots/" + teamPrefix + "_teammate_bg.png"
    readonly property string barSource: "images/top_robots/" + teamPrefix + "_tab_bar.png"
    readonly property string outpostNameSource: isBlue ? "images/top_outpost/blue_outpost_name.png"
                                                       : "images/top_outpost/red_outpost_name.png"
    readonly property string outpostIconSource: isBlue ? "images/top_outpost/blue_outpost_avatar.png"
                                                       : "images/top_outpost/red_outpost_avatar.png"

    Image {
        id: bg
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        width: Math.round(92 * root.contentScale)
        height: Math.round(46 * root.contentScale)
        source: root.cardBackgroundSource
        fillMode: Image.Stretch
        opacity: root.destroyed ? 0.6 : 1.0
    }

    Image {
        source: root.outpostNameSource
        fillMode: Image.PreserveAspectFit
        width: Math.round(50 * root.contentScale)
        height: Math.round(15 * root.contentScale)
        anchors.left: root.isBlue ? undefined : bg.left
        anchors.right: root.isBlue ? bg.right : undefined
        anchors.leftMargin: Math.round(10 * root.contentScale)
        anchors.rightMargin: Math.round(10 * root.contentScale)
        anchors.top: bg.top
        anchors.topMargin: Math.round(5 * root.contentScale)
        opacity: root.destroyed ? 0.7 : 1.0
    }

    UiText {
        id: hpText
        anchors.left: root.isBlue ? undefined : bg.left
        anchors.right: root.isBlue ? bg.right : undefined
        anchors.leftMargin: Math.round(24 * root.contentScale)
        anchors.rightMargin: Math.round(24 * root.contentScale)
        anchors.bottom: hpBar.top
        anchors.bottomMargin: Math.round(2 * root.contentScale)
        // 给四位血量留足宽度，避免默认字体加粗后把 1000 截成省略号。
        width: Math.round(40 * root.contentScale)
        objectName: "outpostHealthValue"
        text: root.hp >= 0 ? root.hp.toString() : "—"
        color: root.destroyed ? "#D4D7DC" : Theme.text
        font.pixelSize: Math.round(13 * root.contentScale)
        font.bold: true
        horizontalAlignment: root.isBlue ? Text.AlignRight : Text.AlignLeft
        elide: Text.ElideRight
        style: Text.Outline
        styleColor: "#101214"
    }

    Image {
        source: root.outpostIconSource
        fillMode: Image.PreserveAspectFit
        width: Math.round(12 * root.contentScale)
        height: Math.round(14 * root.contentScale)
        anchors.left: root.isBlue ? undefined : hpText.right
        anchors.right: root.isBlue ? hpText.left : undefined
        anchors.leftMargin: Math.round(13 * root.contentScale)
        anchors.rightMargin: Math.round(13 * root.contentScale)
        anchors.verticalCenter: hpText.verticalCenter
        anchors.verticalCenterOffset: Math.round(root.contentScale)
        opacity: root.destroyed ? 0.7 : 1.0
    }

    Item {
        id: hpBar
        anchors.bottom: bg.bottom
        anchors.bottomMargin: 0
        anchors.left: root.isBlue ? bg.left : undefined
        anchors.right: root.isBlue ? undefined : bg.right
        anchors.leftMargin: Math.round(3 * root.contentScale)
        anchors.rightMargin: Math.round(3 * root.contentScale)
        width: Math.round(64 * root.contentScale)
        height: Math.round(6 * root.contentScale)

        Image {
            anchors.fill: parent
            source: root.barSource
            fillMode: Image.Stretch
            opacity: root.destroyed ? 0.5 : 0.95
        }

        Rectangle {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width * root.hpRatio
            height: Math.max(2, Math.round(2 * root.contentScale))
            radius: height / 2
            color: root.destroyed ? "#999999" : (root.isBlue ? Theme.teamBlue : Theme.teamRed)
            opacity: root.destroyed ? 0.6 : 0.95
        }
    }

    Rectangle {
        anchors.fill: bg
        color: "#44000000"
        visible: root.destroyed
    }
}
