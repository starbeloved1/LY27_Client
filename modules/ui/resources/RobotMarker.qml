import QtQuick
import LY27.Ui 1.0

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
    readonly property color markerColor: self ? Theme.selfFill : (blue ? Theme.teamBlueFill : Theme.teamRedFill)
    readonly property color affiliationColor: self ? Theme.selfGreen : (blue ? Theme.teamBlue : Theme.teamRed)
    readonly property bool ratioKnown: healthKnown && ratio >= 0
    readonly property color frameColor: ratioKnown ? Theme.ink : Theme.unknown
    width: 1; height: 1; z: 3
    MouseArea {
        objectName: "robotMapHitArea"
        x: -22; y: -26; width: 44; height: 46
        enabled: root.interactive
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: Qt.PointingHandCursor
        onClicked: function(mouse) {
            if (mouse.button === Qt.RightButton) root.cancelRequested()
            else root.activated()
        }
    }
    // 轻微偏移的双层投影托起棋子，不依赖模糊滤镜或额外渲染通道。
    Rectangle { x: -20; y: -18; width: 40; height: 40; radius: 20; color: "#38000000" }
    Rectangle { x: -18.5; y: -16.5; width: 37; height: 37; radius: 18.5; color: "#42000000" }
    Rectangle {
        objectName: "robotSelectionGlow"
        visible: root.selected
        x: -25; y: -25; width: 50; height: 50; radius: 25
        color: "transparent"; border.color: Theme.tactical; border.width: 4; opacity: .13
    }
    Rectangle {
        objectName: "robotSelectionRing"
        visible: root.selected
        x: -22; y: -22; width: 44; height: 44; radius: 22
        color: "transparent"; border.color: Theme.tactical; border.width: 1.6
    }
    Rectangle {
        id: selfRing
        objectName: "robotSelfRing"
        visible: root.self
        x: -20; y: -20; width: 40; height: 40; radius: 20
        color: "transparent"; border.color: Theme.selfGreen; border.width: 1.2
    }
    SequentialAnimation {
        running: root.visible && root.self && root.ratioKnown && root.ratio > 0 && root.ratio < .25
        loops: Animation.Infinite
        onRunningChanged: if (!running) selfRing.opacity = 1
        NumberAnimation { target: selfRing; property: "opacity"; from: 1; to: .60; duration: 700; easing.type: Easing.InOutSine }
        NumberAnimation { target: selfRing; property: "opacity"; to: 1; duration: 700; easing.type: Easing.InOutSine }
    }
    // 带暗色轮廓的楔形与圆边相接，绕定位中心旋转，数字保持正向。
    Item {
        width: 1; height: 1; rotation: root.angle
        Canvas {
            objectName: "robotHeadingWedge"
            x: -6.5; y: -27; width: 13; height: 12
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.beginPath()
                ctx.moveTo(width / 2, 1)
                ctx.lineTo(width - 1.5, height - 1)
                ctx.lineTo(1.5, height - 1)
                ctx.closePath()
                ctx.fillStyle = Theme.headingInk
                ctx.fill()
                ctx.strokeStyle = Theme.ink
                ctx.lineWidth = 1.2
                ctx.lineJoin = "round"
                ctx.stroke()
            }
        }
    }
    Rectangle {
        objectName: "robotBadgeOutline"
        x: -19.5; y: -19.5; width: 39; height: 39; radius: 19.5
        color: "#cc0b0f14"
    }
    Rectangle {
        id: badge
        objectName: "robotNumberBadge"
        x: -18; y: -18; width: 36; height: 36; radius: 18
        color: root.markerColor
        gradient: Gradient {
            GradientStop { position: 0; color: Qt.lighter(root.markerColor, 1.12) }
            GradientStop { position: .5; color: root.markerColor }
            GradientStop { position: 1; color: Qt.darker(root.markerColor, 1.12) }
        }
        Canvas {
            objectName: "robotBadgeHighlight"
            anchors.fill: parent
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.beginPath(); ctx.arc(width / 2, height / 2, width / 2 - 1.5, Math.PI * 1.12, Math.PI * 1.88)
                ctx.strokeStyle = "#4dffffff"; ctx.lineWidth = 1; ctx.stroke()
            }
        }
        UiText {
            anchors.centerIn: parent
            text: root.number; color: Theme.headingInk
            font.pixelSize: 22; font.family: Theme.numberFont; font.bold: true
            style: Text.Outline; styleColor: Theme.ink
        }
    }
    Item {
        id: caption
        objectName: "robotHealthCaption"
        width: 56; height: 12
        readonly property real groupWidth: Math.max(width, buffIcons.width)
        // 血条本体对中车标，等级格附在左侧；边缘处仍收拢完整 Buff 行。
        x: Math.max(2 - root.x, Math.min(-(width + rank.width) / 2, root.mapWidth - root.x - groupWidth - 2))
        readonly property real buffHeight: root.buffs.length ? 16 : 0
        y: Math.max(2 - root.y + buffHeight, Math.min(root.y < 42 + buffHeight ? 21 + buffHeight
            : -40, root.mapHeight - root.y - height - 2))
        Rectangle {
            id: rank
            objectName: "robotLevelCell"
            width: 14; height: parent.height; radius: 2
            color: Theme.overlay; border.color: root.frameColor
            UiText {
                objectName: "robotLevelValue"
                anchors.centerIn: parent
                text: root.rankText.startsWith("Lv.") ? root.rankText.slice(3) : root.rankText
                color: Theme.headingInk; font.family: Theme.numberFont; font.pixelSize: 9; font.bold: true
            }
        }
        Rectangle {
            id: track
            objectName: "robotHealthTrack"
            anchors.left: rank.right
            width: parent.width - rank.width; height: parent.height; radius: 2
            color: Theme.overlay
            Rectangle {
                objectName: "robotHealthFill"
                x: 1; y: 1
                width: root.ratioKnown ? (parent.width - 2) * Math.max(0, Math.min(1, root.ratio)) : 0
                height: parent.height - 2; radius: 1
                color: root.markerColor
                Behavior on width { NumberAnimation { duration: Theme.healthMotion; easing.type: Easing.OutCubic } }
            }
            Rectangle { anchors.fill: parent; radius: parent.radius; color: "transparent"; border.color: root.frameColor }
            UiText {
                objectName: "robotHealthUnknown"
                anchors.fill: parent; visible: !root.ratioKnown
                text: "?"; color: Theme.text; font.pixelSize: 9; font.family: Theme.numberFont
                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            }
        }
    }
    BuffRow {
        id: buffIcons
        objectName: "robotBuffRow"
        buffs: root.buffs; x: caption.x; y: caption.y - height - 2
    }
}
