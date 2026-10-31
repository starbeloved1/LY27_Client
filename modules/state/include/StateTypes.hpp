#pragma once

#include <QList>
#include <QString>
#include <QtGlobal>
#include <optional>

// 所有状态公用的包装模式
namespace ly27 {

enum class StateValidity { Unknown, Fresh, Stale, Disconnected, Retained, Observed };
QString stateValidityName(StateValidity validity);

// 定义字段的来源
struct StateSource {
    StateValidity validity = StateValidity::Unknown;
    QString reason = "not_received";
    QString topic;
    QString sessionId;
    bool received = false;
    qint64 receivedUtcMs = 0;
    qint64 receivedMonotonicNs = 0;
    quint64 connection = 0;
    bool retained = false;
    std::optional<int> robotId;
    std::optional<quint32> round;
    quint64 roundRevision = 0;
    std::optional<qint64> ageMs;
    std::optional<qint64> ttlMs;
};

// value 无值表示没有收到该字段
template<typename T>
struct StateField {
    std::optional<T> value;
    StateSource source;

    bool isFresh() const { return value.has_value() && source.validity == StateValidity::Fresh; } // 判断value是否新鲜
};

// 派生值定义：保留所有输入来源，任意一个输入失效时不再声称派生值新鲜
template<typename T>
struct DerivedState {
    std::optional<T> value;
    QList<StateSource> inputs;

    bool isFresh() const
    {
        if (!value || inputs.isEmpty()) return false;
        for (const auto &input : inputs)
            if (input.validity != StateValidity::Fresh) return false;
        return true;
    }
};

} // 命名空间 ly27
