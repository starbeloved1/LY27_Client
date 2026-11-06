import QtQuick

Item {
    id: root
    property bool blue: false
    property bool self: false
    property int number: 1
    property real angle: 0
    property string rankText: "Lv.—"
    property bool healthKnown: false
    property real ratio: -1
    property real mapWidth: 0
    property real mapHeight: 0
    readonly property color markerColor: self ? "#6eeaa1" : (blue ? "#319de8" : "#e95255")
    readonly property bool ratioKnown: healthKnown && ratio >= 0
    readonly property color frameColor: ratioKnown ? "#efd17c" : "#8192a0"
    width: 1
    height: 1

    // 朝向箭头绕定位中心旋转，编号和状态条始终保持正向。
    Item {
        width: 1; height: 1
        rotation: root.angle
        Canvas {
            x: -5; y: -23; width: 10; height: 7
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.fillStyle = "#d6e5f3"
                ctx.beginPath()
                ctx.moveTo(width / 2, 0)
                ctx.lineTo(width, height)
                ctx.lineTo(0, height)
                ctx.closePath()
                ctx.fill()
            }
        }
    }
    Rectangle {
        objectName: "robotNumberBadge"
        x: -14; y: -14; width: 28; height: 28
        radius: 14
        color: root.markerColor
        border.color: Qt.lighter(root.markerColor, 1.2)
        Text {
            anchors.centerIn: parent
            text: root.number
            color: "#eef5ff"
            font.pixelSize: 20
            font.family: "Arial"
        }
    }
    Item {
        id: caption
        objectName: "robotHealthCaption"
        // 血条中心对齐车标；等级贴在血条左侧，临近地图边界时整体向内收。
        x: Math.max(-root.x + 2, Math.min(-48, root.mapWidth - root.x - width - 2))
        y: root.y < 42 ? 19 : -40
        width: 78; height: 14
        Rectangle {
            id: rank
            objectName: "robotLevelCell"
            width: 18; height: parent.height
            radius: 2
            color: "#f0131d26"
            border.color: root.frameColor
            Text {
                objectName: "robotLevelValue"
                anchors.centerIn: parent
                text: root.rankText.startsWith("Lv.") ? root.rankText.slice(3) : root.rankText
                color: "#f0f6fc"; font.pixelSize: 10; font.bold: true
            }
        }
        Rectangle {
            id: track
            objectName: "robotHealthTrack"
            anchors.left: rank.right
            width: parent.width - rank.width
            height: parent.height
            radius: 3
            color: "#f0131d26"
            // 血量或上限未知时显示问号，与真实零血量的空条区分。
            Rectangle {
                objectName: "robotHealthFill"
                x: 1; y: 1
                width: root.ratioKnown ? (parent.width - 2) * Math.max(0, Math.min(1, root.ratio)) : 0
                height: parent.height - 2
                radius: 2
                color: root.markerColor
                Behavior on width { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
            }
            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: "transparent"
                border.color: root.frameColor
            }
            Text {
                objectName: "robotHealthUnknown"
                anchors.fill: parent
                visible: !root.ratioKnown
                text: "?"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: "#ffffff"; font.pixelSize: 10
            }
        }
    }
}
