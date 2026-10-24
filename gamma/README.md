# gamma — 2DMC 的 3D 新生代

gamma 是 [2DMC](https://github.com/EM-wzl/2DMC) 的 3D 版本线：在开源项目 [OpenMiner](https://github.com/Unarelith/OpenMiner)（C++17 体素沙盒，客户端/服务端架构 + Lua 模组 API）的基础上 fork 改造，让 2DMC 从 Windows 控制台的 2D 世界走向真正的 3D 体素世界。

2DMC 各版本一览：

| 目录 | 形态 | 说明 |
|------|------|------|
| `classic/` `Alpha/` `beta/` `beta1.8/` `beta2.0/` | 2D 控制台版 | 单文件 C++，Windows Console API 宽字符渲染 |
| `gamma/`（本目录） | 3D 体素版 | OpenMiner fork，bgfx + SDL2 渲染，客户端/服务端分离 |
| `Minecraft/` | 游戏运行目录 | 编译产物 `Minecraft.exe` 与配置、存档、模组所在处 |

## 特性

继承自 OpenMiner 的核心能力：

- 伪无限世界（X/Y/Z 三个方向各 2³² 格）
- 客户端/服务端架构，支持多人联机
- 基础世界生成（生物群系、湖泊、树木、洞穴）
- 平滑光照 + 环境光遮蔽
- 昼夜循环（太阳/月亮/星星）
- 维度系统（类似下界）
- Lua 模组 API：方块/物品/配方/生物群系/树/天空/维度/实体定义，以及自定义 GUI
- 方块可带库存并按服务端 tick 更新（工作台、熔炉）
- 存档/读档、材质包系统（部分实现）

2DMC 的本地改造：

- `build.bat` 一键编译脚本：自动探测 MinGW-w64 工具链、初始化 git 子模块、编译后自动把客户端部署为 `../Minecraft/Minecraft.exe`（并附带运行所需 DLL）
- 流体流动系统：水 7 级/岩浆 3 级、失源回退、静置零开销、水+岩浆→黑曜石、桶取放流体（详见 [WIKI.md](WIKI.md) 流体系统章节）
- 修复中文文本渲染模糊：CJK 字形度量整数量化（`source/client/gui/Font.cpp`）
- `external/` 下的第三方库已针对 MinGW-w64 / g++ 打好补丁

## 目录结构

```
gamma/
├── build.bat              # Windows 一键编译脚本（默认 Release + 自动部署）
├── CMakeLists.txt         # CMake 入口（最低 3.6）
├── source/
│   ├── client/            # 客户端（渲染、GUI、输入、字体等）
│   ├── server/            # 服务端
│   └── common/            # 客户端/服务端共用代码（世界、方块、网络协议等）
├── external/              # 第三方库（git 子模块：bgfx、SDL2、LuaJIT、sol2、EnTT、zlib 等）
├── config/                # 默认配置（client.lua、keys.lua 键位）
├── mods/                  # 内置模组（default、creative_inventory）
├── docs/                  # MkDocs 文档源（Lua API、网络协议）
├── tests/                 # 测试（CxxTest，可选构建）
└── screenshot*.png        # 游戏截图
```

游戏运行数据不在源码目录，而在仓库根的 `Minecraft/`：

```
Minecraft/
├── Minecraft.exe              # 客户端（编译后自动部署到此处）
├── openminer_server.exe       # 服务端
├── config/                    # 实际生效的配置与键位
├── mods/                      # 实际加载的模组
├── saves/                     # 存档
├── texturepacks/              # 材质包
└── resources/                 # 贴图、着色器等资源
```

## 环境要求

- Windows
- CMake ≥ 3.12
- MinGW-w64 的 g++（推荐；`external/` 依赖按 MinGW/g++ 16 修补，未装 VS 也能编）
- Git（首次编译需联网拉取子模块）

## 编译

### 一键编译（推荐）

在 `gamma/` 目录下直接运行：

```bat
build.bat
```

脚本会依次完成：检查/拉取子模块 → CMake 配置（MinGW Makefiles）→ 并行编译 → 把产物部署到 `../Minecraft/`。

常用变体：

```bat
build.bat Debug            :: Debug 构建
build.bat RelWithDebInfo   :: 带调试符号
build.bat clean            :: 删除 build 目录
```

可选环境变量：

| 变量 | 说明 |
|------|------|
| `CMAKE_GEN` | 手动指定 CMake 生成器，例如 `set CMAKE_GEN=Ninja && build.bat` |
| `JOBS` | 并行编译任务数（默认自动使用全部核心） |
| `DEPLOY_DIR` | 部署目录，默认 `..\Minecraft`；设为 `none` 禁用部署 |
| `DEPLOY_NAME` | 客户端部署后的文件名，默认 `Minecraft.exe` |

编译产物：`build/openminer.exe`（客户端）、`build/openminer_server.exe`（服务端）。

### 手动编译

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -G "MinGW Makefiles"
cmake --build build --parallel
```

## 运行

- 单人：进入仓库根的 `Minecraft/` 目录，双击 `Minecraft.exe`
- 多人：先启动 `openminer_server.exe`，客户端内连接服务器

## 键位（默认，可在 config/keys.lua 修改）

| 按键 | 功能 |
|------|------|
| <kbd>W</kbd><kbd>A</kbd><kbd>S</kbd><kbd>D</kbd> | 移动 |
| <kbd>Space</kbd> | 跳跃 |
| <kbd>Left Shift</kbd> | 潜行 |
| <kbd>Left Ctrl</kbd> | 疾跑 |
| <kbd>X</kbd> | 飞行 |
| <kbd>E</kbd> | 背包 |
| <kbd>H</kbd> | 创造模式物品窗口 |
| <kbd>T</kbd> | 聊天 |
| <kbd>小键盘 /</kbd> | 命令输入 |
| <kbd>P</kbd> | 方块信息开关 |
| <kbd>F3</kbd> | 调试信息 |
| <kbd>F11</kbd> | 全屏 |
| <kbd>Esc</kbd> | 暂停/退出 |

## 文档

- **gamma Wiki（玩法与系统详解）**：[WIKI.md](WIKI.md) —— 世界与维度、方块/物品/配方全表、聊天命令、模组系统、配置与联机，基于源码与 mods 数据核对编写
- Lua 模组 API 与网络协议（本仓库 `docs/`，MkDocs）：

  ```bash
  pip install -r docs/requirements.txt
  mkdocs serve
  ```

- 上游 OpenMiner 在线文档：[Core API](https://openminer.readthedocs.io/en/latest/lua-api-core/) · [Mod API](https://openminer.readthedocs.io/en/latest/lua-api-mod/) · [GUI API](https://openminer.readthedocs.io/en/latest/lua-api-gui/) · [C++ classes in Lua](https://openminer.readthedocs.io/en/latest/lua-api-cpp/) · [Network Protocol](https://openminer.readthedocs.io/en/latest/network-protocol/)

## 截图

![](screenshot1.png?raw=true)
![](screenshot2.png?raw=true)
![](screenshot3.png?raw=true)
![](screenshot4.png?raw=true)
![](screenshot5.png?raw=true)
![](screenshot6.png?raw=true)

## 致谢与许可

gamma 基于 Unarelith 的 [OpenMiner](https://github.com/Unarelith/OpenMiner)（LGPLv2.1+）改造，感谢上游项目及以下贡献者：

- xMrVizzy（[Faithful 32x](https://www.curseforge.com/minecraft/texture-packs/faithful-32x) 材质包）
- ThePhD（[sol2](https://github.com/ThePhD/sol2)）
- skypjack（[EnTT](https://github.com/skypjack/entt)）

本项目对上游的修改同样以 [LGPLv2.1+](https://www.gnu.org/licenses/old-licenses/lgpl-2.1.en.html) 发布，详见 [LICENSE](LICENSE)。
