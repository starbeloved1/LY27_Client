# LY27 Client

RoboMaster 2027 自定义终端。使用 C++17、Qt 6.8 和 CMake，后端、命令行与测试统一使用 C++，先实现可独立运行的后端，再接入 Qt 图形界面。

桌面端优先提供**重装和指挥手**的信息辅助界面，与官方选手端配合使用。当前 Qt Quick 桌面已显示高清场地底图、顶部赛事/设施信息，以及地图上的机器人位置、等级和血量。暂用 RM26 的图标和信息条素材，地图分割、独立设施和角色专项操作后续接入。代码结构参考 RM26 的页面、业务面板和公共组件划分，具体布局和视觉风格继续讨论；主界面默认不使用图传。步兵前端暂缓，具体安排见 [桌面端方案](docs/desktop-plan.md)。

当前已接入工程基础、MQTT、状态存储与 C++ 业务结构，以及第四阶段的赛事指令。支持 23 个入站 Topic 订阅、解析、记录和重连；状态层整理比赛、全局设施、机器人和角色状态；指令层提供装配、性能选择、兑换/复活、能量机关、地图、哨兵和飞镖操作，统一检查参数、角色、必要状态和发送间隔。第五阶段已接入 UDP 图传接收、分片重组、H.265 软件解码与最新图像接口，支持 CLI 导出 PNG；实际输入设备和完整角色界面尚未接入，图传现场性能仍待验证。

23 个入站 Topic 均已接入解析。其中 `RadarInfoToClient` 的官方外层定义不完整，按用户要求以本地暂定 schema 补全，配合 `LY27_Server` 默认 10 Hz 的八车雷达汇总测试地图；该外层字段号与测试频率仍待官方确认。详见 [协议接入说明](docs/protocol-notes.md) 和 [地图联调说明](docs/map-testing.md)。

## 构建

后端依赖：C++17 编译器、CMake 3.22+、Ninja、Qt 6.8+ 的 Core、Network、Gui（用于图像）、Protobuf 3.15+（含匹配的 `protoc`）、Paho MQTT C 异步库 `paho-mqtt3a`，以及通过 pkg-config 查找的 FFmpeg 开发库 libavcodec ≥58、libavutil ≥56、libswscale ≥5。开启测试时还需要 Qt Test 和 Mosquitto 可执行程序。桌面端另需 Qt Quick 及 QtQuick QML 运行模块；桌面构建默认关闭，后端和 CLI 仍不依赖显示服务器或 Widgets。

在本目录执行，将 Qt 路径替换为本机实际位置：

```bash
cmake --preset dev '-DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/gcc_64;/path/to/protobuf'
cmake --build --preset dev --parallel 2
ctest --preset dev
```

只构建后端可在配置时添加 `-DBUILD_TESTING=OFF`。`release` 预设使用同样的命令方式。

此电脑另有不提交的 `CMakeUserPresets.json`，使用已安装的 Qt 6.8.3 和 Protobuf 3.21.12：

```bash
cmake --preset local-dev
cmake --build --preset local-dev --parallel 2
ctest --preset local-dev
```

## 运行

### 战术地图桌面端

在项目根目录执行：

```bash
./启动终端.sh
```

脚本自动选择本机 `local-dev` 预设（存在时），否则使用 `dev`，完成增量构建后启动桌面程序。从其他目录也可以使用脚本的绝对路径启动。需要指定预设时，例如运行 `LY27_PRESET=release ./启动终端.sh`；新机器仍需先配置本机 Qt 和依赖路径。

手动构建与启动的等价命令：

```bash
cmake --preset local-dev -DLY27_BUILD_DESKTOP=ON
cmake --build --preset local-dev --target ly27_desktop --parallel 2
./build/local-dev/bin/ly27_desktop
```

默认离线，界面未知数据显示“—”，不会自动填演示数值。连接配置中的 MQTT 服务：

```bash
./启动终端.sh --mqtt --red-team "红方队名" --blue-team "蓝方队名"
```

可用 `--config config/terminal.local.json` 指定个人配置；默认读取可执行文件旁的 `config/terminal.example.json`。队名为本地显示参数，当前协议没有上报队名。桌面入口创建并管理同一个 `TerminalRuntime`，默认关闭图传。顶部包含局数、比分、阶段计时、双方基地和固定 R/B 前哨站血量，顶部信息栏采用更紧凑高度；事件侧栏收窄，分析板压缩留白，为地图保留更大空间；我方当前/累计经济、双方累计控制度/总伤害及己方科技/加密等级集中在横跨整个底部的战场分析板，控制度和伤害以红蓝水平柱对比，控制度按当前阵地占领状态显示我方相对对方的瞬时净速度（正绿负红，点/比赛秒），伤害不显示速度，科技与加密归为一组；地图以圆形车号、紧凑血条及左侧等级格显示八台地面机器人的状态，不显示机器人血量数字；并在两侧基地及两座前哨站位置显示无名称、无数字的细血条，前哨站额外叠加放大的 RM26 图标，左侧战场事件侧栏从顶部信息栏下方延伸到分析板上沿，保留最近 50 条，按时间追加并自动滚动；右侧上方地图等比例占满剩余可用空间，底部分析板提供红蓝视角切换。比赛准备/暂停、五秒倒计时、胜负结算复用 RM26 素材；地图上方显示己敌激活计时，事件栏另记录实际金币收入，击杀及前哨站摧毁复用 RM26 的头像与阵营底框播报。具体见 [比赛展示说明](docs/match-presentation.md) 和 [事件说明](docs/events-notes.md)。显示策略与模拟器联调方法见 [地图联调说明](docs/map-testing.md)。

入口为 `apps/desktop/src/main.cpp`，窗口与 C++ 展示适配位于 `modules/ui/include/`、`src/`，QML 和图片位于 `modules/ui/resources/`。底图保持比例，标记跟随实际图片区域缩放。

### 后端与命令行

以下以普通 `dev` 预设为例；使用 `local-dev` 时将路径中的 `build/dev` 改为 `build/local-dev`。

```bash
# 只校验配置并输出 JSON；不会创建日志目录。
./build/dev/bin/ly27_cli --check-config

# 启动后端，输出 JSON 状态，然后正常停止。
./build/dev/bin/ly27_cli --status

# 运行 1 秒后正常停止。
./build/dev/bin/ly27_cli --run-for 1000

# 持续运行；Linux 下 Ctrl+C 或 SIGTERM 正常退出。
./build/dev/bin/ly27_cli

# 查看全部 Topic、方向和协议说明，不加载配置或连接网络。
./build/dev/bin/ly27_cli --topics

# 连接配置中的 Broker，订阅全部入站消息，向 stdout 输出解析结果。
./build/dev/bin/ly27_cli --mqtt --watch

# 有限时联调，退出时输出连接、解析统计和业务状态。
./build/dev/bin/ly27_cli --mqtt --run-for 5000 --status-at-exit

# MQTT 就绪后提交指令文件中的单次操作，输出指令事件。
./build/dev/bin/ly27_cli --mqtt --run-for 3000 --command-file config/performance.json
```

默认使用可执行文件旁的 `config/terminal.example.json`，从其他工作目录启动也可找到它。构建只复制示例文件，不覆盖用户配置。自建配置：

```bash
cp config/terminal.example.json config/terminal.json
./build/dev/bin/ly27_cli --config config/terminal.json --status
```

`--check-config`、`--status`、`--run-for`、`--topics` 互斥。`--mqtt` 显式开启网络，不能与立即退出的检查或状态模式同时使用；`--watch` 需要 `--mqtt`。普通启动仍只运行离线后端。

退出码：`0` 正常退出；`2` 参数、配置或指令文件格式错误；`3` 日志初始化失败；`4` 后端同步初始化失败；`5` 单次指令被拒绝、运行时限内未提交或未获得 MQTT 传输确认。普通接收模式的异步网络错误仍通过日志、连接状态和统计报告，定时结束不代表已经成功连接。指令模式的退出码 `0` 只说明传输得到确认，不代表赛事操作成功。`--help`、`--version`、`--topics` 无需加载配置。

JSON 状态写入 stdout，日志写入 stderr 和日志文件，便于脚本解析。

`--watch` 每个消息输出一行 JSON；与 `--status-at-exit` 同用时末尾还会输出多行格式的最终状态，脚本处理时应选定需要的模式。

状态输出中的 `business_state` 包含 `global`、`self`、`robots`、`role`、全部入站 `topics` 和最近 128 条事件类通知。每个状态带有 `fields`、`source`、`age_ms`、`ttl_ms`、`validity` 和 `reason`。启动后尚未收到的字段保持缺失；断线保留历史值并标记无效，重连后等待新消息。`--status-at-exit` 在停止后输出，所以已接收的数据会标记 `disconnected`。详见 [状态接口说明](docs/state-notes.md)。

C++ 业务代码通过 `stateStore().matchState()`、`globalState()`、`robot(id)`、`self()`、`roleState()` 和 `notifications()` 读取明确的结构，不按 JSON 字符串拼状态。例如 `self()->currentHealth` 汇总私有或全局血量，`self()->maxHealth` 来自机器人静态属性；两者各自保留来源。明确收到不同局次后清理上一局数据。CLI 的 JSON 接口用于诊断，战术分析、提醒和机器人自定义载荷的内部解析按后续功能接入。

## 发送赛事指令

在本地 Broker 联调时，可以创建 `config/performance.json`：

```json
{"command":"performance","shooter":"burst","chassis":"power"}
```

然后使用上述 `--command-file` 命令。该模式要求 `--mqtt`、`--run-for`，不能与 `--watch` 组合；每次运行仅尝试一次，不循环发送。`--command-delay 1200` 可在首次 MQTT 就绪后等待 1200 ms，让周期状态先到达；状态仍未知或过期时拒绝有状态约束的操作，不自动重试。

C++ 模块直接使用相同后端：

```cpp
const auto result = runtime.commands().selectPerformance({
    ly27::ShooterPerformance::Burst, ly27::ChassisPerformance::Power
});
// result.accepted 表示通信层接受提交，后续传输事件由 commandObserved 通知。
```

指令状态位于运行快照的 `commands`，包含最近 128 条记录、待确认数量、最近哨兵结果。收发及指令事件共用 MQTT JSONL 日志。详细接口、指令文件示例、约束和实机验证边界见 [指令接入说明](docs/commands-notes.md)。键鼠发送和自定义载荷内部协议仍预留到输入阶段，不添加摩擦轮等推测实现。

后续输入设计的按键习惯参考、RM26 实际键鼠上传链路和需要修正的冲突，见 [键鼠上传与快捷键参考](docs/input-reference.md)。该文档不表示输入模块已接入。

## 本地订阅体验

先启动一个只监听回环地址的临时 Broker，端口需与个人配置保持一致：

```bash
printf 'listener 3333 127.0.0.1\nallow_anonymous true\npersistence false\n' > /tmp/ly27-mosquitto.conf
mosquitto -c /tmp/ly27-mosquitto.conf
```

另开终端运行 `ly27_cli --mqtt --watch`，再向本地 Broker 注入比赛阶段 4、剩余时间 300 秒的测试消息：

```bash
printf '\x28\x04\x30\xac\x02' | mosquitto_pub -h 127.0.0.1 -p 3333 -q 1 -t GameStatus -s
```

程序会输出 `status: parsed` 和两个对应字段，并在日志目录中写入 MQTT 原始 JSONL 记录。本地可自定测试身份；官方引擎接受的 MQTT 身份必须另行核实。

## 配置重载与身份切换

`TerminalRuntime` 已提供 `applyConfig()`、`reloadConfig()`、`switchIdentity()` 和 `saveCurrentConfig()`。有效变更按原 MQTT/图传启用方式建立新会话；无效配置保留当前运行，初始化失败尝试恢复旧配置。状态仓库和指令对象保持稳定，已有信号连接可继续使用。

```bash
# 持续运行并监听个人配置；身份、网络和日志变更经校验后应用。
./build/dev/bin/ly27_cli --config config/terminal.json --watch-config --mqtt --video
```

默认不监听，不自动保存。应用配置和保存文件分开；保存失败时不覆盖原文件。`--watch-config` 不能与立即退出模式或 `--command-file` 同用。接口、日志同步、失败恢复和另存语义见 [配置接入说明](docs/config-notes.md)。

## 图传

```bash
# 独立启用图传，5 秒后查看状态统计。
./build/dev/bin/ly27_cli --video --run-for 5000 --status-at-exit

# 保存第一张新鲜视频帧，便于无界面验证。
./build/dev/bin/ly27_cli --video --run-for 5000 --save-frame /tmp/ly27-frame.png
```

默认监听 `127.0.0.1:3334`，现场监听地址需在个人配置中调整。MQTT 与图传独立启用，允许同时指定 `--mqtt --video`。未来桌面界面通过 `videoFrameAvailable` 通知及 `runtime.video().latestFrame()` 获取 `QImage`，通过 `videoStateChanged` 判断停流；无需从 CLI 转发。接口、分片约束、恢复策略和现场待验证项见 [图传接入说明](docs/video-notes.md)。

## 配置约定

- `schema_version`：当前为整数 `1`。未知字段、缺失字段和错误类型直接报错，不静默套用默认身份。
- `identity.role`：`heavy`（重装）、`infantry`（步兵）、`commander`（指挥手）。
- `identity.team`：`red` 或 `blue`。
- `identity.station_id`：本地席位名称，不能为空，与服务器 MQTT 身份分别配置。
- `identity.robot_id`：重装为红方 `1`/蓝方 `101`；步兵为红方 `3`、`4`/蓝方 `103`、`104`；指挥手填 `null`。这些是本地身份一致性检查，不表示服务端授权。
- `network.mqtt_client_id`：独立字符串，离线时允许为空，启用 MQTT 时必须填写。官方服务器接受的身份需要核实，尤其不能把指挥手自动当成某台地面机器人。修改机器人身份时同时核对该字段。
- `network.mqtt_host`、`mqtt_port`：`--mqtt` 使用的主机和端口；主机只填写域名或 IP，不带协议前缀。`video_port` 用于 `--video` 的 UDP 监听。可选 `video_bind_address` 默认 `127.0.0.1`，现场需显式配置赛事网卡；`video_source_address` 默认空，可限制发送端 IPv4。端口必须为 1–65535 的整数，示例使用回环地址。
- `logging.directory`：相对路径按**配置文件所在目录**解析；绝对路径直接使用。示例的 `../logs`，对于源码目录下的个人配置会指向项目 `logs/`，对于构建目录中的默认示例则指向 `build/<preset>/bin/logs/`。
- `logging.level`：`debug`、`info`、`warning`、`error`。日志时间使用带 `Z` 的 UTC 时间，每次运行创建独立文件，不覆盖已有日志。

指挥手配置的身份部分例如：

```json
{
  "role": "commander",
  "team": "red",
  "station_id": "red-commander",
  "robot_id": null
}
```

## 代码组织

```text
modules/
├── core/                   角色、阵营和机器人身份
├── config/                 配置读取、校验和路径解析
├── protocol/               schema、Topic 清单、编解码和协议校验
├── mqtt/                   异步传输、订阅、重连及通用发布
├── state/                  C++ 业务状态、机器人归属、来源和有效性
├── commands/               赛事操作、参数/角色/状态检查和限频
├── video/                  UDP 分片重组、H.265 解码与最新图像
├── app/                    组织后端生命周期、日志和状态快照
└── ui/                     Qt Quick 窗口、展示适配与地图资源
apps/cli/src/main.cpp       C++ 命令行入口
apps/desktop/src/main.cpp   C++ 桌面入口（可选构建）
config/                     示例配置与被忽略的个人配置
tests/                      C++ 配置、生命周期、协议、状态、指令、CLI 和集成测试
docs/                       接入路线、协议说明
```

每个模块维护自己的 CMake 文件。只有一个头文件和一个实现文件时直接放在模块根目录，例如：

```text
modules/config/
├── CMakeLists.txt
├── TerminalConfig.hpp
└── TerminalConfig.cpp
```

`core`、`config`、`mqtt`、`protocol` 当前采用上述布局；`app`、`state`、`commands`、`video`、`ui` 有多个文件，保留各自的 `include/` 和 `src/`。手写 C++ 头文件统一使用 `.hpp`，模块通过 CMake 导出自己的头文件路径，引用仍统一写成 `#include "TerminalConfig.hpp"`。Protobuf 生成的 C++ 文件保留在该模块的构建目录，测试仍放在 `tests/`。

依赖关系：`config → core`，`mqtt → config`，`state → core/protocol`，`commands → state/mqtt`，`app → config/protocol/mqtt/state/commands/video`，`cli → app`。桌面依赖为 `desktop → ui → app / Qt Quick`，展示适配读取现有类型化状态，后端不依赖 UI。各模块对应独立的 `ly27_<模块>` 构建目标；根 CMake 只组织模块和公共设置。

后端仍可无显示环境运行。未来桌面入口复用同一个后端，窗口不拥有通信和业务生命周期。构建产物、日志、个人配置和本机依赖路径不提交。
