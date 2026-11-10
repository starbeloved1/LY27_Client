import QtQuick
import LY27.Ui 1.0

Rectangle {
    id: root
    objectName: "battleAnalysisPanel"
    property var match: ({})
    color: "transparent"
    // 整条仪表共用一个表面，内容之间只留细分隔，不再叠三张卡片。
    Rectangle {
        anchors.fill: parent; anchors.topMargin: Theme.gutter
        gradient: Gradient {
            GradientStop { position: 0; color: Theme.panel }
            GradientStop { position: 1; color: Theme.inset }
        }
        Rectangle { width: parent.width; height: 1; color: Theme.hairline }
    }
    Item {
        id: heading
        anchors.left: parent.left; anchors.right: parent.right
        anchors.top: parent.top; anchors.topMargin: Theme.gutter
        height: 32
        SectionTitle {
            id: title
            x: root.width < 1000 ? 16 : 24
            anchors.verticalCenter: parent.verticalCenter
            text: "战场分析"; fontSize: 14
        }
        Rectangle {
            x: title.x + title.width + 16; y: parent.height / 2
            width: Math.max(0, parent.width - x - title.x); height: 1
            color: Theme.hairline
        }
    }
    MatchResources {
        anchors.left: parent.left; anchors.right: parent.right
        // 地图比例产生的余高仍由底板吸收，读数保持成组，避免数字和柱条被拉散。
        y: heading.y + heading.height + Math.max(0, (root.height - heading.y - heading.height - height - 12) / 2)
        height: Math.min(width < 760 ? 196 : 126, Math.max(0, root.height - heading.y - heading.height - 12))
        match: root.match
    }
}
