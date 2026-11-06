import QtQuick

Item {
    id: root
    objectName: "matchPhaseOverlay"
    property var phase: ({})
    property var match: ({})
    property string dismissedToken: ""
    readonly property string kind: phase.kind || "none"
    readonly property string token: phase.token || ""
    readonly property string art: phase.art || ""
    property int frame: frameCount - 1
    readonly property int frameCount: 34
    visible: kind !== "none" && !(kind === "result" && dismissedToken === token)
    onTokenChanged: frame = phase.animateVictory ? 0 : frameCount - 1

    Rectangle { anchors.fill: parent; color: root.kind === "result" ? "#c2080c10" : "#70000000" }
    MouseArea { anchors.fill: parent }

    Timer {
        interval: 40
        repeat: true
        running: root.visible && root.kind === "result" && root.phase.animateVictory
            && root.art !== "" && root.frame < root.frameCount - 1
        onTriggered: ++root.frame
    }
    Image {
        objectName: "matchCountdownImage"
        visible: root.kind === "countdown" || root.kind === "start"
        anchors.centerIn: parent
        width: Math.min(root.kind === "start" || root.phase.remaining === 0 ? 509 : 288, parent.width * 0.70)
        height: Math.min(root.kind === "start" || root.phase.remaining === 0 ? 112 : 310, parent.height * 0.65)
        source: visible ? (root.kind === "start" || root.phase.remaining === 0
            ? "images/gamephase/freeze_start.png" : "images/gamephase/freeze_" + root.phase.remaining + ".png") : ""
        fillMode: Image.PreserveAspectFit
        smooth: true
    }
    Item {
        visible: root.kind === "preparation" || root.kind === "pause" || root.kind === "settlement"
        anchors.centerIn: parent
        width: Math.min(650, root.width - 32)
        height: Math.min(250, root.height - 32)
        Item {
            width: 650; height: 250
            anchors.centerIn: parent
            scale: Math.min(parent.width / width, parent.height / height)
            Image {
                anchors.fill: parent
                source: root.kind === "pause" ? "images/gamephase/gamestatus_yellow.png"
                    : root.phase.urgent ? "images/gamephase/gamestatus_red.png" : "images/gamephase/gamestatus_cyan.png"
                fillMode: Image.Stretch
            }
            Text {
                objectName: "matchPhaseTimer"
                x: 24; y: 40; width: parent.width - 48; height: 45
                text: root.kind === "settlement" ? "—" : (root.phase.timer || "--:--")
                color: root.kind === "pause" || root.phase.urgent ? "#ffda7e" : "#63efff"
                font.pixelSize: 32; font.bold: true
                horizontalAlignment: Text.AlignHCenter
            }
            Text {
                objectName: "matchPhaseTitle"
                x: 24; y: 102; width: parent.width - 48; height: 35
                text: root.phase.title || ""
                color: "#ffffff"; font.pixelSize: 23; font.bold: true
                horizontalAlignment: Text.AlignHCenter
            }
            Text {
                x: 32; y: 155; width: parent.width - 64; height: 50
                text: root.phase.description || ""
                color: "#c2d5e1"; font.pixelSize: 16
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }
        }
    }
    Item {
        visible: root.kind === "result"
        anchors.fill: parent
        Image {
            objectName: "matchVictoryImage"
            anchors.top: parent.top
            anchors.bottom: summary.top
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(1200, parent.width)
            source: root.art !== "" && root.kind === "result" ? "images/resultpanel/" + root.art + "/"
                + String(root.frame).padStart(2, "0") + ".png" : ""
            fillMode: Image.PreserveAspectFit
            cache: false
            smooth: true
        }
        Text {
            objectName: "matchResultTitle"
            visible: root.art === ""
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -70
            text: root.phase.title || ""
            color: "#e8eef3"; font.pixelSize: Math.min(64, root.width * 0.08); font.bold: true
        }
        Column {
            id: summary
            width: Math.min(780, parent.width - 32)
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Math.max(24, parent.height * 0.06)
            spacing: 10
            Text {
                objectName: "matchResultScore"
                width: parent.width
                text: "第 " + (root.match.currentRound > 0 ? root.match.currentRound : "—") + " 局　"
                    + (root.match.redTeam || "红方") + "　" + (root.match.redScore >= 0 ? root.match.redScore : "—")
                    + " : " + (root.match.blueScore >= 0 ? root.match.blueScore : "—") + "　" + (root.match.blueTeam || "蓝方")
                color: "#f3f6fa"; font.pixelSize: 22; font.bold: true
                horizontalAlignment: Text.AlignHCenter
                fontSizeMode: Text.Fit; minimumPixelSize: 12
            }
            Text {
                objectName: "matchResultReason"
                width: parent.width
                text: root.phase.description || ""
                color: "#a9bdcb"; font.pixelSize: 16
                horizontalAlignment: Text.AlignHCenter
            }
            Rectangle {
                objectName: "dismissMatchResult"
                width: 150; height: 34
                anchors.horizontalCenter: parent.horizontalCenter
                color: resultMouse.containsMouse ? "#304655" : "#1c2b36"
                border.color: "#526979"; radius: 3
                Text { anchors.centerIn: parent; text: "收起战报"; color: "#e4edf3"; font.pixelSize: 13 }
                MouseArea {
                    id: resultMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.dismissedToken = root.token
                }
            }
        }
    }
}
