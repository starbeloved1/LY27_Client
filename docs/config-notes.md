# 配置重载与身份切换

## 使用入口

配置仍由 `modules/config/TerminalConfig.hpp/.cpp` 解析和校验。`TerminalRuntime` 新增统一入口，CLI 和未来桌面设置界面共用：

```cpp
// 读取当前配置副本，修改后应用；运行对象及其信号连接保持不变。
auto next = runtime.config();
next.identity.role = ly27::Role::Infantry;
next.identity.team = ly27::Team::Blue;
next.identity.robotId = 104;
next.identity.stationId = "blue-infantry-4";
next.network.mqttClientId = "实际核实的服务器身份";
const auto result = runtime.applyConfig(next);
if (!result.applied) {
    // result.error 是本次失败原因，rollbackError 是恢复旧配置失败的原因。
}

// 从当前来源文件重新读取；也可以传入另一份配置的路径。
const auto reloaded = runtime.reloadConfig();

// 只修改身份时也要求显式传入 MQTT 身份，不自动从机器人编号推导。
const auto switched = runtime.switchIdentity(identity, mqttClientId);

// 保存当前已应用的配置；与应用操作分开，保存失败不会撤销已生效的配置。
QString error;
runtime.saveCurrentConfig("config/terminal.json", error);
```

`config()` 返回只读引用，编辑前应复制。调用入口必须在后端所属 Qt 线程执行。新建配置时需要有效的来源路径、日志目录和完整网络配置；从 `loadConfig()` 或现有 `config()` 复制即可。

`configurationChanged()` 表示本地配置已应用，消费者重新读取身份、状态及图传；`configurationChangeFailed(error, rollbackError)` 报告失败。配置切换内部重启不会额外发出顶层 `started/stopped`，使用配置通知判断切换结果。

## 切换语义

1. 校验完整候选配置，包括角色、阵营、机器人编号的一致性。启用 MQTT 时不能将 MQTT 身份改为空。
2. 无效配置直接拒绝，不停止连接、不清空状态、不改变当前会话。
3. 配置完全相同时按无变更处理，不重启；仅 JSON 排版变化也不重启。
4. 有效变更先停止指令，再断开 MQTT 和图传；未确认指令记录为执行情况未知，写入旧会话记录，不缓存、不重发。
5. 更新状态仓库和指令层的身份，清理旧状态、通知、限频记录和视频帧，建立新会话。
6. 若原来正在运行，使用原来的 MQTT/视频启用组合重新启动；若原来已停止，应用后仍保持停止。
7. 新连接等待新数据；旧回调受 MQTT 代次保护，不得更新新会话。

`TerminalRuntime`、`StateStore`、`CommandDispatcher`、`VideoReceiver` 对象地址保持稳定，已有信号连接可继续使用。身份只由运行层在模块停止期间更新，不开放零散身份 setter。

`ConfigApplyResult`：

- `applied`：请求的新配置已应用。
- `restarted`：成功应用时原后端正在运行，因此执行了重启。
- `rolledBack`：新配置失败后已恢复旧配置，且旧后端同步初始化成功（或原来就处于停止状态）。
- `error`：本次应用失败原因。
- `rollbackError`：恢复旧后端也失败时的原因，此时后端停止，保留旧配置供修复。

恢复旧配置也使用新会话，不恢复旧数据有效性，不重新发送任何业务指令。启动成功只表示资源同步初始化和连接请求已提交，不保证 MQTT 已连通、身份获准或已收到视频。后续异步连接拒绝通过原 MQTT 状态报告，不自动切回其他机器人身份。

## 日志与保存

应用启动日志后，将日志对象传入运行对象，以便重载同步更新日志目录和等级：

```cpp
ly27::RuntimeLogger logger;
QString error;
logger.start(config.logDirectory, config.logLevel, error);
ly27::TerminalRuntime runtime(config, nullptr, &logger);
```

日志由应用持有，必须比运行对象活得更久。CLI 已按此接入。未传入日志对象时，运行对象仍更新 MQTT 记录目录，但不接管外部 Qt 日志处理器。

日志目录变化时先打开新文件再替换旧文件，失败则保留旧日志并恢复旧后端。只改等级而目录不变时不另建日志文件。

`TerminalConfig::toDocument()` 输出可重新加载的配置，`toJson()` 额外包含诊断用的 `source_path`。保存只写配置字段，使用 `QSaveFile` 原子替换并禁止原地覆盖回退；校验或写入失败时不覆盖原文件。保存到其他目录时重新计算日志目录的相对路径，保持其实际位置不变。配置文件限制为 64 KiB。

应用和保存是两个明确操作：先应用、再按用户选择保存。保存失败时运行配置仍然有效，应提示尚未保存；只保存到其他文件不会改变当前监听路径，需要 `reloadConfig(newPath)` 才切换来源文件。没有自动覆盖示例配置的行为。

## 可选文件监听

C++ 使用 `setConfigWatching(true, error)` 开启，默认关闭。CLI 使用：

```bash
./build/local-dev/bin/ly27_cli --config config/terminal.json --watch-config --mqtt --video
```

也可仅使用 `--watch-config --run-for 5000 --status-at-exit` 离线测试身份重载。不能与立即退出模式或 `--command-file` 组合，避免单次操作文件携带到另一身份。

同时监听配置文件和父目录，以适配编辑器原子替换保存。150 ms 防抖、按文件内容过滤无变化；文件暂时无效或删除时保留当前配置，修复或重建后继续尝试。监听不意味着保存尚未完成的多步编辑永远不会触发重启，集中修改建议使用编辑器原子保存或显式 `applyConfig()`。

状态快照增加 `configuration.watching`、`last_error` 和 `applied_utc_ms`。切换到不可监听的配置目录时关闭自动监听并报告日志/状态错误，已经成功应用的配置不撤销。普通日志目录中的文件变化不会在配置内容不变时重启后端。

## 验证范围

本地测试覆盖配置保存后重载、另存时相对路径、无效配置保留会话、角色/阵营切换、模块对象和信号连接稳定、停止状态下应用、视频绑定失败恢复、恢复也失败、日志迁移及失败保留旧日志、原子文件替换、删除重建、CLI 自动重载、本地 Broker 重连，以及旧状态/未确认指令与新身份隔离。

当前未在真实赛事服务器验证各身份的授权与现场切换流程；不推测指挥手身份，不自动尝试其他机器人编号。
