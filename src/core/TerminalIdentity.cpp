#include "TerminalIdentity.h"

namespace ly27 {

QString roleName(Role role)
{
    switch (role) {
    case Role::Heavy: return QStringLiteral("heavy");
    case Role::Infantry: return QStringLiteral("infantry");
    case Role::Commander: return QStringLiteral("commander");
    }
    return {};
}

QString teamName(Team team)
{
    return team == Team::Red ? QStringLiteral("red") : QStringLiteral("blue");
}

std::optional<Role> parseRole(const QString &value)
{
    if (value == QStringLiteral("heavy")) return Role::Heavy;
    if (value == QStringLiteral("infantry")) return Role::Infantry;
    if (value == QStringLiteral("commander")) return Role::Commander;
    return std::nullopt;
}

std::optional<Team> parseTeam(const QString &value)
{
    if (value == QStringLiteral("red")) return Team::Red;
    if (value == QStringLiteral("blue")) return Team::Blue;
    return std::nullopt;
}

QString validateIdentity(const TerminalIdentity &identity)
{
    if (identity.stationId.trimmed().isEmpty()) {
        return QStringLiteral("identity.station_id must not be empty");
    }
    if (identity.role == Role::Commander) {
        return identity.robotId.has_value()
            ? QStringLiteral("identity.robot_id must be null for commander") : QString();
    }
    if (!identity.robotId) {
        return QStringLiteral("identity.robot_id is required for a ground robot");
    }
    const int offset = identity.team == Team::Blue ? 100 : 0;
    const int id = *identity.robotId - offset;
    if (identity.role == Role::Heavy && id != 1) {
        return QStringLiteral("identity.robot_id must be 1 (red) or 101 (blue) for heavy");
    }
    if (identity.role == Role::Infantry && id != 3 && id != 4) {
        return QStringLiteral("identity.robot_id must be 3/4 (red) or 103/104 (blue) for infantry");
    }
    return {};
}

} // namespace ly27
