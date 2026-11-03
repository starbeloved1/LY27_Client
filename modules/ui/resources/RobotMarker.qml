import QtQuick

Item {
    id: root
    property bool blue: false
    property bool self: false
    property int number: 1
    property real angle: 0
    property string healthText: "—"
    property string rankText: "Lv.—"
    property bool healthKnown: false
    property real ratio: -1
    property real mapWidth: 0
    property real mapHeight: 0
    readonly property string team: blue ? "blue" : "red"
    readonly property color markerColor: self ? "#6eeaa1" : (blue ? "#319de8" : "#e95255")
    width: 1
    height: 1
    Image {
        x: -19; y: -19; width: 38; height: 38
        source: root.self ? "images/minimap/self_map_robot.png" : "images/minimap/" + root.team + "_map_robot_lockbg.png"
    }
    Image {
        x: -19; y: -19; width: 38; height: 38
        source: "images/minimap/" + (root.self ? "self" : root.team) + "_map_arrow.png"
        rotation: root.angle
    }
    Image {
        anchors.centerIn: parent
        width: root.number === 7 ? 21 : 15
        height: width
        source: root.number === 7 ? "images/minimap/" + root.team + "_map_guard.png"
                                : "images/minimap/map_robot_id_" + root.number + ".png"
        fillMode: Image.PreserveAspectFit
    }
    Item {
        id: caption
        objectName: "robotHealthCaption"
        x: Math.max(-root.x + 2, Math.min(-56, root.mapWidth - root.x - width - 2))
        y: root.y < 48 ? 23 : -46
        width: 112; height: 24
        Rectangle {
            id: rank
            width: 35; height: parent.height
            color: "#f0131d26"
            border.color: "#6b7e8e"
            Text {
                anchors.centerIn: parent
                text: root.rankText
                color: "#f0f6fc"; font.pixelSize: 11; font.bold: true
            }
        }
        Rectangle {
            id: track
            objectName: "robotHealthTrack"
            anchors.left: rank.right
            anchors.leftMargin: 2
            width: parent.width - rank.width - 2
            height: parent.height
            color: "#f0131d26"
            // 不知道最大血量时不画假满条，只显示已收到的数值。
            Rectangle {
                objectName: "robotHealthFill"
                x: 1; y: 1
                width: root.healthKnown && root.ratio >= 0 ? (parent.width - 2) * Math.max(0, Math.min(1, root.ratio)) : 0
                height: parent.height - 2
                color: root.markerColor
                Behavior on width { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
            }
            Rectangle {
                anchors.fill: parent
                color: "transparent"
                border.color: root.healthKnown ? root.markerColor : "#6b7e8e"
            }
            Text {
                objectName: "robotHealthValue"
                anchors.fill: parent
                text: root.healthText
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: "#ffffff"; font.pixelSize: 13; font.bold: true
                style: Text.Outline; styleColor: "#16212b"
            }
        }
    }
}
