import QtQuick
import LY27.Ui 1.0

// 战术地图与图传小地图共享位置、设施拥有方和控制区绘制，避免两套显示规则漂移。
Item {
    id: root
    property var viewModel: null
    property bool interactive: true
    property real minimumRobotScale: 0
    property real facilityScale: 1
    readonly property bool blueView: viewModel ? viewModel.blueView : false
    readonly property var tactics: viewModel ? viewModel.tactics : null
    property bool flipping: false
    // 在占地比例上适当放大以便识别，上限为旧固定尺寸的九成，车载 UI 同步缩放。
    readonly property real robotScale: viewModel ? Math.max(minimumRobotScale, Math.min(0.9,
        viewModel.robots.footprintPixels(markerLayer.width, markerLayer.height) / 41 * 1.2)) : 0.9
    Connections {
        target: root.viewModel
        function onBlueViewChanged() {
            root.flipping = true
            Qt.callLater(function() { root.flipping = false })
        }
    }
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
    // 地图板本身不描边，只在底图外缘留一圈半透明细线，避免框套框。
    Rectangle {
        objectName: "mapContrastVeil"
        width: battlefield.paintedWidth; height: battlefield.paintedHeight
        anchors.centerIn: parent
        color: "transparent"; border.color: Theme.hairline
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
            enabled: root.interactive && (root.tactics ? root.tactics.commander : false)
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
                interactive: root.interactive && (root.tactics ? root.tactics.commander && positionFresh : false)
                onActivated: root.tactics.clickRobot(robotId)
                onCancelRequested: root.tactics.cancel()
                number: robotNumber
                visualScale: root.robotScale
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
                readonly property string owner: root.viewModel ? root.viewModel.match[modelData.field.replace("Hp", "Owner")] ?? "unknown" : "unknown"
                objectName: "map" + modelData.key
                x: (root.blueView ? 1 - modelData.px / 1662 : modelData.px / 1662) * markerLayer.width
                y: (root.blueView ? 1 - modelData.py / 946 : modelData.py / 946) * markerLayer.height
                outpost: modelData.outpost ?? false
                visualScale: root.facilityScale
                interactive: root.interactive && (root.tactics ? root.tactics.commander : false)
                onActivated: root.tactics.clickFacility(modelData.id)
                onCancelRequested: root.tactics.cancel()
                blue: modelData.outpost ? owner === "blue" : modelData.blue
                ownershipKnown: !modelData.outpost || owner !== "unknown"
                health: root.viewModel ? (root.viewModel.match[modelData.field] ?? -1) : -1
                maximum: modelData.maximum
                buffs: root.viewModel ? (root.viewModel.facilityBuffs[String(modelData.id)] ?? []) : []
                footprintRadius: modelData.radius / 946 * markerLayer.height / root.facilityScale
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
                visualScale: mark.kind === "robot" ? root.robotScale : mark.kind === "facility" ? root.facilityScale : 1
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
            controller: root.interactive ? root.tactics : null
            x: Math.max(2, Math.min((target.mapX ?? 0) * markerLayer.width - width / 2, markerLayer.width - width - 2))
            y: Math.max(2, Math.min((target.mapY ?? 0) * markerLayer.height - height - 12, markerLayer.height - height - 2))
            z: 20
        }
    }
}
