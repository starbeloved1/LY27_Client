import QtQuick
import LY27.Ui 1.0

// 地图标记选择：与地图同一种切角几何，选项格同样不用圆角。
Item {
    id: root
    property var controller
    readonly property var target: controller ? controller.pending : ({})
    readonly property var options: target.options ?? []
    objectName: "mapMarkerMenu"
    visible: options.length > 0
    width: Math.min(parent ? parent.width - 4 : 220, 16 + options.length * 68)
    height: 76
    TechPanel {
        anchors.fill: parent
        cut: 4
        fill: Theme.overlay
        line: Theme.tactical
    }
    UiText {
        x: 10; y: 6
        text: "选择标记"
        color: Theme.tactical; font.pixelSize: 12
    }
    Row {
        x: 8; y: 25; spacing: 4
        Repeater {
            model: root.options
            delegate: Item {
                required property int modelData
                objectName: "mapMarkerOption" + modelData
                width: (root.width - 16 - Math.max(0, root.options.length - 1) * 4) / Math.max(1, root.options.length)
                height: 43
                TechPanel {
                    anchors.fill: parent
                    cut: 3
                    fill: pointer.containsMouse ? "#35434d" : "#202d37"
                    line: pointer.containsMouse ? Theme.tactical : Theme.border
                }
                TacticalSymbol { x: parent.width < 56 ? (parent.width - width) / 2 : 3; y: 8; markerType: modelData; width: 23; height: 23 }
                UiText {
                    visible: parent.width >= 56
                    x: 29; anchors.verticalCenter: parent.verticalCenter
                    text: modelData === 1 ? "攻击" : modelData === 2 ? "防御" : "警戒"
                    color: Theme.tactical; font.pixelSize: 12
                }
                MouseArea {
                    id: pointer
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: function(mouse) {
                        if (mouse.button === Qt.RightButton) root.controller.cancel()
                        else root.controller.chooseMarker(modelData)
                    }
                }
            }
        }
    }
}
