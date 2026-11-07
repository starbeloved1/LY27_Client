import QtQuick
import LY27.Ui 1.0

Item {
    id: root
    property var buffs: []
    property bool tooltipAbove: false
    width: row.implicitWidth
    height: 14
    visible: buffs.length > 0
    Row {
        id: row
        spacing: 2
        Repeater {
            // 固定七种位置，剩余时间变化不销毁图标或重新发起图像加载。
            model: 7
            delegate: Rectangle {
                required property int index
                readonly property var buff: root.buffs.find(function(value) { return value.type === index + 1 }) ?? ({})
                objectName: "mapBuff" + (index + 1)
                visible: buff.icon !== undefined
                width: 14; height: 14; radius: 2
                color: Theme.overlay
                border.color: Theme.border
                Image {
                    objectName: "mapBuffImage"
                    anchors.fill: parent; anchors.margins: 1
                    source: parent.buff.icon ?? ""
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                }
                HoverHandler { id: hover }
                Rectangle {
                    visible: hover.hovered && parent.visible
                    width: label.implicitWidth + 12; height: 22
                    y: root.tooltipAbove ? -25 : 17; z: 30; color: Theme.overlay; radius: 3
                    UiText {
                        id: label
                        anchors.centerIn: parent
                        text: buff.description ?? ""
                        color: Theme.text; font.pixelSize: 11
                    }
                }
            }
        }
    }
}
