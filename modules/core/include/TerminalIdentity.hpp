#pragma once

#include <QString>
#include <optional>

namespace ly27 {

enum class Role { Heavy, Infantry, Commander };
enum class Team { Red, Blue };

// 身份定义
struct TerminalIdentity {
    Role role = Role::Infantry;
    Team team = Team::Red;
    QString stationId;
    std::optional<int> robotId; // 指挥手无机器人编号
};

QString roleName(Role role);
QString teamName(Team team);
std::optional<Role> parseRole(const QString &value);
std::optional<Team> parseTeam(const QString &value);
QString validateIdentity(const TerminalIdentity &identity);

} // 命名空间 ly27
