import QtQuick
import LY27.Ui 1.0

Item {
    id: root
    objectName: "matchResources"
    property var match: ({})
    readonly property bool compact: width < 760
    readonly property real logisticsWidth: compact ? width - 24 : Math.max(280, width * .32)
    readonly property real chartWidth: compact ? (width - 32) / 2 : (width - logisticsWidth - 40) / 2
    readonly property real chartHeight: compact ? height - 80 : height
    MetricComparison {
        x: 12; width: root.chartWidth; height: root.chartHeight
        metric: "ControlPoints"; title: "累计控制度"; showNetRate: true
        redValue: root.match.redControlPoints ?? -1; blueValue: root.match.blueControlPoints ?? -1
        netRate: root.match.controlNetRate ?? null
    }
    MetricComparison {
        x: 20 + root.chartWidth; width: root.chartWidth; height: root.chartHeight
        metric: "Damage"; title: "累计总伤害"
        redValue: root.match.redDamage ?? -1; blueValue: root.match.blueDamage ?? -1
    }
    TechPanel {
        id: logistics
        x: root.compact ? 12 : root.width - width - 12
        y: root.compact ? root.height - height : 0
        width: root.logisticsWidth; height: root.compact ? 72 : root.height
        cut: 4
        fill: Theme.raised
        line: "transparent"
        sheen: Theme.sheen
        readonly property real economyWidth: root.compact ? width * .62 : width * .66
        Rectangle { x: logistics.economyWidth; y: 12; width: 1; height: parent.height - 24; color: Theme.hairline }
        Item {
            width: logistics.economyWidth; height: parent.height
            Repeater {
                model: [
                    {name: "allyEconomy", label: "我方当前经济", value: root.match.allyEconomy ?? -1},
                    {name: "allyTotalEconomy", label: "我方累计经济", value: root.match.allyTotalEconomy ?? -1}
                ]
                delegate: Item {
                    required property int index
                    required property var modelData
                    readonly property bool hasValue: modelData.value >= 0
                    x: index * parent.width / 2; width: parent.width / 2; height: parent.height
                    UiText {
                        objectName: modelData.name + "Label"
                        x: 12; y: 8; width: parent.width - 20; height: 20
                        text: modelData.label; color: Theme.secondary; font.pixelSize: 13
                        fontSizeMode: Text.Fit; minimumPixelSize: 9
                    }
                    UiText {
                        objectName: modelData.name + "Value"
                        x: 12; y: root.compact ? 30 : 36; width: parent.width - 20; height: root.compact ? 34 : parent.height - 46
                        text: hasValue ? String(modelData.value) : "—"
                        color: hasValue ? Theme.text : Theme.unknown
                        numeric: true; font.pixelSize: root.compact ? 30 : 40
                        verticalAlignment: Text.AlignVCenter
                        font.bold: true; font.letterSpacing: -.6
                        fontSizeMode: Text.Fit; minimumPixelSize: 10
                    }
                }
            }
        }
        Item {
            x: logistics.economyWidth + 8; width: parent.width - x - 8; height: parent.height
            Repeater {
                // 科技和加密仍在同一组，数值与标签同行，避免挤占经济的读数空间。
                model: [
                    {name: "allyTechLevel", label: "科技等级", value: root.match.allyTechLevel ?? -1},
                    {name: "allyEncryptionLevel", label: "加密等级", value: root.match.allyEncryptionLevel ?? -1}
                ]
                delegate: Item {
                    required property int index
                    required property var modelData
                    x: 0; y: root.compact ? 4 + index * 32 : 8 + index * (parent.height - 16) / 2
                    width: parent.width; height: root.compact ? 30 : (parent.height - 16) / 2
                    UiText {
                        objectName: modelData.name + "Label"
                        width: parent.width - 38; height: parent.height
                        text: modelData.label; color: Theme.secondary; font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                        fontSizeMode: Text.Fit; minimumPixelSize: 9
                    }
                    UiText {
                        objectName: modelData.name + "Value"
                        x: parent.width - 34; width: 32; height: parent.height
                        text: modelData.value >= 0 ? String(modelData.value) : "—"
                        color: modelData.value >= 0 ? Theme.text : Theme.unknown
                        numeric: true; font.pixelSize: 22; font.bold: true
                        horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }
}
