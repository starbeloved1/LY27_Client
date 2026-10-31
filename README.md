# LY27 Client

RoboMaster 2027 自定义终端。使用 C++17、Qt 6.8 和 CMake，后端、命令行与测试统一使用 C++，先实现可独立运行的后端，再接入 Qt 图形界面。

当前已接入工程基础、MQTT 与第三阶段的状态存储和 C++ 业务结构：支持配置校验、三种角色、运行生命周期、日志、官方 Topic 清单、异步连接、23 个入站 Topic 订阅、接收解析、原始记录和断线重连；状态层将数据整理成比赛、全局设施、机器人和角色状态，每个字段提供来源、时间和有效性。另提供通用发布接口供后续指令模块使用。具体赛事操作、图传和正式 UI 尚未接入。

其中 `RadarInfoToClient` 的官方外层 schema 存在歧义，先保留原始数据并标记待确认；其他 22 个入站 Topic 正常解析。详见 [协议接入说明](docs/protocol-notes.md)。

## 构建

依赖：C++17 编译器、CMake 3.22+、Ninja、Qt 6.8+ 的 Core、Protobuf 3.15+（含匹配的 `protoc`）、Paho MQTT C 异步库 `paho-mqtt3a`。开启测试时还需要 Qt Test、Qt Network 和 Mosquitto 可执行程序。当前不依赖 Widgets、QML 或 FFmpeg。

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
```

默认使用可执行文件旁的 `config/terminal.example.json`，从其他工作目录启动也可找到它。构建只复制示例文件，不覆盖用户配置。自建配置：

```bash
cp config/terminal.example.json config/terminal.json
./build/dev/bin/ly27_cli --config config/terminal.json --status
```

`--check-config`、`--status`、`--run-for`、`--topics` 互斥。`--mqtt` 显式开启网络，不能与立即退出的检查或状态模式同时使用；`--watch` 需要 `--mqtt`。普通启动仍只运行离线后端。

退出码：`0` 正常退出；`2` 参数或配置错误；`3` 日志初始化失败；`4` 后端同步初始化失败。网络连接、订阅被拒绝等异步错误通过日志、连接状态和统计报告，定时结束不代表已经成功连接。`--help`、`--version`、`--topics` 无需加载配置。

JSON 状态写入 stdout，日志写入 stderr 和日志文件，便于脚本解析。

`--watch` 每个消息输出一行 JSON；与 `--status-at-exit` 同用时末尾还会输出多行格式的最终状态，脚本处理时应选定需要的模式。

状态输出中的 `business_state` 包含 `global`、`self`、`robots`、`role`、全部入站 `topics` 和最近 128 条事件类通知。每个状态带有 `fields`、`source`、`age_ms`、`ttl_ms`、`validity` 和 `reason`。启动后尚未收到的字段保持缺失；断线保留历史值并标记无效，重连后等待新消息。`--status-at-exit` 在停止后输出，所以已接收的数据会标记 `disconnected`。详见 [状态接口说明](docs/state-notes.md)。

C++ 业务代码通过 `stateStore().matchState()`、`globalState()`、`robot(id)`、`self()`、`roleState()` 和 `notifications()` 读取明确的结构，不按 JSON 字符串拼状态。例如 `self()->currentHealth` 汇总私有或全局血量，`self()->maxHealth` 来自机器人静态属性；两者各自保留来源。明确收到不同局次后清理上一局数据。CLI 的 JSON 接口用于诊断，战术分析、提醒和机器人自定义载荷的内部解析按后续功能接入。

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

## 配置约定

- `schema_version`：当前为整数 `1`。未知字段、缺失字段和错误类型直接报错，不静默套用默认身份。
- `identity.role`：`heavy`（重装）、`infantry`（步兵）、`commander`（指挥手）。
- `identity.team`：`red` 或 `blue`。
- `identity.station_id`：本地席位名称，不能为空，与服务器 MQTT 身份分别配置。
- `identity.robot_id`：重装为红方 `1`/蓝方 `101`；步兵为红方 `3`、`4`/蓝方 `103`、`104`；指挥手填 `null`。这些是本地身份一致性检查，不表示服务端授权。
- `network.mqtt_client_id`：独立字符串，离线时允许为空，启用 MQTT 时必须填写。官方服务器接受的身份需要核实，尤其不能把指挥手自动当成某台地面机器人。修改机器人身份时同时核对该字段。
- `network.mqtt_host`、`mqtt_port`：`--mqtt` 使用的主机和端口；主机只填写域名或 IP，不带协议前缀。`video_port` 仍为后续图传预留。端口必须为 1–65535 的整数，示例使用回环地址。
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
└── app/                    组织后端生命周期、日志和状态快照
apps/cli/src/main.cpp       C++ 命令行入口
config/                     示例配置与被忽略的个人配置
tests/                      C++ 配置、生命周期、协议、状态、CLI 和集成测试
docs/                       接入路线、协议说明
```

每个模块各自维护 `include/`、`src/` 和 CMake 文件，例如：

```text
modules/config/
├── CMakeLists.txt
├── include/TerminalConfig.hpp
└── src/TerminalConfig.cpp
```

手写 C++ 头文件统一使用 `.hpp`，直接放在各模块的 `include/` 下。模块通过 CMake 导出自己的头文件路径，统一使用 `#include "TerminalConfig.hpp"` 这样的引用。Protobuf 生成的 C++ 文件保留在该模块的构建目录，测试仍放在 `tests/`。

依赖关系：`config → core`，`mqtt → config`，`state → core/protocol`，`app → config/protocol/mqtt/state`，`cli → app`。各模块对应独立的 `ly27_<模块>` 构建目标；根 CMake 只组织模块和公共设置。

后端仍可无显示环境运行。未来桌面入口复用同一个后端，窗口不拥有通信和业务生命周期。构建产物、日志、个人配置和本机依赖路径不提交。
