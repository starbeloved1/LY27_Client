import QtQuick

Column {
    id: root
    objectName: "matchNotices"
    property var controller: null
    property real timedRightInset: 0
    readonly property var timed: controller ? controller.timed : []
    readonly property var event: controller ? controller.event : ({})
    readonly property bool compact: parent.height < 180
    height: implicitHeight
    spacing: compact ? 4 : 6
    visible: timed.length > 0 || event.text !== undefined
    Rectangle {
        objectName: "timedMatchNotice"
        x: (parent.width - root.timedRightInset - width) / 2
        width: Math.min(parent.width - root.timedRightInset, timeLabel.implicitWidth + 30)
        height: root.compact ? 24 : 32; radius: 3
        visible: root.timed.length > 0
        color: "#ed0b1720"
        border.color: "#dfc476"
        Text {
            id: timeLabel
            anchors.fill: parent; anchors.margins: 5
            text: root.timed.length ? root.timed[0].text : ""
            color: "#ffe49b"; font.pixelSize: root.compact ? 12 : 16; font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            fontSizeMode: Text.Fit; minimumPixelSize: 11
        }
    }
    Rectangle {
        id: eventCard
        objectName: "liveMatchNotice"
        property int token: root.event.token ?? 0
        onTokenChanged: {
            enter.stop()
            opacity = 0
            if (token !== 0) enter.start()
        }
        width: parent.width
        height: root.compact ? 48 : eventLabel.implicitHeight + 38
        visible: root.event.text !== undefined
        color: "#ed0b1720"; radius: 3
        border.color: root.event.accent ?? "#E5E5E5"
        Rectangle {
            width: 4; height: parent.height - 8; x: 4; y: 4; radius: 2
            color: root.event.accent ?? "#E5E5E5"
        }
        Text {
            x: 16; y: root.compact ? 3 : 6
            text: "战场通告"; color: "#a9bac9"; font.pixelSize: root.compact ? 9 : 11
        }
        Text {
            id: eventLabel
            x: 16; y: root.compact ? 16 : 24; width: parent.width - 32
            height: root.compact ? 28 : implicitHeight
            text: root.event.text ?? ""
            color: root.event.accent ?? "#E5E5E5"
            font.pixelSize: root.compact ? 12 : 18; font.bold: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }
        SequentialAnimation {
            id: enter
            NumberAnimation { target: eventCard; property: "opacity"; from: 0; to: 1; duration: 140 }
            PauseAnimation { duration: 4450 }
            NumberAnimation { target: eventCard; property: "opacity"; to: 0; duration: 350 }
        }
    }
}
