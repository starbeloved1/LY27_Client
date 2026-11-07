import QtQuick
import LY27.Ui 1.0

// 战场通告共用阵地激活提示的切角底框，尺寸和文字层级保持一致。
Item {
    id: root
    property string heading: ""
    property string message: ""
    property string detail: ""
    property string messageObjectName: ""
    property color accent: Theme.secondary
    property bool compact: false
    width: 300
    height: compact ? 50 : 84
    HudPanel {
        anchors.fill: parent
        accent: root.accent
        panelOpacity: .78
    }
    Rectangle {
        x: 12; y: 0; width: Math.min(parent.width * .48, 150); height: root.compact ? 12 : 20
        color: Qt.rgba(root.accent.r, root.accent.g, root.accent.b, .10)
    }
    UiText {
        x: 16; y: 2; width: parent.width - 32; height: root.compact ? 9 : 15
        text: root.heading
        color: Theme.text; font.pixelSize: root.compact ? 8 : 10; font.bold: true
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    Column {
        objectName: "noticeBody"
        anchors.centerIn: parent
        anchors.alignWhenCentered: false
        width: parent.width - 24
        spacing: root.compact ? 2 : 3
        UiText {
            objectName: root.messageObjectName
            width: parent.width
            height: root.detail.length ? (root.compact ? 14 : 24) : (root.compact ? 18 : 28)
            text: root.message
            color: Theme.text; font.pixelSize: root.compact ? 11 : 15; font.bold: true
            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            fontSizeMode: Text.Fit; minimumPixelSize: root.compact ? 8 : 10
        }
        UiText {
            objectName: "noticeDetail"
            visible: root.detail.length > 0
            width: parent.width; height: root.compact ? 11 : 18
            text: root.detail
            color: Theme.secondary; font.pixelSize: root.compact ? 10 : 12
            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            fontSizeMode: Text.Fit; minimumPixelSize: 8
        }
    }
}
