import QtQuick
import LY27.Ui 1.0

FocusScope {
    id: root
    objectName: "terminalSettings"
    property var session: null
    readonly property bool opened: desktopWindow.settingsVisible
    focus: opened
    onOpenedChanged: if (opened) forceActiveFocus()
    Keys.onEscapePressed: desktopWindow.settingsVisible = false

    Rectangle {
        anchors.fill: parent
        visible: root.opened
        color: "#66000000"
        MouseArea { anchors.fill: parent; onClicked: desktopWindow.settingsVisible = false }
    }
    TechPanel {
        id: panel
        objectName: "terminalSettingsPanel"
        visible: root.opened
        width: Math.min(340, root.width - 24)
        height: content.height + 32
        anchors.right: parent.right; anchors.rightMargin: 12
        y: 46
        fill: Theme.panel; line: Theme.border; cut: 6
        MouseArea { anchors.fill: parent }
        Column {
            id: content
            x: 16; y: 16; width: parent.width - 32; spacing: 12
            Item {
                width: parent.width; height: 24
                UiText { text: "终端设置"; font.pixelSize: Theme.heading; font.bold: true }
                UiText {
                    anchors.right: parent.right
                    text: "×"; font.pixelSize: 22; color: closeMouse.containsMouse ? Theme.text : Theme.secondary
                    MouseArea {
                        id: closeMouse
                        anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: desktopWindow.settingsVisible = false
                    }
                }
            }
            UiText { text: "操控角色"; color: Theme.secondary; font.pixelSize: Theme.body }
            Grid {
                width: parent.width; columns: 2; spacing: 8
                Repeater {
                    model: [
                        { key: "heavy", label: "重装", name: "roleHeavy" },
                        { key: "commander", label: "指挥手", name: "roleCommander" },
                        { key: "infantry3", label: "3 号步兵", name: "roleInfantry3" },
                        { key: "infantry4", label: "4 号步兵", name: "roleInfantry4" }
                    ]
                    delegate: TechPanel {
                        required property var modelData
                        objectName: modelData.name
                        readonly property bool selected: root.session ? root.session.seat === modelData.key : false
                        width: (parent.width - 8) / 2; height: 36
                        fill: selected || roleMouse.containsMouse ? Theme.raised : Theme.inset
                        line: selected ? Theme.tactical : Theme.border
                        UiText { anchors.centerIn: parent; text: modelData.label; color: selected ? Theme.tactical : Theme.text }
                        MouseArea {
                            id: roleMouse
                            anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            enabled: root.session ? !root.session.busy : false
                            onClicked: root.session.selectIdentity(modelData.key, root.session.team)
                        }
                    }
                }
            }
            UiText { text: "阵营"; color: Theme.secondary; font.pixelSize: Theme.body }
            Row {
                width: parent.width; spacing: 8
                Repeater {
                    model: [{ key: "red", label: "红方", color: Theme.teamRed, name: "teamRed" },
                            { key: "blue", label: "蓝方", color: Theme.teamBlue, name: "teamBlue" }]
                    delegate: TechPanel {
                        required property var modelData
                        objectName: modelData.name
                        readonly property bool selected: root.session ? root.session.team === modelData.key : false
                        width: (parent.width - 8) / 2; height: 36
                        fill: selected || teamMouse.containsMouse ? Theme.raised : Theme.inset
                        line: selected ? modelData.color : Theme.border
                        UiText { anchors.centerIn: parent; text: modelData.label; color: modelData.color; font.bold: selected }
                        MouseArea {
                            id: teamMouse
                            anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            enabled: root.session ? !root.session.busy : false
                            onClicked: root.session.selectIdentity(root.session.seat, modelData.key)
                        }
                    }
                }
            }
            TechPanel {
                objectName: "fullScreenOption"
                width: parent.width; height: 36
                fill: screenMouse.containsMouse ? Theme.raised : Theme.inset
                line: desktopWindow.fullScreen ? Theme.tactical : Theme.border
                UiText { x: 12; anchors.verticalCenter: parent.verticalCenter; text: "全屏模式" }
                Rectangle {
                    width: 17; height: 17
                    anchors.right: parent.right; anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    color: desktopWindow.fullScreen ? Theme.tactical : "transparent"
                    border.color: desktopWindow.fullScreen ? Theme.tactical : Theme.secondary
                    UiText { anchors.centerIn: parent; text: "✓"; color: Theme.ink; visible: desktopWindow.fullScreen }
                }
                MouseArea {
                    id: screenMouse
                    anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: desktopWindow.fullScreen = !desktopWindow.fullScreen
                }
            }
            UiText {
                width: parent.width
                visible: text.length > 0
                text: root.session ? root.session.error : ""
                color: Theme.danger; wrapMode: Text.Wrap; font.pixelSize: Theme.caption
            }
        }
    }
    TechPanel {
        objectName: "settingsButton"
        visible: !desktopWindow.videoVisible || root.opened
        width: 28; height: 28
        anchors.right: parent.right; anchors.rightMargin: 10
        y: 8
        fill: gearMouse.containsMouse || root.opened ? Theme.raised : Theme.inset
        line: gearMouse.containsMouse || root.opened ? Theme.secondary : Theme.border
        Canvas {
            anchors.centerIn: parent; width: 18; height: 18
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.beginPath()
                for (let index = 0; index < 32; ++index) {
                    const angle = index * Math.PI / 16
                    const radius = index % 4 === 1 || index % 4 === 2 ? 8 : 6
                    const x = 9 + Math.cos(angle) * radius, y = 9 + Math.sin(angle) * radius
                    if (index === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
                }
                ctx.closePath(); ctx.strokeStyle = Theme.text; ctx.lineWidth = 1.2; ctx.stroke()
                ctx.beginPath(); ctx.arc(9, 9, 2.4, 0, Math.PI * 2); ctx.stroke()
            }
        }
        MouseArea {
            id: gearMouse
            anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
            enabled: !!root.session
            onClicked: desktopWindow.settingsVisible = !root.opened
        }
    }
}
