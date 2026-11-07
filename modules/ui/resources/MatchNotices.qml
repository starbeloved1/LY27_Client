import QtQuick

Item {
    id: root
    objectName: "matchNotices"
    property var controller: null
    property var mechanisms: []
    property bool animationsAllowed: true
    property string epoch: ""
    readonly property var timed: controller ? controller.timed : []
    readonly property var results: controller ? controller.results : []
    readonly property bool compact: parent.height < 180
    height: cards.height
    visible: cards.layoutKeys.length > 0
    MechanismNotices {
        id: cards
        width: parent.width
        notices: root.mechanisms
        timed: root.timed
        results: root.results
        compact: root.compact
        animationsAllowed: root.animationsAllowed
        epoch: root.epoch
        maximumHeight: Math.max(0, Math.min(root.parent.height - root.y - 4,
            root.compact ? root.parent.height : root.parent.height * .45))
    }
}
