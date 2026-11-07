import QtQuick
import LY27.Ui 1.0

Item {
    id: root
    property int health: -1
    property int maximum: 10000
    property bool blue: false
    property bool outpost: false
    property bool ownershipKnown: true
    property var buffs: []
    property bool interactive: false
    signal activated()
    signal cancelRequested()
    property real footprintRadius: 0
    property real mapWidth: 0
    property real mapHeight: 0
    property real visualScale: 1
    readonly property real localScale: Math.max(visualScale, .000001)
    readonly property real localMapX: x / localScale
    readonly property real localMapY: y / localScale
    readonly property bool tiny: mapHeight < 180
    readonly property bool healthKnown: health >= 0 && maximum > 0
    readonly property real ratio: healthKnown ? Math.max(0, Math.min(1, health / maximum)) : 0
    readonly property color accent: ownershipKnown ? (blue ? Theme.teamBlueFill : Theme.teamRedFill) : Theme.unknown
    width: 1; height: 1
    scale: visualScale
    transformOrigin: Item.TopLeft
    z: 2
    MouseArea {
        objectName: "facilityMapHitArea"
        readonly property real extent: Math.max(26, root.footprintRadius)
        x: -extent; y: -extent; width: extent * 2; height: extent * 2
        enabled: root.interactive
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: Qt.PointingHandCursor
        onClicked: function(mouse) {
            if (mouse.button === Qt.RightButton) root.cancelRequested()
            else root.activated()
        }
    }
    Image {
        id: icon
        objectName: "facilityMapIcon"
        visible: root.outpost
        source: root.outpost ? "image://outpost/" + (root.ownershipKnown ? (root.blue ? "blue" : "red") : "unknown") + "_map_outpost.png" : ""
        width: root.tiny ? Math.min(33, root.mapHeight * 0.20) : 62
        height: width * 72 / 60
        x: -width / 2; y: -height / 2
        fillMode: Image.PreserveAspectFit
        smooth: true; mipmap: true
        opacity: root.health === 0 ? 0.5 : 1
    }
    Item {
        id: caption
        objectName: "facilityMapCaption"
        // 建筑血条比车辆更细长，便于在地图上区分设施与机器人。
        width: 72; height: 8
        // 血条放在设施轮廓和图标上方；临近图片边缘时整体向内收。
        readonly property real groupWidth: Math.max(width, buffIcons.width)
        x: Math.max(2 - root.localMapX, Math.min(-width / 2, root.mapWidth / root.localScale - root.localMapX - groupWidth - 2))
        y: Math.max(2 - root.localMapY + (root.buffs.length ? 16 : 0),
            -Math.max(root.footprintRadius, root.outpost ? icon.height / 2 : 0) - height - 5)
        Rectangle {
            id: track
            objectName: "facilityMapHealthTrack"
            anchors.fill: parent
            radius: 2
            color: Theme.overlay
            Rectangle {
                objectName: "facilityMapHealthFill"
                x: 1; y: 1
                height: parent.height - 2
                width: (parent.width - 2) * root.ratio
                radius: 1
                color: root.accent
                Behavior on width { NumberAnimation { duration: Theme.healthMotion; easing.type: Easing.OutCubic } }
            }
            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: "transparent"
                border.color: root.healthKnown ? Theme.ink : Theme.unknown
            }
            UiText {
                objectName: "facilityMapHealthUnknown"
                visible: !root.healthKnown
                anchors.fill: parent
                text: "?"
                color: Theme.text; font.pixelSize: 8; numeric: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
    BuffRow {
        id: buffIcons
        objectName: "facilityBuffRow"
        buffs: root.buffs
        x: caption.x
        y: caption.y - height - 2
    }
}
