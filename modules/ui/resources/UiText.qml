import QtQuick
import LY27.Ui 1.0

// 中文标签使用统一字体，数值组件显式改用 Theme.numberFont。
Text {
    font.family: Theme.textFont
    font.pixelSize: Theme.body
    color: Theme.text
}
