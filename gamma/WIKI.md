# gamma Wiki

> 本 Wiki 基于 `gamma/` 目录的源码（OpenMiner fork）与 `mods/default`、`mods/creative_inventory` 的实际 Lua 数据逐文件核对编写（2026-10-18）。与控制台 2D 版的 [WIKI.md](../WIKI.md) 对应，本文档覆盖 gamma 3D 版的世界、物品、配方、命令、模组与联机系统。

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

`dimensions.lua` 定义 `nether`：下界荒地群系（下界岩地表/下层、灵魂沙滩、岩浆液体）、暗红天空（无天体）、**重力 1.4**（下落更快）。矿物：下界金矿（flood_fill p=0.004、矿脉 4，掉 3 金粒）、远古残骸（random_walk p=0.0005，熔炼下界碎片 → 下界锭 → 升级钻石镐/剑/斧为下界工具，挖掘速度 12 全游戏最高）。进入方式：合成传送门方块（10 黑曜石）放置后右键。

| 维度 | ID | 生物群系 | 天空 | 特性 |
|------|----|----------|------|------|
| 主世界 | `overworld` | 草原、沙漠 | 蓝天 + 太阳/月亮/星星 | 标准重力 |
| 下界 | `nether` | 下界荒地 | 暗红色天空（无天体） | **重力 1.4**（下落更快） |

**传送门**（`default:portal`）：对手持方块右键激活即切换维度（`(dimension + 1) % 2`，即在主世界与下界之间往返），并传送到同层对应坐标，聊天栏提示 "Swoosh! Changing dimension..."。**传送门方块可合成：10 黑曜石 → 1 个**（控制台版规则）；黑曜石门框点燃机制仍未实现。

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
| 下界荒地 | `netherland` | -0.3 / -1 | 下界岩 / 下界岩，沙滩灵魂沙，液体岩浆 | — | — | — |

生物群系同时定义 `portal = default:portal`、`portal_frame = default:obsidian`（传送门框架方块目前仅为数据定义，尚无搭门点燃逻辑）。

### 树木（trees.lua）

| 树 | 树干 | 树叶 | 树干高度 |
|----|------|------|---------|
| 橡树 `oak` | 橡木原木 | 橡木树叶 | 3–6 |
| 仙人掌 `tree_cactus` | 仙人掌 | 无 | 3–4 |

## 方块列表

共 **97 个方块**：`blocks.lua` 定义 71 个（含下界金矿、远古残骸），加上 `blocks/workbench.lua`、`blocks/furnace.lua`、`blocks/door.lua`（上下两半），以及流体系统（fluids.lua）的 22 个内部方块。名称即游戏内显示（default 模组已中文化）。

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

### 功能与特殊方块

| 方块 | ID | 硬度 | 采集 | 特性 |
|------|----|------|------|------|
| 工作台 | `workbench` | 1 | 斧 | 右键打开 3×3 合成界面，详见[交互方块](#交互方块与工具行为) |
| 熔炉 | `furnace` | 2 | 镐 | 可旋转；点燃后为光源，详见[交互方块](#交互方块与工具行为) |
| 木门（开发中） | `door_wood_lower` / `door_wood_upper` | — | — | 双方块结构，右键开合，自动放置上半 |
| 传送门 | `portal` | — | — | 玻璃渲染；右键切换主世界/下界 |
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

共 **110 个物品**：`items.lua` 定义 79 个 + 工具 30 个（6 材料 × 5 类）+ 剪刀。物品定义不含食物回复/攻击力数值（引擎暂未使用这些字段）。

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
- 共 **81 条合成配方 + 21 条熔炼配方**（`recipes.lua`）

### 合成配方（76 条）

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

### 熔炉

熔炉 3 个槽位：输入（0）、输出（1，只读）、燃料（2，仅接受 `group:om_fuel`）。熔炼进度 **200 tick**/件；点燃时熔炉切换为发光状态。

**燃料**（燃烧值 = 可供 tick 数）：

| 燃料 | 燃烧值 |
|------|--------|
| 煤炭 `coal` | 1600 |
| 木炭 `charcoal` | 1600 |

即 1 煤可熔炼 8 件（1600 / 200）。

**熔炼配方（20 条）**：

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

## 交互方块与工具行为

| 对象 | 触发 | 行为 |
|------|------|------|
| 工作台 | 右键 | 打开 3×3 合成 GUI（放置时创建 3×3 方块数据）；产物 Shift 点击进背包 |
| 熔炉 | 右键 | 打开熔炼 GUI（输入/输出/燃料 + 两根进度条：熔炼进度 200 tick、剩余燃烧时间） |
| 传送门 | 右键 | 切换主世界 ↔ 下界，同坐标传送 |
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

### 游泳

水/岩浆方块（含全部流体变体）已取消碰撞（`is_collidable = false`），玩家可以进入水中：

- **入水判定**：脚部或身体中部处于液体方块（`draw_type = liquid`）即视为在水中
- **游泳物理**（`ClientPlayer::updatePosition`）：水中重力 ×0.25 缓慢下沉，下沉速度上限为跳跃速度 ×0.4；按住空格键持续上浮（不受跳跃门控，上浮速度为跳跃速度 ×0.5）
- **水下雾效**：相机进入液体格时自动生效（水蓝色/岩浆橙红，来自方块定义的 fog_depth/fog_color）
- 真机验证：真机自检证明流动逻辑在取消碰撞后不受影响（77 格分布不变）

## 生物

第一批生物由 `mods/default/entities.lua` 实现（贴图取自原版资源包 Default-Java-26.2，Pillow 裁切重组为引擎图集格式）：

| 生物 | ID | 行为 | 生成 |
|------|----|------|------|
| 猪 | `default:pig` | 客户端伪游荡动画（水平踱步+起伏） | 挖草方块概率生成 + 玩家进服时在周围生成，会话上限 12 |
| 僵尸 | `default:zombie` | 踱步+绕竖直轴慢转 | 同上，会话上限 8 |

**当前引擎硬限制**（受 OpenMiner 实体 API 能力约束，详见 gamma/_mobs_research/mobs_report.md）：

- 无生命值/伤害系统 → 生物不可被击杀、无掉落物
- Lua 无法写入实体位置、无法枚举实体、无逐实体 tick → 真实游荡/追逐 AI 与距离 despawn 需引擎改造（per-entity tick + 位置写入 + 实体枚举）
- 生物会话内只增不减（有上限），随存档持久化
- 生物的"游荡"是客户端动画表现，服务端位置静态

## 掉落物系统

- 默认（`use_item_drops = false`）：挖掘方块直接把掉落物塞进玩家背包
- 开启 `use_item_drops`（服务器 `config/server.lua` 的 `mod_options`，或 `/option default:use_item_drops true`）：掉落物以 `default:item_drop` 实体生成——0.25 大小的旋转小方块，带上下浮动动画，0.25 碰撞箱；玩家接触后自动拾取并消失

## 聊天命令

服务端 `ChatCommandHandler` 实现（<kbd>T</kbd> 聊天，<kbd>小键盘 /</kbd> 直接进入命令模式）。输入未知命令时会回显可用命令列表：

| 命令 | 用法 | 说明 |
|------|------|------|
| `/help` | `/help` | 列出全部命令 |
| `/option` | `/option <name> [<value>]` | 查询/设置服务器选项（如 `default:use_item_drops`） |
| `/time` | `/time [set\|add] <value>` | 支持别名 `day`/`sunrise`(1000)、`noon`(6000)、`sunset`(11000)、`night`(13000)、`midnight`(18000)、`HH:MM` 时间格式与纯 tick 数 |
| `/tp` | `/tp x y z` | 传送到指定坐标 |
| `/tps` | `/tps` | 查看服务器 TPS |
| `/stop` | `/stop` | 关闭服务器 |

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
| `defaultUsername` | "" | 玩家名 |
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

- 纯 **TCP** 协议，默认端口 **4242**
- 登录握手：`ClientConnect`（用户名）→ `ClientOk`（分配 ClientID + 是否单人）/ `ClientRefused`
- 每个 tick 服务端广播 `ServerTick`（携带当前时间 u64）
- 完整包格式见 `docs/network-protocol.md`（含方块更新、玩家位置、方块数据、GUI、聊天等全部包定义）

## Lua 模组 API

文档源在 `docs/`（MkDocs），本地预览：

```bash
pip install -r docs/requirements.txt
mkdocs serve
```

覆盖范围：

- **Core API**：`openminer` 全局对象（注册表、模组加载器、配置读取）
- **Mod API**：`mod:block` / `mod:item` / `mod:crafting_recipe` / `mod:smelting_recipe` / `mod:biome` / `mod:dimension` / `mod:sky` / `mod:tree` / `mod:entity` / `mod:key` / `mod:option` 及事件监听 `openminer:add_listener(Event.*, ...)`
- **GUI API**：`LuaGUI` 的 image / inventory / inventory_data / crafting / progress_bar / scroll_bar / text_input 控件
- **C++ 类绑定**：ItemStack、World、Player 等在 Lua 中的方法（如 `world:set_block`、`player:dimension()`、`mods["default"]:give_item_stack`）

内置模组本身就是 API 的最佳示例（如红石灯状态切换、下界疣随机刻生长、熔炉 on_tick、掉落物实体 on_collision）。

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
- **着色器**：`resources/shaders/`（bgfx：basic/game/screen/skybox 的 .vs/.fs/.def）
- **中文渲染修复**：`source/client/gui/Font.cpp` 对 CJK 字形度量做整数量化（commit `4e74f3c`），解决中文发虚
- **世界生成**：按生物群系 `blocks` + `res`（矿物 gen_type：`flood_fill` / `random_walk`）+ `trees` + `flora` 生成；矿石目前完全按生物群系定义（源码注释中已规划改为按 worldtype）
- **编译**：CMake ≥ 3.12；`OM_BUILD_TESTS` 开关控制 CxxTest 测试；`build.bat` 默认 MinGW Makefiles + 自动部署

## 与控制台版（2D）的差异

| 方面 | 控制台版（见 [WIKI.md](../WIKI.md)） | gamma（3D） |
|------|--------------------------------------|-------------|
| 维度 | 2D 平面 10001×1001 | 3D 体素（伪无限） |
| 渲染 | Windows 控制台宽字符 | bgfx + SDL2 窗口 |
| 怪物/Boss | 僵尸/骷髅/苦力怕/蜘蛛/僵尸猪灵、OI Boss、世界吞噬者、递归 Boss | **猪+僵尸已实现**（伪游荡、可生成；击杀/追逐需引擎实体系统改造） |
| 流体 | 静态水/岩浆 + 分级流动、水+岩浆→黑曜石、桶取放 | **已移植 3D**：同规则（水 7 级/岩浆 3 级）+ 静置零开销 + 可游泳，见[流体系统](#流体系统) |
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
- **无 /give 命令**：服务端命令不含 `/give`，测试物品需借助创造模式物品栏（<kbd>H</kbd>）
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
