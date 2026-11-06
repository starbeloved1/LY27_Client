import QtQuick

Rectangle {
    id: root
    property string metric: ""
    property string title: ""
    property real redValue: -1
    property real blueValue: -1
    property bool showNetRate: false
    property var netRate: null
    readonly property real maximum: Math.max(1, redValue, blueValue)
    color: "#16212a"
    radius: 4

    function known(value) { return typeof value === "number" && isFinite(value) }
    function rateText(value) {
        if (!known(value)) return "— 点/s"
        return (value > 0 ? "+" : value < 0 ? "−" : "") + Math.abs(value) + " 点/s"
    }
    function rateColor(value) {
        return !known(value) || Math.abs(value) < 0.05 ? "#9baebc" : value > 0 ? "#60D8B7" : "#FF737A"
    }
    Text {
        id: titleText
        objectName: root.metric + "Title"
        x: 10; y: 2; width: root.showNetRate ? Math.min(110, (parent.width - 32) * 0.42) : parent.width - 20; height: 24
        text: root.title; color: "#e4edf4"; font.pixelSize: 16; font.bold: true
        fontSizeMode: Text.Fit; minimumPixelSize: 12
        verticalAlignment: Text.AlignVCenter
    }
    Text {
        objectName: root.metric + "NetRate"
        visible: root.showNetRate
        x: titleText.x + titleText.width + 12; y: titleText.y
        width: Math.max(0, parent.width - x - 10); height: titleText.height
        text: "我方净增速  " + root.rateText(root.netRate)
        color: root.rateColor(root.netRate); font.pixelSize: 14
        fontSizeMode: Text.Fit; minimumPixelSize: 10
        horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignVCenter
    }
    Repeater {
        model: [
            {prefix: "red", label: "红方", value: root.redValue, accent: "#FF737A"},
            {prefix: "blue", label: "蓝方", value: root.blueValue, accent: "#70bfff"}
        ]
        delegate: Item {
            required property int index
            required property var modelData
            readonly property real textHeight: root.height >= 112 ? 28 : 26
            x: 10; y: 28 + index * (root.height - 32) / 2
            width: root.width - 20; height: (root.height - 32) / 2
            Text {
                objectName: modelData.prefix + root.metric + "Label"
                width: 34; height: parent.textHeight
                text: modelData.label; color: modelData.accent; font.pixelSize: 14
                verticalAlignment: Text.AlignVCenter
            }
            Text {
                objectName: modelData.prefix + root.metric + "Value"
                x: 38; width: parent.width - 38; height: parent.textHeight
                text: modelData.value >= 0 ? String(modelData.value) : "—"
                color: "#eef3f7"; font.pixelSize: root.height >= 112 ? 24 : 22; font.bold: true
                fontSizeMode: Text.Fit; minimumPixelSize: 8
                verticalAlignment: Text.AlignVCenter
            }
            Rectangle {
                y: parent.textHeight + 2; width: parent.width; height: 8; radius: 2; color: "#293944"
                Rectangle {
                    objectName: modelData.prefix + root.metric + "Bar"
                    width: parent.width * (modelData.value >= 0 ? modelData.value / root.maximum : 0)
                    height: parent.height; radius: 2; color: modelData.accent
                }
            }
        }
    }
}
