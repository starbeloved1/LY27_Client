import QtQuick
import LY27.Ui 1.0

Rectangle {
    id: root
    objectName: "eventPanel"
    property var eventModel: null
    readonly property bool cramped: width < 180
    color: Theme.panel
    SectionTitle { id: title; x: 12; y: 11; text: "战场事件" }
    UiText {
        anchors.right: parent.right; anchors.rightMargin: 12; anchors.verticalCenter: title.verticalCenter
        text: list.count; color: Theme.secondary; font.pixelSize: Theme.caption; numeric: true
    }
    Rectangle { x: 12; y: 42; width: parent.width - 24; height: 1; color: Theme.hairline }
    ListView {
        id: list
        objectName: "eventList"
        anchors.fill: parent
        anchors.topMargin: 50; anchors.bottomMargin: 12
        anchors.leftMargin: 12; anchors.rightMargin: 12
        model: root.eventModel
        clip: true; spacing: 0
        boundsBehavior: Flickable.StopAtBounds
        property bool followTail: true
        function scrollToTail() { if (followTail) positionViewAtEnd() }
        function requestTailScroll() { followTail = true; Qt.callLater(scrollToTail) }
        // 保持现有历史顺序、自动跟随和手动查看行为，只更改行的视觉层级。
        onCountChanged: requestTailScroll()
        onHeightChanged: requestTailScroll()
        onWidthChanged: requestTailScroll()
        onContentHeightChanged: if (followTail) Qt.callLater(scrollToTail)
        onMovementStarted: followTail = false
        delegate: Item {
            id: row
            required property int index
            required property int eventId
            required property string eventTime
            required property string eventText
            required property string eventColor
            required property string eventTimeSource
            readonly property bool income: eventId === 0
            readonly property bool major: eventId === 1 || eventId === 2 || eventId === 11 || eventId === 14
            readonly property bool territory: eventId >= 5 && eventId <= 8
            width: ListView.view.width
            height: Math.max(40, message.implicitHeight + (root.cramped ? 36 : 18))
            Rectangle { width: parent.width; height: 1; anchors.bottom: parent.bottom; color: Theme.hairline }
            UiText {
                x: 0; y: 10; width: 50; height: 16
                text: row.eventTime; color: Theme.muted; font.pixelSize: Theme.caption; numeric: true
                // 时间列固定字号，不随窗口宽度缩放；超过五个字符时省略。
                textFormat: Text.PlainText; elide: Text.ElideRight
                HoverHandler { id: timeHover }
                TechPanel {
                    visible: timeHover.hovered; y: 18; z: 4
                    width: timeSource.implicitWidth + 12; height: 22; cut: 2
                    fill: Theme.overlay; line: Theme.border
                    UiText { id: timeSource; anchors.centerIn: parent; text: row.eventTimeSource; font.pixelSize: Theme.caption }
                }
            }
            TechPanel {
                x: 54; y: 10; width: 13; height: 15; cut: 2
                fill: row.income ? Qt.rgba(Theme.positive.r, Theme.positive.g, Theme.positive.b, .12) : "transparent"
                line: "transparent"
                // 实心几何符号不依赖字体字形，保证小尺寸下仍能看清事件颜色。
                Rectangle {
                    anchors.centerIn: parent
                    visible: !row.income && !row.major
                    width: row.territory ? 7 : 6; height: width
                    radius: row.territory ? 0 : width / 2
                    rotation: row.territory ? 45 : 0
                    color: row.eventColor; antialiasing: true
                }
                UiText {
                    anchors.centerIn: parent
                    visible: row.income || row.major
                    text: row.income ? "¥" : "!"
                    color: row.income ? Theme.positive : row.eventColor
                    font.pixelSize: row.income ? 14 : Theme.caption; font.bold: true
                }
            }
            UiText {
                id: message
                objectName: "eventMessage" + row.index
                x: root.cramped ? 0 : 72; y: root.cramped ? 28 : 8; width: parent.width - x
                text: row.eventText
                color: row.income ? Theme.secondary : row.major ? row.eventColor : Theme.text
                font.pixelSize: Theme.body; font.bold: row.major
                wrapMode: Text.Wrap; textFormat: Text.PlainText; lineHeight: 1.10
            }
        }
    }
    UiText {
        anchors.centerIn: list; width: list.width
        visible: list.count === 0
        text: "暂无事件"; color: Theme.muted; font.pixelSize: Theme.body
        horizontalAlignment: Text.AlignHCenter
    }
}
