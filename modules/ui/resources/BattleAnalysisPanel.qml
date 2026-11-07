import QtQuick
import LY27.Ui 1.0

Rectangle {
    id: root
    objectName: "battleAnalysisPanel"
    property var match: ({})
    color: Theme.panel
    Rectangle { width: parent.width; height: 1; color: Theme.border }
    Item {
        id: heading
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
        height: 32
        UiText {
            x: 12; anchors.verticalCenter: parent.verticalCenter
            text: "战场分析"; color: Theme.text; font.pixelSize: Theme.body; font.bold: true
        }
    }
    MatchResources {
        anchors.left: parent.left; anchors.right: parent.right
        anchors.top: heading.bottom; anchors.bottom: parent.bottom; anchors.bottomMargin: 8
        match: root.match
    }
}
