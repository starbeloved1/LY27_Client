# LY27 Client

RoboMaster 2027 自定义终端。使用 C++17、Qt 6.8 和 CMake，先实现可独立运行的后端，再接入 Qt Quick/QML 界面。

当前完成第一阶段：工程基础、配置校验、重装/步兵/指挥手身份、运行生命周期、控制台和文件日志、命令行入口。当前程序不会连接 MQTT 或监听图传端口；状态输出会明确标记这些模块尚未实现。

## 构建

依赖：C++17 编译器、CMake 3.22+、Ninja、Qt 6.8+ 的 Core；开启测试时还需要 Qt Test。第一阶段不依赖 Widgets、QML、FFmpeg、Paho 或 Protobuf。

在本目录执行，将 Qt 路径替换为本机实际位置：

```bash
cmake --preset dev -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/gcc_64
cmake --build --preset dev --parallel 2
ctest --preset dev
```

只构建后端可在配置时添加 `-DBUILD_TESTING=OFF`。`release` 预设使用同样的命令方式。

此电脑另有不提交的 `CMakeUserPresets.json`，使用已安装的 Qt 6.8.3：

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
```

默认使用可执行文件旁的 `config/terminal.example.json`，从其他工作目录启动也可找到它。构建只复制示例文件，不覆盖用户配置。自建配置：

```bash
cp config/terminal.example.json config/terminal.json
./build/dev/bin/ly27_cli --config config/terminal.json --status
```

`--check-config`、`--status`、`--run-for` 三者互斥。退出码：`0` 正常退出；`2` 参数或配置错误；`3` 日志初始化失败。`--help` 和 `--version` 无需加载配置。

JSON 状态写入 stdout，日志写入 stderr 和日志文件，便于脚本解析。

## 配置约定

- `schema_version`：当前为整数 `1`。未知字段、缺失字段和错误类型直接报错，不静默套用默认身份。
- `identity.role`：`heavy`（重装）、`infantry`（步兵）、`commander`（指挥手）。
- `identity.team`：`red` 或 `blue`。
- `identity.station_id`：本地席位名称，不能为空，与服务器 MQTT 身份分别配置。
- `identity.robot_id`：重装为红方 `1`/蓝方 `101`；步兵为红方 `3`、`4`/蓝方 `103`、`104`；指挥手填 `null`。这些是本地身份一致性检查，不表示服务端授权。
- `network.mqtt_client_id`：独立字符串，允许暂时为空；未来连接前必须核实服务器接受的身份，尤其不能把指挥手自动当成某台地面机器人。修改机器人身份时同时核对该字段。
- `network.mqtt_host`、`mqtt_port`、`video_port`：后续模块的连接配置，第一阶段只读取和展示。端口必须为 1–65535 的整数。示例使用回环地址。
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
apps/cli/main.cpp             参数解析、控制台入口、进程退出处理
src/app/TerminalRuntime.*    后端创建、启动、停止和状态快照
src/app/RuntimeLogger.*      Qt 日志接入、级别过滤和文件写入
src/config/TerminalConfig.*  JSON 读取、严格校验、路径解析
src/core/TerminalIdentity.*  角色、阵营、机器人身份及一致性校验
config/                     可提交的示例配置与被忽略的个人配置
tests/                      配置、生命周期及真实 CLI 子进程测试
docs/roadmap.md              后续功能接入顺序
```

`ly27_core` 仅链接 Qt Core，`ly27_cli` 调用它。未来桌面入口复用同一个后端，窗口不拥有通信和业务生命周期。构建产物、日志、个人配置和本机 Qt 路径不提交。
