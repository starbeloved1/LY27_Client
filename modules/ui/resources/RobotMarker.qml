import QtQuick

Item {
    id: root
    property bool blue: false
    property bool self: false
    property bool selected: false
    property bool interactive: false
    signal activated()
    signal cancelRequested()
    property int number: 1
    property real angle: 0
    property string rankText: "Lv.—"
    property bool healthKnown: false
    property real ratio: -1
    property var buffs: []
    property real mapWidth: 0
    property real mapHeight: 0
    readonly property color markerColor: selected ? "#d8b13f" : self ? "#6eeaa1" : (blue ? "#319de8" : "#e95255")
    readonly property bool ratioKnown: healthKnown && ratio >= 0
    readonly property color frameColor: ratioKnown ? "#efd17c" : "#8192a0"
    width: 1
    height: 1
    z: 3
    MouseArea {
        objectName: "robotMapHitArea"
        x: -22; y: -24; width: 44; height: 44
        enabled: root.interactive
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: Qt.PointingHandCursor
        onClicked: function(mouse) {
            if (mouse.button === Qt.RightButton) root.cancelRequested()
            else root.activated()
        }
    }

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
        objectName: "robotBadgeOutline"
        x: -16; y: -16; width: 32; height: 32
        radius: 16
        color: "#ee0b1118"
    }
    Rectangle {
        objectName: "robotNumberBadge"
        x: -14; y: -14; width: 28; height: 28
        radius: 14
        color: root.markerColor
        border.color: "#f2f7ff"
        border.width: 1.3
        Text {
            anchors.centerIn: parent
            text: root.number
            color: "#eef5ff"
            font.pixelSize: 20
            font.family: "Arial"
            font.bold: true
            style: Text.Outline
            styleColor: "#172430"
        }
    }
    Item {
        id: caption
        objectName: "robotHealthCaption"
        // 血条中心对齐车标；等级贴在血条左侧，临近地图边界时整体向内收。
        readonly property real groupWidth: Math.max(width, buffIcons.width)
        x: Math.max(-root.x + 2, Math.min(-48, root.mapWidth - root.x - groupWidth - 2))
        readonly property real buffHeight: root.buffs.length ? 16 : 0
        // 常规位置保留原有血条/车标间距；上边缘放不下时整组移到车标下方。
        y: Math.max(2 - root.y + buffHeight, Math.min(root.y < 42 + buffHeight ? 19 + buffHeight
            : -40, root.mapHeight - root.y - height - 2))
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
    BuffRow {
        id: buffIcons
        objectName: "robotBuffRow"
        buffs: root.buffs
        x: caption.x
        y: caption.y - height - 2
    }
}
