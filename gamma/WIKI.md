# gamma Wiki

> 本 Wiki 基于 `gamma/` 目录的源码（OpenMiner fork）与 `mods/default`、`mods/creative_inventory` 的实际 Lua 数据逐文件核对编写（2026-10-25）。与控制台 2D 版的 [WIKI.md](../WIKI.md) 对应，本文档覆盖 gamma 3D 版的世界、物品、配方、命令、模组与联机系统。

---

## 目录

- [项目概述](#项目概述)
- [快速开始](#快速开始)
- [目录结构](#目录结构)
- [操作指南](#操作指南)
- [世界与维度](#世界与维度)
- [方块列表](#方块列表)
- [物品列表](#物品列表)
- [合成与熔炼](#合成与熔炼)
- [交互方块与工具行为](#交互方块与工具行为)
- [流体系统](#流体系统)
- [生物](#生物)
- [掉落物系统](#掉落物系统)
- [聊天命令](#聊天命令)
- [模组系统](#模组系统)
- [配置文件](#配置文件)
- [联机与网络协议](#联机与网络协议)
- [Lua 模组 API](#lua-模组-api)
- [材质包](#材质包)
- [技术细节](#技术细节)
- [与控制台版（2D）的差异](#与控制台版2d的差异)
- [已知问题](#已知问题)
- [版本沿革](#版本沿革)
- [致谢与许可](#致谢与许可)

---

## 项目概述

gamma 是 2DMC 的 3D 体素版本，基于 Unarelith 的开源项目 [OpenMiner](https://github.com/Unarelith/OpenMiner)（C++17）fork 改造：

- **渲染**：bgfx + SDL2 窗口，支持平滑光照、环境光遮蔽、区块视距设置
- **架构**：客户端/服务端分离，TCP 联机（默认端口 4242），单人游戏也走本地服务端
- **内容定义**：世界内容（方块、物品、配方、生物群系、维度、天空、GUI）全部由 Lua 模组（sol2 绑定 LuaJIT）定义，内置 `default`（已中文化）与 `creative_inventory` 两个模组
- **本地改造**：`build.bat` 一键编译部署（MinGW-w64）、CJK 字形度量整数量化修复中文渲染模糊、`external/` 依赖的 MinGW 补丁

## 快速开始

```bat
cd gamma
build.bat          :: 一键编译（CMake ≥ 3.12 + MinGW-w64 g++），自动部署到 ..\Minecraft\Minecraft.exe
..\Minecraft\Minecraft.exe   :: 双击开始单人游戏
```

联机：先运行 `Minecraft\openminer_server.exe`，客户端主菜单填服务器地址（默认 `localhost:4242`）。更多构建变体（`Debug`、`clean`、`CMAKE_GEN`、`DEPLOY_DIR` 等）见 [gamma/README.md](README.md)。

## 目录结构

```
gamma/
├── source/
│   ├── client/          # 客户端：渲染、HUD（F3 调试层）、GUI、输入、字体
│   ├── server/          # 服务端：世界管理、聊天命令、网络
│   └── common/          # 两端共用：方块/物品注册表、配方、网络协议、Lua 绑定
├── config/              # 默认配置：client.lua（客户端）、keys.lua（键位）
├── mods/                # 内置模组：default（游戏内容）、creative_inventory
├── docs/                # MkDocs 文档源：Lua API、网络协议
├── tests/               # CxxTest 测试（CMake 选项 OM_BUILD_TESTS）
├── external/            # 子模块：bgfx、SDL2、LuaJIT、sol2、EnTT、zlib、glm、tinyxml2
└── texturepacks/        # 材质包目录

Minecraft/               # 游戏运行目录（编译产物部署地）
├── Minecraft.exe / openminer_server.exe
├── config/              # 实际生效配置（client.lua、keys.lua、server.lua）
├── mods/                # 实际加载的模组
├── saves/               # 存档
├── resources/           # 贴图、着色器、字体
└── texturepacks/
```

## 操作指南

### 键位

以下键位来自 `config/keys.lua` 与 `source/client/core/KeyboardHandler.cpp`，可在游戏目录 `config/keys.lua` 中修改：

| 按键 | 功能 | 说明 |
|------|------|------|
| <kbd>W</kbd><kbd>A</kbd><kbd>S</kbd><kbd>D</kbd> | 移动 | 左右键配合前后键斜向移动 |
| <kbd>Space</kbd> | 跳跃 | |
| <kbd>X</kbd> | 飞行（Fly） | 客户端 `isFlyModeEnabled` 配合 |
| <kbd>Left Shift</kbd> | 潜行（Sneak） | |
| <kbd>Left Ctrl</kbd> | 疾跑（Sprint） | |
| <kbd>E</kbd> | 打开背包 | default 模组注册的 mod:key |
| <kbd>H</kbd> | 创造模式物品窗口 | creative_inventory 模组注册 |
| <kbd>T</kbd> | 聊天 | |
| <kbd>小键盘 /</kbd> | 命令输入 | 等价打开聊天并以 `/` 开头 |
| <kbd>P</kbd> | 方块信息开关（Block info toggle） | |
| <kbd>F3</kbd> | 调试覆盖层 | `Shift+F3` 附加渲染器信息输出 |
| 鼠标左键 | 挖掘方块 | 硬度越高越慢，工具匹配可加速 |
| 鼠标右键 | 放置方块 / 交互 / 使用工具 | 见[交互方块与工具行为](#交互方块与工具行为) |

### HUD 与调试

- 常驻 HUD：快捷栏、准星、FPS 计数器（可在 client.lua 关闭）
- <kbd>F3</kbd> 调试覆盖层：区块调试视图、光照图查看器（`isLightmapViewerEnabled`）、性能分析窗口（`isProfilerWindowEnabled`）

## 世界与维度

### 下界（nether）

`dimensions.lua` 定义 `nether`：四个生物群系（下界荒地、下界荒地新群系 `nether_wastes`、灵魂沙峡谷、玄武岩三角洲）、暗红天空（无天体）、**重力 1.4**（下落更快）。矿物：下界金矿（flood_fill p=0.004、矿脉 4，掉 3 金粒）、远古残骸（random_walk p=0.0005，熔炼下界碎片 → 下界锭 → 升级钻石镐/剑/斧为下界工具，挖掘速度 12 全游戏最高）、下界石英矿（flood_fill p=0.005、矿脉 6，掉 1 下界石英）。进入方式：合成传送门方块（10 黑曜石）放置后右键。

| 维度 | ID | 生物群系 | 天空 | 特性 |
|------|------|----|----------|------|------|
| 主世界 | `overworld` | 草原、沙漠 | 蓝天 + 太阳/月亮/星星 | 标准重力；村庄结构 |
| 下界 | `nether` | 下界荒地、下界荒地、灵魂沙峡谷、玄武岩三角洲 | 暗红色天空（无天体） | **重力 1.4**（下落更快）；岩浆湖/玄武岩柱/萤石簇/灵魂沙斑块 + 下界要塞 + 隧道网络 |
| 末地 | `end` | 末地荒地 `end_barrens`、末地高地 `end_highlands` | 暗紫色天空（无天体，400 星） | 整片末地石；紫珀矿脉；末地城 + 折跃门 + 末地要塞 |

**维度 ID**：`overworld = 0`、`nether = 1`、`end = 2`（按 `dimensions.lua` 声明顺序分配）。

### 传送门（Minecraft 风格：维度专属门 + 目标维度留返回门）

gamma 的传送门是**双向配对**的，和 MC 一致，**不是**维度循环：

| 方块 | 存在于 | 右键后 | 在目标维度生成 |
|------|--------|--------|--------------|
| `default:portal`（紫） | 主世界 | → 下界 (dim 1) | `default:nether_portal` |
| `default:nether_portal`（紫） | 下界 | → 主世界 (dim 0) | `default:portal` |
| `default:end_portal`（绿+黑点） | 末地 | → 主世界 (dim 0) | `default:portal` |

**机制**（`blocks.lua` 的 `activate_portal`）：

1. 右键 → 计算目标维度
2. 在目标维度**以玩家位置为中心 ±32 格**搜索已有的返回门（`find_existing_portal`）
3. 找到 → 传送到那扇门；没找到 → 调用服务端 `server:create_portal(dim, x, y, z, portal_id, frame_id)` **就地造一扇** 4×5 的门
4. 传送（`server:send_player_change_dimension`）

因此从主世界进下界后，站在下界那扇紫色门旁右键就能**原路返回**，而不是像早期版本那样按 `(dimension + 1) % 3` 循环（那样在末地会莫名其妙跳回主世界）。

**`create_portal` 的服务端实现**（`ServerCommandHandler::createPortal`）：

- 在目标维度放置 **4(x) × 5(z)** 的门：外圈门框（黑曜石），内部 **2×3 = 6 格**传送门方块（MC 的开口尺寸），底部再补一格门框当站台
- **必须先把门跨越的所有 chunk 地形生成完再放方块**——否则后生成的 chunk 地形会覆盖已放好的门
- 坐标换算必须与 `World::getChunkAtBlockPos()` 完全一致：`chunkX = x/16`、`chunkY = y/16`（CHUNK_DEPTH）、`chunkZ = z/32`（CHUNK_HEIGHT）。引擎里 **y 是垂直轴、z 是高度方向的 chunk 索引轴**，写反了会导致方块落到别的 chunk 或直接丢失
- 方块变化通过 `BlockUpdate` 广播给客户端

**贴图**：`portal.png`（紫）、`nether_portal.png`（紫）、`end_portal.png`（绿+黑点）。

**传送门方块可合成：10 黑曜石 → 1 个**；黑曜石门框点燃机制仍未实现。

#### 末地内容

- **方块**：`end_stone`、`end_bricks`、`purpur_block`（紫珀块）、`purpur_pillar`（紫珀柱）、`end_crystal_block`（发光末地水晶）、`end_portal_frame`（末地传送门框架）、`chorus_plant`（紫颂植株）
- **物品**：`chorus_fruit`（紫颂果）、`end_shard`（末地碎片）
- **配方**：4 末地碎片 → 紫珀块；2 紫珀块（竖排）→ 2 紫珀柱；4 末地石（2×2）→ 4 末地石砖；末地碎片 + 玻璃 → 末地水晶方块
- **群系**：末地荒地（纯末地石，无树无花）；末地高地（额外紫颂植株、更密的紫珀矿脉）

#### 世界生成结构（TerrainGenerator）

所有结构都是**确定性**的（按世界坐标 hash，与区块生成顺序无关），且**只写本 chunk**，越界跳过；非目标维度一律空操作。`generate()` 调用顺序：
`fastNoiseGeneration` → `generateVillages` → `generateNetherFeatures` → `generateNetherTunnels` → `generateNetherFortress` → `generateMineshafts` → `generateEndCity`

| 结构 | 维度 | 区域 cell | 内容 |
|------|------|-----------|------|
| 村庄 | overworld | 160 | 3~5 座房屋（圆石墙 + 木板屋顶 + 玻璃窗）、水井、碎石道路 |
| 下界要塞 | nether | 96 | 桥廊（3 宽，带矮墙）+ 7×7 塔楼（带窗）+ 萤石灯 + 支撑柱 |
| 下界隧道 | nether | — | 每 chunk 2 条蜿蜒隧道（y 30~60），嵌岩浆块/下界石英 |
| 大型岩浆湖 | nether | 64 | 半径 8~14、深 3~5，黑曜石底 + 岩浆块/玄武岩岸 |
| 末地城 | end | 128 | 7×7/9×9 塔楼，高 16~24，中空，分层地板 + 2×2 窗 + 紫珀尖顶 |
| 末地折跃门 | end | 256 | 黑曜石 4×5 框架 + 末地石基座 |

**验证方式**：用无头 TCP 客户端连上服务器，通过 Lua 自测在目标维度用 `world:get_block` 采样结构方块，或临时在 `TerrainGenerator::generate()` 末尾加计数日志。实测：村庄 `cobble=198 + planks=147`、下界要塞 `nether_bricks=246`、末地城 `end_bricks=22 + purpur=2684`（每 chunk 峰值），三个维度区块生成零错误。

#### 存档格式与维度兼容

`WorldSaveBasicBackend` 的存档有三种格式，**都能读，写永远用最新的 OMZ2**：

| 格式 | 布局 | 维度数 |
|------|------|--------|
| **OMZ2**（当前） | `[魔数 OMZ2][u32 原始长度][zlib( u32 维度数 + 维度数据 + 玩家 + ticks )]` | 由头部维度数决定 |
| **OMZ1** | `[魔数 OMZ1][u32 原始长度][zlib( 维度数据 + 玩家 + ticks )]` | 固定 2 |
| **裸格式**（最旧） | 直接是 `sf::Packet` 数据 | 固定 2 |

**为什么要区分维度数**：末地维度是后加的，`m_worldList` 从 2 个变成 3 个。如果旧存档（只写了 2 个维度）被按 3 个维度读，第 3 次循环会把后面的「玩家列表 / 游戏时间」当成 `chunkCount` 误读，导致生成海量空区块或存档数据错位。因此 OMZ1 和裸格式**固定按 2 个维度读**，多出来的维度跳过（日志 `Save has fewer dimensions than the mod defines; skipping extra dimension N`），用默认种子保持可玩；下次保存自动升级为 OMZ2。

已用 5 种存档实测加载（裸格式 2 维 / OMZ1 2 维 / OMZ2 3 维 / OMZ2 2 维 / 真实 OMZ2 存档），全部零错误、服务器正常存活。

#### 下界地形装饰（引擎）

`TerrainGenerator::generateNetherFeatures`（`source/server/world/TerrainGenerator.cpp`）只在维度字符串 ID 为 `default:nether` 时生效，其余维度是空操作，不改变主世界生成结果。所有装饰都基于生成期确定的世界坐标采样（跨区块边界一致），且只在目标格原本就是预期的方块/空气时才写入：

| 装饰 | 说明 |
|------|------|
| 岩浆湖 | 两层平滑噪声选出盆地（约半树下界）与湖面形状，湖面 z 6–18，深度 2–4 格；只替换下界岩，保留顶部岩壳 |
| 玄武岩柱 | 约 1/40 列出现，2–6 格高，立于地表；柱顶 1/3 概率换成黑石 |
| 萤石簇 | 约 1/200 列出现，位于 z 20–30 的悬挂小簇（2–5 格，竖条 + 两侧灯泡） |
| 灵魂沙斑块 | 约 1/30 列的地表下界岩换成灵魂沙，只动表层 |

### 天空与昼夜（sky.lua）

- 主世界白天天空/雾色 RGB(50, 153, 204)，昼夜循环速度 `daylight_cycle.speed = 1.0`
- 太阳贴图尺寸 512；月亮带 8 相位（相位图 32px）；星空 1000 颗、单星尺寸 4
- 下界天空/雾色 RGB(154, 50, 33)，无天体对象

时间制与 Minecraft 相同：一个昼夜 24000 tick。`/time set day` = 1000、`noon` = 6000、`sunset` = 11000、`night` = 13000、`midnight` = 18000、`sunrise` = 1000。

### 生物群系（biomes.lua）

| 生物群系 | ID | 温度/降水 | 地表/下层 | 树木 | 植被 | 矿物 |
|----------|----|-----------|-----------|------|------|------|
| 草原 | `grassland` | 0.5 / 0.3 | 草方块+泥土 / 石头，沙滩沙子 | 橡树 p≈1.04% | 草丛 25%、蒲公英 2.5% | 煤矿 `flood_fill` p=0.001（矿脉 2）；铁矿 `random_walk` p=0.003（矿脉 8） |
| 沙漠 | `desert` | -0.3 / -0.7 | 沙子 / 石头 | 仙人掌 0.1% | 枯灌木 0.5%（沙上） | — |
| 下界荒地 | `netherland` | -0.3 / -1 | 下界岩 / 下界岩，沙滩灵魂沙，液体岩浆 | — | — | 下界石英矿 p=0.005、下界金矿 p=0.004、荧石 p=0.003（均 `flood_fill`）；岩浆块 p=0.002（`random_walk`）；远古残骸 p=0.0005 |
| 下界荒地 | `nether_wastes` | -0.1 / -1 | 下界岩 / 下界岩，沙滩灵魂沙 | — | 下界疣 3.5%（灵魂沙上） | 下界石英矿 p=0.006、下界金矿 p=0.004、荧石 p=0.004；岩浆块 p=0.002 |
| 灵魂沙峡谷 | `soul_sand_valley` | 0.3 / -1 | 灵魂沙 / 下界岩 | — | 下界疣 6%（灵魂沙上） | 下界石英矿 p=0.002、玄武岩 p=0.003、岩浆块 p=0.002 |
| 玄武岩三角洲 | `basalt_deltas` | 0.7 / -1 | 玄武岩 / 黑石，沙滩岩浆块 | — | — | 岩浆块 p=0.005、玄武岩 p=0.004、下界石英矿 p=0.001 |

> 注意：下界原群系的矿物表键名此前误写为 `res`，导致下界金矿/远古残骸/石英矿实际不生成，已改为引擎读取的 `ores`。

生物群系同时定义 `portal = default:portal`、`portal_frame = default:obsidian`（传送门框架方块目前仅为数据定义，尚无搭门点燃逻辑）。

### 树木（trees.lua）

| 树 | 树干 | 树叶 | 树干高度 |
|----|------|------|---------|
| 橡树 `oak` | 橡木原木 | 橡木树叶 | 3–6 |
| 仙人掌 `tree_cactus` | 仙人掌 | 无 | 3–4 |

## 方块列表

共 **103 个方块**：`blocks.lua` 定义 77 个（含下界金矿、远古残骸、下界扩展 6 种），加上 `blocks/workbench.lua`、`blocks/furnace.lua`、`blocks/door.lua`（上下两半），以及流体系统（fluids.lua）的 22 个内部方块。名称即游戏内显示（default 模组已中文化）。

「硬度」越高挖掘越慢；「采集」指 `harvest_requirements`（不满足时仍可挖掉但无掉落）；「—」表示未显式设定，走引擎默认值。

### 自然方块

| 方块 | ID | 硬度 | 采集 | 掉落与特性 |
|------|----|------|------|-----------|
| 泥土 | `dirt` | — | — | 属 `om_material_dirt` 组（铲子有效） |
| 草方块 | `grass` | — | — | 掉落泥土；顶面贴图染色 RGB(129,191,91) |
| 石头 | `stone` | 1.5 | 镐 | 掉落圆石 |
| 圆石 | `cobblestone` | 2 | 镐 | |
| 沙子 | `sand` | — | — | `om_material_sand` 组 |
| 红沙 | `red_sand` | — | — | |
| 黏土 | `clay` | — | — | 掉落 4 黏土球 |
| 冰 | `ice` | — | — | 玻璃渲染（半透明） |
| 雪 | `snow` | — | — | |
| 水 | `water` | — | — | 液体渲染，半透明；入水有蓝色雾效（深度 20）；静态源，可被空桶取走，扰动后激活流动（见[流体系统](#流体系统)） |
| 岩浆 | `lava` | — | — | 液体渲染，**光源**；橙红色雾效（深度 10）；静态源，遇水熄灭为黑曜石 |

### 矿物与矿石

| 方块 | ID | 硬度 | 采集 | 掉落 |
|------|----|------|------|------|
| 煤矿石 | `coal_ore` | 3 | 镐 | 煤炭 ×1 |
| 铁矿石 | `iron_ore` | 3 | 镐 | 自身 |
| 金矿石 | `gold_ore` | 2 | 镐 | 自身 |
| 钻石矿石 | `diamond_ore` | 2 | 镐 | 自身 |
| 绿宝石矿石 | `emerald_ore` | 2 | 镐 | 自身 |
| 红石矿石 | `redstone_ore` | 2 | 镐 | 自身 |
| 青金石矿石 | `lapis_ore` | 2 | 镐 | 自身 |
| 安山岩 / 花岗岩 / 闪长岩 | `stone_andesite` / `stone_granite` / `stone_diorite` | 各 1.5 | 镐 | |

### 矿物块与建筑方块

| 方块 | ID | 硬度 | 采集 |
|------|----|------|------|
| 煤炭块 / 铁块 / 金块 / 钻石块 / 绿宝石块 / 红石块 / 青金石块 | `coal_block` 等 | 各 2 | 镐 |
| 石砖 / 苔石砖 / 裂纹石砖 / 切制石砖 | `stonebrick` 等 | 各 2 | 镐 |
| 苔石 | `cobblestone_mossy` | 2 | 镐 |
| 红砖块 | `bricks` | 2 | 镐 |
| 砂岩 / 红砂岩及平滑、切制变体 | `sandstone` 等 | 各 2 | 镐 |
| 下界砖块 / 红色下界砖块 | `nether_bricks` / `red_nether_brick` | 各 2 | 镐 |
| 末地石 / 末地石砖 | `end_stone` / `end_bricks` | 各 2 | 镐 |
| 干草捆 | `hay_block` | 2 | 镐 |
| 书架 | `bookshelf` | — | — |

### 木制品

| 方块 | ID | 硬度 | 特性 |
|------|----|------|------|
| 橡木原木 | `oak_wood` | — | `om_material_wood` 组（斧有效） |
| 橡木木板 | `oak_planks` | 1 | `default_planks` 组（合成用） |
| 橡木树叶 | `oak_leaves` | 0 | 树叶渲染，染色 RGB(106,173,51) |
| 橡木台阶 | `oak_slab` | — | 半格碰撞箱（0–0.5） |

### 下界方块

| 方块 | ID | 硬度 | 采集 | 特性 |
|------|----|------|------|------|
| 下界岩 | `netherrack` | — | 镐 | |
| 下界金矿 | `nether_gold_ore` | 3 | 镐 | 掉落 3 金粒；下界荒地生成 |
| 远古残骸 | `ancient_debris` | 8 | 镐 | 熔炼为下界碎片；下界荒地稀有生成（需钻石镐级工具才划算） |
| 灵魂沙 | `soul_sand` | — | — | `om_material_sand` 组；下界疣的种植基底 |
| 黑曜石 | `obsidian` | 8 | 镐 | gamma 中与远古残骸并列硬度最高；10 个可合成传送门 |
| 下界石英矿 | `nether_quartz_ore` | 3 | 镐 | 掉落 1 下界石英；四个下界群系均有生成（也可熔炼得石英） |
| 岩浆块 | `magma_block` | 2 | 镐 | **光源**；4 岩浆膏合成 |
| 玄武岩 | `basalt` | 2 | 镐 | 灵魂沙峡谷/玄武岩三角洲地表；引擎玄武岩柱同样使用它 |
| 下界疣块 | `nether_wart_block` | 1 | — | 9 下界疣合成，也可分解回 9 下界疣 |
| 黑石 | `blackstone` | 2 | 镐 | 玄武岩三角洲下层；8 个可代替圆石合成熔炉 |
| 石英块 | `quartz_block` | 1 | 镐 | 4 下界石英合成，可分解回 4 下界石英 |

### 功能与特殊方块

| 方块 | ID | 硬度 | 采集 | 特性 |
|------|----|------|------|------|
| 工作台 | `workbench` | 1 | 斧 | 右键打开 3×3 合成界面，详见[交互方块](#交互方块与工具行为) |
| 熔炉 | `furnace` | 2 | 镐 | 可旋转；点燃后为光源，详见[交互方块](#交互方块与工具行为) |
| 木门（开发中） | `door_wood_lower` / `door_wood_upper` | — | — | 双方块结构，右键开合，自动放置上半 |
| 传送门 | `portal` | — | — | 玻璃渲染；主世界的门，右键通往下界 |
| 下界传送门 | `nether_portal` | — | — | 玻璃渲染；下界的门，右键回主世界 |
| 末地传送门 | `end_portal` | — | — | 玻璃渲染；末地的门，右键回主世界 |
| 红石灯 | `redstone_lamp` | — | — | 右键切换亮/灭两种状态，亮态为光源 |
| 耕地 | `farmland` | — | — | 15/16 高度碰撞箱；干/湿两态贴图 |
| 草径 | `grass_path` | — | — | 15/16 高度碰撞箱；铲子右键草方块得到 |
| 下界疣 | `nether_wart` | 0 | — | 仅能种在灵魂沙上；随机刻生长（p=0.01，3 阶段），成熟右键收获并重置 |
| 种子（开发中） | `seeds` | 0 | — | 仅能种在耕地上；8 个生长阶段，成熟右键收获小麦 |
| 火把（开发中） | `torch` | 0 | — | 光源、无碰撞，细棒碰撞箱 |
| 仙人掌 | `cactus` | 0 | — | 特殊渲染与碰撞箱；破坏下节时上节一并消失 |
| 甘蔗 | `reeds` | 0 | — | X 形渲染；破坏下节时上节一并消失 |
| 蒲公英 | `dandelion` | 0 | — | X 形渲染，半格碰撞箱 |
| 草丛 | `tallgrass` | 0 | — | X 形渲染；剪刀右键可获取（见[工具](#工具与剪刀)） |
| 枯萎的灌木 | `deadbush` | 0 | — | X 形渲染 |
| 荧石 | `glowstone` | — | — | 光源 |
| 黏液块 | `slime` | — | — | 玻璃渲染 |
| 调试方块 | `debug` | — | — | 六面不同调试贴图 |

## 物品列表

共 **111 个物品**：`items.lua` 定义 80 个 + 工具 30 个（6 材料 × 5 类）+ 剪刀。物品定义不含食物回复/攻击力数值（引擎暂未使用这些字段）。

### 工具与剪刀（tools.lua）

由 `register_tool()` 批量生成，名称为英文自动拼接（如 `Wooden Pickaxe`）：

| 材料 | 挖掘速度 `mining_speed` | 采集能力 |
|------|------------------------|----------|
| 木（wooden） | 2 | 镐/斧/铲对应 Capability |
| 石（stone） | 4 | 同上 |
| 铁（iron） | 6 | 同上 |
| 金（golden） | 8 | 同上 |
| 钻石（diamond） | 10 | 同上 |
| 下界合金（netherite） | 12 | 同上 |

每材料 5 件：斧、锄、镐、铲、剑（剑无挖掘加成）。工具 `max_stack_size = 1`。

- **镐**：对 `om_material_stone` 组生效
- **斧**：对 `om_material_wood` 组生效
- **铲**：对 `om_material_dirt` / `om_material_sand` 组生效；右键草方块 → 草径
- **锄**：右键草方块/泥土 → 耕地（上方须为空气）
- **剪刀**（单独定义）：右键草丛 → 获得草丛物品；右键橡木树叶 → 获得树叶物品

### 材料类（items.lua 节选）

| 物品 | ID | 备注 |
|------|----|------|
| 木棍 | `stick` | 基础材料 |
| 煤炭 / 木炭 | `coal` / `charcoal` | 均属燃料组 `om_fuel`，燃烧值 **1600** |
| 铁锭 / 金锭 | `iron_ingot` / `gold_ingot` | |
| 钻石 / 绿宝石 / 青金石 | `diamond` / `emerald` / `lapis_lazuli` | |
| 红石粉 / 荧石粉 | `redstone_dust` / `glowstone_dust` | |
| 铁粒 / 金粒 | `iron_nugget` / `gold_nugget` | 9 粒合成 1 锭 |
| 红砖 / 下界砖 | `brick` / `netherbrick` | 黏土球/下界岩烧制 |
| 下界石英 | `nether_quartz` | 下界石英矿掉落/熔炼；合成石英块 |
| 黏土球 | `clay_ball` | 挖黏土掉落 4 个 |
| 线 / 羽毛 / 燧石 / 皮革 | `string` / `feather` / `flint` / `leather` | |
| 骨头 / 骨粉 | `bone` / `bone_meal` | 1 骨头 → 3 骨粉 |
| 烈焰棒 / 烈焰粉 | `blaze_rod` / `blaze_powder` | 1 棒 → 2 粉 |
| 岩浆膏 | `magma_cream` | 烈焰粉 + 黏液球 |
| 黏液球 / 雪球 | `slimeball` / `snowball` | 4 雪球合成雪块 |
| 纸 / 书 | `paper` / `book_normal` | 3 甘蔗 → 纸；2 纸+1 皮革 → 书 |
| 碗 / 桶 | `bowl` / `bucket_empty` | |
| 水桶 / 岩浆桶 / 奶桶 | `bucket_water` / `bucket_lava` / `bucket_milk` | |
| 箭 | `arrow` | 燧石+木棍+羽毛 → 4 |
| 糖 / 鸡蛋 / 蜘蛛眼 / 发酵蜘蛛眼 | `sugar` 等 | |

### 食物与农产品

| 物品 | ID | | 物品 | ID |
|------|----|-|------|----|
| 小麦 | `wheat` | | 面包 | `bread` |
| 苹果 / 金苹果 | `apple` / `apple_golden` | | 胡萝卜 / 金胡萝卜 | `carrot` / `golden_carrot` |
| 甜菜根 / 甜菜种子 / 甜菜汤 | `beetroot` 等 | | 曲奇 | `cookie` |
| 西瓜片 / 闪烁的西瓜片 | `melon` / `melon_speckled` | | 南瓜派 | `pumpkin_pie` |
| 南瓜种子 / 西瓜种子 | `seeds_pumpkin` / `seeds_melon` | | 毒马铃薯 / 马铃薯 / 烤马铃薯 | `potato_poisonous` / `potato` / `potato_baked` |
| 生/熟牛肉（牛排） | `beef_raw` / `beef_cooked` | | 生/熟猪排 | `porkchop_raw` / `porkchop_cooked` |
| 生/熟羊肉 | `mutton_raw` / `mutton_cooked` | | 生/熟鸡肉 | `chicken_raw` / `chicken_cooked` |
| 生/熟兔肉 | `rabbit_raw` / `rabbit_cooked` | | 生鳕鱼 / 熟鳕鱼 | `fish_cod_raw` / `fish_cod_cooked` |
| 生鲑鱼 / 熟鲑鱼 | `fish_salmon_raw` / `fish_salmon_cooked` | | 河豚 | `fish_pufferfish_raw` |
| 蘑菇煲 / 兔肉煲 | `mushroom_stew` / `rabbit_stew` | | 腐肉 | `rotten_flesh` |

## 合成与熔炼

### 机制

- 背包（<kbd>E</kbd>）自带 **2×2** 合成格；工作台提供 **3×3**（背包 2×2 放不下 3 宽图案的配方）
- 配方支持 `group:` 材料组：`group:default_planks`（任意木板）、燃料组 `group:om_fuel`
- 共 **87 条合成配方 + 23 条熔炼配方**（`recipes.lua`）

### 合成配方（87 条）

**工具（25 条）**——图案固定，仅材料不同（`#` = 材料，`|` = 木棍）：

| 工具 | 图案（3×3） | 材料 |
|------|------------|------|
| 斧 | `##` / `#|` / ` |` | 木=木板，石=圆石，铁=铁锭，金=金锭，钻=钻石 |
| 锄 | `##` / ` |` / ` |` | 同上 |
| 镐 | `###` / `| ` / `| ` | 同上 |
| 铲 | `#` / `|` / `|` | 同上 |
| 剑 | `#` / `#` / `|` | 同上 |

**基础（4 条）**：

| 配方 | 材料 → 产物 |
|------|------------|
| 木棍 | 2 木板（任意）→ 4 木棍 |
| 木板 | 1 橡木原木 → 4 橡木木板 |
| 工作台 | 4 木板（任意）→ 1 |
| 熔炉 | 8 圆石（环形）→ 1 |

**建筑与装饰（21 条）**：红砖块（4 红砖）、石砖（4 石头）、橡木台阶（3 木板 → 3）、书架（木板+书夹心）、砂岩（4 沙）、红砂岩（4 红沙）、平滑砂岩（4 砂岩 → 4）、平滑红砂岩（4 红砂岩 → 4）、末地石砖（9 末地石）、荧石（4 荧石粉）、下界砖块（4 下界砖）、红色下界砖块（下界砖+下界疣交错）、红石灯（4 红石粉+荧石）、黏液块（4 黏液球）、雪块（4 雪球）、干草捆（9 小麦）。

**矿物块（14 条，双向）**：煤炭/铁/金/青金石/红石/钻石/绿宝石各 9 材料合成 1 块，1 块分解回 9 材料。

**杂项（12 条）**：箭（燧石/木棍/羽毛 → 4）、碗（3 木板）、桶（3 铁锭）、书（2 纸+1 皮革）、骨粉（1 骨头 → 3）、烈焰粉（1 烈焰棒 → 2）、岩浆膏（烈焰粉+黏液球）、铁锭（9 铁粒）、金锭（9 金粒）、铁粒（1 铁锭 → 9）、金粒（1 金锭 → 9）、闪烁的西瓜片（8 金粒+西瓜片）。

**食物（4 条）**：面包（3 小麦）、金苹果（8 金锭+苹果）、金胡萝卜（8 金粒+胡萝卜）、甜菜汤（6 甜菜根+碗 → 4）。

**下界装备线（6 条）**：下界锭（4 金锭+4 下界碎片环形）、钻石镐/剑/斧 + 1 下界锭 → 下界镐/剑/斧（挖掘速度 12，全游戏最高）；远古残骸熔炼 → 下界碎片；传送门（10 黑曜石 → 1，右键切换主世界/下界）。

**下界材料（6 条）**：石英块（4 下界石英，2×2）、石英块分解（1 → 4 下界石英）、岩浆块（4 岩浆膏）、下界疣块（9 下界疣）、下界疣块分解（1 → 9 下界疣）、黑石熔炉（8 黑石环形，可代替圆石版）。

### 熔炉

熔炉 3 个槽位：输入（0）、输出（1，只读）、燃料（2，仅接受 `group:om_fuel`）。熔炼进度 **200 tick**/件；点燃时熔炉切换为发光状态。

**燃料**（燃烧值 = 可供 tick 数）：

| 燃料 | 燃烧值 |
|------|--------|
| 煤炭 `coal` | 1600 |
| 木炭 `charcoal` | 1600 |

即 1 煤可熔炼 8 件（1600 / 200）。

**熔炼配方（23 条）**：

| 输入 → 输出 | | 输入 → 输出 |
|------------|-|------------|
| 铁矿石 → 铁锭 | | 圆石 → 石头 |
| 煤矿石 → 煤炭 | | 沙子 → 玻璃 |
| 金矿石 → 金锭 | | 红沙 → 玻璃 |
| 绿宝石矿石 → 绿宝石 | | 橡木原木 → 木炭 |
| 红石矿石 → 红石粉 | | 黏土球 → 红砖 |
| 青金石矿石 → 青金石 | | 下界岩 → 下界砖 |
| 马铃薯 → 烤马铃薯 | | 生牛肉 → 牛排 |
| 生猪排 → 熟猪排 | | 生羊肉 → 熟羊肉 |
| 生鸡肉 → 熟鸡肉 | | 生兔肉 → 熟兔肉 |
| 生鳕鱼 → 熟鳕鱼 | | 生鲑鱼 → 熟鲑鱼 |
| 远古残骸 → 下界碎片 | | 下界石英矿 → 下界石英 |
| 下界金矿 → 金粒 ×3 | | |

## 交互方块与工具行为

| 对象 | 触发 | 行为 |
|------|------|------|
| 工作台 | 右键 | 打开 3×3 合成 GUI（放置时创建 3×3 方块数据）；产物 Shift 点击进背包 |
| 熔炉 | 右键 | 打开熔炼 GUI（输入/输出/燃料 + 两根进度条：熔炼进度 200 tick、剩余燃烧时间） |
| 传送门 | 右键 | 维度专属门 + 目标维度留返回门（主世界↔下界↔末地，见上） |
| 红石灯 | 右键 | 亮/灭状态切换 |
| 木门 | 右键 | 上下两半联动开合（旋转数据 ±1）；放下半时自动生成上半，破坏任意一半时另一半消失 |
| 下界疣 | 右键（成熟） | 收获 1 下界疣物品并回到 0 阶段 |
| 种子 | 右键（成熟） | 收获 1 小麦并回到 0 阶段 |
| 甘蔗 / 仙人掌 | 破坏 | 下节被破坏时上方各节一并消失 |
| 锄 | 右键 | 草方块/泥土 → 耕地（上方为空时） |
| 铲 | 右键 | 草方块 → 草径 |
| 剪刀 | 右键 | 草丛/树叶 → 直接获得对应物品 |

## 流体系统

2DMC 为 gamma 移植了控制台版的流体玩法（OpenMiner 上游未实现流体），全部逻辑在 `mods/default/fluids.lua`，纯 Lua 实现；引擎侧仅有 4 处配套小改（见[技术细节](#技术细节)）。

### 规则

| 规则 | 说明 |
|------|------|
| 层级 | 水 7 级、岩浆 3 级：离源越远层级越大，可流动距离越短 |
| 向下优先 | 下方为空气时向下生成最强（第 1 级）流动水柱，落地后再横向铺开 |
| 横向扩散 | 每流动 1 格层级 +1；到达最末级（水 7/岩浆 3）停止 |
| 失源回退 | 支撑方块被挖掉后流动方块逐级回退消失（级联由邻居激活驱动） |
| 静置 | 流动稳定后转为不 tick 的静态副本（`*_still_N`），稳态零开销；再次扰动自动重新激活 |
| 水遇岩浆 | 岩浆方（含流动岩浆）变黑曜石；黑曜石壳会保护内部岩浆（与 Minecraft 一致） |
| 岩浆更慢 | 岩浆用随机刻（概率 0.3/tick），平均流速约为水的 1/3 |
| 无限水 | 未实现（两水源不生成新源） |

### 方块形态

每个流体三种形态（22 个新方块均带 `ci_ignore` 组，不进创造物品栏；注册在所有既有方块之后，旧存档兼容）：

- `water_active` / `lava_active`：被扰动的静态源（会 tick），平静后还原为 `water`/`lava`
- `water_flow_1..7` / `lava_flow_1..3`：流动中（会 tick），水随层级透明度递减
- `water_still_1..7` / `lava_still_1..3`：静置副本（不 tick）

### 桶（对应控制台版玩法）

- **空桶 + 液体** → 水桶/岩浆桶：持空桶对准液体右键（客户端已允许空桶射线选中液体）；取走流动/静置格时该格消失，取静态源则源仍在
- **水桶/岩浆桶** → 对着方块面右键放置静态水源，桶返还空桶
- 也可从创造物品栏直接拿 `水`/`岩浆` 方块放置，落地即激活流动

### 已知限制

- 液体格默认不可被准星选中（持空桶除外）；移除流动水的办法是向其格内放置方块（引擎本就允许向液体格放置）
- 流动到未加载区块边界会停住（放置读回校验防止无限重试），再次扰动即可恢复
- 无底虚空中的水柱会一直下落，直到触碰方块或世界底部
- 大面积泄洪时每格变化都会同步一次网络包，超大洪峰可能产生瞬时流量峰值
- **水流会被实心方块挡住**（泥土/石头等），这是正确行为；但花草类（矮草/蒲公英/枯萎灌木/甘蔗/下界疣/小麦/耕地）会被水冲掉并让水继续扩散（2026.10 修复，此前遇到花草就停住不流）
- 源头水在一段时间内没有可流方向时会静置为静态 `water`（不 tick）；之后挖开旁边仍会由 `fluid_kick_neighbors` 重新激活

### 游泳

水/岩浆方块（含全部流体变体）已取消碰撞（`is_collidable = false`），玩家可以进入水中：

- **入水判定**：脚部或身体中部处于液体方块（`draw_type = liquid`）即视为在水中
- **游泳物理**（`ClientPlayer::updatePosition`）：水中重力 ×0.25 缓慢下沉，下沉速度上限为跳跃速度 ×0.4；按住空格键持续上浮（不受跳跃门控，上浮速度为跳跃速度 ×0.5）
- **水下雾效**：相机进入液体格时自动生效（水蓝色/岩浆橙红，来自方块定义的 fog_depth/fog_color）
- 真机验证：真机自检证明流动逻辑在取消碰撞后不受影响（77 格分布不变）

## 生物

生物系统由 `mods/default/entities.lua` 实现。**AI 与自然生成都由服务端 `Event.ServerTick` 事件驱动**（引擎每 20tps 触发一次），不再依赖客户端动画或方块 `on_tick` 标记——因此存档重载后已持久化的实体也会继续行动。

### 贴图来源与部件拆分

贴图取自 **MC 原版实体贴图**（64×32 / 64×64 图集），来源：<https://github.com/void717doll/OpenMiner/tree/master/mods/default/textures/mobs>。因为引擎的 `TextureAtlas` 只接受 16×16 的方块贴图，源图集被裁切成 4 张：

```
<mob>_face.png     头正面    <mob>_side.png   身体侧面
<mob>_top.png      身体顶部  <mob>_bottom.png 腿部
```

每种生物注册 **3 个方块**（`<mob>_body` / `<mob>_head` / `<mob>_leg`），引擎的 `properties.parts` 允许**每个部件用不同的 `block_id`**，因此：

- 躯干 → `<mob>_body`（side/top/bottom）
- 头 → `<mob>_head`（**South/North 面 = face**，所以一眼能看出朝向）
- 腿 → `<mob>_leg`（side）

同时保留旧的 `<mob>_block` 作为兼容别名（旧存档/旧引用仍能解析）。

> ⚠️ **贴图必须同步到 texturepack**：客户端在 `texturePack = "minecraft"` 时**只从 `Minecraft/texturepacks/minecraft/blocks/` 读取**方块贴图，根本不看 `mods/default/textures/`（见 `TextureAtlas::loadFromRegistry`）。新增方块贴图后请跑 `powershell -File tools/sync_assets.ps1`（会一并同步并**校验 texturepack 是否覆盖全部方块贴图**）。

### 生物列表

| 生物 | ID | 尺寸 | 行为 | 生成 |
|------|----|------|------|------|
| 猪 | `default:pig` | 0.75 | 随机游荡/待机（speed 0.02） | 主世界 被动 |
| 牛 | `default:cow` | 0.9 | 同上 | 主世界 被动 |
| 羊 | `default:sheep` | 0.9 | 同上 | 主世界 被动 |
| 鸡 | `default:chicken` | 0.6 | 同上 | 主世界 被动 |
| 兔子 | `default:rabbit_brown` | 0.5 | 同上 | 主世界 被动 |
| 豹猫 | `default:ocelot` | 0.7 | 同上 | 主世界 被动 |
| 狼 | `default:wolf` | 0.8 | 同上 | 主世界 被动 |
| 美角 | `default:axolotl_brown` | 0.42 | 同上 | 主世界 被动 |
| 雪傀儡 | `default:snow_golem` | 1.0 | 同上 | 主世界 被动 |
| 鱿鱼 | `default:squid` | 0.95 | 同上（**要求脚下 1~3 格内有水**） | 主世界 被动 |
| 僵尸 | `default:zombie` | 0.9 | 同上 | 主世界 敌对 |
| 骷髅 | `default:skeleton` | 1.8 | 同上 | 主世界 敌对 |
| 苦力怕 | `default:creeper` | 0.9 | 同上 | 主世界 敌对 |
| 蜘蛛 | `default:spider` | 0.8 | 同上（speed 0.045，较快） | 主世界 敌对 |
| 史莱姆 | `default:slime` | 1.2 | 同上 | 主世界 敌对 |
| 蝙蝠 | `default:bat` | 0.89 | 同上 | 主世界 敌对 |
| 僵尸猪灵 | `default:zombified_piglin` | 0.9 | 同上（speed 0.045） | 下界 |
| 僵尸猪（猪灵变体） | `default:pig_zombie` | 0.9 | 同上 | 下界 |
| 恶魂 | `default:ghast` | 1.2 | 同上 | 下界 |

### 掉落物

生物被玩家击杀时，引擎触发 `Event.EntityKilled` 事件（`LuaEventType::EntityKilled`），`mods/default/mob_drops.lua` 监听它并按表掉落 `item_drop` 实体（玩家走过即拾取）。

掉落表（`mob_drops.lua` 的 `mob_drop_table`）：

| 生物 | 掉落 |
|------|------|
| 猪 | 生猪排 1-3 |
| 牛 | 生牛肉 1-3、皮革 0-2 |
| 羊 | 生羊肉 1-2、羊毛 1 |
| 鸡 | 生鸡肉 1、羽毛 0-2 |
| 兔子 | 生兔肉 0-1、兔子皮 0-1 |
| 僵尸 | 腐肉 0-2 |
| 骷髅 | 骨头 0-2、箭 0-2 |
| 苦力怕 | 火药 0-2 |
| 蜘蛛 | 线 0-2、蜘蛛眼 0-1 |
| 史莱姆 | 黏液球 0-2 |
| 鱿鱼 | 线 0-2 |
| 豹猫 | 皮革 0-1 |
| 僵尸猪灵 | 腐肉 0-1 |
| 恶魂 | 火药 0-2 |
| 雪傀儡 | 雪球 0-15 |
| 狼 / 蝙蝠 / 美角 | 无 |

最小数量为 0 的物品按 `dropChance = 0.75` 的概率掉落。

### 生成规则

- **维度分离**：主世界刷 pig/zombie/sheep/spider/creeper，下界只刷 zombified_piglin（`kinds_for_dimension`，维度 ID 0/1）
- **数量上限按真实实体计数**：`mobs_max_total = 40`（每维度）、`mobs_max_per_type = 12`；用 `world:get_entities_by_type()` 实数，不用会话内假计数。选项可用 `/option default:mobs_max_total 60` 调整，`/option default:mobs_enabled false` 可整体关闭
- **位置**：环带采样落在玩家 **26~48 格**外（`SPAWN_MIN_DISTANCE` / `SPAWN_MAX_DISTANCE`），并对所有在线玩家校验 **24 格**排除半径（`PLAYER_EXCLUSION_RADIUS`）。⚠️ 采样距离**必须大于**排除半径，否则采样点全落在排除圈内，一只都刷不出来
- **净空检查**：生成点从参考高度向下扫第一块非空气方块，其上方必须有 **hitbox 高度**（猪 0.75 / 僵尸 0.9 / 蜘蛛 0.8 …，见 `mob_defs[].height`）的空气，而不是固定 2 格——否则较高的生物会卡进低矮地形
- **种类轮换**：按 `spawn_round` 轮换 `kinds` 列表，避免单一类型刷屏（对应参考实现的 `passive[(tick/200) % 5]`）
- **确定性随机**：见下方「确定性随机」——不再用未播种的 `math.random`（每次重启序列相同，玩家能看出规律）
- **触发时机**：`ServerTick` 每 `mobs_spawn_interval`（默认 200 tick = 10 秒）补一次；`PlayerConnected` 进服补一批；挖草方块/下界岩仍保留概率生成作为补充
- **不 despawn**：生物只按上限限制新增，现存生物不会因距离被移除，随存档持久化

### 确定性随机

`entities.lua` 内置一套**精确的 32 位 LCG**（`state = state * 1103515245 + 12345 mod 2^32`），用 LuaJIT 的 `bit` 库做 16 位分段乘法保证跨机器、跨会话完全一致。

**为什么不用 `java.util.Random`**：Java 的 Random 是 48 位 LCG（`seed * 0x5DEECE66D + 0xB`），中间乘积 80 位——**超出 Lua double 的 53 位精度**，纯 Lua 模拟会让状态失控（实测生成的坐标达到 `-1117139` 这种离谱数值，生物全部生成失败）。LuaJIT 的 `bit` 库只支持 32 位，所以改用 32 位 LCG：保留"确定性"这个核心价值，放弃与 MC 逐位一致。

**史莱姆区块判定**（`slime_chunk`）另有一套精确实现：用 `bit` 库模拟 Java 的 int32 溢出，区块中约 **9.9%** 判定为史莱姆区块（MC 实际约 10%）。

### 视线与光照 API（C++）

参考实现（GitHub `void717doll/OpenMiner`）的生物 AI 依赖几个 gamma 原本没有的底层能力，现已在 `ServerWorld::initUsertype` 补齐：

| Lua 方法 | 语义 |
|----------|------|
| `world:get_ticks()` | 当前游戏 tick（`GameTime::getTicks()`） |
| `world:is_daytime()` | 主世界且 `ticks % dayLength < 12000`；下界/末地恒 false |
| `world:get_light(x, y, z)` | 该位置最大光照（sunlight 与 torchlight 取大者）；区块未加载返回 -1 |
| `world:path_clear(fx,fy,fz, tx,ty,tz)` | 视线检测：射线步进（每格 8 步），被可碰撞方块挡住返回 false |

`path_clear` 的实现见 `source/common/world/EntityPath.hpp`（`clearPath`）。未加载的区块**不阻挡**视线（返回 true），因为未加载区域不应影响 AI 判断。

### AI 行为

`ServerTick` 监听器每 tick 枚举本维度全部实体（`world:get_entities()`），对已注册生物类型执行：

- **状态机**：每 20–60 tick 决定 walk/idle；walk 时以 70% 概率随机取 8 个方向（含对角线）之一，持续 40–80 tick
- **移动**：先试完整步长再试半步；目标列向下吸附一格，支持一格上坡/下坡，不悬空
- **贴地修正**：脚下是空气且更下方有支撑时逐 tick 下落；脚部嵌方块且头顶有空间时抬升。未加载区块返回全空气，此时保持原位，避免坠入虚空
- **类型守卫**：只处理 `mob_defs` 中登记的类型，`item_drop` 等非生物实体与存档重载后组件不完整的实体会被安全跳过

碰撞提示已中文化（`on_collision` 按类型限频约 2 秒一条）：猪"哼哼！"、羊"咩！"、蜘蛛"嘶嘶！"、僵尸猪灵"愤怒的哼声！"；僵尸与苦力怕的文案明确标注当前引擎没有伤害/爆炸系统。

### 血量与攻击

生物带 `HealthComponent`（`max_health` 在 `mod:entity` 表顶层定义，缺省 0 = 非生物）：

| 生物 | 最大生命 |
|------|----------|
| 猪 | 10 |
| 僵尸 | 20 |
| 羊 | 8 |
| 蜘蛛 | 16 |
| 苦力怕 | 20 |
| 僵尸猪灵 | 20 |

- **攻击方式**：准星指向生物（6 格内、约 25° 锥角）时，HUD 在屏幕上方显示该生物的 `Mob HP x/y`；按鼠标左键发送 `PlayerAttackEntity`(0x3C)，服务端校验距离后扣血
- **攻击距离**：服务端硬校验 **6 格**（`dx²+dy²+dz² > 36` 直接忽略）。距离外攻击不生效，这是服务端权威的反作弊校验；实测自然生成的生物常在 7–12 格外，需走近（或 `/tp`）才能命中
- **伤害值**：手持物品的 `group:om_weapon` 值，空手为 1
- **受伤反馈**：`HealthComponent::hurtTimer` 置 6 tick（约 0.3 秒），随 `EntityHealth`(0x56) 包同步，客户端 HUD 文字与实体本体同时闪红
- **击杀**：血量降到 0 时服务端广播 `EntityDespawn`(0x51) 并把实体从场景移除，生物消失。**当前没有掉落物**（掉落物系统未接实体死亡）
- 血量随实体存档持久化（`ComponentType::Health = 9`），重载后恢复

### 外观（多部件）

生物不再是"一个方块"，而是由多个轴对齐立方体拼成（`properties.parts`）：

| 生物 | 部件数 | 构成（z 区间） | hitbox |
|------|--------|----------------|--------|
| 僵尸 / 僵尸猪灵 | 4 | 腿 0~0.30 → 躯干 0.30~0.74 → 头 0.62~0.90 | 0.90 |
| 苦力怕 | 4 | 腿 0~0.28 → 躯干 0.28~0.70 → 头 0.59~0.85 | 0.85 |
| 猪 | 7 | 4 腿 0~0.22 → 扁躯干(2 片) 0.22~0.54 → 头 0.50~0.75 | 0.75 |
| 羊 | 7 | 4 腿 0~0.27 → 扁躯干(2 片) 0.27~0.65 → 头 0.61~0.90 | 0.90 |
| 蜘蛛 | 8 | 8 腿 0~0.16 → 扁身体 0.16~0.42 → 头 0.42~0.60 | 0.80 |

**几何硬约束**（违反就会出现"腿没接上躯干/头插进躯干"的错乱）：
- 相邻部件 z 区间必须**首尾相接**（腿顶 == 躯干底，躯干顶 == 头底），不留空隙
- 所有部件最高点必须 **≤ hitbox 高度**
- x/y 方向以实体中心对称

**实现要点**：
- 每个部件有 `size` / `offset`（底面中心相对脚底的位置）/ `block_id`；`offset` 在实体旋转**之后**施加，所以整只生物转身时部件保持刚性、不会散开
- 引擎的部件是立方体，"扁躯干"用**上下两片**（各 `bodyH/2`）叠出来，总高精确等于 `bodyH`
- 实体渲染**开启深度测试**（`InventoryCube::draw` 里 `states.isDepthTestEnabled = m_isEntity`）。此前恒为 `false`，多部件互相穿透，表现为身上出现条纹状穿模
- 引擎侧配套：`DrawableComponent` 支持多 drawable；`DrawableDef` 序列化带部件计数；`InventoryCubeDef` 新增 `offset` 字段
- 验证方法：用无头客户端解码 `EntityDrawableDef(0x55)` 包，逐部件打印 `size`/`offset`/z 区间，自动断言"无空隙 + 不超 hitbox"

### 生成密度

- 上限：总数 12、单类型 4、每玩家局部 8（`mobs_max_total` / `mobs_max_per_type` / `mobs_max_per_player`）
- 间隔：每 200 tick（10 秒）补一次（`mobs_spawn_interval`）
- 挖掘生成只作为极低概率彩蛋：仅在草方块上（下界为下界岩）有 ~0.6%–1.8% 概率，且同样受上限约束
- **`mod:option` 用 `insert_or_assign` 覆盖**：`config/server.lua` 在 mods 加载前解析，会把上次退出时保存的旧上限写进 options；若 `mod:option` 只用 `emplace`，新默认值永远不生效（这是"生物大量刷新"的真实原因）

### 当前限制（必须注意）

- **没有掉落物**：生物被击杀后直接消失，不掉落物品/经验
- 生物之间、生物与玩家之间没有追逐/仇恨/群体 AI，只有随机游荡
- 没有生物音效、没有骑乘/繁殖
- 生物是多块 `inventorycube` 拼成的"方块模型"，不是原版逐骨骼动画模型（腿不会摆动）

## 掉落物系统

- 默认（`use_item_drops = false`）：挖掘方块直接把掉落物塞进玩家背包
- 开启 `use_item_drops`（服务器 `config/server.lua` 的 `mod_options`，或 `/option default:use_item_drops true`）：掉落物以 `default:item_drop` 实体生成——0.25 大小的旋转小方块，带上下浮动动画，0.25 碰撞箱；玩家接触后自动拾取并消失

## 聊天命令

服务端 `ChatCommandHandler` 实现（<kbd>T</kbd> 聊天，<kbd>小键盘 /</kbd> 直接进入命令模式）。输入未知命令时会回显可用命令列表。控制台版（beta 1.6 / beta 2.x）的全部命令已移植到 gamma：

| 命令 | 用法 | 说明 |
|------|------|------|
| `/help` | `/help` | 列出全部命令 |
| `/give` | `/give <物品> [数量]` | 给予物品（数量 1–64）。物品名可写 `stone`（自动补全为 `default:stone`）或 `mod:item` 全名 |
| `/heal` | `/heal` | 生命值回满 |
| `/life` | `/life <0-20>` | 设置生命上限（会同步把当前生命值夹到上限内） |
| `/kill` | `/kill` | 自杀并回到当前列地表上方（无独立出生点系统） |
| `/gamemode` | `/gamemode [0\|1]` | 切换/设置游戏模式：**0 创造 / 1 生存**。切到 0 时客户端自动开启飞行 |
| `/clear` | `/clear` | 清空背包 |
| `/summon` | `/summon <生物> [数量]` | 在玩家附近召唤生物（数量 1–20）。支持英文 ID 与中文名（`猪`/`僵尸`/`羊`/`蜘蛛`/`苦力怕`/`僵尸猪灵`），以及别名 `pigman`/`猪灵`。落点同样要求脚部两格空气 |
| `/cheat` | `/cheat` | 获取一套下界装备（别名 `/EM_wzl`）。gamma 只有下界工具（无盔甲物品），缺失条目安全跳过 |
| `/option` | `/option <name> [<value>]` | 查询/设置服务器选项（如 `default:use_item_drops`、`default:mobs_max_total`） |
| `/time` | `/time [set\|add] <value>` | 支持别名 `day`/`sunrise`(1000)、`noon`(6000)、`sunset`(11000)、`night`(13000)、`midnight`(18000)、`HH:MM` 时间格式与纯 tick 数 |
| `/tp` | `/tp x y z` | 传送到指定坐标 |
| `/tps` | `/tps` | 查看服务器 TPS |
| `/stop` | `/stop` | 关闭服务器 |

`/summon` 底层转调 Lua 的 `mods["default"]:spawn_entity`（复用 `LuaEntityLoader::spawnEntity` 的完整初始化：组件、tick 标记、网络同步），并用 `safe_script` 调用以避免 Lua 错误拖垮服务端（见[技术细节](#技术细节)）。已在真实服务端（无头客户端）验证 `/summon pig 3`、`/help`、`/stop` 可用。

## 模组系统

### 模组结构

一个模组 = `mods/` 下的一个文件夹，至少包含：

```
mods/my_mod/
├── config.lua   # mod_config = { id = "my_mod", dependencies = {} }
└── init.lua     # mods["my_mod"] = openminer.mod_loader:register_mod("my_mod")
                 # 之后 dofile 各内容文件（blocks/items/recipes/...）
```

- 依赖写在 `config.lua` 的 `dependencies` 里（如 `creative_inventory` 依赖 `*default`）
- 游戏目录的 `mods/` 才是实际加载位置（即 `Minecraft/mods/`）

### 内置模组

- **default**：全部游戏内容（方块/物品/工具/配方/天空/树/生物群系/维度/背包 GUI/掉落物实体）。事件监听（`listeners.lua`）：玩家连接时**赠送 1 把石斧**；激活传送门时发送维度切换提示（原版还演示了 BlockPlaced/BlockDigged 监听，已注释）
- **creative_inventory**：<kbd>H</kbd> 打开创造模式物品栏——注册表里除第一个物品（空气）与带 `ci_ignore` 组物品（如门的上半）之外的所有物品，无限数量，带搜索框（按物品名过滤）与滚动条，下方一行快捷栏

## 配置文件

### client.lua（客户端，游戏目录 config/）

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `renderDistance` | 8 | 视距（区块） |
| `cameraFOV` | 70 | 视场角 |
| `screenWidth` × `screenHeight` | 1600 × 1050 | 窗口分辨率 |
| `guiScale` | 3 | GUI 缩放 |
| `ambientOcclusion` | 1 | 环境光遮蔽 |
| `isSmoothLightingEnabled` | true | 平滑光照 |
| `isVerticalSyncEnabled` | true | 垂直同步 |
| `maxBlockReach` | 7 | 方块交互距离 |
| `mouseSensitivity` | 8 | 鼠标灵敏度 |
| `isFlyModeEnabled` / `isNoClipEnabled` | false | 飞行/穿墙（调试用） |
| `defaultServerAddress` | localhost:4242 | 多人直连默认地址 |
| `defaultUsername` | "" | 玩家名（标题页可编辑；空/非法时回退 `Player`） |
| `texturePack` | "" | 材质包 |
| `isChunkMinimapEnabled` / `isLightmapViewerEnabled` / `isProfilerWindowEnabled` | false | F3 系调试窗口 |

### server.lua（服务端，游戏目录 config/）

```lua
max_players = 5
max_item_stack_size = 64
render_distance = 8
mod_options = {
    ["default:use_item_drops"] = false,
}
```

### keys.lua

见[操作指南](#操作指南)，`GameKey::Fly`（X）等引擎键位也在此文件中重绑定。

## 联机与网络协议

- 纯 **TCP** 协议，默认端口 **4242**；服务端监听 `0.0.0.0`（局域网可达）
- 登录握手：`ClientConnect`（用户名）→ `ClientOk`（分配 ClientID + 是否单人）/ `ClientRefused`
- 每个 tick 服务端广播 `ServerTick`（携带当前时间 u64）
- **新增网络包**（`source/common/network/Network.hpp`）：
  - `PlayerHealth = 0x3A`：客户端 ID（u16）+ 当前生命值（u8）+ 生命上限（u8）。玩家登录时与 `/heal`、`/life`、`/kill` 后发送
  - `PlayerGamemode = 0x3B`：客户端 ID（u16）+ 游戏模式（u8，0 创造 / 1 生存）。`/gamemode` 与登录时发送；客户端收到自己的包后按模式 0 开启飞行
  - `LanInfoRequest = 0x3D`：客户端请求局域网信息（无字段）
  - `LanInfo = 0x3E`：服务端回传 `端口(u16)` + `地址数量(u32)` + 每个地址（string）
  - 客户端 `HUD` 左上角常驻显示 `HP 当前/上限`，生命值 ≤ 6 时变红
- 完整包格式见 `docs/network-protocol.md`（含方块更新、玩家位置、方块数据、GUI、聊天等全部包定义；`PlayerHealth`/`PlayerGamemode`/`LanInfo*` 尚未补进该文档）

### 玩家名

- **标题页有「玩家名称」输入框**（`TitleScreenState`），预填 `Config::defaultUsername`，写入 config 持久化
- 名称规则：仅字母/数字/下划线，长度 ≤ 20；空或非法时回退为 `Player`
- 单人/多人两条路径都使用该名称（`ClientConnect` 包携带）
- 服务端用 `connectPlayer(username, ...)` 创建角色，`PlayerSpawn(0x35)` 把名字广播给所有客户端
- **多人游戏时其他玩家头顶显示名字**（`HUD::updatePlayerNameLabels`）：把玩家世界坐标投影到屏幕，
  32 格内显示、16 格外线性淡出；用 `unique_ptr<Text>` 缓存避免每帧重建

### 开放局域网

两种入口：

1. **聊天命令 `/publish`**（别名 `/openlan`、`/lan`）：输出「已开放到局域网: <ip>:<端口>」+ 本机全部 IPv4 + 使用提示
2. **ESC 暂停菜单 →「开放局域网」**（`LanPublishState`）：
   - 端口输入框（默认 4242，可编辑）
   - 「开放局域网」按钮：向服务端请求 `LanInfo`，显示本机所有 `IP:端口`
   - 「返回」按钮
   - 状态提示：当前世界实际监听端口 / 新端口将在下次进入世界生效

服务端启动时也会在日志打印所有 `LAN: <ip>:<port>`，方便直接从日志读取。

## Lua 模组 API

文档源在 `docs/`（MkDocs），本地预览：

```bash
pip install -r docs/requirements.txt
mkdocs serve
```

覆盖范围：

- **Core API**：`openminer` 全局对象（注册表、模组加载器、配置读取）
- **Mod API**：`mod:block` / `mod:item` / `mod:crafting_recipe` / `mod:smelting_recipe` / `mod:biome` / `mod:dimension` / `mod:sky` / `mod:tree` / `mod:entity` / `mod:key` / `mod:option` 及事件监听 `openminer:add_listener(Event.*, ...)`
- **事件类型**：`Event.BlockPlaced` / `BlockDigged` / `BlockActivated` / `ItemActivated` / `PlayerConnected`，以及 **`Event.ServerTick`**（每个服务端 tick 触发，参数为 `ServerCommandHandler`）
- **GUI API**：`LuaGUI` 的 image / inventory / inventory_data / crafting / progress_bar / scroll_bar / text_input 控件
- **C++ 类绑定**：ItemStack、World、Player 等在 Lua 中的方法（如 `world:set_block`、`player:dimension()`、`mods["default"]:give_item_stack`）

生物系统相关的 Lua 绑定（引擎新增）：

| 绑定 | 说明 |
|------|------|
| `server:get_world(dimension)` | `ServerCommandHandler` 方法，取指定维度的 `ServerWorld` |
| `world:heightmap()` | 返回该列地形高度图对象，`get_highest_block_at(x, y)` / `get_highest_chunk_at(x, y)` 查地表高度 |
| `world:get_entities()` | 枚举本维度全部网络实体（`EntityWrapper`，下标从 1） |
| `world:get_entities_by_type(type)` | 按类型字符串（如 `"default:pig"`）枚举实体 |
| `entity:get_position()` / `entity:set_position(x, y, z)` | 读写实体位置（写入会触发下一 tick 位置同步） |
| `player:health()` / `max_health()` / `gamemode()` 及对应 `set_*` | 生命值与游戏模式（服务端权威，不存档） |

内置模组本身就是 API 的最佳示例（如红石灯状态切换、下界疣随机刻生长、熔炉 on_tick、掉落物实体 on_collision、`entities.lua` 的 ServerTick 驱动 AI）。

## 材质包

- 材质包放在游戏目录 `texturepacks/`，在 `config/client.lua` 的 `texturePack` 填包名启用
- 方块/物品贴图位于模组内 `textures/`（如 `Minecraft/mods/default/textures/blocks/`），直接替换 PNG 亦可改内容外观
- 注意：材质包系统继承自上游，属部分实现

## 技术细节

- **源码模块**：`source/client`（渲染/HUD/GUI/输入/字体）、`source/server`（世界控制、聊天命令、网络）、`source/common`（注册表、配方匹配、协议、Lua 绑定）
- **流体系统配套引擎小改**（2026.10）：
	- `ServerChunk::tick`：在副本上迭代 ticking 列表，允许 Lua on_tick 内 set_block 增删自身（流体静置/删除）
	- `ServerBlock::onTick`：向 Lua 回调追加第 5 参 `ServerCommandHandler`
	- `ServerCommandHandler`：新增 Lua 绑定 `send_block_update`（同步方块修改）与 `send_player_inv_update`（同步背包）
	- 客户端 `BlockCursorRaycast`/`BlockCursor`：持空桶可选中液体；持水桶/岩浆桶右键放置流体并返还空桶
- **Lua 事件与实体回调的安全性**（2026.10）：
	- `LuaCore::onEvent` 改用 `sol::protected_function` 调用监听器。LuaJIT 的 `lj_err_throw` 以 Windows SEH 异常抛出，C++ 的 `catch (const sol::error&)` **抓不到**，此前只要 Lua 报错就会直接终止服务端进程；现在错误只记日志。
	- 命令 `/summon` 内部的 Lua 调用同样改为 `safe_script`，不再使用未保护的 `lua.script`。
	- `EntityWrapper` 的 usertype 同时注册 `type`（属性）与 `get_type`（方法）。此前只有属性名，而 `entities.lua` 调用 `entity:get_type()`，导致生物 `on_tick` 第一行就抛错、AI 从不执行（表现为「生物站桩不动」）。
	- 存档重载后按实体类型重新打 `EntityTickTag`（`WorldSaveBasicBackend::loadEntities`），否则持久化的生物会永久静止。
	- `EntityTickTag` 本身不存档（`is_savable=false`），靠实体类型字符串（`ComponentType::EntityID`）在重载时恢复。
- **`ServerConfig::options` 跨 Lua state 修复**：`config/server.lua` 在 ServerConfig 自己的临时 `sol::state` 里解析，把它的 `sol::object` 直接返回给游戏 Lua 会得到 nil。现改为存与 state 无关的 `std::variant<std::monostate, bool, double, std::string>`，`get_config` 在调用方 state 里重建对象。这也是 `mobs_enabled` 读成 nil、生物不生成的根因。同时 `LuaMod::option` 改用带 mod 前缀的完整键查找，避免与 server.lua 的值重复插入。
- **配置优先级修复（用户设置 vs mod 默认值）**：此前 `mod:option` 用 `insert_or_assign` 把 mod 声明的默认值**无条件覆盖**玩家在 `config/server.lua` 里的设置——玩家把 `mobs_enabled` 改成 `true` 完全没效果（会被 mod 里的 `false` 覆盖）。而更早用 `emplace` 时又反过来：配置文件里的**陈旧历史值**永远压过 mod 新声明的默认值（生物上限从 40 改成 12 不生效，导致生物刷屏）。
  现在引入 `ServerConfig::userOptions` 区分两类值：
  - **用户显式设置过的**（`config/server.lua` 里存在该键，或用 `/option` 命令改过）→ 优先，mod 默认值不能覆盖
  - **mod 声明的默认值** → 每次启动由 mod 重新声明，改了立即生效

  保存时**只写 `userOptions`**（mod 默认值不写进文件），这样文件里不会积累陈旧历史值，mod 改默认值后下次启动就能生效。`ServerConfig::isUserDefined(key)` 提供查询接口。
- **流体流向花草**：`fluids.lua` 的水/岩浆现在可以冲掉 `tallgrass`、`dandelion`、`deadbush`、`reeds`、`nether_wart`、`wheat`、`farmland`（`REPLACEABLE` 表，缺失 id 自动跳过）。此前水流只认空气格，遇到花草就判定「无处可流」并静置回退——这是玩家看到「水不流动」的最常见原因。
- **着色器**：`resources/shaders/`（bgfx：basic/game/screen/skybox 的 .vs/.fs/.def）
- **中文渲染修复**：`source/client/gui/Font.cpp` 对 CJK 字形度量做整数量化（commit `4e74f3c`），解决中文发虚
- **世界生成**：按生物群系 `blocks` + `ores`（矿物 gen_type：`flood_fill` / `random_walk`）+ `trees` + `flora` 生成；矿石目前完全按生物群系定义（源码注释中已规划改为按 worldtype）。注意：下界群系原先误写成 `res`，引擎只识别 `ores`，导致下界金矿/远古残骸从不生成，已修正。
- **下界专属地形**：`TerrainGenerator::generateNetherFeatures`（仅 `default:nether`）在基础地形之后追加岩浆湖、玄武岩柱、萤石簇、灵魂沙斑块；纯函数式、按世界坐标确定性生成、只写本 chunk，主世界与其它维度完全不受影响。
- **编译**：CMake ≥ 3.12；`OM_BUILD_TESTS` 开关控制 CxxTest 测试；`build.bat` 默认 MinGW Makefiles + 自动部署

## 与控制台版（2D）的差异

| 方面 | 控制台版（见 [WIKI.md](../WIKI.md)） | gamma（3D） |
|------|--------------------------------------|-------------|
| 维度 | 2D 平面 10001×1001 | 3D 体素（伪无限） |
| 渲染 | Windows 控制台宽字符 | bgfx + SDL2 窗口 |
| 怪物/Boss | 僵尸/骷髅/苦力怕/蜘蛛/僵尸猪灵、OI Boss、世界吞噬者、递归 Boss | **已实现 6 种生物**：猪/僵尸/羊/蜘蛛/苦力怕/僵尸猪灵，服务端权威游荡 AI + 自然生成 + 多部件外形 + 血量/受击闪红/HUD 血条/可击杀 |
| 流体 | 静态水/岩浆 + 分级流动、水+岩浆→黑曜石、桶取放 | **已移植 3D**：同规则（水 7 级/岩浆 3 级）+ 静置零开销 + 可游泳 + 可冲掉花草，见[流体系统](#流体系统) |
| 方块/物品 | 93 种（ID 0–92） | 95 方块 + 103 物品（Lua 定义，可热扩展） |
| 合成 | 背包内五页配方列表 | 2×2 背包格 + 工作台 3×3，图案匹配 |
| 农业与流通 | 无 | 耕地/下界疣/小麦种植、熔炉食物烹饪、矿物块压缩 |
| 存档 | v3 二进制 `2DMCBNK` 多线程增量 | OpenMiner 存档（`.omw` 世界目录，详见上游文档） |
| 联机 | 无 | TCP 客户端/服务端 |
| 模组 | 无（改源码） | Lua 模组 API + 事件监听 |

## 已知问题

以下问题均在当前源码/数据中实际存在：

- **不可用配方**：黏液块配方引用 `default:slime_ball`，但物品实际 ID 是 `slimeball`；纸配方引用 `default:sugar_cane`，该物品未定义（甘蔗方块 `reeds` 无对应物品）——两条配方均无法匹配
- **名称错误**：草丛 `tallgrass` 的显示名写作"草方块"，与真正的草方块 `grass` 重名
- **开发中内容**：种子（小麦）、火把、木门的名称自带"（开发中）"标记，火把不可自然获取
- **传送门无搭建逻辑**：黑曜石门框点燃机制未实现，但传送门方块本身已可合成（10 黑曜石）
- **食物无效果**：物品定义不含饥饿/回血字段，进食系统未实现
- **液体不可被常规挖掘**：除持空桶外射线不选中液体；移除流动水的办法是向其格内放置方块，桶详见[流体系统](#流体系统)
- **工具名称未中文化**：`tools.lua` 自动生成的名称是英文（Wooden Pickaxe 等），与其余中文物品名不一致
- **材质包系统部分实现**（继承上游 issue #34）

## 版本沿革

| 提交 | 内容 |
|------|------|
| `ccc4cb0` beta2.0 → `cecc1fe` gamma1.0 | 引入 gamma（OpenMiner fork 中文化 + 游戏目录 Minecraft/） |
| `4e74f3c` | 修复中文文本渲染模糊（CJK 字形度量整数量化，Font.cpp） |
| `b45fae7` / `2aa12f6` | 新增 `build.bat`：一键编译、自动初始化子模块、编译后自动部署到 `../Minecraft`（含 MinGW 运行时 DLL） |
| 2026.10 | **流体系统**：新增 `mods/default/fluids.lua`（水 7 级/岩浆 3 级流动、静置零开销、水+岩浆→黑曜石、桶取放流体）；引擎配套 4 处小改；26 项 Lua VM 直跑断言全通过 |
| 2026.10 | **真机自检插桩 + 游泳**：ServerApplication AUTOTEST 钩子（2DMC_AUTOTEST=1 触发，每 20 帧采样测试区，结果走引擎日志）；ClientPlayer 游泳物理（重力 ×0.25、按空格持续上浮、沉速限速）；水/岩浆及全部流体变体 `is_collidable = false`；真机自检确认流动 77 格距离=层级静置稳定 |
| 2026.10 | **第一批生物**：entities.lua 实现 `default:pig` + `default:zombie`（客户端伪游荡动画、挖方块/进服生成、会话上限、原版贴图 Pillow 重组）；实体 API 硬限制详见[生物](#生物)章节 |
| 2026.10 | **下界内容完整化**：下界金矿/远古残骸（下界荒地生成）、下界碎片/下界锭/下界镐�剑/斧（挖掘速度 12）、远古残骸熔炼、传送门合成配方（10 黑曜石）；8 张 32x32 贴图取自 Default-Java-26.2 |

## 致谢与许可

- [OpenMiner](https://github.com/Unarelith/OpenMiner)（Unarelith 及贡献者，LGPLv2.1+）——gamma 的引擎与默认模组内容来源，`mods/` 内所有 Lua 文件保留其版权头
- ThePhD（[sol2](https://github.com/ThePhD/sol2)）、skypjack（[EnTT](https://github.com/skypjack/entt)）、xMrVizzy（Faithful 32x 材质）
- 本项目对上游的修改同样以 LGPLv2.1+ 发布，见 [LICENSE](LICENSE)
