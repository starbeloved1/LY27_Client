import QtQuick

Item {
    id: root
    objectName: "matchResources"
    property var match: ({})
    readonly property bool compact: width < 760
    readonly property real logisticsWidth: compact ? width - 24 : Math.min(250, Math.max(175, width * 0.23))
    readonly property real chartWidth: compact ? (width - 36) / 2 : (width - logisticsWidth - 48) / 2
    readonly property real chartHeight: compact ? height - 46 : height
    MetricComparison {
        x: 12; width: root.chartWidth; height: root.chartHeight
        metric: "ControlPoints"; title: "累计控制度"; showNetRate: true
        redValue: root.match.redControlPoints ?? -1; blueValue: root.match.blueControlPoints ?? -1
        netRate: root.match.controlNetRate ?? null
    }
    MetricComparison {
        x: 24 + root.chartWidth; width: root.chartWidth; height: root.chartHeight
        metric: "Damage"; title: "累计总伤害"
        redValue: root.match.redDamage ?? -1; blueValue: root.match.blueDamage ?? -1
    }
    Rectangle {
        id: logistics
        x: root.compact ? 12 : root.width - width - 12
        y: root.compact ? root.height - height : 0
        width: root.logisticsWidth; height: root.compact ? 40 : root.height
        color: "#16212a"; radius: 4
        Item {
            width: root.compact ? parent.width * 0.6 : parent.width
            height: root.compact ? parent.height : parent.height * 0.5
            Repeater {
                model: [
                    {name: "allyEconomy", label: "我方当前经济", value: root.match.allyEconomy ?? -1},
                    {name: "allyTotalEconomy", label: "我方累计经济", value: root.match.allyTotalEconomy ?? -1}
                ]
                delegate: Item {
                    required property int index
                    required property var modelData
                    x: index * parent.width / 2; width: parent.width / 2; height: parent.height
                    Text {
                        objectName: modelData.name + "Label"
                        x: 6; y: 4; width: parent.width - 12; height: 15
                        text: modelData.label
                        color: "#a6b8c6"; font.pixelSize: 10
                        fontSizeMode: Text.Fit; minimumPixelSize: 8
                    }
                    Text {
                        objectName: modelData.name + "Value"
                        x: 6; y: 20; width: parent.width - 12; height: root.compact ? 20 : 27
                        text: modelData.value >= 0 ? String(modelData.value) : "—"
                        color: "#f3d28a"; font.pixelSize: root.compact ? 18 : 24; font.bold: true
                        fontSizeMode: Text.Fit; minimumPixelSize: 8
                    }
                }
            }
        }
        Item {
            x: root.compact ? parent.width * 0.6 : 0
            y: root.compact ? 0 : parent.height * 0.53
            width: root.compact ? parent.width * 0.4 : parent.width
            height: root.compact ? parent.height : parent.height * 0.47
            // 科技与加密属于同一组，使用相邻双格展示。
            Repeater {
                model: [
                    {name: "allyTechLevel", label: "科技等级", value: root.match.allyTechLevel ?? -1},
                    {name: "allyEncryptionLevel", label: "加密等级", value: root.match.allyEncryptionLevel ?? -1}
                ]
                delegate: Item {
                    required property int index
                    required property var modelData
                    x: index * parent.width / 2; width: parent.width / 2; height: parent.height
                    Text {
                        objectName: modelData.name + "Label"
                        x: 6; y: 4; width: parent.width - 12; height: 15
                        text: modelData.label; color: "#a6b8c6"; font.pixelSize: 10
                        horizontalAlignment: Text.AlignHCenter
                        fontSizeMode: Text.Fit; minimumPixelSize: 8
                    }
                    Text {
                        objectName: modelData.name + "Value"
                        x: 6; y: 20; width: parent.width - 12; height: 20
                        text: modelData.value >= 0 ? String(modelData.value) : "—"
                        color: "#f3d28a"; font.pixelSize: 18; font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }
    }
}
