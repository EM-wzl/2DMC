# 2DMC

2DMC 是一个 Minecraft 式沙盒游戏项目：2025 年 7 月从 Windows 控制台里的一个 2D 小原型起步，历经控制台版（Alpha → Beta 1.x）、OpenGL 图形版（beta1.8 / beta2.0），演进到如今的 3D 体素版 **gamma**（基于开源项目 [OpenMiner](https://github.com/Unarelith/OpenMiner) fork 改造）。

## 版本线一览

| 目录 | 形态 | 说明 |
|------|------|------|
| `classic/` | 2D 控制台 | 2025.7 最初的三个单文件原型（世界数组、挖掘、早期合成） |
| `Alpha/` | 2D 控制台 | Alpha 0.1.8–0.8：光照、实体、昼夜、Boss 等系统逐步成形 |
| `beta/` | 2D 控制台 | beta1.0–2.1 单文件源码存档；详细 Wiki 基于 `beta1.6.cpp` |
| `beta1.8/` | 2D 图形（OpenGL） | 把 `beta1.8.cpp` 完整重写为贴图渲染，功能全保留，新增耐久度系统、MC 皮肤、矿脉贴图 |
| `beta2.0/` | 2D 图形（OpenGL） | 模块化 `src/` 工程，地牢重做与多阶段数值/内存修复 |
| `gamma/` | **3D 体素（当前主力）** | OpenMiner fork：客户端/服务端架构 + Lua 模组 API |
| `Minecraft/` | 游戏运行目录 | gamma 编译产物 `Minecraft.exe` 与配置、存档、模组所在处 |
| `video/` `2DMC-launch-video/` `2DMC-remotion-video/` | 视频素材 | 宣传视频成片与 Remotion 工程文件 |
| [WIKI.md](WIKI.md) | 文档 | 控制台版（beta1.6）逐行核对编写的详细玩法 Wiki |

---

## gamma —— 3D 版（当前主力）

在 OpenMiner（C++17、bgfx + SDL2 渲染）基础上 fork 改造，让 2DMC 走进 3D 体素世界。

**继承自 OpenMiner**：伪无限世界（X/Y/Z 各 2³² 格）、客户端/服务端多人架构、平滑光照 + 环境光遮蔽、昼夜循环、维度系统、Lua 模组 API（方块/物品/配方/生物群系/维度/自定义 GUI）、可带库存的方块（工作台、熔炉）、存档与材质包系统。

**2DMC 本地改造**：

- `gamma/build.bat` 一键编译：自动探测 MinGW-w64 工具链、初始化 git 子模块、编译后自动把客户端部署为 `Minecraft/Minecraft.exe`（附带运行所需 DLL）
- 流体流动系统：水 7 级/岩浆 3 级、失源回退、静置零开销、水+岩浆→黑曜石、桶取放流体
- 修复中文文本渲染模糊（CJK 字形度量整数量化）
- `external/` 第三方库已针对 MinGW-w64 / g++ 打好补丁

### 编译与运行

环境要求：Windows、CMake ≥ 3.12、MinGW-w64 的 g++、Git（首次编译联网拉取子模块）。

```bat
build.bat              :: 仓库根直接运行（自动定位 gamma 文件夹）
:: 或
cd gamma
build.bat
```

编译完成后双击 `Minecraft/Minecraft.exe` 开始游戏；联机先启动同目录的 `openminer_server.exe`。

常用变体：`build.bat Debug`、`build.bat clean`；环境变量 `CMAKE_GEN` / `JOBS` / `DEPLOY_DIR` / `DEPLOY_NAME` 可定制生成器与部署，详见 [gamma/README.md](gamma/README.md)。

### 默认键位（可在 Minecraft/config/keys.lua 修改）

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
| <kbd>F3</kbd> / <kbd>F11</kbd> | 调试信息 / 全屏 |

---

## beta1.8 / beta2.0 —— 2D 图形版

把控制台版的玩法完整搬到 OpenGL 窗口渲染，仍是纯 MinGW 静态链接、无第三方 DLL 依赖。

- **beta1.8**：在 `beta1.8/` 目录运行 `build.bat` 生成 `2DMC18.exe`（`build.bat console` 保留控制台输出便于调试）。物品/方块贴图放在 `assets/tiles/`，替换同名 PNG 即可换皮、删除自动回退程序化画法；MC 皮肤放进 `assets/skins/`，游戏内 Esc → 设置 → ★ 玩家皮肤 切换。详见 [beta1.8/README.md](beta1.8/README.md)。
- **beta2.0**：在 `beta2.0/` 目录运行 `build.bat` 生成 `2DMC.exe`。源码按 `platform / render / world / entity / game / core` 模块拆分，包含地牢重做与存档链路、战斗数值、实体系统等多阶段修复。

---

## 控制台版（Alpha → Beta 1.x）与详细 Wiki

最快的玩法体验入口（无需 CMake，任意支持 C++17 的 g++）：

```bash
g++ -std=c++17 -O2 -o 2DMC.exe beta/beta1.6.cpp
./2DMC.exe
```

完整的控制台版玩法文档——物品与方块表、五页合成配方、熔炼、怪物与 Boss（OI Boss / 世界吞噬者 / 递归 Boss）、装备与武器数值、命令系统、Perlin 噪声世界生成、下界维度、v3 存档二进制格式、Alpha 0.1.8 → Beta 1.6 逐版本历史——已迁移至 [WIKI.md](WIKI.md)。
