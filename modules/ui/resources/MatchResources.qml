import QtQuick

Rectangle {
    id: root
    objectName: "matchResources"
    property var match: ({})
    height: 56
    color: "#10171d"
    readonly property var entries: [
        {name: "redControlPoints", label: "红方累计控制度", value: match.redControlPoints ?? -1, accent: "#ff8787"},
        {name: "allyEconomy", label: "我方实时经济 · " + (match.allyTeamLabel || "—"), value: match.allyEconomy ?? -1, accent: "#f3d28a"},
        {name: "blueControlPoints", label: "蓝方累计控制度", value: match.blueControlPoints ?? -1, accent: "#70bfff"}
    ]
    Rectangle { width: parent.width; height: 1; color: "#293743" }
    Repeater {
        model: root.entries
        delegate: Item {
            required property int index
            required property var modelData
            x: index * width
            width: root.width / 3
            height: root.height
            Rectangle {
                visible: parent.index > 0
                y: 11; width: 1; height: parent.height - 22
                color: "#2b3843"
            }
            Text {
                objectName: modelData.name + "Label"
                x: 8; y: 6; width: parent.width - 16; height: 16
                text: modelData.label
                color: "#a6b8c6"; font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            Text {
                objectName: modelData.name + "Value"
                x: 8; y: 23; width: parent.width - 16; height: 27
                text: modelData.value >= 0 ? String(modelData.value) : "—"
                color: modelData.value >= 0 ? modelData.accent : "#758695"
                font.pixelSize: 21; font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}
