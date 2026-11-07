import QtQuick
import QtQuick.Effects
import LY27.Ui 1.0

// 一个稳定的展示槽位：保留离场快照，动画结束后再释放位置。
Item {
    id: root
    property string slotKey: ""
    property var snapshot: ({})
    property var pending: ({})
    property bool presented: false
    property bool leaving: false
    property bool motionAllowed: true
    property bool compact: false
    property real lift: -20
    property real blurAmount: .65
    property real targetX: 0
    property real targetY: 0
    readonly property bool motionRunning: appearance.running || horizontal.running || vertical.running
    signal released()
    objectName: slotKey === "rune" || slotKey === "dart" ? slotKey + "ResultNotice"
        : slotKey.startsWith("mechanism") ? "mechanismNotice" + slotKey.slice(9) : snapshot.objectName || ""
    visible: presented
    opacity: 0
    height: compact ? 50 : 84
    transform: Translate { y: root.lift }

    function place(nextX, nextY) {
        if (!presented || !motionAllowed) {
            horizontal.stop(); vertical.stop()
            targetX = nextX; targetY = nextY; x = nextX; y = nextY
            return
        }
        if (targetX !== nextX) {
            targetX = nextX; horizontal.stop(); horizontal.to = nextX; horizontal.start()
        }
        if (targetY !== nextY) {
            targetY = nextY; vertical.stop(); vertical.to = nextY; vertical.start()
        }
    }
    function reset() {
        appearance.stop(); horizontal.stop(); vertical.stop()
        presented = false; leaving = false
        snapshot = ({}); pending = ({})
        opacity = 0; lift = -20; blurAmount = .65
    }
    function enter(data) {
        snapshot = data; pending = data; leaving = false
        presented = true
        appearance.stop()
        fade.to = 1; slide.to = 0; soften.to = 0
        appearance.start()
    }
    function exit() {
        if (!presented || leaving) return
        leaving = true
        appearance.stop()
        fade.to = 0; slide.to = -20; soften.to = .65
        appearance.start()
    }
    function synchronize(data) {
        pending = data
        if (!data.identity) {
            if (motionAllowed) exit()
            else reset()
        } else if (!presented) {
            enter(data)
        } else if (snapshot.identity !== data.identity) {
            // 新事件替换旧事件时，先送走旧框，再让新框入场。
            exit()
        } else {
            snapshot = data
            if (leaving) enter(data)
        }
    }
    NumberAnimation { id: horizontal; target: root; property: "x"; duration: 380; easing.type: Easing.InOutCubic }
    NumberAnimation { id: vertical; target: root; property: "y"; duration: 380; easing.type: Easing.InOutCubic }
    ParallelAnimation {
        id: appearance
        NumberAnimation { id: fade; target: root; property: "opacity"; duration: 360; easing.type: root.leaving ? Easing.InOutCubic : Easing.OutCubic }
        NumberAnimation { id: slide; target: root; property: "lift"; duration: 420; easing.type: root.leaving ? Easing.InOutCubic : Easing.OutCubic }
        NumberAnimation { id: soften; target: root; property: "blurAmount"; duration: 420; easing.type: root.leaving ? Easing.InOutCubic : Easing.OutCubic }
        onFinished: {
            if (!root.leaving) return
            if (root.pending.identity) root.enter(root.pending)
            else {
                root.reset()
                root.released()
            }
        }
    }
    Item {
        objectName: "animatedNoticeArtwork"
        anchors.fill: parent
        BattleNoticeCard {
            anchors.fill: parent
            visible: !root.snapshot.kind
            heading: root.snapshot.heading || ""
            message: root.snapshot.message || ""
            detail: root.snapshot.detail || ""
            messageObjectName: root.snapshot.messageObjectName || ""
            accent: root.snapshot.accent || Theme.secondary
            compact: root.compact
        }
        EventResultPanel {
            anchors.fill: parent
            visible: !!root.snapshot.kind
            result: root.snapshot
            compact: root.compact
        }
        // 仅出入场时启用模糊；软件渲染仍保留完整的位移与淡入淡出。
        layer.enabled: appearance.running && GraphicsInfo.api !== GraphicsInfo.Software
            && GraphicsInfo.api !== GraphicsInfo.Unknown
        layer.effect: MultiEffect {
            blurEnabled: true
            blurMax: 12
            blur: root.blurAmount
            autoPaddingEnabled: true
        }
    }
}
