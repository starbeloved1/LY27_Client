#include "BuffPresentation.hpp"

namespace ly27 {

QVariantList mapBuffs(const QMap<BuffType, BuffState> &buffs)
{
    QVariantList result;
    for (auto it = buffs.cbegin(); it != buffs.cend(); ++it) {
        const auto &buff = it.value();
        if (!buff.robotId.isFresh() || !buff.type.isFresh() || !buff.level.isFresh() ||
            !buff.reportedRemainingSeconds.isFresh() || !*buff.reportedRemainingSeconds.value || !*buff.level.value) continue;
        const int type = int(it.key());
        const qint64 level = *buff.level.value;
        if (level < 0 && it.key() != BuffType::Defense) continue;
        QString icon, name;
        switch (it.key()) {
        case BuffType::Attack: icon = "attack"; name = QStringLiteral("攻击增益"); break;
        case BuffType::Defense:
            icon = level < 0 ? "vulnerability" : "defense";
            name = level < 0 ? QStringLiteral("易伤") : QStringLiteral("防御增益"); break;
        case BuffType::Cooling: icon = "cooling"; name = QStringLiteral("射击热量冷却增益"); break;
        case BuffType::ChassisPower: icon = "power"; name = QStringLiteral("底盘功率增益"); break;
        case BuffType::Recovery: icon = "recovery"; name = QStringLiteral("回血增益"); break;
        case BuffType::RemoteAmmo: icon = "ammo"; name = QStringLiteral("可兑换允许发弹量"); break;
        case BuffType::TerrainWarning: icon = "terrain"; name = QStringLiteral("地形跨越预备"); break;
        }
        if (icon.isEmpty()) continue;
        const auto age = buff.reportedRemainingSeconds.source.ageMs.value_or(0);
        const auto seconds = qMax<qint64>(0, qint64(*buff.reportedRemainingSeconds.value) - age / 1000);
        const auto unit = type == 1 || type == 2 || type == 4 || type == 5 ? QStringLiteral("%")
            : type == 3 ? QStringLiteral("/s") : QString{};
        result.append(QVariantMap{{"type", type}, {"level", level}, {"remainingSeconds", seconds},
            {"icon", "image://buff/" + icon},
            {"description", QStringLiteral("%1：%2%3 · %4秒").arg(name).arg(qAbs(level)).arg(unit).arg(seconds)}});
    }
    return result;
}

}
