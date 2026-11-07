#include "TerminalViewModel.hpp"

namespace ly27 {
namespace {
template<class T> QVariant reading(const StateField<T> &field)
{
    return field.isFresh() ? QVariant::fromValue(*field.value) : QVariant(-1);
}
}

void TerminalViewModel::updatePlayerHud()
{
    QVariantMap units;
    for (int id : {1, 3, 4, 7, 101, 103, 104, 107}) {
        const auto robot = m_store.robot(id);
        if (!robot) continue;
        units.insert(QString::number(id), QVariantMap{{"hp", reading(robot->currentHealth)},
            {"maxHp", reading(robot->maxHealth)}, {"level", reading(robot->level)}});
    }
    const auto robot = m_store.self();
    const RobotState r = robot.value_or(RobotState{});
    // 27 上报的是距升级还需获得的经验，不能把它直接当作经验条总量。
    const QVariant goal = r.currentExperience.isFresh() && r.experienceForUpgrade.isFresh()
        ? QVariant(double(*r.currentExperience.value) + *r.experienceForUpgrade.value) : QVariant(-1);
    const QVariantMap self{{"bound", bool(robot)}, {"robotId", m_identity.robotId.value_or(0)},
        {"blue", m_identity.team == Team::Blue}, {"role", roleName(m_identity.role)},
        {"currentHP", reading(r.currentHealth)}, {"maxHP", reading(r.maxHealth)}, {"level", reading(r.level)},
        {"currentHeat", reading(r.currentHeat)}, {"heatLimit", reading(r.maxHeat)},
        {"muzzleVelocity", reading(r.lastProjectileSpeed)}, {"remainingAmmo", reading(r.remainingAmmo)},
        {"bufferEnergy", reading(r.currentBufferEnergy)}, {"maxBufferEnergy", reading(r.maxBufferEnergy)},
        {"maxPower", reading(r.maxPower)}, {"currentExp", reading(r.currentExperience)}, {"maxExp", goal},
        {"outOfCombatKnown", r.outOfCombat.isFresh()},
        {"isOutOfCombat", r.outOfCombat.isFresh() && *r.outOfCombat.value}};
    const QVariantMap result{{"self", self}, {"units", units}};
    if (result != m_playerHud) { m_playerHud = result; emit playerHudChanged(); }
}

}
