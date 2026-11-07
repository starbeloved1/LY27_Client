// 伤害分类与图标参考 RM26（复旦大学星云 EGA 战队，MIT），面板与模块卡片使用现有主题。
import QtQuick
import LY27.Ui 1.0

Item {
    id: root
    objectName: "injuryPanel"
    property var stats: ({})
    readonly property real panelScale: Math.min(1, (parent.width - 24) / 360, (parent.height - 24) / 704)
    width: 360 * panelScale
    height: 704 * panelScale
    TechPanel {
        objectName: "injuryPanelFrame"
        width: 360; height: 704
        scale: root.panelScale
        transformOrigin: Item.TopLeft
        cut: 8
        fill: Theme.overlay; line: Theme.border; sheen: Theme.sheen
        UiText {
            x: 18; y: 15; width: 324; height: 28
            text: "伤害面板"
            color: Theme.text; font.pixelSize: 21; font.bold: true
        }
        UiText {
            objectName: "injuryRobot"
            x: 18; y: 47; width: 324; height: 20
            text: (root.stats.robotText || "") + " · 本次存活受伤统计"
            color: Theme.secondary; font.pixelSize: 12
        }
        Rectangle { x: 18; y: 77; width: 324; height: 1; color: Theme.hairline }
        UiText {
            x: 18; y: 90; text: "累计受伤"
            color: Theme.secondary; font.pixelSize: 15
        }
        UiText {
            objectName: "injuryTotal"
            numeric: true
            x: 160; y: 83; width: 182; height: 31
            text: root.stats.totalText || "—"
            color: Theme.text; font.pixelSize: 25; font.bold: true
            horizontalAlignment: Text.AlignRight
        }
        Column {
            x: 18; y: 130; width: 324; spacing: 4
            Repeater {
                model: 8
                delegate: Item {
                    required property int index
                    readonly property var row: root.stats.rows ? root.stats.rows[index] : ({})
                    width: 324; height: 27
                    Image {
                        x: 0; y: 4; width: 19; height: 19
                        source: row.icon ? "images/injury/" + row.icon : ""
                        fillMode: Image.PreserveAspectFit
                    }
                    UiText {
                        x: 27; width: 129; height: parent.height
                        text: row.label || ""
                        color: Theme.secondary; font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                    }
                    UiText {
                        objectName: "injuryValue_" + (row.key || "")
                        numeric: true
                        x: 155; width: 94; height: parent.height
                        text: row.valueText || "—"
                        color: Theme.text; font.pixelSize: 13
                        horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignVCenter
                    }
                    UiText {
                        objectName: "injuryPercent_" + (row.key || "")
                        x: 259; width: 65; height: parent.height
                        text: row.percentText || "—"
                        color: Theme.secondary; font.pixelSize: 12
                        fontSizeMode: Text.Fit; minimumPixelSize: 8
                        horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
        UiText {
            objectName: "injuryKiller"
            x: 18; y: 391; width: 324; height: 20
            text: "击杀者：" + (root.stats.killerText || "未知")
            color: Theme.secondary; font.pixelSize: 12
        }
        Rectangle { x: 18; y: 423; width: 324; height: 1; color: Theme.hairline }
        SectionTitle { x: 18; y: 437; text: "模块状态"; fontSize: 17 }
        Grid {
            objectName: "injuryModuleGrid"
            x: 18; y: 470; width: 324; columns: 2; spacing: 8
            Repeater {
                model: root.stats.modules || []
                delegate: TechPanel {
                    required property var modelData
                    objectName: "injuryModule_" + modelData.key
                    readonly property int moduleState: modelData.state
                    readonly property color stateColor: moduleState === 1 ? Theme.positive
                        : moduleState === 0 ? Theme.danger : moduleState === 2 ? Theme.tactical : Theme.unknown
                    width: 158; height: 30; cut: 3
                    fill: Theme.inset
                    line: Qt.rgba(stateColor.r, stateColor.g, stateColor.b, .55)
                    UiText {
                        objectName: "injuryModuleLabel_" + modelData.key
                        x: 10; width: 86; height: parent.height
                        text: modelData.label; color: Theme.text; font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                    }
                    UiText {
                        objectName: "injuryModuleStatus_" + modelData.key
                        anchors.right: parent.right; anchors.rightMargin: 10
                        width: 48; height: parent.height
                        text: modelData.stateText; color: stateColor; font.pixelSize: 11
                        horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
        UiText {
            x: 18; y: 674; width: 324; height: 18
            text: "按住 ` / ~ 查看，松开收起"
            color: Theme.muted; font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
