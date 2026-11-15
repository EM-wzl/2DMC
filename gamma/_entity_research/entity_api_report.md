# 实体引擎能力扩展报告（任务 C）

日期：2026-10-24 ｜ 分支：工作树（未 commit）｜ 构建目录：gamma/build（Release，MinGW）

## 0. 结论

三项能力（服务端逐实体 on_tick、Lua 写位置并同步客户端、Lua 枚举实体/玩家）已实现并通过服务端运行验证。位置同步**复用既有的 `EntityPosition (0x52)` 全链路**（服务端 NetworkController 序列化 + 客户端 addComponentCommandCallback），未新增网络命令——见 §2 的取舍说明。

## 1. 改动文件

| 文件 | 改动 |
|---|---|
| `common/scene/EntityCallbackContainer.hpp` | +`tickCallback`（sol::unsafe_function）、+`EntityTickTag` 组件（含 1 字节成员与空 packet 操作符，原因见 §5） |
| `common/scene/component/ComponentType.hpp` | +`EntityTick = 8` |
| `common/scene/Scene.cpp` | registerComponents 注册 EntityTickTag（不可序列化/存档） |
| `server/scene/controller/EntityTickController.{hpp,cpp}` | **新增**：逐实体 tick 控制器 |
| `server/scene/ServerScene.{hpp,cpp}` | 控制器顺序 tick→碰撞→网络；setServer/setWorld 传递 |
| `server/scene/EntityWrapper.{hpp,cpp}` | +type/get_position/set_position/valid |
| `server/lua/loader/LuaEntityLoader.cpp` | tryLoadCallbacks 加载 `on_tick`；spawnEntity 给带 on_tick 的实体打 EntityTickTag |
| `server/network/ServerCommandHandler.{hpp,cpp}` | +scriptEngine() 访问器；usertype +get_players |
| `server/world/ServerWorld.cpp` | ctor `m_scene.setWorld(this)`；usertype +get_entities/get_entities_by_type；TEMP 实体验证钩子（§6） |
| `server/CMakeLists.txt` | +EntityTickController.cpp |

未动：Network.hpp、客户端全部源码、ServerApplication.*、mods/、entities.lua、blocks.lua、fluids.lua、autotest.lua。

## 2. 网络包（重要取舍）

任务要求「新增 EntityPosUpdate 包」。摸底发现 **`Network::Command::EntityPosition = 0x52` 的端到端链路已存在但服务端从未再次触发**：

- 服务端：`NetworkController::update` 每 tick 遍历带 NetworkComponent 的实体，`serialize<PositionComponent>` 仅当 `isUpdated==true` 时发包，然后清零标志（Scene.cpp:74-88）——这就是「按 tick 节流、仅变化时发送」；
- 客户端：`ClientCommandHandler.cpp:378` 已注册 `EntityPosition` 回调，`get_or_emplace<PositionComponent>` + deserialize 应用，`RenderingController::draw` 直接用 position.x/y/z 渲染（渲染跟随零改动）；
- 包格式：`Command(0x52) + entt::entity id + f64 x + f64 y + f64 z + u16 dimension`，TCP，`sendToAllClients` 广播（多人可见）；新连入客户端由 `sendEntities` 全量补发。

因此 `set_position` 只写组件并置 `isUpdated=true`，同步自动发生。新增一条平行的 EntityPosUpdate 命令会与 0x52 完全重复，故未加（Network.hpp 无 diff）。

## 3. 新增 Lua API（签名）

```lua
-- mod:entity 定义块内（与 on_collision 并列）
on_tick = function(entity, pos, world, server) end
-- entity: EntityWrapper；pos: {x,y,z}（只读快照）；world: ServerWorld；server: ServerCommandHandler
-- 每 server tick（20tps）调用一次；回调抛错时 logError 并停掉该实体 tick（防刷屏）

-- EntityWrapper 新增
entity:get_type()                        -- -> "default:pig"（spawn 时的 string id）
entity:get_position()                    -- -> x, y, z（三个 double）
entity:set_position(x, y, z)             -- 服务端权威；置 isUpdated，下一 tick 广播 0x52
entity:valid()                           -- despawn 后缓存的 wrapper 是否仍指向存活实体
entity.id                                -- 既有
-- 注意：只支持同维度内移动（dimension 不变）；跨维度请 despawn + respawn

-- 枚举
server:get_players()                     -- -> 表：全部 ServerPlayer（含未就绪），下标 1 起
world:get_entities()                     -- -> 表：本维度全部网络实体 EntityWrapper
world:get_entities_by_type("default:pig")-- -> 表：按类型过滤
```

AI 选目标示例：`for _, p in ipairs(server:get_players()) do ... p.x/y/z/dimension() ... end`。

## 4. 验证记录

| 检查 | 层级 | 结果 |
|---|---|---|
| `build.bat` Release 构建+部署 | 编译/链接 | 通过（EXIT=0，中途 1 次链接失败为并行代理构建窗口撕裂，touch 头文件全量重编后消失，见 §7） |
| 逐实体 tick + set_position | **运行** | `2DMC_ENTITYTEST=1` 冒烟 14s：mover x 8.5→22，每秒 +1.0（0.05/tick×20tps），28 条 `[ENTITYTEST] mover pos` 日志，位置确实变化 |
| 位置包广播 | **运行** | gdb 断点采样 `Server::sendToAllClients` 调用栈：命中 `NetworkController::update` 组件序列化路径（即 mover 的 0x52 包）与 sendServerTick 路径交替出现 |
| 服务器冒烟（默认关闭钩子） | **运行** | Minecraft 目录 12s，无 `[E]` 行，`[ENTITYTEST]` 无输出（env 门控生效） |
| 客户端应用/渲染跟随 | 检视 | 未改客户端；`EntityPosition` 应用与渲染链路是 mobs 初始位置同步已在用的既有代码（entities.lua 的猪/僵尸客户端渲染正确即其佐证） |

复跑命令：`cd Minecraft && openminer_server.exe --port 4302`（先 `set 2DMC_ENTITYTEST=1`，注意 PS 需 `[Environment]::SetEnvironmentVariable`）；日志 grep `ENTITYTEST|\[E\]`。

## 5. 性能注意

- 无 on_tick 的实体（item_drop 等）**零 per-tick 开销**：EntityTickController 的 view 只遍历带 EntityTickTag 的实体；打标发生在 spawn 时（一次 map 查找）。
- 带 on_tick 实体的每 tick 成本 = 1 个 view 遍历 + 每 {x,y,z} 一个 Lua 表分配 + 1 次 sol::unsafe_function 调用。几十个实体量级无压力；上百个常驻 AI 实体时建议引擎侧再加每 N tick 的节流参数。
- 位置包仅在 set_position 后的下一 tick 发送一次（isUpdated 节流），静止实体零流量。
- EntityTickController 先收集实体再回调：on_tick 内 spawn/despawn 不会使 entt 迭代器失效；回调错误自动摘除该实体 tick 标记。

## 6. TEMP 验证钩子（默认关闭，移除方法）

- `ServerWorld.cpp::update` 内标注 TEMP 的 env 门控块 + 文件顶部 `runEntityTestHook`（内嵌 Lua：注册 `2dmctest:mover` 模型与 on_tick、spawn 到 (8.5,8.5,地表+2)）；
- `EntityTickController.cpp::update` 内标注 TEMP 的每秒位置日志块。
- 移除：删除两处 TEMP 标注块即可（钩子不触碰 ServerApplication 的 `2DMC_AUTOTEST` 流体钩子）。

## 7. 已知问题与剩余缺口

**非我引入、需父 agent 知悉**：构建期间并行代理修改了 `TerrainGenerator.hpp`（18:22:42，与我的 ServerWorld.cpp.obj 同秒编译），导致一轮「撕裂构建」（ServerScene this 指针偏移 0x28 错位，gdb 定位）——touch 头文件全量重编后消失。**建议并行代理的改动合流后再全量重建一次。**

**功能缺口（按优先级）**：
1. **生命值/伤害**：无 HealthComponent、无 EntityDamage 包、无 on_damage 回调——近战/投射物伤害不可行；
2. **存档重载不打 tick 标**：EntityTickTag is_savable=false，存档里的 on_tick 实体重载后停止 tick（WorldSaveBasicBackend::loadEntities 不在本任务文件范围）；建议 loadEntities 里按类型回调容器补打标，或 Lua 侧重 spawn；
3. **跨维度移动**：set_position 不改 dimension，实体不能跨维度迁移；
4. **客户端插值**：0x52 直接覆写位置，高速移动会视觉跳变；客户端 translation 动画与服务端位置互相覆写（AnimationController 直改 position.x/y/z）——给 AI 实体加动画时需注意；
5. **实体 AI 所需的其他钩子**：on_spawn/on_despawn、实体间碰撞、玩家输入事件作用于实体、方块 on_tick 与实体无交互通道；
6. docs/lua-api-cpp.md 与 docs/network-protocol.md 未更新（不在本任务文件范围），代码内「Please update docs」注释所指内容以本报告为准。
