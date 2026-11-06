import QtQuick

Item {
    id: root
    property int health: -1
    property int maximum: 10000
    property bool blue: false
    property bool outpost: false
    property real footprintRadius: 0
    property real mapWidth: 0
    property real mapHeight: 0
    readonly property bool tiny: mapHeight < 180
    readonly property bool healthKnown: health >= 0 && maximum > 0
    readonly property real ratio: healthKnown ? Math.max(0, Math.min(1, health / maximum)) : 0
    readonly property color accent: blue ? "#319de8" : "#e95255"
    width: 1; height: 1
    z: 2
    Image {
        id: icon
        objectName: "facilityMapIcon"
        visible: root.outpost
        source: root.outpost ? "images/minimap/" + (root.blue ? "blue" : "red") + "_map_outpost.png" : ""
        width: root.tiny ? Math.min(28, root.mapHeight * 0.18) : 44
        height: width * 72 / 60
        x: -width / 2; y: -height / 2
        fillMode: Image.PreserveAspectFit
        smooth: true; mipmap: true
        opacity: root.health === 0 ? 0.5 : 1
    }
    Item {
        id: caption
        objectName: "facilityMapCaption"
        width: 60; height: 14
        // 血条放在设施轮廓和图标上方；临近图片边缘时整体向内收。
        x: Math.max(2 - root.x, Math.min(-width / 2, root.mapWidth - root.x - width - 2))
        y: Math.max(2 - root.y,
            -Math.max(root.footprintRadius, root.outpost ? icon.height / 2 : 0) - height - 5)
        Rectangle {
            id: track
            objectName: "facilityMapHealthTrack"
            anchors.fill: parent
            radius: 3
            color: "#f0131d26"
            Rectangle {
                objectName: "facilityMapHealthFill"
                x: 1; y: 1
                height: parent.height - 2
                width: (parent.width - 2) * root.ratio
                radius: 2
                color: root.accent
                Behavior on width { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
            }
            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: "transparent"
                border.color: root.healthKnown ? "#efd17c" : "#8192a0"
            }
            Text {
                objectName: "facilityMapHealthUnknown"
                visible: !root.healthKnown
                anchors.fill: parent
                text: "?"
                color: "#ffffff"; font.pixelSize: 10
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}
