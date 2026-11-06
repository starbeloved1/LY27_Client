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
            model: root.notices
            delegate: Item {
                required property var modelData
                objectName: "mechanismNotice" + modelData.id
                width: root.cardWidth
                height: width * 132 / 540
                HudPanel {
                    width: 540; height: 132
                    scale: parent.width / width
                    transformOrigin: Item.TopLeft
                    title: modelData.target
                    accent: modelData.accent
                    eyebrow: modelData.enemy ? "敌方推进" : "己方推进"
                    panelOpacity: 0.90
                    Text {
                        objectName: "mechanismMessage"
                        x: 20; y: 42; width: parent.width - 40; height: 34
                        text: modelData.actor + "还有 " + modelData.seconds + " 秒"
                        color: "#f4fbff"; font.pixelSize: 29; font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        fontSizeMode: Text.Fit; minimumPixelSize: 15
                    }
                    Text {
                        x: 20; y: 83; width: parent.width - 40; height: 28
                        text: "激活" + modelData.target
                        color: "#d4e8f2"; font.pixelSize: 20; font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }
    }
}
