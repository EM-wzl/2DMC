# gamma 生物系统（第一批：pig / zombie）实现报告

日期：2026-10-24　范围：仅 gamma/mods/default/（entities.lua 新增 + init.lua 末尾一行）与贴图文件，未动任何 C++。

## 一、实体 API 能力边界（源码检视结论）

已支持的：
- `mod:entity` 定义：`visual` 仅支持 `inventorycube`（LuaEntityLoader.cpp::tryLoadVisual，其他类型报 "unknown"；通过 `block_id` 引用一个已注册方块，按其六面贴图 + boundingBox 造型渲染，InventoryCube.cpp）。`is_rotatable` 必须为 true，否则客户端 RenderingController 的 view 缺 RotationComponent 不绘制。`animation` 支持 rotation（绕轴逐帧累积旋转）与 translation（客户端逐帧累加本地位置，min/max 往返）。`hitbox` 为 `{x,y,z,sizeX,sizeY,sizeZ}`（相对实体位置的偏移+尺寸）。
- 回调仅 `on_collision(entity, player, server)`：玩家 hitbox 与实体相交期间每 tick 触发（CollisionController.cpp）。
- 生成：`mods[..]:spawn_entity(id, {position, dimension[, item_stack]})`（position+dimension 必填）；despawn 仅 on_collision 内 `mod:despawn_entity(entity)`。
- 持久化：带 NetworkComponent 的实体随存档整体保存/重载（WorldSaveBasicBackend.cpp，Position/Rotation/Animation/Drawable/Hitbox/EntityID 均 savable）。
- 周期性驱动：方块 `on_tick(pos, block, chunk, world, server)`（tick_randomly=false 时每 server tick）；事件监听仅 BlockPlaced/BlockDigged/BlockActivated/ItemActivated/PlayerConnected（LuaCore.cpp）。
- 无生命值/伤害系统（全源码 `\bhealth\b|\bdamage\b` 零匹配）。

不支持的（引擎缺口，本次未改 C++）：
1. Lua 不能写实体位置：PositionComponent 无 sol usertype（EntityWrapper 只暴露 id/position/network/item_stack，position 指向未注册类型）；不能枚举实体；无 per-entity tick/on_spawn/on_despawn → **真实移动/追逐 AI、距离 despawn 均不可行**。所需引擎改造：① 注册 PositionComponent usertype（可写 x/y/z + isUpdated）；② 增加 per-entity Lua tick（如 `on_update(entity, server)` 由服务端 scene 驱动）；③ 提供 `world:entities()` 枚举。
2. visual 单一（一个 cube，无 box 模型/sprite），同类实体动画同相位。
3. PlayerConnected 监听器无 world 对象，无法做表面检测。

## 二、做了什么

- `gamma/mods/default/entities.lua`（新增，唯一逻辑改动）：
  - 两个技术方块 `default:pig_block` / `default:zombie_block`（inventorycube 需要引用方块贴图；六面 tiles 顺序 Top,Bottom,East,West,South,North；脸放南/北面）。不参与放置，不进存档方块 ID 映射；注册顺序在 fluids.lua 之后，数字 ID 纯追加，旧存档兼容。
  - `default:pig`（size 0.75 贴地居中，hitbox 同步）与 `default:zombie`（0.9）。AI 上限=客户端动画伪游荡：pig 水平踱步 ±0.5 格 + 起伏，zombie 另一轴踱步 + 绕 Z 慢转（0.08°/帧）。服务端位置不动。
  - on_collision 限频聊天：pig "Oink oink!"（~2s 一次）、zombie "grabs you!"（无伤害可施加，文案已注明）。
  - 生成：BlockDigged 概率触发（pig 12%、zombie 6%），带表面搜索（固体+上方两格空气）与草方块检测（`world:get_block_def(...):string_id()=="default:grass"`；注意 openminer.registry 在加载期为 nil，服务端 loadMods 后才 setRegistry——首个版本在此踩坑，冒烟测试抓到后改为运行时比对）；PlayerConnected 在玩家周围生成 2 猪 1 僵尸（无 world 对象，只能生成在脚底水平面）。会话内计数上限 pig 12 / zombie 8（mod:option 可配，default:mobs_enabled 开关）。
- `gamma/mods/default/init.lua`：末尾仅追加一行 `dofile("entities.lua")`。
- 贴图：`gamma/_mobs_research/make_mob_textures.py`（Pillow）从 `Default-Java-26.2/.../entity/pig/pig_temperate.png`（64x64，经典箱式 UV，像素采样验证）与 `entity/zombie/zombie.png` 裁剪 脸/身侧/背/腹/腿 区域，居中方形画布 + 主色填充，NEAREST 整数倍放大到 32x32（图集 tile 尺寸=undefined.png=32，TextureAtlas 要求全图集同尺寸正方形）。成品写入 `gamma/mods/default/textures/blocks/`（引擎只从该目录加载方块贴图——这是任务指定 entities/ 路径之外必须补的事实约束）；`textures/entities/` 按任务要求存副本；两处均已同步到 `Minecraft/mods/default/`。预览拼图 `gamma/_mobs_research/mob_tiles_preview.png` 已目检（猪脸/僵尸脸清晰可辨）。
- 回退清单：删除 entities.lua、init.lua 末行、上述 8+8 张贴图与两个脚本/报告文件即可完全还原；无 C++/git 改动。

## 三、验证了什么

- 构建：`build.bat` 全目标 100% 通过（Lua 改动无需重链，属例行验证）。服务端 exe 部署到 Minecraft\ 被并行工程师运行中的实例（PID 1220）占用而失败，未重试覆盖——不影响本次 Lua 验证，后续部署需等其退出。
- 冒烟（层级：集成/真实入口）：因 Minecraft\ 端口与日志被运行中实例占用，在 `_mobs_smoke/` 沙箱（复制 Minecraft\mods）用最新 `gamma/build/openminer_server.exe --port 4299` 跑 12 秒后杀掉。第一轮抓到 `entities.lua:126 attempt to index 'registry' (nil)`（见上，已修复）；修复后第二轮日志零错误、`Mod "default" loaded`、运行 12 秒存活（存档 `gamma/_mobs_research/smoke_test_log.txt`，可复跑：复制 Minecraft\mods 至沙箱目录，在其中运行 `gamma\build\openminer_server.exe --port 4299`，12 秒后杀进程查 `logs\openminer_server.log`）。
- 检视层：hitbox/visual/animation 各字段与 LuaEntityLoader、TilesDef::getTextureForFace、Box 构造逐一比对；`default:grass` 字符串 ID 与 blocks.lua 核对；pig_block/zombie_block 与 items.lua/blocks.lua/creative_inventory mod 无 ID 冲突。

## 四、没验证什么

- 运行时路径未跑：无客户端连接，PlayerConnected/BlockDigged 生成、on_collision 聊天、客户端渲染与动画均未实际触发——只验证到"加载零错误"这一层。建议有客户端环境时人工验证：连接服务器应见 3 只生物、挖草方块偶发生成、撞猪出 Oink。若 on_collision/生成报错会进同一日志。
- 客户端图集加载（贴图被 atlas 打包、实体渲染成猪/僵尸方块脸）未在真实客户端确认。
- 长期累积行为：实体持久化 + 会话计数重启清零 → 多次重启会话每会话最多再补 15 只；无距离 despawn 手段（引擎缺口），长期人口只增不减，需接受或做引擎改造。
- math.random 未播种（无 os 库），每次服务器运行生成序列相同，属轻微已知问题。
