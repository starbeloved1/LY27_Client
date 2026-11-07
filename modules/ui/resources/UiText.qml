import QtQuick
import LY27.Ui 1.0

// 中文标签使用统一字体；数值组件设 numeric: true 改为 Theme.numberFont。
// 数值字体自动启用等宽等高数字（tnum/lnum），计时器与比分跳变时宽度不抖动。
Text {
    property bool numeric: false
    font.family: numeric ? Theme.numberFont : Theme.textFont
    font.pixelSize: Theme.body
    font.features: numeric ? { "tnum": 1, "lnum": 1 } : {}
    color: Theme.text
}
