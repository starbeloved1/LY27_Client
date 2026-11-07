import QtQuick
import LY27.Ui 1.0

Rectangle {
    id: root
    objectName: "battleAnalysisPanel"
    property var match: ({})
    property bool blueView: false
    signal switchViewRequested()
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
        UiText {
            x: 92; anchors.verticalCenter: parent.verticalCenter
            width: Math.max(0, viewButton.x - x - 12)
            text: root.match.fresh ? "即时战况 / 己方后勤" : "等待赛事数据 · 请确认 MQTT 连接"
            color: Theme.muted; font.pixelSize: Theme.caption; elide: Text.ElideRight
        }
        Rectangle {
            id: viewButton
            objectName: "mapViewButton"
            width: 122; height: 24; radius: 2
            anchors.right: parent.right; anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            color: viewMouse.pressed ? Theme.divider : viewMouse.containsMouse ? Theme.raised : Theme.inset
            border.color: viewMouse.containsMouse ? Theme.secondary : Theme.border
            Behavior on color { ColorAnimation { duration: Theme.quickMotion } }
            UiText {
                anchors.centerIn: parent
                text: (root.blueView ? "蓝方视角" : "红方视角") + "  ⇄"
                color: Theme.text; font.pixelSize: Theme.caption
            }
            MouseArea {
                id: viewMouse
                anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                onClicked: root.switchViewRequested()
            }
        }
    }
    MatchResources {
        anchors.left: parent.left; anchors.right: parent.right
        anchors.top: heading.bottom; anchors.bottom: parent.bottom; anchors.bottomMargin: 8
        match: root.match
    }
}
