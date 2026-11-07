import QtQuick
import LY27.Ui 1.0

// 45° 切角容器：圆形代表单位/实体，切角代表界面/面板，与地图的军工几何同一种语言。
// 只用轮廓与底色，不带标题区、纹理和 eyebrow；需要完整通告框时继续用 HudPanel。
Item {
    id: root
    property real cut: 3
    property color fill: Theme.panel
    property color line: Theme.border
    property real fillOpacity: 1
    property real lineWidth: 1
    // 切角不得大于短边的 12%，否则小尺寸件会退化成三角形。
    readonly property real cornerCut: Math.min(root.cut, Math.min(width, height) * .12)
    default property alias content: holder.data

    Behavior on fill { ColorAnimation { duration: Theme.quickMotion } }
    Behavior on line { ColorAnimation { duration: Theme.quickMotion } }

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true
        onPaint: {
            const ctx = getContext("2d")
            const w = width, h = height
            const c = root.cornerCut
            ctx.clearRect(0, 0, w, h)
            if (w <= 0 || h <= 0) return
            // 描边居中落在路径上，整体内缩半个像素，避免外半圈被裁掉。
            const inset = root.lineWidth > 0 && root.line.a > 0 ? .5 : 0
            ctx.beginPath()
            ctx.moveTo(c, inset)
            ctx.lineTo(w - inset, inset)
            ctx.lineTo(w - inset, h - c)
            ctx.lineTo(w - c, h - inset)
            ctx.lineTo(inset, h - inset)
            ctx.lineTo(inset, c)
            ctx.closePath()
            ctx.fillStyle = Qt.rgba(root.fill.r, root.fill.g, root.fill.b, root.fill.a * root.fillOpacity)
            ctx.fill()
            if (inset > 0) {
                ctx.lineWidth = root.lineWidth
                ctx.strokeStyle = Qt.rgba(root.line.r, root.line.g, root.line.b, root.line.a)
                ctx.stroke()
            }
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Connections {
            target: root
            function onFillChanged() { canvas.requestPaint() }
            function onLineChanged() { canvas.requestPaint() }
            function onFillOpacityChanged() { canvas.requestPaint() }
            function onLineWidthChanged() { canvas.requestPaint() }
            function onCutChanged() { canvas.requestPaint() }
        }
    }
    Item {
        id: holder
        anchors.fill: parent
    }
}
