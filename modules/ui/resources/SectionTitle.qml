import QtQuick
import LY27.Ui 1.0

// 各面板共用的区块标题：左侧强调短竖条与 HudPanel 顶部强调线呼应，字号位置一致，把各区缝成一个整体。
Item {
    id: root
    property alias text: label.text
    property int fontSize: 14
    implicitWidth: label.x + label.implicitWidth
    implicitHeight: 20
    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        width: 2; height: 12
        color: Theme.secondary
    }
    UiText {
        id: label
        x: 9; anchors.verticalCenter: parent.verticalCenter
        color: Theme.text; font.pixelSize: root.fontSize; font.bold: true
    }
}
