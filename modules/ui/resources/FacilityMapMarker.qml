import QtQuick

Item {
    id: root
    property string label: ""
    property int health: -1
    property int maximum: 10000
    property bool blue: false
    property real footprintRadius: 0
    property real mapWidth: 0
    property real mapHeight: 0
    readonly property bool compact: mapWidth < 700 || mapHeight < 350
    readonly property real ratio: health < 0 ? 0 : Math.max(0, Math.min(1, health / maximum))
    readonly property color accent: blue ? "#3caff4" : "#ee6468"
    width: 1; height: 1
    z: 2
    Item {
        id: caption
        objectName: "facilityMapCaption"
        width: root.compact ? 92 : 112
        height: root.compact ? 32 : 41
        // 锚点是图片中设施中心，显示块放在设施轮廓上方并留出间距。
        x: Math.max(2 - root.x, Math.min(-width / 2, root.mapWidth - root.x - width - 2))
        y: Math.max(2 - root.y, -root.footprintRadius - height - (root.compact ? 3 : 6))
        Rectangle {
            anchors.fill: parent
            color: "#ed101a23"
            border.color: root.health < 0 ? "#728694" : root.accent
            radius: 2
        }
        Text {
            width: parent.width
            height: root.compact ? 14 : 18
            text: root.label
            color: root.health < 0 ? "#a2b4c1" : root.accent
            font.pixelSize: root.compact ? 10 : 11
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        Rectangle {
            id: track
            objectName: "facilityMapHealthTrack"
            x: 3; y: root.compact ? 14 : 18
            width: parent.width - 6; height: parent.height - y - 3
            color: "#19222a"
            Rectangle {
                objectName: "facilityMapHealthFill"
                height: parent.height
                width: parent.width * root.ratio
                color: root.accent
                opacity: 0.85
                Behavior on width { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
            }
            Text {
                objectName: "facilityMapHealthValue"
                anchors.fill: parent
                text: root.health >= 0 ? root.health.toString() : "—"
                color: "#ffffff"
                font.pixelSize: root.compact ? 12 : 14
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                style: Text.Outline; styleColor: "#17212b"
            }
        }
    }
}
