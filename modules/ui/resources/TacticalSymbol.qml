import QtQuick
import LY27.Ui 1.0

Canvas {
    id: root
    property int markerType: 1
    width: 26; height: 26
    onMarkerTypeChanged: requestPaint()
    onPaint: {
        const ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)
        ctx.save()
        ctx.scale(width / 26, height / 26)
        ctx.fillStyle = Theme.tactical
        ctx.strokeStyle = Theme.tactical
        ctx.lineWidth = 2.5
        ctx.lineJoin = "round"
        ctx.lineCap = "round"
        if (markerType === 1) {
            // 剑尖、剑刃、护手和握柄。
            ctx.beginPath()
            ctx.moveTo(21, 3); ctx.lineTo(20, 10); ctx.lineTo(11, 19)
            ctx.lineTo(7, 15); ctx.lineTo(16, 6); ctx.closePath(); ctx.fill()
            ctx.beginPath(); ctx.moveTo(5, 13); ctx.lineTo(13, 21)
            ctx.moveTo(8, 18); ctx.lineTo(4, 22); ctx.stroke()
        } else if (markerType === 2) {
            ctx.beginPath()
            ctx.moveTo(13, 3); ctx.lineTo(22, 7); ctx.lineTo(20, 16)
            ctx.quadraticCurveTo(18, 21, 13, 24)
            ctx.quadraticCurveTo(8, 21, 6, 16)
            ctx.lineTo(4, 7); ctx.closePath(); ctx.stroke()
            ctx.beginPath(); ctx.moveTo(13, 8); ctx.lineTo(13, 18)
            ctx.moveTo(9, 12); ctx.lineTo(17, 12); ctx.stroke()
        } else {
            ctx.beginPath(); ctx.moveTo(13, 5); ctx.lineTo(13, 15); ctx.stroke()
            ctx.beginPath(); ctx.arc(13, 21, 1.8, 0, Math.PI * 2); ctx.fill()
        }
        ctx.restore()
    }
}
