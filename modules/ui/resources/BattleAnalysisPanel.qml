import QtQuick
import LY27.Ui 1.0

Rectangle {
    id: root
    objectName: "battleAnalysisPanel"
    property var match: ({})
    color: "transparent"
    // 顶部保留统一接缝，加高内容区以容纳更大的指标读数。
    Rectangle {
        anchors.fill: parent; anchors.topMargin: Theme.gutter
        color: Theme.panel
    }
    Item {
        id: heading
        anchors.left: parent.left; anchors.right: parent.right
        anchors.top: parent.top; anchors.topMargin: Theme.gutter
        height: 36 - Theme.gutter
        SectionTitle { x: 12; anchors.verticalCenter: parent.verticalCenter; text: "战场分析"; fontSize: 16 }
    }
    MatchResources {
        anchors.left: parent.left; anchors.right: parent.right
        anchors.top: heading.bottom; anchors.bottom: parent.bottom; anchors.bottomMargin: 8
        match: root.match
    }
}
