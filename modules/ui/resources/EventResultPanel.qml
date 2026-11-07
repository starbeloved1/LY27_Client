import QtQuick
import LY27.Ui 1.0

// 保留左上分类标签；正文仅显示结果及有效统计，不附加角标或确认说明。
Item {
    id: root
    property var result: ({})
    property bool compact: false
    readonly property color accent: result.accent || Theme.secondary
    HudPanel { anchors.fill: parent; accent: root.accent; panelOpacity: .88; showDiagonalTexture: false }
    UiText {
        x: 12; y: 2; width: parent.width - 24; height: root.compact ? 9 : 15
        text: root.result.heading || ""
        color: Theme.secondary; font.pixelSize: root.compact ? 8 : 10
    }
    Item {
        anchors.fill: parent
        Column {
            objectName: "noticeBody"
            anchors.centerIn: parent
            anchors.alignWhenCentered: false
            width: parent.width - 24
            spacing: root.compact ? 2 : 3
            UiText {
                objectName: "resultMessage"
                width: parent.width; height: root.compact ? 14 : 26
                text: root.result.message || ""
                color: root.accent; font.pixelSize: root.compact ? 14 : 20; font.bold: true
                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                fontSizeMode: Text.Fit; minimumPixelSize: root.compact ? 10 : 12
            }
            UiText {
                objectName: "resultDetail"
                visible: text.length > 0
                width: parent.width; height: root.compact ? 11 : 18
                text: root.result.detail || ""
                color: Theme.text; font.pixelSize: root.compact ? 10 : 13
                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                fontSizeMode: Text.Fit; minimumPixelSize: 9
            }
        }
    }
}
