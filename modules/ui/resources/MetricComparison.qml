import QtQuick
import LY27.Ui 1.0

// 两侧使用相同数值刻度。底板由分析区统一提供，指标只组织文字与轨道。
Item {
    id: root
    property string metric: ""
    property string title: ""
    property real redValue: -1
    property real blueValue: -1
    property bool showNetRate: false
    property var netRate: null
    readonly property real maximum: Math.max(1, redValue, blueValue)
    readonly property bool shortLayout: height < 100

    function known(value) { return typeof value === "number" && isFinite(value) }
    function rateText(value) {
        if (!known(value)) return "— 点/s"
        return (value > 0 ? "+" : value < 0 ? "−" : "") + Math.abs(value) + " 点/s"
    }
    function rateColor(value) {
        return !known(value) || Math.abs(value) < 0.05 ? Theme.secondary : value > 0 ? Theme.positive : Theme.danger
    }
    Item { objectName: root.metric + "Card"; anchors.fill: parent }
    UiText {
        id: titleText
        objectName: root.metric + "Title"
        width: root.showNetRate ? Math.min(90, root.width * .36) : root.width
        height: 20
        text: root.title; color: Theme.secondary; font.pixelSize: 13
        verticalAlignment: Text.AlignVCenter
        fontSizeMode: Text.Fit; minimumPixelSize: 9
    }
    UiText {
        objectName: root.metric + "NetRate"
        visible: root.showNetRate
        x: titleText.width + 8; y: titleText.y
        width: Math.max(0, parent.width - x); height: titleText.height
        text: "我方净增速  " + root.rateText(root.netRate)
        color: root.rateColor(root.netRate); font.pixelSize: 12
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
            x: index * (root.width + 20) / 2
            y: root.shortLayout ? 20 : 23
            width: (root.width - 20) / 2
            height: root.height - y
            UiText {
                objectName: modelData.prefix + root.metric + "Label"
                x: 10; height: 16; width: parent.width - 10
                text: modelData.label; color: hasValue ? modelData.accent : Theme.unknown
                font.pixelSize: 11
            }
            Rectangle {
                y: 6; width: 4; height: 4; radius: 2
                color: hasValue ? modelData.accent : Theme.unknown
            }
            UiText {
                objectName: modelData.prefix + root.metric + "Value"
                y: 17; width: parent.width; height: parent.height - y - (root.shortLayout ? 12 : 22)
                text: hasValue ? String(modelData.value) : "—"
                color: hasValue ? Theme.text : Theme.unknown
                numeric: true; font.pixelSize: root.shortLayout ? 34 : 46; font.bold: true
                font.letterSpacing: -.5
                // 累计值保留全部数字，不用万、M 等缩写改变读数精度。
                fontSizeMode: Text.Fit; minimumPixelSize: 10
                verticalAlignment: Text.AlignVCenter
            }
            Rectangle {
                y: parent.height - 8; width: parent.width; height: 3
                color: hasValue ? Theme.divider : "transparent"
                Rectangle {
                    objectName: modelData.prefix + root.metric + "Bar"
                    width: parent.width * (hasValue ? modelData.value / root.maximum : 0)
                    height: parent.height; color: modelData.accent
                }
                // 未知使用分段轨道，真实零值保留完整的空轨道。
                Repeater {
                    model: hasValue ? 0 : 8
                    Rectangle {
                        required property int index
                        x: index * parent.width / 8; width: parent.width / 8 - 4; height: 1
                        color: Theme.border
                    }
                }
            }
        }
    }
}
