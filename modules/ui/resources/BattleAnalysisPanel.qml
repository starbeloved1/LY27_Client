import QtQuick

Rectangle {
    id: root
    objectName: "battleAnalysisPanel"
    property var match: ({})
    property bool blueView: false
    signal switchViewRequested()
    color: "#10171d"

    Rectangle { width: parent.width; height: 1; color: "#344653" }
    Item {
        id: heading
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 28
        Text {
            x: 14
            anchors.verticalCenter: parent.verticalCenter
            text: "战场分析"
            color: "#d9e6ef"
            font.pixelSize: 14
            font.bold: true
        }
        Text {
            x: 116
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(0, viewButton.x - x - 12)
            text: root.match.fresh ? "控制度净速度：按当前占领状态 · 点/比赛秒" : "等待赛事数据 · 请确认 MQTT 连接"
            color: "#8297a8"
            font.pixelSize: 11
            elide: Text.ElideRight
        }
        Rectangle {
            id: viewButton
            objectName: "mapViewButton"
            width: 110
            height: 26
            radius: 3
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            color: viewMouse.containsMouse ? "#304352" : "#25323e"
            Text {
                anchors.centerIn: parent
                text: root.blueView ? "蓝方视角 ⇄" : "红方视角 ⇄"
                color: "#e6edf5"
                font.pixelSize: 12
            }
            MouseArea {
                id: viewMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.switchViewRequested()
            }
        }
    }
    MatchResources {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: heading.bottom
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 6
        match: root.match
    }
}
