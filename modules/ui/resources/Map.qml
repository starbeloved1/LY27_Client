import QtQuick
import LY27.Ui 1.0

Rectangle {
    id: root
    width: 1280
    height: 800
    color: Theme.background
    property var viewModel: terminal
    readonly property bool blueView: viewModel ? viewModel.blueView : false
    readonly property var tactics: viewModel ? viewModel.tactics : null
    readonly property real mapAspect: 1662 / 946
    readonly property bool videoVisible: desktopWindow.videoVisible
    // 地图栏本身按底图比例定尺寸，横向余量归事件栏，纵向余量归分析板。
    readonly property real preferredEventWidth: Math.round(Math.min(250, Math.max(170, width * 0.155)))
    readonly property real minimumAnalysisHeight: width < 760 ? 226 : 164
    readonly property real fittedMapWidth: Math.max(0, Math.min(width - preferredEventWidth - Theme.gutter,
        Math.max(0, height - header.height - minimumAnalysisHeight) * mapAspect))
    readonly property real fittedMapHeight: fittedMapWidth / mapAspect
    MatchHeader {
        id: header
        visible: !root.videoVisible
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        match: root.viewModel ? root.viewModel.match : ({})
    }
    BattlefieldMap {
        id: mapArea
        viewModel: root.viewModel
        visible: !root.videoVisible
        objectName: "mapArea"
        anchors.top: header.bottom
        anchors.right: parent.right
        width: root.fittedMapWidth
        height: root.fittedMapHeight
        clip: true
        TechPanel {
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottomMargin: 6
            width: Math.min(parent.width - 12, hint.implicitWidth + 24)
            height: 26; cut: 3; z: 21
            visible: !root.videoVisible && (root.tactics ? root.tactics.commander || root.tactics.feedback.length > 0 : false)
            fill: Theme.overlay
            line: Theme.divider
            UiText {
                id: hint
                anchors.fill: parent; anchors.margins: 4
                text: root.tactics ? root.tactics.feedback || "指挥手：选择己方车 → 点击目标；右键 / Esc 取消" : ""
                color: Theme.tactical; font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight
            }
        }
        TechPanel {
            id: selfBuffPanel
            objectName: "selfBuffPanel"
            readonly property bool compact: mapArea.height < 180
            readonly property var buffs: root.viewModel ? root.viewModel.selfBuffs : []
            visible: buffs.length > 0
            x: compact ? parent.width - width - 8 : 8
            y: compact ? 6 : parent.height - height - 8
            width: compact ? selfBuffs.width + 12 : Math.max(134, selfBuffs.width + 20)
            height: compact ? 24 : 48
            cut: 3; z: 20
            fill: Theme.overlay; line: Theme.border
            UiText {
                x: 10; y: 5
                visible: !selfBuffPanel.compact
                text: "本机增益"; color: Theme.secondary; font.pixelSize: 11
            }
            BuffRow {
                id: selfBuffs
                objectName: "selfBuffRow"
                x: selfBuffPanel.compact ? 6 : 10
                y: selfBuffPanel.compact ? 5 : 25
                buffs: selfBuffPanel.buffs
                tooltipAbove: !selfBuffPanel.compact
            }
        }
        MatchNotices {
            id: matchNotices
            anchors.top: parent.top
            anchors.topMargin: parent.height < 180 ? (selfBuffPanel.visible ? 34 : 6) : 12
            anchors.left: parent.left
            anchors.leftMargin: 12
            width: parent.width - 24
            controller: root.viewModel ? root.viewModel.notices : null
            mechanisms: root.viewModel ? root.viewModel.mechanisms : []
            animationsAllowed: root.viewModel ? (root.viewModel.match.noticesLive ?? false) : false
            epoch: root.viewModel ? (root.viewModel.match.noticeEpoch ?? "") : ""
            z: 12
        }
        BattleAnnouncement {
            anchors.horizontalCenter: parent.horizontalCenter
            // 击杀/前哨摧毁播报保持固定位置，覆盖通告框，不随框的数量向下挪动。
            y: Math.max(4, Math.min(24, parent.height - height - 4))
            width: Math.min(600, parent.width - 24)
            height: 50
            notice: root.viewModel ? root.viewModel.battleNotice : ({})
            z: 20
        }
    }
    EventPanel {
        id: events
        visible: !root.videoVisible
        anchors.top: header.bottom
        anchors.bottom: analysis.top
        anchors.left: parent.left
        width: root.width - mapArea.width - Theme.gutter
        eventModel: root.viewModel ? root.viewModel.events : null
    }
    BattleAnalysisPanel {
        id: analysis
        visible: !root.videoVisible
        // 不侵占地图；底图受窗口宽度限制时，剩余高度由分析板吸收。
        height: Math.max(root.minimumAnalysisHeight, root.height - header.height - root.fittedMapHeight)
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        match: root.viewModel ? root.viewModel.match : ({})
    }
    Item {
        anchors.fill: parent
        // 两个页面共享同一阶段弹层，切换 P 不得重新播放已关闭的胜利界面。
        z: 180
        MatchPhaseOverlay {
            anchors.fill: parent
            phase: root.viewModel ? root.viewModel.phase : ({})
            match: root.viewModel ? root.viewModel.match : ({})
        }
    }
    PlayerVideoPage {
        objectName: "videoPage"
        anchors.fill: parent
        visible: root.videoVisible
        viewModel: root.viewModel
        session: sessionSettings
        z: 150
    }
    DamagePanel {
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        visible: desktopWindow.injuryPanelVisible
        stats: root.viewModel ? root.viewModel.injury : ({})
        z: 200
    }
    TerminalSettings {
        anchors.fill: parent
        session: sessionSettings
        z: 300
    }
}
