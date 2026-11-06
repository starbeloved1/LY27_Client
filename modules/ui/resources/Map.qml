import QtQuick

Rectangle {
    id: root
    width: 1280
    height: 800
    color: "#11161b"
    property var viewModel: terminal
    readonly property bool blueView: viewModel ? viewModel.blueView : false
    readonly property real mapAspect: 1662 / 946
    MatchHeader {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        match: root.viewModel ? root.viewModel.match : ({})
    }
    Item {
        id: mapArea
        objectName: "mapArea"
        anchors.top: header.bottom
        anchors.bottom: analysis.top
        anchors.left: events.right
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
                    rankText: levelText
                    healthKnown: hpKnown
                    ratio: hpRatio
                    mapWidth: markerLayer.width
                    mapHeight: markerLayer.height
                    Behavior on x { enabled: positionFresh && !root.flipping; NumberAnimation { duration: 85 } }
                    Behavior on y { enabled: positionFresh && !root.flipping; NumberAnimation { duration: 85 } }
                }
            }
            Repeater {
                // 对应底图 1662 × 946 中的设施中心；两端中部为基地，河道两侧的小圆塔为前哨站。
                model: [
                    {key: "RedBase", field: "redBaseHp", blue: false, px: 171, py: 461, radius: 62, maximum: 10000},
                    {key: "BlueBase", field: "blueBaseHp", blue: true, px: 1514, py: 464, radius: 62, maximum: 10000},
                    {key: "ROutpost", outpost: true, field: "rOutpostHp", blue: false, px: 667, py: 685, radius: 30, maximum: 1000},
                    {key: "BOutpost", outpost: true, field: "bOutpostHp", blue: true, px: 1028, py: 237, radius: 30, maximum: 1000}
                ]
                delegate: FacilityMapMarker {
                    required property var modelData
                    objectName: "map" + modelData.key
                    x: (root.blueView ? 1 - modelData.px / 1662 : modelData.px / 1662) * markerLayer.width
                    y: (root.blueView ? 1 - modelData.py / 946 : modelData.py / 946) * markerLayer.height
                    outpost: modelData.outpost ?? false
                    blue: modelData.blue
                    health: root.viewModel ? (root.viewModel.match[modelData.field] ?? -1) : -1
                    maximum: modelData.maximum
                    footprintRadius: modelData.radius / 946 * markerLayer.height
                    mapWidth: markerLayer.width
                    mapHeight: markerLayer.height
                }
            }
        }
        MechanismNotices {
            id: mechanismNotices
            anchors.top: parent.top
            anchors.topMargin: 12
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(1320, parent.width - 24)
            notices: root.viewModel ? root.viewModel.mechanisms : []
            z: 10
        }
        BattleAnnouncement {
            anchors.horizontalCenter: parent.horizontalCenter
            y: Math.max(4, Math.min(parent.height - height - 4,
                mechanismNotices.height > 0 ? mechanismNotices.y + mechanismNotices.height + 8 : 24))
            width: Math.min(600, parent.width - 24)
            height: 50
            notice: root.viewModel ? root.viewModel.battleNotice : ({})
            z: 11
        }
    }
    EventPanel {
        id: events
        anchors.top: header.bottom
        anchors.bottom: analysis.top
        anchors.left: parent.left
        width: Math.round(Math.min(250, Math.max(170, root.width * 0.155)))
        eventModel: root.viewModel ? root.viewModel.events : null
    }
    property bool flipping: false
    Connections {
        target: root.viewModel
        function onBlueViewChanged() {
            root.flipping = true
            Qt.callLater(function() { root.flipping = false })
        }
    }
    BattleAnalysisPanel {
        id: analysis
        // 宽度受限时用地图之外的余高放分析板；高度受限时保留紧凑信息区，将其余空间让给地图。
        height: Math.round(Math.max(width < 760 ? 184 : 148, Math.min(width < 760 ? 194 : 170,
            root.height - header.height - (root.width - events.width) / root.mapAspect)))
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        match: root.viewModel ? root.viewModel.match : ({})
        blueView: root.blueView
        onSwitchViewRequested: if (root.viewModel) root.viewModel.blueView = !root.blueView
    }
    MatchPhaseOverlay {
        anchors.fill: parent
        phase: root.viewModel ? root.viewModel.phase : ({})
        match: root.viewModel ? root.viewModel.match : ({})
        z: 100
    }
}
