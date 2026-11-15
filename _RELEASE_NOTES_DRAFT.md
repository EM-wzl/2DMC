# 2DMC / gamma — 发布说明（草稿）

> 版本：v0.9.0「末地与生物」
> 日期：2026-11-14
> 仓库：https://github.com/EM-wzl/2DMC

---

## 本次发布概要

本版本把 gamma（OpenMiner 3D 分支）从一个只有主世界与下界的原型，扩展为具备**三个维度**、**19 种生物**、**多类世界生成结构**、**完整掉落系统**、**生物外观升级**、**局域网开放**、**玩家名系统**与**大量稳定性修复**的可玩版本。

同时修复了一个会导致「一进世界就退出」的致命问题，以及多处会在畸形输入/损坏存档下让服务端崩溃的边界缺陷。

---

## ✨ 新功能

### 维度与传送门

- **新增末地维度**（`default:end`，id=2）：`sky_end` 暗紫天空、`end_barrens` / `end_highlands` 两个群系、末地石/紫珀块/末地石砖/末地水晶/紫颂植株等方块与配套配方。
- 维度顺序固定为 `overworld(0) → nether(1) → end(2)`。
- **Minecraft 式双向传送门**（不再是维度循环）：
  - 主世界 `default:portal` → 下界，并在下界生成 `default:nether_portal`
  - 下界 `default:nether_portal` → 主世界
  - 末地 `default:end_portal` → 主世界
  - 传送时会在目标维度搜索已有的返回门，找不到才新建（4×5 门框、内部 2×3 传送门方块、黑曜石站台），并广播 `BlockUpdate`。

### 生物系统

- **19 种生物**：从 6 种扩展到 19 种（新增牛、鸡、骷髅、狼、史莱姆、鱿鱼、豹猫、兔子、美角、雪傀儡、蝙蝠、恶魂、僵尸猪人等）。
- **按部件拆分贴图**：每种生物注册 `<mob>_body` / `<mob>_head` / `<mob>_leg` 三个方块，头部的 South/North 面使用脸部贴图，一眼能看出朝向；贴图取自 MC 原版 64×32/64×64 实体图集，裁切为 16×16 后按引擎要求统一为 32×32。
- **生成规则改进**：hitbox 高度净空检查、24 格玩家排除半径（采样距离同步调整到 26~48 格，避免采样点全落在排除圈内）、种类按轮次轮换、昼夜加权（白天偏被动、夜晚偏敌对）、鱿鱼要求脚下有水。
- **确定性随机**：实现精确的 32 位 LCG（LuaJIT `bit` 库分段乘法），替代未播种的 `math.random`；`slime_chunk()` 判定实测约 9.86% 区块为史莱姆区块。
- **生物掉落**：新增 `Event.EntityKilled` 事件与 `server:kill_entity(entity)` API，`mods/default/mob_drops.lua` 按表掉落 16 类物品（生猪排/牛肉/皮革/骨头/箭/火药/线/蜘蛛眼/黏液球/腐肉/雪球…），玩家走过即拾取。
- **底层能力**：`world:get_ticks()` / `is_daytime()` / `get_light(x,y,z)` / `path_clear(...)` 四个 Lua API（视线检测用 `EntityPath.hpp` 的 `clearPath`，每格 8 步采样 + 真实 boundingBox 判定）。

### 世界生成结构

`TerrainGenerator::generate()` 现在按顺序生成：

| 结构 | 维度 | 说明 |
|------|------|------|
| 村庄 | 主世界 | 房屋 + 水井 + 道路（160 格一个） |
| 下界地形装饰 | 下界 | 岩浆湖 / 玄武岩柱 / 萤石簇 / 灵魂沙斑块 |
| 下界隧道 + 大型岩浆湖 | 下界 | 确定性「蠕虫路径」哈希，O(路径长度) |
| 下界要塞 | 下界 | 桥廊 + 7×7 塔楼 + 支撑柱（96 格一个） |
| 废弃矿井 | 全维度 | 既有实现 |
| 末地城 + 折跃门 | 末地 | 塔楼 + 黑曜石框架（128 / 256 格） |

所有结构均为确定性（世界坐标 hash）、只写本 chunk、非目标维度空操作。

### 玩家与联机

- **标题页玩家名输入框**：预填 `Config::defaultUsername`，单人与多人共用；服务端按此创建角色，`PlayerSpawn` 广播，HUD 在其他玩家头顶显示名字。
- **开放局域网**：`/publish`（别名 `/openlan` / `/lan`）输出本机全部 IPv4 + 端口；ESC 菜单「开放局域网」页可选端口并展示 `IP:端口`；`LanInfoRequest/LanInfo` 协议仅限单机主机查询。
- `ServerConnectState` 支持 `IP:端口` 解析，默认端口 4242。
- 服务端单机模式内嵌监听改为接受局域网外部连接，修正 `isSingleplayer` 标志只对回环主机为 true。

### 内容与工具

- **新方块/物品/配方**：末地系列、生物部件、维度传送门、`gunpowder`（火药）、紫珀/末地石砖/末地水晶/紫颂果等配方。
- **存档压缩**：`OMZ1` → `OMZ2`（zlib + 魔数 + 维度数），586→190 字节（32.4%）；兼容 OMZ1 与无魔数裸格式。
- **配置优先级修复**：`ServerConfig::userOptions` 区分「玩家显式设置」与「mod 默认值」，玩家改 `config/server.lua` 或使用 `/option` 后不再被 mod 默认值覆盖，也不会把陈旧默认值写回文件。
- **构建/部署**：`build.bat` 部署前把旧 exe 改名再复制（不再杀进程、不再静默失败）；`tools/sync_assets.ps1` 一键同步 gamma↔Minecraft↔texturepack 并校验贴图覆盖与尺寸一致。
- **文档**：`WIKI.md` 大幅扩充；新增 `tools/check_textures.py`、`tools/unify_texture_size.py`、`tools/mob_textures_src/`（MC 原版实体图集备份）。

---

## 🐛 关键修复

### 致命：一进世界就退出

- **根因**：客户端 `TextureAtlas::loadFromRegistry` 要求图集内所有贴图尺寸一致；新裁切的生物贴图是 16×16，与原有 32×32 混用，抛 `Fatal error at TextureAtlas.cpp:127` → 进世界加载贴图时客户端直接退出。
- **修复**：所有方块/物品贴图统一为 32×32（含 `debug/` 子目录），`tools/unify_texture_size.py` 固化该检查并集成进 `sync_assets.ps1`。

### 网络 / 协议健壮性

- `CompressedPacket::onReceive`：补 `size < 2` 边界检查、16 MiB 解压上限；解压失败时 `clear()+return`，不再把「半解压/未初始化」缓冲区当有效包 append（原来会让后续协议解析错位甚至越界）。
- 修正 zlib `Z_BUF_ERROR(-5)` 误报：输出缓冲按包头精确分配、刚好填满属正常，`rc==-5 && dstSize==uncompressedSize` 视为成功。
- `sendPlayerChangeDimension` / `get_world` / `killEntity` 增加维度范围校验，避免 deque 下标越界 UB。

### 存档健壮性

- `WorldSaveBasicBackend::load`：加 `endOfPacket()` 检查、数量上限、变量初始化、方块 ID 范围检查；损坏/截断存档不再让服务端以 `0xC0000409` 崩溃，而是记录错误后以空世界继续运行。
- `ServerApplication::init()` 对 `m_worldController.load()` 加 try/catch 兜底。
- OMZ2 头部写入维度数，旧格式（OMZ1 / 裸格式）固定按 2 维度读取，多余维度跳过，避免把玩家列表误当 `chunkCount`。

### 联机 / 服务端

- `handleClientMessages` 不再对 `ClientDisconnect` 执行两次回调；`disconnectClient` 用 `find` 替代 `operator[]` 避免遍历 `m_commands` 时 rehash。
- `disconnectAllClients` 改用 `while + front()`，避免 range-for 中 erase 导致迭代器失效。
- 修正 `PlayerSpawn` 首字段误发 `std::string`（应为 `u16 clientID`）、`PlayerAttackEntity` 用网络 ID 反查实体、`heldItemStack()` 槽位越界崩溃。
- 非阻塞挂起握手队列（5 秒超时），端口扫描器/半开连接不再卡住服务端。

### UI

- 标题页重排为「标题 → 玩家名输入框 → 菜单」三个互不重叠的竖直区间，全部用实测尺寸计算；输入框尺寸固定像素并抵消 `guiScale` 缩放；内置布局自检日志，回归时告警。

### 生物 / 渲染

- 生物死亡时 `EntityDespawn` 同步清理客户端 `m_entityMap`（entt 句柄复用问题）。
- HP 不再锁在 1；`EntityWrapper::damage` 支持降到 0 并触发服务端 despawn。
- 生物部件几何重写，相邻部件 z 区间严格相接、总高不超过 hitbox。
- `RenderingController` 支持多部件 `DrawableDef`，不再只显示一块。
- 关闭生物功能时保留全部 AI 代码，仅用 `mobs_enabled=false` 开关。

---

## ✅ 验证

| 项 | 结果 |
|----|------|
| `gamma\build.bat Release` | EXIT=0 |
| `ctest` | 11/11 通过（33 断言） |
| 存档矩阵 | OMZ1 / 裸格式 / OMZ2 / 3 个真实存档 / 损坏档，6/6 存活、零崩溃 |
| 多人联机 | 双客户端互相可见、断线各只触发 1 次 offline、零错误 |
| `/option` 持久化 | 正确写回 `config/server.lua` |
| 客户端进世界 | `--singleplayer --shot` 退出码 0、日志零 `[E]`、画面正常 |
| 贴图 | blocks 294 / items 136 张，全部 32×32，引用全覆盖 |
| 部署 | server/client MD5 一致 |

> 全程遵守「禁止 GUI/键鼠自动化实测」约束（除用户明确授权的一次标题页实测外）：验证通过无头 TCP 客户端、gdb、协议断言、单元测效、日志与静态检查完成。

---

## ⚠️ 已知事项 / 使用说明

1. **材质包需自备**：客户端在 `texturePack = "minecraft"` 时只从 `Minecraft/texturepacks/minecraft/{blocks,items,gui}/` 读取贴图。该目录为第三方材质包（Faithful 32x 等），**未纳入版本控制**（`.gitignore` 忽略 `*.png`）。clone 后请把材质包放到该目录，并运行 `tools/sync_assets.ps1` 校验覆盖与尺寸（缺一张或尺寸不一致都会导致进世界失败）。
2. `tools/` 下的校验脚本需要 Python + Pillow。
3. 生物默认关闭（`mobs_enabled=false`）；在 `config/server.lua` 改成 `true` 或用 `/option default:mobs_enabled true` 开启。
4. 单人主机断开即停服（保证存档保存），局域网访客断开不影响。
5. `2DMC_MINESHAFT_CHECK=1` 环境变量会启用矿井生成验证日志，仅调试用。

---

## 📦 变更规模

- 125 个文件改动，约 +10,294 / −2,917 行（不含已暂存的早期变更）
- 4 路独立智能体并行复查（构建/C++、Lua/内容、运行时/协议、发布安全）

---

## 致谢 / 来源

- 上游：[Unarelith/OpenMiner](https://github.com/Unarelith/OpenMiner)（LGPL-2.1）
- 生物贴图与实现参考：[void717doll/OpenMiner](https://github.com/void717doll/OpenMiner) fork
- 材质：Faithful 32x（https://www.curseforge.com/minecraft/texture-packs/faithful-32x）
