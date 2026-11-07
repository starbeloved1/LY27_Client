import QtQuick
import LY27.Ui 1.0

// 战场通告共用阵地激活提示的切角底框，尺寸和文字层级保持一致。
Item {
    id: root
    property string heading: ""
    property string eyebrow: ""
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
        x: 12; y: 0; width: Math.min(parent.width * .48, 150); height: root.compact ? 17 : 23
        color: Qt.rgba(root.accent.r, root.accent.g, root.accent.b, .10)
    }
    UiText {
        x: 16; y: 3; width: parent.width - 108; height: root.compact ? 14 : 18
        text: root.heading
        color: Theme.text; font.pixelSize: root.compact ? 9 : 11; font.bold: true
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    UiText {
        x: parent.width - 88; y: 3; width: 76; height: root.compact ? 14 : 18
        text: root.eyebrow
        color: root.accent; font.pixelSize: root.compact ? 8 : 9
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    UiText {
        objectName: root.messageObjectName
        x: 12; y: root.compact ? 18 : 27; width: parent.width - 24
        height: root.detail.length ? (root.compact ? 15 : 25) : parent.height - y - 5
        text: root.message
        color: Theme.text; font.pixelSize: root.compact ? 12 : 15; font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.Wrap
        fontSizeMode: Text.Fit; minimumPixelSize: root.compact ? 9 : 11
    }
    UiText {
        visible: root.detail.length > 0
        x: 12; y: root.compact ? 33 : 54; width: parent.width - 24; height: root.compact ? 14 : 23
        text: root.detail
        color: Theme.secondary; font.pixelSize: root.compact ? 10 : 12
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        fontSizeMode: Text.Fit; minimumPixelSize: 9
    }
}
