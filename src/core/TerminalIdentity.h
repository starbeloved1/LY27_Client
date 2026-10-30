#pragma once

#include <QString>
#include <optional>

namespace ly27 {

enum class Role { Heavy, Infantry, Commander };
enum class Team { Red, Blue };

struct TerminalIdentity {
    Role role = Role::Infantry;
    Team team = Team::Red;
    QString stationId;
    // Commander is a station role, not a ground robot.
    std::optional<int> robotId;
};

QString roleName(Role role);
QString teamName(Team team);
std::optional<Role> parseRole(const QString &value);
std::optional<Team> parseTeam(const QString &value);
QString validateIdentity(const TerminalIdentity &identity);

} // namespace ly27
