#pragma once

#include "RobotState.hpp"
#include <QVariantList>

namespace ly27 {

// 共用地图 Buff 适配；27 的数值是实际增益，不使用 26 的等级到百分比映射。
QVariantList mapBuffs(const QMap<BuffType, BuffState> &buffs);

}
