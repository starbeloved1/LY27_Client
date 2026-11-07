import QtQuick
import LY27.Ui 1.0

Rectangle {
    id: root
    width: 1280
    height: 800
    color: Theme.background
    property var viewModel: terminal
    readonly property bool blueView: viewModel ? viewModel.blueView : false
    readonly property var tactics: viewModel ? viewModel.tactics : null
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
        anchors.leftMargin: Theme.gap
        anchors.right: parent.right
        clip: true
        Rectangle { anchors.fill: parent; color: Theme.panel }
        Image {
            id: battlefield
            objectName: "battlefieldImage"
            anchors.fill: parent
            source: "image://battlefield/map"
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
            rotation: root.blueView ? 180 : 0
        }
        // 保留验收标识作为图片内框，不再叠灰色蒙层；色调由缓存的图片提供器完成。
        Rectangle {
            objectName: "mapContrastVeil"
            width: battlefield.paintedWidth; height: battlefield.paintedHeight
            anchors.centerIn: parent
            color: "transparent"; border.color: Theme.divider
        }
        Rectangle {
            anchors.fill: parent
            color: "transparent"; border.color: Theme.border
            z: 2
        }
        // 标记跟随实际图片区域，不把窗口留白算进场地。
        Item {
            id: markerLayer
            objectName: "mapMarkerLayer"
            width: battlefield.paintedWidth
            height: battlefield.paintedHeight
            anchors.centerIn: parent
            MouseArea {
                objectName: "tacticalMapHitArea"
                anchors.fill: parent
                enabled: root.tactics ? root.tactics.commander : false
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: function(mouse) {
                    if (mouse.button === Qt.RightButton) root.tactics.cancel()
                    else root.tactics.clickMap(mouse.x / width, mouse.y / height)
                }
            }
            TerritoryOverlayItem {
                objectName: "territoryOverlay"
                anchors.fill: parent
                blueView: root.blueView
                territories: root.viewModel ? root.viewModel.territories : ({})
            }
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
                    required property var activeBuffs
                    objectName: "mapRobot" + robotId
                    x: mapX * markerLayer.width
                    y: mapY * markerLayer.height
                    visible: positionKnown
                    opacity: positionFresh ? 1 : 0.38
                    blue: isBlue
                    self: isSelf
                    selected: root.tactics ? root.tactics.selectedReceiver === robotId : false
                    interactive: root.tactics ? root.tactics.commander && positionFresh : false
                    onActivated: root.tactics.clickRobot(robotId)
                    onCancelRequested: root.tactics.cancel()
                    number: robotNumber
                    angle: heading
                    rankText: levelText
                    healthKnown: hpKnown
                    ratio: hpRatio
                    buffs: activeBuffs
                    mapWidth: markerLayer.width
                    mapHeight: markerLayer.height
                    Behavior on x { enabled: positionFresh && !root.flipping; NumberAnimation { duration: 85 } }
                    Behavior on y { enabled: positionFresh && !root.flipping; NumberAnimation { duration: 85 } }
                }
            }
            Repeater {
                model: root.tactics ? root.tactics.facilities : []
                delegate: FacilityMapMarker {
                    required property var modelData
                    objectName: "map" + modelData.key
                    x: (root.blueView ? 1 - modelData.px / 1662 : modelData.px / 1662) * markerLayer.width
                    y: (root.blueView ? 1 - modelData.py / 946 : modelData.py / 946) * markerLayer.height
                    outpost: modelData.outpost ?? false
                    interactive: root.tactics ? root.tactics.commander : false
                    onActivated: root.tactics.clickFacility(modelData.id)
                    onCancelRequested: root.tactics.cancel()
                    blue: modelData.blue
                    health: root.viewModel ? (root.viewModel.match[modelData.field] ?? -1) : -1
                    maximum: modelData.maximum
                    buffs: root.viewModel ? (root.viewModel.facilityBuffs[String(modelData.id)] ?? []) : []
                    footprintRadius: modelData.radius / 946 * markerLayer.height
                    mapWidth: markerLayer.width
                    mapHeight: markerLayer.height
                }
            }
            Repeater {
                // 接收席位固定，不随目标车的每次位置更新销毁、重建图标。
                model: root.viewModel ? root.viewModel.robots : null
                delegate: TacticalMarker {
                    required property int robotId
                    readonly property var mark: root.tactics ? (root.tactics.markerStates[String(robotId)] ?? ({})) : ({})
                    property bool tracking: false
                    objectName: "tacticalMarker" + robotId
                    visible: mark.type !== undefined
                    onVisibleChanged: {
                        tracking = false
                        Qt.callLater(function() { tracking = visible })
                    }
                    x: (mark.mapX ?? 0) * markerLayer.width
                    y: (mark.mapY ?? 0) * markerLayer.height
                    markerType: mark.type ?? 1
                    receiver: root.tactics && root.tactics.commander ? String(robotId % 100) : ""
                    lift: (mark.kind === "robot" ? 64 : mark.kind === "facility"
                        ? Math.max(34, mark.radius / 946 * markerLayer.height) + 43 : 22) + (mark.buffRow ? 16 : 0)
                    mapWidth: markerLayer.width
                    mapHeight: markerLayer.height
                    z: 5
                    Behavior on x { enabled: tracking && !root.flipping; NumberAnimation { duration: 85 } }
                    Behavior on y { enabled: tracking && !root.flipping; NumberAnimation { duration: 85 } }
                }
            }
            MapMarkerMenu {
                controller: root.tactics
                x: Math.max(2, Math.min((target.mapX ?? 0) * markerLayer.width - width / 2, markerLayer.width - width - 2))
                y: Math.max(2, Math.min((target.mapY ?? 0) * markerLayer.height - height - 12, markerLayer.height - height - 2))
                z: 20
            }
        }
        TechPanel {
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottomMargin: 6
            width: Math.min(parent.width - 12, hint.implicitWidth + 24)
            height: 26; cut: 3; z: 21
            visible: root.tactics ? root.tactics.commander || root.tactics.feedback.length > 0 : false
            fill: Theme.overlay
            line: Theme.divider
            UiText {
                id: hint
                anchors.fill: parent; anchors.margins: 4
                text: root.tactics ? root.tactics.feedback || "指挥手：选择己方车 → 点击目标；右键 / Esc 取消" : ""
                color: Theme.tactical; font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight
            }
        }
        TechPanel {
            id: selfBuffPanel
            objectName: "selfBuffPanel"
            readonly property bool compact: mapArea.height < 180
            readonly property var buffs: root.viewModel ? root.viewModel.selfBuffs : []
            visible: buffs.length > 0
            x: compact ? parent.width - width - 8 : 8
            y: compact ? 6 : parent.height - height - 8
            width: compact ? selfBuffs.width + 12 : Math.max(134, selfBuffs.width + 20)
            height: compact ? 24 : 48
            cut: 3; z: 20
            fill: Theme.overlay; line: Theme.border
            UiText {
                x: 10; y: 5
                visible: !selfBuffPanel.compact
                text: "本机增益"; color: Theme.secondary; font.pixelSize: 11
            }
            BuffRow {
                id: selfBuffs
                objectName: "selfBuffRow"
                x: selfBuffPanel.compact ? 6 : 10
                y: selfBuffPanel.compact ? 5 : 25
                buffs: selfBuffPanel.buffs
                tooltipAbove: !selfBuffPanel.compact
            }
        }
        MatchNotices {
            id: matchNotices
            anchors.top: parent.top
            anchors.topMargin: parent.height < 180 ? (selfBuffPanel.visible ? 34 : 6) : 12
            anchors.left: parent.left
            anchors.leftMargin: 12
            width: parent.width - 24
            controller: root.viewModel ? root.viewModel.notices : null
            mechanisms: root.viewModel ? root.viewModel.mechanisms : []
            animationsAllowed: root.viewModel ? (root.viewModel.match.noticesLive ?? false) : false
            epoch: root.viewModel ? (root.viewModel.match.noticeEpoch ?? "") : ""
            z: 12
        }
        BattleAnnouncement {
            anchors.horizontalCenter: parent.horizontalCenter
            // 击杀/前哨摧毁播报保持固定位置，覆盖通告框，不随框的数量向下挪动。
            y: Math.max(4, Math.min(24, parent.height - height - 4))
            width: Math.min(600, parent.width - 24)
            height: 50
            notice: root.viewModel ? root.viewModel.battleNotice : ({})
            z: 20
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
        // 保留三个分区；窄屏为后勤保留第二行，常规窗口压缩面板空白。
        height: width < 760 ? 198 : 132
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        match: root.viewModel ? root.viewModel.match : ({})
    }
    MatchPhaseOverlay {
        anchors.fill: parent
        phase: root.viewModel ? root.viewModel.phase : ({})
        match: root.viewModel ? root.viewModel.match : ({})
        z: 100
    }
    DamagePanel {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        visible: desktopWindow.injuryPanelVisible
        stats: root.viewModel ? root.viewModel.injury : ({})
        z: 200
    }
}
