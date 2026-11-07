import QtQuick
import LY27.Ui 1.0

Flickable {
    id: root
    objectName: "mechanismNotices"
    property var notices: []
    property var timed: []
    property var results: []
    property bool compact: false
    property real maximumHeight: parent.height
    property bool animationsAllowed: true
    property string epoch: ""
    property var slots: []
    property var layoutKeys: []
    property bool ready: false
    readonly property var timedIds: [1, 31, 91, 181, 241, 256, 331, 421]
    readonly property int columns: Math.max(1, Math.floor((width + 8) / (compact ? 208 : 288)))
    readonly property real cardWidth: Math.min(compact ? 240 : 340, (width - (columns - 1) * 8) / columns)
    readonly property real cardHeight: compact ? 50 : 84
    readonly property int visibleRows: Math.max(0, Math.floor((maximumHeight + 8) / (cardHeight + 8)))
    readonly property bool motionRunning: slots.some(function(slot) { return slot.motionRunning })
    contentWidth: width
    contentHeight: layoutKeys.length ? Math.ceil(layoutKeys.length / columns) * (cardHeight + 8) - 8 : 0
    height: Math.min(contentHeight, visibleRows > 0 ? visibleRows * (cardHeight + 8) - 8 : Math.max(0, maximumHeight))
    visible: layoutKeys.length > 0
    clip: contentHeight > height
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick

    function schedule() { if (ready) Qt.callLater(reconcile) }
    function reset() {
        if (!ready) return
        slots.forEach(function(slot) { slot.reset() })
        layoutKeys = []; contentY = 0
        schedule()
    }
    function desired() {
        if (!animationsAllowed) return []
        const result = []
        notices.forEach(function(notice) {
            result.push({ key: "mechanism" + notice.id, identity: "mechanism" + notice.id,
                objectName: "mechanismNotice" + notice.id, heading: notice.target,
                accent: notice.accent,
                message: notice.actor + "还有 " + notice.seconds + " 秒", detail: "激活" + notice.target,
                messageObjectName: "mechanismMessage" })
        })
        timed.forEach(function(notice, index) {
            result.push({ key: "timed" + notice.id, identity: "timed" + notice.id,
                objectName: index === 0 ? "timedMatchNotice" : "timedMatchNotice" + index,
                heading: "战术提示",
                message: notice.text, accent: Theme.secondary })
        })
        results.forEach(function(notice) {
            result.push({ key: notice.kind, identity: notice.kind + notice.token,
                objectName: notice.kind + "ResultNotice", kind: notice.kind,
                heading: notice.kind === "rune" ? "能量机关激活" : "飞镖命中",
                message: notice.text, detail: notice.detail, accent: notice.accent })
        })
        return result
    }
    function reconcile() {
        if (!ready) return
        const wanted = desired()
        const keys = wanted.map(function(data) { return data.key })
        // 离场期间保留原位置，完整淡出后再让右侧卡片平移补位。
        layoutKeys.forEach(function(key, index) {
            const slot = slots.find(function(value) { return value.slotKey === key })
            if (keys.indexOf(key) < 0 && slot && slot.presented)
                keys.splice(Math.min(index, keys.length), 0, key)
        })
        layoutKeys = keys
        slots.forEach(function(slot) {
            const index = keys.indexOf(slot.slotKey)
            if (index >= 0) slot.place((index % columns) * (cardWidth + 8), Math.floor(index / columns) * (cardHeight + 8))
            slot.synchronize(wanted.find(function(data) { return data.key === slot.slotKey }) || ({}))
        })
    }
    onNoticesChanged: schedule()
    onTimedChanged: schedule()
    onResultsChanged: schedule()
    onCardWidthChanged: schedule()
    onCardHeightChanged: schedule()
    onColumnsChanged: schedule()
    onEpochChanged: reset()
    onAnimationsAllowedChanged: { if (!animationsAllowed) reset(); else schedule() }
    Component.onCompleted: {
        const values = []
        for (let index = 0; index < mechanisms.count; ++index) values.push(mechanisms.itemAt(index))
        for (let index = 0; index < timeNodes.count; ++index) values.push(timeNodes.itemAt(index))
        for (let index = 0; index < resultCards.count; ++index) values.push(resultCards.itemAt(index))
        slots = values; ready = true; reconcile()
    }
    Rectangle {
        parent: root
        x: root.width - 2; y: root.visibleArea.yPosition * root.height
        width: 2; height: Math.max(12, root.visibleArea.heightRatio * root.height)
        visible: root.contentHeight > root.height && root.height > 0
        color: Theme.secondary; opacity: .5; z: 1
    }
    Repeater {
        id: mechanisms
        model: 6
        delegate: AnimatedNotice {
            required property int index
            slotKey: "mechanism" + (index + 1)
            width: root.cardWidth
            compact: root.compact
            motionAllowed: root.animationsAllowed
            onReleased: root.schedule()
        }
    }
    Repeater {
        id: timeNodes
        model: 8
        delegate: AnimatedNotice {
            required property int index
            slotKey: "timed" + root.timedIds[index]
            width: root.cardWidth
            compact: root.compact
            motionAllowed: root.animationsAllowed
            onReleased: root.schedule()
        }
    }
    Repeater {
        id: resultCards
        model: ["rune", "dart"]
        delegate: AnimatedNotice {
            required property string modelData
            slotKey: modelData
            width: root.cardWidth
            compact: root.compact
            motionAllowed: root.animationsAllowed
            onReleased: root.schedule()
        }
    }
}
