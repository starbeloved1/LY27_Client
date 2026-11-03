import QtQuick

Rectangle {
    id: root
    width: 1280
    height: 800
    color: "#11161b"
    property var viewModel: terminal
    readonly property bool blueView: viewModel ? viewModel.blueView : false
    MatchHeader {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        match: root.viewModel ? root.viewModel.match : ({})
    }
    Item {
        id: mapArea
        anchors.top: header.bottom
        anchors.bottom: footer.top
        anchors.left: parent.left
        anchors.right: parent.right
        clip: true
        Image {
            id: battlefield
            objectName: "battlefieldImage"
            anchors.fill: parent
            source: "maps/battlefield.png"
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
            rotation: root.blueView ? 180 : 0
        }
        // 标记跟随实际图片区域，不把窗口留白算进场地。
        Item {
            id: markerLayer
            objectName: "mapMarkerLayer"
            width: battlefield.paintedWidth
            height: battlefield.paintedHeight
            anchors.centerIn: parent
            Repeater {
                model: root.viewModel ? root.viewModel.robots : null
                delegate: RobotMarker {
                    required property int robotId
                    required property int robotNumber
                    required property bool isBlue
                    required property bool isSelf
                    required property real mapX
                    required property real mapY
                    required property real heading
                    required property bool positionKnown
                    required property bool positionFresh
                    required property string hpText
                    required property string levelText
                    required property bool hpKnown
                    required property real hpRatio
                    objectName: "mapRobot" + robotId
                    x: mapX * markerLayer.width
                    y: mapY * markerLayer.height
                    visible: positionKnown
                    opacity: positionFresh ? 1 : 0.38
                    blue: isBlue
                    self: isSelf
                    number: robotNumber
                    angle: heading
                    healthText: hpText
                    rankText: levelText
                    healthKnown: hpKnown
                    ratio: hpRatio
                    mapWidth: markerLayer.width
                    mapHeight: markerLayer.height
                    Behavior on x { enabled: positionFresh && !root.flipping; NumberAnimation { duration: 85 } }
                    Behavior on y { enabled: positionFresh && !root.flipping; NumberAnimation { duration: 85 } }
                }
            }
        }
    }
    property bool flipping: false
    Connections {
        target: root.viewModel
        function onBlueViewChanged() {
            root.flipping = true
            Qt.callLater(function() { root.flipping = false })
        }
    }
    Rectangle {
        id: footer
        height: 36
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        color: "#171f27"
        Text {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            text: root.viewModel && root.viewModel.match.fresh
                  ? "赛事数据已接收 · 图标变淡表示位置过期"
                  : "等待赛事数据 · 请确认 MQTT 连接已启用"
            color: "#a4b1bd"
            font.pixelSize: 12
            width: parent.width - viewButton.width - 44
            elide: Text.ElideRight
        }
        Rectangle {
            id: viewButton
            width: 110
            height: 27
            radius: 3
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            color: "#25323e"
            Text {
                anchors.centerIn: parent
                text: root.blueView ? "蓝方视角 ⇄" : "红方视角 ⇄"
                color: "#e6edf5"
                font.pixelSize: 12
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: if (root.viewModel) root.viewModel.blueView = !root.blueView
            }
        }
    }
}
