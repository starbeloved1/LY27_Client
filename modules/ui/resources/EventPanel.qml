import QtQuick

Rectangle {
    id: root
    objectName: "eventPanel"
    property var eventModel: null
    color: "#111b24"
    border.color: "#2b3b49"
    Text {
        x: 14; y: 12
        text: "战场事件"
        color: "#f0c478"; font.pixelSize: 16; font.bold: true
    }
    Text {
        anchors.right: parent.right; anchors.rightMargin: 14
        y: 16
        text: list.count + " / 50"
        color: "#8da2b3"; font.pixelSize: 11
    }
    Rectangle { x: 12; y: 42; width: parent.width - 24; height: 1; color: "#324452" }
    ListView {
        id: list
        objectName: "eventList"
        anchors.fill: parent
        anchors.topMargin: 53; anchors.bottomMargin: 12
        anchors.leftMargin: 12; anchors.rightMargin: 12
        model: root.eventModel
        clip: true
        spacing: 8
        boundsBehavior: Flickable.StopAtBounds
        // 沿用 26：按收到的顺序追加，新增消息滚动至列表末尾。
        onCountChanged: Qt.callLater(function() { list.positionViewAtEnd() })
        onHeightChanged: Qt.callLater(function() { list.positionViewAtEnd() })
        delegate: Rectangle {
            id: row
            required property int index
            required property string eventTime
            required property string eventText
            required property string eventColor
            required property string eventTimeSource
            width: ListView.view.width
            height: message.implicitHeight + 42
            color: "#192630"
            radius: 3
            Rectangle { width: 3; height: parent.height - 12; y: 6; color: row.eventColor; radius: 1 }
            Text {
                x: 12; y: 8; width: parent.width - 24
                text: row.eventTime + " · " + row.eventTimeSource
                color: "#91a8ba"; font.pixelSize: 10
                textFormat: Text.PlainText
            }
            Text {
                id: message
                objectName: "eventMessage" + row.index
                x: 12; y: 27; width: parent.width - 24
                text: row.eventText
                color: row.eventColor; font.pixelSize: 12
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                lineHeight: 1.15
            }
        }
    }
    Text {
        anchors.centerIn: list
        width: list.width
        visible: list.count === 0
        text: "暂无事件"
        color: "#708697"; font.pixelSize: 13
        horizontalAlignment: Text.AlignHCenter
    }
}
