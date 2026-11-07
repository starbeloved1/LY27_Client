// 参考 RM26 DamagePanel（复旦大学星云 EGA 战队，MIT）：暗色浮层、金色标题、伤害图标与数值占比行。
import QtQuick
import LY27.Ui 1.0

Item {
    id: root
    objectName: "injuryPanel"
    property var stats: ({})
    width: Math.min(360, parent.width - 24)
    height: Math.min(452, parent.height - 24)
    TechPanel {
        width: 360; height: 452
        scale: Math.min(root.width / width, root.height / height)
        transformOrigin: Item.TopLeft
        cut: 8
        fill: "#ed14141f"; line: "#665b37"
        UiText {
            x: 18; y: 15; width: 324; height: 28
            text: "伤害面板"
            color: Theme.text; font.pixelSize: 21; font.bold: true
        }
        UiText {
            objectName: "injuryRobot"
            x: 18; y: 47; width: 324; height: 20
            text: (root.stats.robotText || "") + " · 本次存活受伤统计"
            color: "#b9c5cf"; font.pixelSize: 12
        }
        Rectangle { x: 18; y: 77; width: 324; height: 1; color: "#43414a" }
        UiText {
            x: 18; y: 90; text: "累计受伤"
            color: "#dddddd"; font.pixelSize: 15
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
                        color: "#c4c4ca"; font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                    }
                    UiText {
                        objectName: "injuryValue_" + (row.key || "")
                        numeric: true
                        x: 155; width: 94; height: parent.height
                        text: row.valueText || "—"
                        color: "#eeeeee"; font.pixelSize: 13
                        horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignVCenter
                    }
                    UiText {
                        objectName: "injuryPercent_" + (row.key || "")
                        x: 259; width: 65; height: parent.height
                        text: row.percentText || "—"
                        color: "#a6a6af"; font.pixelSize: 12
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
            color: "#b9c5cf"; font.pixelSize: 12
        }
        UiText {
            x: 18; y: 424; width: 324; height: 18
            text: "按住 ` / ~ 查看，松开收起"
            color: "#8d8d99"; font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
