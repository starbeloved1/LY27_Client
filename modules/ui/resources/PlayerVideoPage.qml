import QtQuick
import LY27.Ui 1.0

// 独立的只读选手视图。视频仍由同一后端解码，不创建第二套 MQTT/UDP 或上行按键。
Rectangle {
    id: root
    property var viewModel: null
    property var session: null
    readonly property var player: viewModel ? viewModel.playerHud.self : ({})
    color: "#000000"
    clip: true
    MouseArea { anchors.fill: parent }
    VideoSurface {
        objectName: "videoSurface"
        anchors.fill: parent
        frame: root.session ? root.session.videoFrame : null
    }
    UiText {
        objectName: "videoStatus"
        anchors.centerIn: parent
        visible: !root.session || !root.session.videoAvailable
        text: root.session ? root.session.videoStatus : "图传未启动"
        color: Theme.secondary; font.pixelSize: Theme.heading
        z: 2
    }
    Item {
        id: hud
        objectName: "playerHudCanvas"
        width: 1920; height: 1080
        anchors.centerIn: parent
        scale: Math.min(root.width / width, root.height / height)
        PlayerTopBar { viewModel: root.viewModel }
        PlayerAimingHud {
            objectName: "playerAimingHud"
            anchors.centerIn: parent
            robot: root.player
            visible: !!root.player.bound
        }
        PlayerRobotStatus {
            objectName: "playerRobotStatus"
            // RM26 左下角面板以 1530×910 为尺寸基准，保留其在 HUD 内的占比。
            transformOrigin: Item.TopLeft
            scale: Math.min(parent.width / 1530, parent.height / 910)
            x: 18; y: parent.height - height * scale - 12
            robot: root.player
            buffs: root.viewModel ? root.viewModel.selfBuffs : []
            modules: root.viewModel ? root.viewModel.injury.modules || [] : []
            visible: !!root.player.bound
        }
        UiText {
            x: 24; y: parent.height - 48
            visible: !root.player.bound
            text: "指挥手 · 未绑定机器人"
            color: Theme.secondary
        }
        Rectangle {
            objectName: "playerMiniMapFrame"
            width: 370; height: width * 946 / 1662
            anchors.right: parent.right; anchors.rightMargin: 16
            anchors.bottom: parent.bottom; anchors.bottomMargin: 16
            color: Theme.background; border.color: Theme.border
            BattlefieldMap {
                objectName: "playerMiniMap"
                anchors.fill: parent; anchors.margins: 1
                viewModel: root.viewModel
                interactive: false
                minimumRobotScale: .38
                facilityScale: .4
            }
        }
        MatchNotices {
            x: 18; y: 170; width: parent.width - 36
            controller: root.viewModel ? root.viewModel.notices : null
            mechanisms: root.viewModel ? root.viewModel.mechanisms : []
            animationsAllowed: root.visible && (root.viewModel ? root.viewModel.match.noticesLive ?? false : false)
            epoch: root.viewModel ? root.viewModel.match.noticeEpoch ?? "" : ""
        }
        BattleAnnouncement {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 166; width: 600; height: 50
            notice: root.viewModel ? root.viewModel.battleNotice : ({})
            z: 5
        }
    }
}
