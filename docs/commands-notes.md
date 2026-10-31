# 赛事指令接入

第四阶段加入 `modules/commands/`，将终端操作转换为七类赛事出站消息。依据工作区 2027 通信协议第 1.3.1 节、2.1 节和 2.2.18、2.2.21–32 节，以及 2027 比赛规则的装配、能量机关、哨兵和飞镖相关条款。

## 文件和调用链

```text
CLI / 后续界面
    → CommandRequest 或具体 C++ 方法
    → CommandDispatcher：参数、角色、必要状态、限频
    → MessageCodec::encode
    → MqttTransport::publish（QoS 1，无 retained）
```

- `include/CommandTypes.hpp`：操作枚举、请求结构、提交结果和 CLI 请求解析入口。
- `include/CommandDispatcher.hpp`：统一发送入口、运行生命周期和观察信号。
- `src/CommandDispatcher.cpp`：检查、编码、发布、传输记录和结果观察。
- `src/CommandRequest.cpp`：CLI JSON 到 C++ 请求的转换，不负责网络发送。

`TerminalRuntime` 拥有指令模块，调用 `runtime.commands()`。未来界面和其他功能模块使用相同入口，不直接发布赛事 Topic。指令模块只读取 `StateStore`，不会因为一次发送成功就修改装配、血量、弹量或其他服务器状态。

```cpp
const auto result = runtime.commands().assembly({ly27::AssemblyOperation::Start, 2});
if (!result.accepted) {
    // result.code/error 说明拒绝原因，retryAfterMs 表示限频还需等待多久。
}
```

## 当前覆盖与约束

| C++ 入口 | Topic | 本地角色策略与检查 |
| --- | --- | --- |
| `assembly` | `AssemblyCommand` | 重装；操作 0/1/2，难度 1–4；开始装配要求最高难度新鲜且已解锁 |
| `selectPerformance` | `RobotPerformanceSelectionCommand` | 重装/步兵；至少选一个有效的发射机构/底盘体系 |
| `selectSentryMode` | 同上 | 指挥手；自动 0、半自动 1，显式零值保留 |
| `common` | `CommonCommand` | 地面机器人；17mm 兑换供步兵、42mm 供重装；数量为正，17mm 为 10 的倍数 |
| `activateRune` | `RuneActivateCommand` | 步兵；机关状态须新鲜且为未激活；固定发送 activate=1 |
| `dart` | `DartCommand` | 指挥手；目标 1–5；更换目标要求闸门关闭，确认发射要求完全开启，状态须新鲜 |
| `sentry` | `SentryCtrlCommand` | 指挥手；只接受协议定义的 1、3–12，编号 2 不合法 |
| `map` | `MapClickCmd` | 接收者必须是己方，目标编号必须是对方；检查接收范围、标记、字母和有限非负坐标 |

这些是应用侧的操作范围，不是服务器授权结果。RFID、余额、可用次数、操作时段、姿态冷却和场地交互等仍由引擎最终裁定，应用不根据不完整状态猜测全部赛事规则。

常用指令的复活与远程兑换读取以下新鲜标记：

- 确认免费复活：`self.respawn.canFreeRespawn`。
- 金币复活：`self.respawn.canPay`。
- 远程补弹：`self.canRemoteAmmo`。
- 远程补血：`self.canRemoteHeal`。

缺失或过期返回 `state_unavailable`，明确不允许返回 `state_denied`。CommonCommand 类型 3–6 的 `param` 在当前协议中没有具体定义，因此保持缺失；不把任意数量或猜测值写进去。普通弹量兑换只检查角色、数量和线格式，不在本地推算价格或库存。

装配确认和取消保留恢复操作入口，不以未知最高难度阻止提交，但仍检查操作/难度范围和连接。飞镖 `open=false`、哨兵自动控制 `0` 等明确零值照常编码。取消、关闭等提交是否能执行仍由引擎反馈决定。

键鼠 `KeyboardMouseControl` 和自定义 `CustomControl` 已有协议/传输层接口，本阶段不增加设备采集、周期控制或自定义载荷内部实现。接收自定义字节仍保留在 `RobotState.customData`。

## 地图策略与发送频率

所有赛事指令按 Topic 共享发送间隔，普通 Topic 至少 100 ms，即最高 10 Hz；同一 Topic 的不同动作也共享限频。只有通信层接受提交才占用发送间隔，被参数/角色/状态检查拒绝不会占用。

地图不直接套用协议清单的最高 2 Hz：

- 指挥手入口至少间隔 500 ms，可指定己方接收者或按协议广播。
- 地面操作手入口至少间隔 3000 ms，只发送给绑定的本机机器人，作为规则中的半自动地图入口。

通信协议第 1.3.1 节仍使用“云台手”称呼，2027 规则取消云台手并由指挥手操作相关系统。本地按上述角色映射实施限制，指挥手 MQTT 身份与现场具体权限仍需核实。地面机器人的半自动控制适用条件由现场引擎判定，客户端不会因为能够构造地图请求就宣称已获得此模式。

地图字段对应关系：`marker` 写入官方 `mode`（1 攻击、2 防御、3 警戒、4 自定义）；目标方式写入官方 `type`（1 坐标、2 对方机器人）。指定接收者补齐为固定 7 字节，广播时该列表为 7 个零。敌方机器人目标的地图坐标显式为零。自定义图标仅允许 C–L、N、O、Q–Z；坐标不硬编码为某张地图的物理尺寸。

## 提交、传输和执行

`CommandSubmission.accepted=true` 表示 Paho 接受了当前发布请求；返回本地 UUID 用于跟踪本次提交。这个 UUID 不写入官方消息，不能用来要求服务器回传。

每条记录分别保存：

- `transport=queued`：等待 MQTT 传输确认。
- `transport=delivered`：观察到 QoS 1 传输确认，不是引擎执行完成。
- `transport=failed`：观察到通信库失败回调；也不能反推服务器绝对没有执行。
- `transport=uncertain`：断线、停止或 5 秒内没有收到确认，执行情况未知。
- `execution=not_confirmed`：当前没有足够依据将某次请求标为业务执行成功。

最多保留 128 条提交记录，最多 32 条等待传输确认。断线拒绝新操作，不缓存离线操作，不自动重试业务操作；重连只恢复接收订阅，不主动重放点击。会话重新启动清空记录和发送间隔。MQTT QoS 1 自身具备至少一次语义，网络协议层可能重复投递，不能据此承诺服务器只执行一次。

哨兵 `SentryCtrlResult.command_id` 是操作种类，不是请求唯一编号。结果码 0 按官方含义是该操作的成功报告，但协议无法可靠区分同种操作的多次请求。因此最近服务器结果单独保存为 `last_server_result`，标记 `correlation=unavailable_no_request_id`，不强行更改某条本地提交的 `execution`。retained 或早于当前会话的结果不作为本次运行的新反馈。

## CLI 联调

创建一个 JSON 文件，每次运行仅提交一个请求：

```bash
./build/local-dev/bin/ly27_cli --config config/terminal.json \
  --mqtt --run-for 4000 --command-file /tmp/ly27-operation.json
```

要求 `--mqtt` 和 `--run-for`，不能同时 `--watch`。`--command-delay 1200` 表示首次 MQTT 就绪后等待 1200 ms 再尝试；需要周期状态的操作可以据此等待服务器推送，但未知状态仍会被拒绝，CLI 不循环重试。

指令文件示例：

```json
{"command":"assembly","operation":"start","difficulty":2}
```

```json
{"command":"performance","shooter":"burst","chassis":"power"}
```

```json
{"command":"sentry-mode","mode":"automatic"}
```

```json
{"command":"common","action":"exchange-17","amount":100}
```

```json
{"command":"rune"}
```

```json
{"command":"dart","target":2}
```

```json
{"command":"dart","open":false}
```

```json
{"command":"sentry","action":"defense"}
```

```json
{"command":"map","range":"specified","receivers":[1,7],"marker":"attack","position":{"x":5.0,"y":3.0}}
```

装配操作为 `start/confirm/cancel`；性能选项为 `cooling/burst`、`health/power`；哨兵模式为 `automatic/semi-automatic`。常用动作还有 `exchange-42/confirm-respawn/buy-respawn/remote-ammo/remote-heal`，只有两个 exchange 动作接受 amount。飞镖还支持 `launch:true`，可选字段保持存在性。哨兵动作包括 `supply-ammo/remote-ammo/remote-heal/confirm-respawn/buy-respawn/attack/defense/move/enhanced-attack/enhanced-defense/enhanced-move`。

地图 `range` 还支持 `except-sentry/including-sentry`；`marker` 支持 `attack/defense/warning/custom`，自定义时额外填写 `ascii`。使用 `enemy:103` 可替代 `position`，二者只能选一个。JSON 字段严格检查类型和未知键，文件上限为 64 KiB。

stdout 输出 `command_submitted`、`command_transport`、`command_rejected`、`server_result_observed` 等 JSON 行。`--status-at-exit` 还会追加多行格式的最终运行快照，脚本应按所需模式处理。指令模式退出 0 只说明这次提交获得 MQTT 传输确认；拒绝、未就绪、超时未提交或未确认退出 5。

日志共用 `mqtt_<session>.jsonl`：实际接受的发布记录为 `direction=tx`，保存 Topic、字段和原始载荷 Base64；传输和拒绝事件为 `direction=command_event`，接收数据仍为 `direction=rx`。

## 验证与待联调

本地测试使用独立回环 Mosquitto，验证协议字节、显式零值、角色、数量、状态允许标记、发送间隔、地图编号与接收者、结果不误关联、JSON 入口和 CLI 实际收包及发送日志。另用只接受 CONNECT、不回复 PUBACK 的对端验证超时和停止后的未知状态。

尚未连接真实赛事引擎：身份和 Topic 权限、CommonCommand 未定义参数、字段是否允许部分更新、真实场地交互条件和上述本地角色策略需要现场验证。没有实现多次判罚的独立机器人账本、自动业务重试、装配自动流程或机器人自定义协议解析。
