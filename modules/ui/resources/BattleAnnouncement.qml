import QtQuick
import LY27.Ui 1.0

Item {
    id: root
    objectName: "battleAnnouncement"
    property var notice: ({})
    readonly property var token: notice.token ?? 0
    visible: token !== 0
    opacity: 0
    onTokenChanged: {
        animation.stop()
        opacity = 0
        if (token !== 0) animation.start()
    }
    BattleAnnouncementItem {
        objectName: "battleAnnouncementArtwork"
        anchors.fill: parent
        notice: root.notice
    }
    SequentialAnimation {
        id: animation
        NumberAnimation { target: root; property: "opacity"; to: 1; duration: 100 }
        PauseAnimation { duration: 2900 }
        NumberAnimation { target: root; property: "opacity"; to: 0; duration: 500 }
    }
}
