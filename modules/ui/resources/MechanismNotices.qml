import QtQuick

Flickable {
    id: root
    objectName: "mechanismNotices"
    property var notices: []
    readonly property int columns: Math.max(1, Math.min(3, Math.floor(width / 300)))
    readonly property real cardWidth: (width - (columns - 1) * 8) / columns
    contentWidth: width
    contentHeight: cards.height
    height: Math.min(contentHeight, parent.height * 0.45)
    visible: notices.length > 0
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    Grid {
        id: cards
        columns: root.columns
        spacing: 8
        Repeater {
            // 最多六类机制，固定槽位保留 Canvas，更新秒数时不销毁重建背景。
            model: 6
            delegate: Item {
                required property int index
                readonly property var notice: root.notices[index] || ({})
                objectName: notice.id ? "mechanismNotice" + notice.id : ""
                visible: notice.id !== undefined
                width: root.cardWidth
                height: width * 132 / 540
                HudPanel {
                    width: 540; height: 132
                    scale: parent.width / width
                    transformOrigin: Item.TopLeft
                    title: notice.target || ""
                    accent: notice.accent || "#36d6ff"
                    eyebrow: notice.enemy ? "敌方推进" : "己方推进"
                    panelOpacity: 0.90
                    Text {
                        objectName: "mechanismMessage"
                        x: 20; y: 42; width: parent.width - 40; height: 34
                        text: notice.id ? notice.actor + "还有 " + notice.seconds + " 秒" : ""
                        color: "#f4fbff"; font.pixelSize: 29; font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        fontSizeMode: Text.Fit; minimumPixelSize: 15
                    }
                    Text {
                        x: 20; y: 83; width: parent.width - 40; height: 28
                        text: notice.id ? "激活" + notice.target : ""
                        color: "#d4e8f2"; font.pixelSize: 20; font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }
    }
}
