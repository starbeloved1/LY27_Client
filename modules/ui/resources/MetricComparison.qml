import QtQuick
import LY27.Ui 1.0

Rectangle {
    id: root
    property string metric: ""
    property string title: ""
    property real redValue: -1
    property real blueValue: -1
    property bool showNetRate: false
    property var netRate: null
    readonly property real maximum: Math.max(1, redValue, blueValue)
    color: Theme.raised
    radius: 4

    function known(value) { return typeof value === "number" && isFinite(value) }
    function rateText(value) {
        if (!known(value)) return "— 点/s"
        return (value > 0 ? "+" : value < 0 ? "−" : "") + Math.abs(value) + " 点/s"
    }
    function rateColor(value) {
        return !known(value) || Math.abs(value) < 0.05 ? Theme.secondary : value > 0 ? Theme.positive : Theme.danger
    }
    UiText {
        id: titleText
        objectName: root.metric + "Title"
        x: 12; y: 6
        width: root.showNetRate ? Math.min(100, root.width * .40) : root.width - 24
        height: 20
        text: root.title; color: Theme.secondary; font.pixelSize: Theme.body
        font.bold: true
        verticalAlignment: Text.AlignVCenter
        fontSizeMode: Text.Fit; minimumPixelSize: 11
    }
    UiText {
        objectName: root.metric + "NetRate"
        visible: root.showNetRate
        x: titleText.x + titleText.width + 8; y: titleText.y
        width: Math.max(0, parent.width - x - 12); height: titleText.height
        text: "我方净增速  " + root.rateText(root.netRate)
        color: root.rateColor(root.netRate); font.pixelSize: Theme.body
        fontSizeMode: Text.Fit; minimumPixelSize: 9
        horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignVCenter
    }
    Repeater {
        model: [
            {prefix: "red", label: "红方", value: root.redValue, accent: Theme.teamRed},
            {prefix: "blue", label: "蓝方", value: root.blueValue, accent: Theme.teamBlue}
        ]
        delegate: Item {
            required property int index
            required property var modelData
            readonly property bool hasValue: modelData.value >= 0
            x: 12 + index * (root.width - 16) / 2
            y: 30
            width: (root.width - 32) / 2
            height: root.height - y - 8
            UiText {
                objectName: modelData.prefix + root.metric + "Label"
                x: 10; height: 16; width: parent.width - 10
                text: modelData.label; color: hasValue ? modelData.accent : Theme.unknown
                font.pixelSize: Theme.caption
            }
            Rectangle { x: 0; y: 5; width: 4; height: 4; radius: 1; color: hasValue ? modelData.accent : Theme.unknown }
            UiText {
                objectName: modelData.prefix + root.metric + "Value"
                y: 16; width: parent.width; height: Math.max(25, parent.height - 24)
                text: hasValue ? String(modelData.value) : "—"
                color: hasValue ? Theme.text : Theme.unknown
                font.family: Theme.numberFont; font.pixelSize: Theme.number; font.bold: true
                // 极大累计值仍显示完整数字，不把协议数值格式化为不精确的缩写。
                fontSizeMode: Text.Fit; minimumPixelSize: 10
                verticalAlignment: Text.AlignVCenter
            }
            Rectangle {
                y: parent.height - 3; width: parent.width; height: 3; radius: 1
                color: hasValue ? Theme.divider : "transparent"
                border.width: hasValue ? 0 : 1; border.color: Theme.border
                Rectangle {
                    objectName: modelData.prefix + root.metric + "Bar"
                    width: parent.width * (hasValue ? modelData.value / root.maximum : 0)
                    height: parent.height; radius: 1; color: modelData.accent
                }
                // 未收到数据时保留细分段标记，与真实零值的完整空轨道区分。
                Repeater {
                    model: hasValue ? 0 : 6
                    Rectangle { required property int index; x: index * parent.width / 6; width: 3; height: parent.height; color: Theme.unknown }
                }
            }
        }
    }
}
