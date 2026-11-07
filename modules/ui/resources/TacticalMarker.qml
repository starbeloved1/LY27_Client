import QtQuick
import LY27.Ui 1.0

Item {
    id: root
    property int markerType: 1
    property string receiver: ""
    property real lift: 20
    property real mapWidth: 0
    property real mapHeight: 0
    property real visualScale: 1
    readonly property real localScale: Math.max(visualScale, 0.000001)
    width: 1; height: 1
    scale: visualScale
    transformOrigin: Item.TopLeft
    TechPanel {
        id: badge
        objectName: "tacticalMarkerBadge"
        width: 34; height: 34; cut: 6
        x: Math.max(2 - root.x / root.localScale, Math.min(-width / 2, (root.mapWidth - root.x) / root.localScale - width - 2))
        y: root.y / root.localScale >= root.lift + height / 2 + 2 ? -root.lift - height / 2 : 22
        fill: Theme.overlay
        line: Theme.tactical; lineWidth: 2
        TacticalSymbol { anchors.centerIn: parent; markerType: root.markerType }
        UiText {
            visible: root.receiver.length > 0
            anchors.left: parent.right; anchors.leftMargin: 3
            anchors.verticalCenter: parent.verticalCenter
            text: root.receiver
            color: Theme.tactical; font.pixelSize: 12; font.bold: true
            style: Text.Outline; styleColor: Theme.background
        }
    }
    Rectangle {
        width: 4; height: 4; radius: 2
        x: -2; y: -2; color: Theme.tactical
    }
}
