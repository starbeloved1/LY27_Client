import QtQuick
import LY27.Ui 1.0

// 战况仪表带：战力对比、后勤读数共用基线，窗口变窄时后勤整组换到第二行。
Item {
    id: root
    objectName: "matchResources"
    property var match: ({})
    readonly property bool compact: width < 760
    readonly property real sidePadding: width < 1000 ? 16 : 24
    readonly property real sectionGap: compact ? 24 : 40
    readonly property real innerWidth: width - sidePadding * 2
    readonly property real logisticsWidth: compact ? innerWidth : innerWidth * .40
    readonly property real chartWidth: compact ? (innerWidth - sectionGap) / 2 : (innerWidth - logisticsWidth - sectionGap * 2) / 2
    readonly property real chartHeight: compact ? (height - 20) / 2 : height

    MetricComparison {
        x: root.sidePadding; width: root.chartWidth; height: root.chartHeight
        metric: "ControlPoints"; title: "累计控制度"; showNetRate: true
        redValue: root.match.redControlPoints ?? -1; blueValue: root.match.blueControlPoints ?? -1
        netRate: root.match.controlNetRate ?? null
    }
    MetricComparison {
        x: root.sidePadding + root.chartWidth + root.sectionGap
        width: root.chartWidth; height: root.chartHeight
        metric: "Damage"; title: "累计总伤害"
        redValue: root.match.redDamage ?? -1; blueValue: root.match.blueDamage ?? -1
    }
    Rectangle {
        x: root.sidePadding + root.chartWidth + root.sectionGap / 2
        y: 4; width: 1; height: root.chartHeight - 8; color: Theme.hairline
    }
    Rectangle {
        x: root.compact ? root.sidePadding : logistics.x - root.sectionGap / 2
        y: root.compact ? root.chartHeight + 10 : 4
        width: root.compact ? root.innerWidth : 1
        height: root.compact ? 1 : root.height - 8
        color: Theme.hairline
    }
    Item {
        id: logistics
        x: root.compact ? root.sidePadding : root.width - width - root.sidePadding
        y: root.compact ? root.chartHeight + 20 : 0
        width: root.logisticsWidth; height: root.chartHeight
        readonly property real economyWidth: width * .55
        readonly property real valueY: height < 100 ? 22 : 40

        Item {
            id: economy
            width: logistics.economyWidth - 20; height: parent.height
            UiText {
                objectName: "allyEconomyLabel"
                width: parent.width; height: 20
                text: "我方当前经济"; color: Theme.secondary; font.pixelSize: 13
            }
            UiText {
                objectName: "allyEconomyValue"
                y: logistics.valueY; width: parent.width; height: parent.height - y - 22
                text: root.match.allyEconomy >= 0 ? String(root.match.allyEconomy) : "—"
                color: root.match.allyEconomy >= 0 ? Theme.text : Theme.unknown
                numeric: true; font.pixelSize: root.compact ? 36 : 46; font.bold: true
                font.letterSpacing: -.5; verticalAlignment: Text.AlignVCenter
                fontSizeMode: Text.Fit; minimumPixelSize: 10
            }
            UiText {
                id: totalLabel
                objectName: "allyTotalEconomyLabel"
                y: parent.height - 18; width: 52; height: 18
                text: "累计经济"; color: Theme.muted; font.pixelSize: 11
                verticalAlignment: Text.AlignVCenter
            }
            UiText {
                objectName: "allyTotalEconomyValue"
                x: totalLabel.width + 8; y: totalLabel.y
                width: parent.width - x; height: totalLabel.height
                text: root.match.allyTotalEconomy >= 0 ? String(root.match.allyTotalEconomy) : "—"
                color: root.match.allyTotalEconomy >= 0 ? Theme.secondary : Theme.unknown
                numeric: true; font.pixelSize: 16
                verticalAlignment: Text.AlignVCenter
                fontSizeMode: Text.Fit; minimumPixelSize: 8
            }
        }
        Rectangle {
            x: logistics.economyWidth; y: 4; width: 1; height: parent.height - 8
            color: Theme.hairline
        }
        Item {
            id: levels
            x: logistics.economyWidth + 20; width: parent.width - x; height: parent.height
            Repeater {
                model: [
                    {name: "allyTechLevel", label: "科技等级", value: root.match.allyTechLevel ?? -1},
                    {name: "allyEncryptionLevel", label: "加密等级", value: root.match.allyEncryptionLevel ?? -1}
                ]
                delegate: Item {
                    required property int index
                    required property var modelData
                    x: index * (levels.width + 12) / 2
                    width: (levels.width - 12) / 2; height: levels.height
                    UiText {
                        objectName: modelData.name + "Label"
                        width: parent.width; height: 20
                        text: modelData.label; color: Theme.secondary; font.pixelSize: 13
                        fontSizeMode: Text.Fit; minimumPixelSize: 9
                    }
                    UiText {
                        objectName: modelData.name + "Value"
                        y: logistics.valueY; width: parent.width; height: parent.height - y - 22
                        text: modelData.value >= 0 ? String(modelData.value) : "—"
                        color: modelData.value >= 0 ? Theme.text : Theme.unknown
                        numeric: true; font.pixelSize: root.compact ? 32 : 40
                        verticalAlignment: Text.AlignVCenter
                        fontSizeMode: Text.Fit; minimumPixelSize: 10
                    }
                    // 中性底线用于收口，不把未知的等级上限画成进度条。
                    Rectangle {
                        y: parent.height - 8; width: Math.min(40, parent.width); height: 2
                        color: Theme.border
                    }
                }
            }
        }
    }
}
