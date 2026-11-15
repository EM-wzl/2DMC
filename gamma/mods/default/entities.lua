--
-- =====================================================================================
--  entities.lua — 生物系统（pig / zombie / sheep / spider / creeper / zombified_piglin）
--
--  【当前状态：生物功能默认关闭（mobs_enabled = false）】
--  关闭时：不自然生成、不驱动 AI、不触发碰撞消息，并在 ServerTick / PlayerConnected
--  里清理已存在的生物实体（旧存档里的生物、/summon 产生的生物），实现"隐藏生物"。
--  所有 AI / 生成代码全部保留，把下面的 mod:option("mobs_enabled", false) 改回 true
--  即可整体恢复生物功能。
--
--  服务端权威 AI：ServerTick 每 tick 驱动（存档重载后即使没有恢复 on_tick 标记，
--  已持久化实体仍会继续行动）；实体自己的 on_tick 保留为兼容入口。
--  自然生成：ServerTick 每 3 秒、PlayerConnected 以及原有挖掘概率共同补充。
--  主世界与下界分别计数和限制类型；所有生成都检查真实地表、脚部空间和玩家间距。
--  生物只由真实实体数量计数，不按距离 despawn，存档重载后仍持续存在。
-- =====================================================================================
--

-- ---------------------------------------------------------------------------
-- 1. 技术方块：inventorycube visual 需要引用一个已注册方块来获得六面贴图。
--    tiles 六元素与面的对应（TilesDef::getTextureForFace，size=6 行）：
--    {Top, Bottom, East, West, South, North} —— 脸部贴图放南/北面。
--    这些方块不会被放置到世界中（不参与存档方块 ID 映射，无兼容性影响）。
--
--    每种生物拆成 body / head / leg 三个方块（见 1.5 节说明）：
--      <mob>_body：躯干 —— 顶/底用 _top/_bottom，四侧用 _side
--      <mob>_head：头   —— 顶/底/四侧同上，但南/北面换成 _face（正面朝 -y 旋转轴）
--      <mob>_leg ：腿   —— 顶/底用 _top/_bottom，四侧用 _side
--    三个方块贴图集完全相同、仅脸部位置不同；这样头部一眼能看出朝向。
-- ---------------------------------------------------------------------------
local function mob_part_block(id, label, face, side, top, bottom)
	mod:block {
		id = id,
		name = label,
		tiles = {top, bottom, side, side, face, face},
	}
end

-- 统一登记一种生物的三件套贴图方块
local function mob_blocks(mob, label, face, side, top, bottom)
	mob_part_block(mob .. "_body", label .. " Body", side, side, top, bottom)
	mob_part_block(mob .. "_head", label .. " Head", face, side, top, bottom)
	mob_part_block(mob .. "_leg",  label .. " Leg",  side, side, top, bottom)
end

-- 兼容旧引用：这些名字在旧版本里被 pack / 存档外逻辑引用过，保留为"整只生物一块"的别名。
-- 它们与新三件套使用同一套贴图，保证老代码/旧存档里出现的 *_block 仍然能解析。
local function legacy_block(id, label, face, side, top, bottom)
	mod:block {
		id = id,
		name = label,
		tiles = {top, bottom, side, side, face, face},
	}
end

-- === 已有 6 种生物（保留 *_block 兼容名 + 新增 body/head/leg 三件套）===
legacy_block("pig_block",         "Pig",               "pig_face.png",     "pig_side.png",     "pig_top.png",     "pig_bottom.png")
legacy_block("zombie_block",      "Zombie",            "zombie_face.png",  "zombie_side.png",  "zombie_top.png",  "zombie_bottom.png")
legacy_block("sheep_block",       "Sheep",             "sheep_face.png",   "sheep_side.png",   "sheep_top.png",   "sheep_bottom.png")
legacy_block("spider_block",      "Spider",            "spider_face.png",  "spider_side.png",  "spider_top.png",  "spider_bottom.png")
legacy_block("creeper_block",     "Creeper",           "creeper_face.png", "creeper_side.png", "creeper_top.png", "creeper_bottom.png")
legacy_block("zpiglin_block",     "Zombified Piglin",  "zpiglin_face.png", "zpiglin_side.png", "zpiglin_top.png", "zpiglin_bottom.png")

mob_blocks("pig",         "Pig",              "pig_face.png",      "pig_side.png",      "pig_top.png",      "pig_bottom.png")
mob_blocks("zombie",      "Zombie",           "zombie_face.png",   "zombie_side.png",   "zombie_top.png",   "zombie_bottom.png")
mob_blocks("sheep",       "Sheep",            "sheep_face.png",    "sheep_side.png",    "sheep_top.png",    "sheep_bottom.png")
mob_blocks("spider",      "Spider",           "spider_face.png",   "spider_side.png",   "spider_top.png",   "spider_bottom.png")
mob_blocks("creeper",     "Creeper",          "creeper_face.png",  "creeper_side.png",  "creeper_top.png",  "creeper_bottom.png")
mob_blocks("zpiglin",     "Zombified Piglin", "zpiglin_face.png",  "zpiglin_side.png",  "zpiglin_top.png",  "zpiglin_bottom.png")

-- === 新增生物（三件套）===
mob_blocks("cow",         "Cow",          "cow_face.png",         "cow_side.png",         "cow_top.png",         "cow_bottom.png")
mob_blocks("chicken",     "Chicken",      "chicken_face.png",     "chicken_side.png",     "chicken_top.png",     "chicken_bottom.png")
mob_blocks("skeleton",    "Skeleton",     "skeleton_face.png",    "skeleton_side.png",    "skeleton_top.png",    "skeleton_bottom.png")
mob_blocks("wolf",        "Wolf",         "wolf_face.png",        "wolf_side.png",        "wolf_top.png",        "wolf_bottom.png")
mob_blocks("ocelot",      "Ocelot",       "ocelot_face.png",      "ocelot_side.png",      "ocelot_top.png",      "ocelot_bottom.png")
mob_blocks("slime",       "Slime",        "slime_face.png",       "slime_side.png",       "slime_top.png",       "slime_bottom.png")
mob_blocks("squid",       "Squid",        "squid_face.png",       "squid_side.png",       "squid_top.png",       "squid_bottom.png")
mob_blocks("ghast",       "Ghast",        "ghast_face.png",       "ghast_side.png",       "ghast_top.png",       "ghast_bottom.png")
mob_blocks("bat",         "Bat",          "bat_face.png",         "bat_side.png",         "bat_top.png",         "bat_bottom.png")
mob_blocks("rabbit_brown","Rabbit",       "rabbit_brown_face.png","rabbit_brown_side.png","rabbit_brown_top.png","rabbit_brown_bottom.png")
mob_blocks("axolotl_brown","Axolotl",     "axolotl_brown_face.png","axolotl_brown_side.png","axolotl_brown_top.png","axolotl_brown_bottom.png")
mob_blocks("snow_golem",  "Snow Golem",   "snow_golem_face.png",  "snow_golem_side.png",  "snow_golem_top.png",  "snow_golem_bottom.png")
mob_blocks("pig_zombie",  "Zombie Pigman","pig_zombie_face.png",  "pig_zombie_side.png",  "pig_zombie_top.png",  "pig_zombie_bottom.png")

-- ---------------------------------------------------------------------------
-- 1.5 多部件外形：生物不再只显示一个方块，而是"腿 + 躯干 + 头"若干方块拼成。
--     每个部件是轴对齐立方体，尺寸从 (0,0,0) 长到 (size,size,size)，
--     offset 是它**底面中心**相对实体脚底的位置（在实体旋转之后施加，
--     所以整只生物转身时部件保持刚性，不会散开）。
--
--     每个部件**引用不同的方块**（<mob>_body / <mob>_head / <mob>_leg），
--     因此可以各自使用不同贴图（见第 1 节的 mob_blocks），
--     头部的 _face 贴图只出现在头的南/北面，转身时能看出朝向。
--
--     几何约束（必须满足，否则会出现"腿没接上躯干/头插进躯干"的错乱）：
--       * 相邻部件 z 区间首尾相接：腿顶 == 躯干底，躯干顶 == 头底
--       * 最高点不超过 hitbox 高度
--       * x/y 方向以实体中心对称
-- ---------------------------------------------------------------------------
local function parts(...)
	return {...}
end

-- ===========================================================================
--  确定性随机（精确 32 位 LCG）
--
--  为什么要自己实现：Lua 的 math.random 在未显式播种时，每次进程启动
--  都会产生**完全相同**的序列（LuaJIT 默认种子固定），导致"每次重启
--  服务器生成的生物位置都一样"，玩家很快就会发现规律。
--
--  为什么不用 java.util.Random：
--  Java 的 Random 是 48 位 LCG（seed * 0x5DEECE66D + 0xB），
--  中间乘积是 80 位——**超出 Lua double 的 53 位精度**，无法精确表示。
--  之前用纯 Lua double 模拟时状态会失控（实测生成的坐标达到 -1117139
--  这种离谱数值，生物全刷不出来）。LuaJIT 的 `bit` 库只支持 32 位，
--  所以这里改用**精确的 32 位 LCG**：
--
--      state = (state * 1103515245 + 12345) mod 2^32
--
--  用 16 位分段乘法在 bit 库里精确计算，保证跨机器、跨会话完全一致。
--  史莱姆区块判定另有一套精确实现（见下方 slime_chunk）。
-- ===========================================================================
local bitlib = rawget(_G, "bit")

-- 32 位 LCG 的乘子常量拆分（用于 16 位分段乘法）
local LCG_MULT = 1103515245
local LCG_MULT_LO = LCG_MULT % 65536
local LCG_MULT_HI = math.floor(LCG_MULT / 65536)

-- state * LCG_MULT mod 2^32 —— 用 16 位分段乘法精确计算
local function lcg_mul(state)
	if not bitlib then return 0 end

	local lo = bitlib.band(state, 0xFFFF)
	local hi = bitlib.band(bitlib.rshift(state, 16), 0xFFFF)
	local lolo = bitlib.band(lo, 0xFFFF)
	local lohi = bitlib.band(bitlib.rshift(lo, 16), 0xFFFF)
	local hilo = bitlib.band(hi, 0xFFFF)
	local hihi = bitlib.band(bitlib.rshift(hi, 16), 0xFFFF)

	local p0 = bitlib.band(lolo * LCG_MULT_LO, 0xFFFF)
	local p1 = bitlib.band(lolo * LCG_MULT_HI + lohi * LCG_MULT_LO, 0xFFFF)
	local p2 = bitlib.band(lolo * hilo + lohi * LCG_MULT_HI + hilo * LCG_MULT_LO, 0xFFFF)

	local newLo = bitlib.bor(p0, bitlib.lshift(p1, 16))
	local newHi = bitlib.band(p2, 0xFFFF)
	return bitlib.bor(newLo, bitlib.lshift(newHi, 16))
end

-- 返回新的 32 位状态（无符号）
local function lcg_next(state)
	if not bitlib then return (state + 1) % 4294967296 end
	return bitlib.band(lcg_mul(state) + 12345, 0xFFFFFFFF)
end

-- 把任意整数折叠成 32 位无符号种子
local function fold32(value)
	if not bitlib then
		value = value % 4294967296
		if value < 0 then value = value + 4294967296 end
		return value
	end
	return bitlib.band(value, 0xFFFFFFFF)
end

-- 从 seed 派生一个 0..1 的"位置噪声"：同一个 seed 永远得到同一个结果
local function noise01(seed)
	local s = lcg_next(fold32(seed))
	if not bitlib then return (s % 1000000) / 1000000.0 end
	return bitlib.band(bitlib.rshift(s, 8), 0xFFFFFF) / 16777216.0
end

-- ===========================================================================
--  史莱姆区块判定（slimeChunk）
--
--  参考实现（void717doll/OpenMiner 的 RetroMobSpawn.hpp）用的是 java 的
--  32 位 int 溢出语义：乘积先按 int32 溢出，再提升为 64 位参与运算。
--  这里用 bit 库精确模拟 int32 溢出，保证结果与 MC 一致（约 10% 的区块）。
-- ===========================================================================
local function int32_mul(a, b)
	if not bitlib then return 0 end
	-- 用 16 位分段乘法做精确的 32 位乘法
	local alo = bitlib.band(a, 0xFFFF)
	local ahi = bitlib.band(bitlib.rshift(a, 16), 0xFFFF)
	local blo = bitlib.band(b, 0xFFFF)
	local bhi = bitlib.band(bitlib.rshift(b, 16), 0xFFFF)

	local p0 = bitlib.band(alo * blo, 0xFFFF)
	local p1 = bitlib.band(alo * bhi + ahi * blo, 0xFFFF)
	local p2 = bitlib.band(ahi * bhi, 0xFFFF)

	local lo = bitlib.bor(p0, bitlib.lshift(p1, 16))
	local hi = bitlib.band(p2, 0xFFFF)
	return bitlib.bor(lo, bitlib.lshift(hi, 16))
end

local function slime_chunk(world_seed, cx, cy)
	if not bitlib then return false end

	-- worldSeed + (cx*cx*4987142) + (cx*5947611) + (cy*cy)*4392871 + (cy*389711)
	-- 其中 cx*cx*4987142 按 int32 溢出计算
	local cx2 = int32_mul(cx, cx)
	local cy2 = int32_mul(cy, cy)
	local t1 = int32_mul(cx2, 4987142)
	local t2 = int32_mul(cx, 5947611)
	local t3 = int32_mul(cy2, 4392871)
	local t4 = int32_mul(cy, 389711)

	local mixed = bitlib.band(world_seed + t1 + t2 + t3 + t4, 0xFFFFFFFF)

	-- Random(mixed ^ 987234911).nextInt(10) == 0
	local state = bitlib.bxor(mixed, 987234911)
	state = lcg_next(state)
	state = lcg_next(state)
	-- nextInt(10)：取 31 位再对 10 取模
	local value = bitlib.band(bitlib.rshift(state, 1), 0x7FFFFFFF)
	return (value % 10) == 0
end
-- 以 (cx, cy) 为水平中心、z 为底面，放一个 size 立方体
local function cube(size, cx, cy, z, block)
	return {size = size, offset = {cx - size / 2, cy - size / 2, z}, block_id = block}
end

-- 四足动物（横向趴卧）：4 条腿 -> 扁躯干 -> 头（头在躯干前端略低）
--   引擎的部件是立方体，所以"扁躯干"用两层薄立方体叠出来：
--   上下各 bodyH/2，总高精确等于 bodyH，水平尺寸 bodyLen x bodyWid。
--   legH + bodyH + headH - headDrop 必须 <= hitbox 高度。
--
--   mob 是**不带 default: 前缀的 mob 名**（如 "pig"），
--   三个部件分别引用 default:<mob>_body / _head / _leg 三个独立方块，
--   因此头、身、腿可以各用各的贴图（头部的 _face 只在头的南/北面）。
local function quadruped(mob, bodyLen, bodyWid, bodyH, legH, headH, headForward, headDrop)
	local bodyBlock = "default:" .. mob .. "_body"
	local headBlock = "default:" .. mob .. "_head"
	local legBlock  = "default:" .. mob .. "_leg"
	local zBody = legH
	local zHead = legH + bodyH - headDrop

	local half = bodyH / 2
	return parts(
		-- 躯干下半（z: legH ~ legH+half）
		{size = half, offset = {-bodyLen / 2, -bodyWid / 2, zBody}, block_id = bodyBlock},
		-- 躯干上半（z: legH+half ~ legH+bodyH），与下半严格相接
		{size = half, offset = {-bodyLen / 2, -bodyWid / 2, zBody + half}, block_id = bodyBlock},
		-- 头（躯干前端）
		{size = headH, offset = {-headH / 2, -bodyWid / 2 - headForward, zHead}, block_id = headBlock},
		-- 4 条腿（躯干四角下方，与躯干底面严格相接）
		cube(legH, -bodyLen / 2 + legH / 2, -bodyWid / 2 + legH / 2, 0, legBlock),
		cube(legH,  bodyLen / 2 - legH / 2, -bodyWid / 2 + legH / 2, 0, legBlock),
		cube(legH, -bodyLen / 2 + legH / 2,  bodyWid / 2 - legH / 2, 0, legBlock),
		cube(legH,  bodyLen / 2 - legH / 2,  bodyWid / 2 - legH / 2, 0, legBlock)
	)
end

-- 双足直立：2 条腿 -> 躯干 -> 头（头正上方）
--   legH + bodyH + headH 必须 <= hitbox 高度
--   mob 同上：less 前缀，部件引自 <mob>_body/_head/_leg。
local function biped(mob, bodySize, legH, bodyH, headSize)
	local bodyBlock = "default:" .. mob .. "_body"
	local headBlock = "default:" .. mob .. "_head"
	local legBlock  = "default:" .. mob .. "_leg"
	local legGap = bodySize / 2 - legH / 2
	local zBody = legH
	local zHead = legH + bodyH
	return parts(
		-- 躯干
		cube(bodySize, 0, 0, zBody, bodyBlock),
		-- 头（正中，与躯干顶面严格相接）
		cube(headSize, 0, 0, zHead, headBlock),
		-- 2 条腿（左右分开，与躯干底面严格相接）
		cube(legH, -legGap, 0, 0, legBlock),
		cube(legH,  legGap, 0, 0, legBlock)
	)
end

-- ---------------------------------------------------------------------------
-- 2. 服务端游荡 AI。ServerTick 是主驱动，保证存档重载后未恢复 tick 标记的
--    持久化实体仍行动；on_tick 保留为已有实体 API 的兼容回调。
-- ---------------------------------------------------------------------------
local mob_defs = {
	-- speed: 每 tick 的水平移动速度
	-- height: hitbox 高度（用于生成点的净空检查：必须有 height 格空间）
	--          必须与 mod:entity 里 hitbox 的第 6 个分量（sizeZ）完全一致
	-- home:  是否自然生成（"overworld_passive"/"overworld_hostile"/"nether"）

	-- === 原有 6 种 ===
	["default:pig"]              = {speed = 0.02,  height = 0.75},
	["default:zombie"]           = {speed = 0.02,  height = 0.9},
	["default:sheep"]            = {speed = 0.02,  height = 0.9},
	["default:spider"]           = {speed = 0.045, height = 0.8},
	["default:creeper"]          = {speed = 0.02,  height = 0.85},
	["default:zombified_piglin"] = {speed = 0.045, height = 0.9},

	-- === 新增：主世界被动 ===
	["default:cow"]              = {speed = 0.018, height = 0.9},
	["default:chicken"]          = {speed = 0.02,  height = 0.6},
	["default:rabbit_brown"]     = {speed = 0.035, height = 0.5},
	["default:ocelot"]           = {speed = 0.03,  height = 0.7},
	["default:wolf"]             = {speed = 0.028, height = 0.85},
	["default:axolotl_brown"]    = {speed = 0.02,  height = 0.4},
	["default:snow_golem"]       = {speed = 0.018, height = 1.0},

	-- === 新增：主世界敌对 ===
	["default:skeleton"]         = {speed = 0.022, height = 0.95},
	["default:slime"]            = {speed = 0.02,  height = 0.6},
	["default:bat"]              = {speed = 0.05,  height = 0.45},

	-- === 新增：下界 ===
	["default:ghast"]            = {speed = 0.015, height = 1.2},
	["default:pig_zombie"]       = {speed = 0.045, height = 0.9},

	-- === 水系（无液体游泳逻辑，仅作为装饰性生成在水面附近）===
	["default:squid"]            = {speed = 0.02,  height = 0.5},
}

local DIRS = {
	{1, 0}, {-1, 0}, {0, 1}, {0, -1},
	{0.7071, 0.7071}, {0.7071, -0.7071}, {-0.7071, 0.7071}, {-0.7071, -0.7071},
}

local mob_ai_state = {}  -- [entity id] = {dim, mode="walk"/"idle", dx, dy, timer}
local mob_ticks = 0

-- ---------------------------------------------------------------------------
-- 2.0 生物功能总开关（mobs_enabled，默认 false = 关闭/隐藏生物）
--
--   关闭时：
--     * mob_on_tick / mob_on_collision 立即返回（旧存档里残留的生物即使被
--       EntityTickController 调用 on_tick 也不会移动、不会发碰撞消息）
--     * BlockDigged / PlayerConnected / ServerTick 的所有生成路径立即返回
--     * ServerTick 每 tick 把已存在的生物实体 despawn（清理旧存档与 /summon），
--       因此客户端不会收到 EntitySpawn，实现"隐藏生物"
--   代码全部保留：把第 4 节的 mod:option("mobs_enabled", false) 改成 true 即可恢复。
-- ---------------------------------------------------------------------------
local function mobs_enabled()
	return openminer:get_config("default:mobs_enabled") == true
end

-- 清理一个维度里所有生物实体（只清理 mob_defs 里登记的生物，item_drop 等不受影响）。
-- 返回清理数量。despawn_entity 会广播 EntityDespawn 并从场景 registry 销毁实体。
local function despawn_mobs_in_world(world)
	local removed = 0
	for _, entity in ipairs(world:get_entities()) do
		if entity:valid() then
			local ok, entity_type = pcall(function() return entity:get_type() end)
			if ok and mob_defs[entity_type] then
				mods["default"]:despawn_entity(entity)
				removed = removed + 1
			end
		end
	end
	return removed
end

local function pick_dir(st)
	local d = DIRS[math.random(#DIRS)]
	st.dx, st.dy = d[1], d[2]
end

local function mob_on_tick(entity, pos, world, server)
	-- 生物功能关闭：旧存档里已存在的生物即使被打上 EntityTickTag 也不会移动。
	-- 同时在这里就地 despawn：EntityTickController 在 NetworkController 之前运行，
	-- 因此该实体不会被广播 EntitySpawn，客户端完全看不到（旧存档残留与 /summon
	-- 生成的生物都走这条路径）。ServerTick 里还有一层兜底清理。
	if not mobs_enabled() then
		if entity ~= nil and type(entity.get_type) == "function" and entity:valid() then
			local ok, entity_type = pcall(function() return entity:get_type() end)
			if ok and mob_defs[entity_type] then
				mods["default"]:despawn_entity(entity)
			end
		end
		return
	end

	-- ServerTick 会枚举本维度全部实体（含 item_drop 等非生物）。
	-- 只处理已注册的生物类型；sol2 usertype 的 type() 是 "userdata" 而非
	-- "table"，所以只检查方法是否存在，避免把合法 EntityWrapper 挡掉。
	if entity == nil or type(entity.get_type) ~= "function" then return end
	local entity_type = entity:get_type()
	local def = mob_defs[entity_type]
	if not def then return end

	local id = entity:id()
	local dim = world:dimension():id()
	local st = mob_ai_state[id]
	if not st or st.dim ~= dim then
		st = {dim = dim, mode = "idle", dx = 0, dy = 0, timer = math.random(20, 60)}
		mob_ai_state[id] = st
	end

	st.timer = st.timer - 1
	if st.timer <= 0 then
		if math.random(100) <= 70 then
			st.mode = "walk"
			pick_dir(st)
		else
			st.mode = "idle"
		end
		st.timer = math.random(40, 80)
	end

	local x, y, z = entity:get_position()
	local feet = math.floor(z + 0.0001)

	if st.mode == "walk" then
		-- 先试完整步长再试半步；在目标列向下吸附一格，可上下坡但不悬空。
		for step = def.speed, 0.009, -0.01 do
			local nx = x + st.dx * step
			local ny = y + st.dy * step
			local bx, by = math.floor(nx), math.floor(ny)
			local current = world:get_block(bx, by, feet)
			local below = world:get_block(bx, by, feet - 1)
			if current == 0 and below ~= 0 and world:get_block(bx, by, feet + 1) == 0 then
				entity:set_position(nx, ny, feet)
				return
			end
			-- 一格上坡：目标脚部为空、下方为地面，且头顶仍有空间。
			if world:get_block(bx, by, feet) == 0
				and world:get_block(bx, by, feet + 1) == 0
				and world:get_block(bx, by, feet + 2) == 0
				and below ~= 0 then
				entity:set_position(nx, ny, feet + 1)
				return
			end
			-- 一格下坡：脚部和头部为空，下方有地面，落回方块顶面。
			if world:get_block(bx, by, feet - 1) ~= 0 and below == 0 then
				entity:set_position(nx, ny, feet - 1)
				return
			end
		end
		pick_dir(st)
	end

	-- 贴地/重力：脚下是空气且更下方有支撑时逐 tick 下落；脚部嵌方块且
	-- 头顶有空间时抬升。未加载区块返回全空气，此时保持原位，避免坠入虚空。
	if world:get_block(math.floor(x), math.floor(y), feet - 1) == 0
		and world:get_block(math.floor(x), math.floor(y), feet - 2) ~= 0 then
		entity:set_position(x, y, z - 0.05)
	elseif world:get_block(math.floor(x), math.floor(y), feet) ~= 0
		and world:get_block(math.floor(x), math.floor(y), feet + 1) == 0 then
		entity:set_position(x, y, z + 0.05)
	end
end

-- on_collision：玩家与实体相交期间每 tick 触发，限频到约 2 秒一次（40 tick）。
local chat_counters = {}
local collision_messages = {
	-- 原有 6 种
	["default:pig"]              = "猪：哼哼！",
	["default:zombie"]           = "僵尸抓住了你！（当前引擎没有伤害系统）",
	["default:sheep"]            = "羊：咩！",
	["default:spider"]           = "蜘蛛：嘶嘶！",
	["default:creeper"]          = "苦力怕：嘶……（当前引擎没有爆炸或伤害系统）",
	["default:zombified_piglin"] = "僵尸猪灵：愤怒的哼声！",

	-- 新增
	["default:cow"]              = "牛：哞——",
	["default:chicken"]          = "鸡：咯咯哒！",
	["default:rabbit_brown"]     = "兔子：噗。",
	["default:ocelot"]           = "豹猫：喵？",
	["default:wolf"]             = "狼：汪！",
	["default:axolotl_brown"]    = "美西螈：咕噜咕噜。",
	["default:snow_golem"]       = "雪傀儡：嘎吱……",
	["default:skeleton"]         = "骷髅：骨头碰撞的声音。（当前引擎没有弓箭）",
	["default:slime"]            = "史莱姆：啪嗒！",
	["default:bat"]              = "蝙蝠：吱吱！",
	["default:ghast"]            = "恶魂：哀嚎——（当前引擎没有火球）",
	["default:pig_zombie"]       = "僵尸猪人：低沉的猪叫。",
	["default:squid"]            = "鱿鱼：噗叽。",
}

local function mob_on_collision(entity, player, server)
	-- 生物功能关闭：不触发任何生物碰撞消息。
	if not mobs_enabled() then return end

	local t = entity:get_type()
	chat_counters[t] = (chat_counters[t] or 0) + 1
	if chat_counters[t] % 40 == 0 then
		server:send_chat_message(0, collision_messages[t] or "生物碰到了你。", player:client())
	end
end

-- ---------------------------------------------------------------------------
-- 3. 实体定义（多部件外形：头/身/腿各用独立方块与贴图）
--
--    每种生物的 hitbox 高度必须与 mob_defs 里的 height **完全一致**，
--    否则 find_surface_z 的净空检查会算错空间（生物会卡进地形）。
--    几何总高（腿+躯干+头）必须 <= hitbox 高度。
-- ---------------------------------------------------------------------------
mod:entity {
	id = "pig",
	max_health = 10,
	properties = {
		-- 腿 0.22 + 躯干 0.32 + 头(0.25，下沉 0.04) → 最高点 0.75 = hitbox ✔
		parts = quadruped("pig", 0.60, 0.42, 0.32, 0.22, 0.25, 0.10, 0.04),
		is_rotatable = true,
		hitbox = {-0.375, -0.375, 0, 0.75, 0.75, 0.75},
	},
	on_tick = mob_on_tick,
	on_collision = mob_on_collision,
}

mod:entity {
	id = "zombie",
	max_health = 20,
	properties = {
		-- 腿 0.30 + 躯干 0.32 + 头 0.28 = 0.90 = hitbox ✔
		parts = biped("zombie", 0.44, 0.30, 0.32, 0.28),
		is_rotatable = true,
		hitbox = {-0.45, -0.45, 0, 0.9, 0.9, 0.9},
	},
	on_tick = mob_on_tick,
	on_collision = mob_on_collision,
}

mod:entity {
	id = "sheep",
	max_health = 8,
	properties = {
		-- 腿 0.27 + 躯干 0.38 + 头(0.29，下沉 0.04) → 最高点 0.90 = hitbox ✔
		parts = quadruped("sheep", 0.58, 0.46, 0.38, 0.27, 0.29, 0.10, 0.04),
		is_rotatable = true,
		hitbox = {-0.45, -0.45, 0, 0.9, 0.9, 0.9},
	},
	on_tick = mob_on_tick,
	on_collision = mob_on_collision,
}

mod:entity {
	id = "spider",
	max_health = 16,
	properties = {
		-- 蜘蛛（hitbox 0.8）：趴着的扁平身体 + 前端小头 + 8 条细腿。
		-- 腿 0~0.16、身体 0.16~0.42、头 0.42~0.60，总高 0.60 < 0.8 ✔
		-- 相邻部件 z 区间严格相接，无空隙无重叠。
		parts = parts(
			-- 身体（扁平，z 0.16~0.42）
			cube(0.26, 0, 0, 0.16, "default:spider_body"),
			-- 头（身体前端，z 0.42~0.60）
			cube(0.18, 0, -0.30, 0.42, "default:spider_head"),
			-- 8 条细腿（左右各 4，z 0~0.16，与身体底面严格相接）
			cube(0.16, -0.26, -0.30, 0, "default:spider_leg"),
			cube(0.16,  0.26, -0.30, 0, "default:spider_leg"),
			cube(0.16, -0.26, -0.12, 0, "default:spider_leg"),
			cube(0.16,  0.26, -0.12, 0, "default:spider_leg"),
			cube(0.16, -0.26,  0.12, 0, "default:spider_leg"),
			cube(0.16,  0.26,  0.12, 0, "default:spider_leg")
		),
		is_rotatable = true,
		hitbox = {-0.4, -0.4, 0, 0.8, 0.8, 0.8},
	},
	on_tick = mob_on_tick,
	on_collision = mob_on_collision,
}

mod:entity {
	id = "creeper",
	max_health = 20,
	properties = {
		-- 腿 0.28 + 躯干 0.31 + 头 0.26 = 0.85 = hitbox ✔
		parts = biped("creeper", 0.42, 0.28, 0.31, 0.26),
		is_rotatable = true,
		hitbox = {-0.425, -0.425, 0, 0.85, 0.85, 0.85},
	},
	on_tick = mob_on_tick,
	on_collision = mob_on_collision,
}

mod:entity {
	id = "zombified_piglin",
	max_health = 20,
	properties = {
		-- 腿 0.30 + 躯干 0.32 + 头 0.28 = 0.90 = hitbox ✔
		parts = biped("zpiglin", 0.44, 0.30, 0.32, 0.28),
		is_rotatable = true,
		hitbox = {-0.45, -0.45, 0, 0.9, 0.9, 0.9},
	},
	on_tick = mob_on_tick,
	on_collision = mob_on_collision,
}

-- ---------------------------------------------------------------------------
-- 3.1 新增生物
-- ---------------------------------------------------------------------------

-- 牛（四足，体型比猪大）
mod:entity {
	id = "cow",
	max_health = 10,
	properties = {
		-- 腿 0.24 + 躯干 0.40 + 头(0.32，下沉 0.06) → 最高点 0.90 = hitbox ✔
		parts = quadruped("cow", 0.74, 0.50, 0.40, 0.24, 0.32, 0.12, 0.06),
		is_rotatable = true,
		hitbox = {-0.45, -0.45, 0, 0.9, 0.9, 0.9},
	},
	on_tick = mob_on_tick,
	on_collision = mob_on_collision,
}

-- 鸡（四足几何复用：矮身体 + 小腿，看起来像小鸡）
mod:entity {
	id = "chicken",
	max_health = 4,
	properties = {
		-- 腿 0.12 + 躯干 0.28 + 头(0.24，下沉 0.04) → 最高点 0.60 = hitbox ✔
		parts = quadruped("chicken", 0.34, 0.30, 0.28, 0.12, 0.24, 0.08, 0.04),
		is_rotatable = true,
		hitbox = {-0.3, -0.3, 0, 0.6, 0.6, 0.6},
	},
	on_tick = mob_on_tick,
	on_collision = mob_on_collision,
}

-- 兔子（小体型四足）
mod:entity {
	id = "rabbit_brown",
	max_health = 3,
	properties = {
		-- 腿 0.08 + 躯干 0.22 + 头(0.20，下沉 0.00) → 最高点 0.50 = hitbox ✔
		parts = quadruped("rabbit_brown", 0.30, 0.24, 0.22, 0.08, 0.20, 0.10, 0.00),
		is_rotatable = true,
		hitbox = {-0.25, -0.25, 0, 0.5, 0.5, 0.5},
	},
	on_tick = mob_on_tick,
	on_collision = mob_on_collision,
}

-- 豹猫
mod:entity {
	id = "ocelot",
	max_health = 10,
	properties = {
		-- 腿 0.20 + 躯干 0.26 + 头(0.28，下沉 0.04) → 最高点 0.70 = hitbox ✔
		parts = quadruped("ocelot", 0.52, 0.34, 0.26, 0.20, 0.28, 0.10, 0.04),
		is_rotatable = true,
		hitbox = {-0.35, -0.35, 0, 0.7, 0.7, 0.7},
	},
	on_tick = mob_on_tick,
	on_collision = mob_on_collision,
}

-- 狼
mod:entity {
	id = "wolf",
	max_health = 8,
	properties = {
		-- 腿 0.24 + 躯干 0.32 + 头(0.30，下沉 0.01) → 最高点 0.85 = hitbox ✔
		parts = quadruped("wolf", 0.60, 0.34, 0.32, 0.24, 0.30, 0.12, 0.01),
		is_rotatable = true,
		hitbox = {-0.425, -0.425, 0, 0.85, 0.85, 0.85},
	},
	on_tick = mob_on_tick,
	on_collision = mob_on_collision,
}

-- 美西螈（矮小）
mod:entity {
	id = "axolotl_brown",
	max_health = 14,
	properties = {
		-- 腿 0.06 + 躯干 0.24 + 头(0.20，下沉 0.10) → 最高点 0.40 = hitbox ✔
		-- 头部前伸 0.10，视觉 y 范围 [-0.23, 0.13]，故 hitbox y 需覆盖 ±0.23
		parts = quadruped("axolotl_brown", 0.36, 0.26, 0.24, 0.06, 0.20, 0.10, 0.10),
		is_rotatable = true,
		hitbox = {-0.24, -0.24, 0, 0.48, 0.48, 0.4},
	},
	on_tick = mob_on_tick,
	on_collision = mob_on_collision,
}

-- 雪傀儡（双足，高瘦）
mod:entity {
	id = "snow_golem",
	max_health = 4,
	properties = {
		-- 腿 0.30 + 躯干 0.40 + 头 0.30 = 1.00 = hitbox ✔
		parts = biped("snow_golem", 0.40, 0.30, 0.40, 0.30),
		is_rotatable = true,
		hitbox = {-0.35, -0.35, 0, 0.7, 0.7, 1.0},
	},
	on_tick = mob_on_tick,
	on_collision = mob_on_collision,
}

-- 骷髅（双足）
mod:entity {
	id = "skeleton",
	max_health = 20,
	properties = {
		-- 腿 0.32 + 躯干 0.34 + 头 0.29 = 0.95 = hitbox ✔
		parts = biped("skeleton", 0.44, 0.32, 0.34, 0.29),
		is_rotatable = true,
		hitbox = {-0.475, -0.475, 0, 0.95, 0.95, 0.95},
	},
	on_tick = mob_on_tick,
	on_collision = mob_on_collision,
}

-- 史莱姆（矮方块状）
mod:entity {
	id = "slime",
	max_health = 16,
	properties = {
		-- 腿 0.06 + 躯干 0.34 + 头(0.20，下沉 0.00) → 最高点 0.60 = hitbox ✔
		parts = quadruped("slime", 0.44, 0.44, 0.34, 0.06, 0.20, 0.00, 0.00),
		is_rotatable = true,
		hitbox = {-0.3, -0.3, 0, 0.6, 0.6, 0.6},
	},
	on_tick = mob_on_tick,
	on_collision = mob_on_collision,
}

-- 蝙蝠（极小，贴地飞行的观感）
mod:entity {
	id = "bat",
	max_health = 6,
	properties = {
		-- 腿 0.04 + 躯干 0.21 + 头(0.20，下沉 0.00) → 最高点 0.45 = hitbox ✔
		parts = quadruped("bat", 0.26, 0.22, 0.21, 0.04, 0.20, 0.06, 0.00),
		is_rotatable = true,
		hitbox = {-0.225, -0.225, 0, 0.45, 0.45, 0.45},
	},
	on_tick = mob_on_tick,
	on_collision = mob_on_collision,
}

-- 恶魂（巨大）
mod:entity {
	id = "ghast",
	max_health = 10,
	properties = {
		-- 腿 0.20 + 躯干 0.60 + 头(0.40，下沉 0.00) → 最高点 1.20 = hitbox ✔
		parts = quadruped("ghast", 0.90, 0.90, 0.60, 0.20, 0.40, 0.00, 0.00),
		is_rotatable = true,
		hitbox = {-0.6, -0.6, 0, 1.2, 1.2, 1.2},
	},
	on_tick = mob_on_tick,
	on_collision = mob_on_collision,
}

-- 僵尸猪人（下界，体型同僵尸猪灵）
mod:entity {
	id = "pig_zombie",
	max_health = 20,
	properties = {
		-- 腿 0.30 + 躯干 0.32 + 头 0.28 = 0.90 = hitbox ✔
		parts = biped("pig_zombie", 0.44, 0.30, 0.32, 0.28),
		is_rotatable = true,
		hitbox = {-0.45, -0.45, 0, 0.9, 0.9, 0.9},
	},
	on_tick = mob_on_tick,
	on_collision = mob_on_collision,
}

-- 鱿鱼（柔软矮体）
mod:entity {
	id = "squid",
	max_health = 10,
	properties = {
		-- 腿 0.10 + 躯干 0.26 + 头(0.16，下沉 0.02) → 最高点 0.50 = hitbox ✔
		parts = quadruped("squid", 0.34, 0.34, 0.26, 0.10, 0.16, 0.00, 0.02),
		is_rotatable = true,
		hitbox = {-0.25, -0.25, 0, 0.5, 0.5, 0.5},
	},
	on_tick = mob_on_tick,
	on_collision = mob_on_collision,
}

-- ---------------------------------------------------------------------------
-- 4. 自然生成：主世界/下界分开的真实计数，所有入口共用同一套检查。
--
--    mobs_enabled 是总开关，默认 false（关闭生物功能，见文件头说明）。
--    注意：LuaMod::option 用 insert_or_assign 覆盖 config/server.lua 里读到的旧值，
--    所以这里的默认值就是最终生效值；config/server.lua 里的
--    ["default:mobs_enabled"] 也同步为 false，避免运行期 /option 被误设回 true。
-- ---------------------------------------------------------------------------
mod:option("mobs_enabled", false)
-- 上限收紧：之前 40/12 会让画面里挤满生物（用户反馈"大量刷新"）。
-- 现在按玩家数量动态计算：每人最多 8 只，总数上限 12，单类型上限 4。
mod:option("mobs_max_total", 12)
mod:option("mobs_max_per_type", 4)
-- 每个玩家周围的"局部"上限：避免多人时在同一个点堆一堆
mod:option("mobs_max_per_player", 8)
-- 生成间隔（tick）：60 = 3 秒。改成 200（10 秒）降低刷新频率
mod:option("mobs_spawn_interval", 200)

-- 生成规则：
--   * 主世界被动生物（白天偏好）：pig / sheep / cow / chicken / rabbit_brown /
--     ocelot / wolf / axolotl_brown / snow_golem
--   * 主世界敌对生物（夜晚偏好）：zombie / skeleton / creeper / spider / slime / bat
--   * 下界：zombified_piglin / pig_zombie / ghast
--   * 鱿鱼 squid 只在有水的列生成（见 spawn_allowed_here）
--
-- 两个列表都是"允许生成在哪个维度"，实际的昼夜/亮度筛选在
-- pick_kind_for_time / spawn_allowed_here 里做。
local OVERWORLD_PASSIVE = {
	"pig", "sheep", "cow", "chicken", "rabbit_brown",
	"ocelot", "wolf", "axolotl_brown", "snow_golem", "squid",
}
local OVERWORLD_HOSTILE = {
	"zombie", "skeleton", "creeper", "spider", "slime", "bat",
}
local NETHER_KINDS = {"zombified_piglin", "pig_zombie", "ghast"}

-- 合并列表用于计数/清理（顺序无所谓，去重交给调用方）
local OVERWORLD_KINDS = {}
do
	local seen = {}
	for _, list in ipairs({OVERWORLD_PASSIVE, OVERWORLD_HOSTILE}) do
		for _, kind in ipairs(list) do
			if not seen[kind] then
				seen[kind] = true
				OVERWORLD_KINDS[#OVERWORLD_KINDS + 1] = kind
			end
		end
	end
end

local function kinds_for_dimension(dim)
	if dim == 0 then return OVERWORLD_KINDS end
	if dim == 1 then return NETHER_KINDS end
	return {}
end

-- 判断 world:is_daytime() 是否可用（task-18 新增 API）；不可用时按"白天"处理，
-- 保证旧引擎/旧存档下依然能生成被动生物。
local function world_is_daytime(world)
	local ok, value = pcall(function() return world:is_daytime() end)
	if ok and type(value) == "boolean" then return value end
	return true
end

-- 按昼夜挑一组候选：白天以被动为主（约 75%），夜里以敌对为主（约 85%）。
-- 用 math.random 选组即可 —— 具体种类由 try_spawn 的轮换逻辑决定，
-- 这里的概率只影响"这一轮走被动还是敌对"。
local function pick_kind_pool(world, dimension)
	if dimension == 1 then return NETHER_KINDS end
	if dimension ~= 0 then return {} end

	local daytime = world_is_daytime(world)
	local hostile_chance = daytime and 25 or 85
	if math.random(100) <= hostile_chance then
		return OVERWORLD_HOSTILE
	end
	return OVERWORLD_PASSIVE
end

-- 该类型的生成是否受环境限制（目前只有 squid 需要"水边"）。
-- 未加载区块读到的方块 id 为 0，此时不加限制，避免整片区域刷不出来。
local WATER_BLOCK_IDS = nil
local function water_block_ids()
	if WATER_BLOCK_IDS then return WATER_BLOCK_IDS end
	WATER_BLOCK_IDS = {}
	local ok, def = pcall(function()
		return openminer.registry:get_block_from_string("default:water")
	end)
	if ok and def then WATER_BLOCK_IDS[def:id()] = true end
	return WATER_BLOCK_IDS
end

local function spawn_allowed_here(world, kind, x, y, z)
	if kind ~= "squid" then return true end
	-- 鱿鱼：脚下 1~3 格内需要有水方块
	local ids = water_block_ids()
	for dz = 0, 3 do
		if ids[world:get_block(x, y, z - 1 - dz)] then return true end
	end
	return false
end

-- 每个维度单独计数；不使用 # 统计以类型字符串为键的表。
local function count_mobs(world, kinds)
	local counts, total = {}, 0
	for _, kind in ipairs(kinds) do
		local n = #world:get_entities_by_type("default:" .. kind)
		counts[kind] = n
		total = total + n
	end
	return counts, total
end

local function can_spawn(counts, total, kind)
	if not mobs_enabled() then return false end
	local per_type = openminer:get_config("default:mobs_max_per_type") or 4
	local max_total = openminer:get_config("default:mobs_max_total") or 12
	return total < max_total and (counts[kind] or 0) < per_type
end

-- 从玩家/挖掘点 zref 向下找地表。
--
-- 净空检查：找到地面后，其上方必须有 **height 格**的空气（按该生物的
-- hitbox 高度算），而不是固定 2 格 —— 否则像僵尸（0.9 高）这种较高的
-- 生物会生成在只有 2 格空间的低矮地形里，卡进方块。
--
-- 未加载区块返回空气，扫描至下限仍未找到地面就放弃，不在虚空生成。
local function find_surface_z(world, x, y, zref, height)
	local clearance = math.max(2, math.ceil(height or 2))
	local top = math.floor(zref) + 8
	local bottom = math.floor(zref) - 16
	for z = top, bottom, -1 do
		if world:get_block(x, y, z) ~= 0 then
			-- 检查地面之上 clearance 格是否全为空
			for dz = 1, clearance do
				if world:get_block(x, y, z + dz) ~= 0 then
					return nil
				end
			end
			return z + 1
		end
	end
	return nil
end

-- 玩家排除半径（格）。参考实现用 24 格，避免生物在玩家眼皮底下刷新。
local PLAYER_EXCLUSION_RADIUS = 24

-- 生成采样距离范围（格）。必须大于 PLAYER_EXCLUSION_RADIUS，
-- 否则采样点全部落在排除圈内，一次都刷不出来。
local SPAWN_MIN_DISTANCE = 26
local SPAWN_MAX_DISTANCE = 48

local function far_from_players(server, x, y, dimension)
	local limit = PLAYER_EXCLUSION_RADIUS * PLAYER_EXCLUSION_RADIUS
	for _, player in ipairs(server:get_players()) do
		if player:is_online() and player:dimension() == dimension then
			local dx, dy = x - player:x(), y - player:y()
			if dx * dx + dy * dy < limit then return false end
		end
	end
	return true
end

local function spawn_mob(kind, x, y, z, dimension)
	mods["default"]:spawn_entity("default:" .. kind, {
		position = {x, y, z},
		dimension = dimension,
	})
end

-- 生成尝试计数器：用于「按轮次轮换生物种类」，
-- 保证一轮（5 次）里不会连续刷同一种生物。
local spawn_round = 0

local function try_spawn(server, world, player, dimension, counts, total, forced_kind, pos)
	if not mobs_enabled() then return total end

	-- 候选池：
	--   * forced_kind（挖掘触发等指定种类）优先，只尝试它一个
	--   * 否则按昼夜从 OVERWORLD_PASSIVE / OVERWORLD_HOSTILE 选一组
	--     （下界固定 NETHER_KINDS），再在组内按轮次轮换，
	--     避免单一类型刷屏（参考实现用 passive[(tick/200) % 5] 的做法）
	local pool
	if forced_kind then
		pool = {forced_kind}
	else
		pool = pick_kind_pool(world, dimension)
	end
	if #pool == 0 then return total end

	-- 在池里找一个"当前还能刷"的种类：受 per-type 上限限制的可能已经满了，
	-- 逐个尝试（最多一轮），避免因为池里某一种满了就整轮刷不出来。
	local kind = nil
	for i = 1, #pool do
		local idx = (spawn_round + i - 1) % #pool + 1
		local candidate = pool[idx]
		if can_spawn(counts, total, candidate) then
			kind = candidate
			spawn_round = spawn_round + i
			break
		end
	end
	if not kind then return total end

	-- 确定性随机：种子由「世界种子 + 玩家 id + 生成序号」派生，
	-- 保证同一世界同一位置的生成结果可复现（不受 math.random 未播种影响）。
	-- world:seed() 目前不保证存在（可能尚未暴露给 Lua），用 pcall 兜底。
	local player_id = 0
	local ok_id, pid = pcall(function() return player:client_id() end)
	if ok_id and pid then player_id = pid end

	local world_seed = 0
	local ok_seed, wseed = pcall(function() return world:seed() end)
	if ok_seed and wseed then world_seed = wseed end

	local seed = fold32(world_seed * 341873128712 + player_id * 132897987541 + spawn_round * 2654435761)

	local px, py, pz = player:x(), player:y(), player:z()
	local x, y, zref
	if pos then
		local r1 = noise01(seed)
		local r2 = noise01(seed + 1)
		x, y, zref = pos.x + math.floor(r1 * 11) - 5, pos.y + math.floor(r2 * 11) - 5, pos.z
	else
		-- 环带采样：距离 SPAWN_MIN_DISTANCE ~ SPAWN_MAX_DISTANCE 格。
		--
		-- 重要：采样距离必须**大于** PLAYER_EXCLUSION_RADIUS，
		-- 否则采样出来的点全部落在"离玩家太近"的排除圈里，
		-- 每次生成都会被 far_from_players 拒绝 → 一只生物都刷不出来。
		local r1 = noise01(seed)
		local r2 = noise01(seed + 1)
		local angle = r1 * math.pi * 2
		local distance = SPAWN_MIN_DISTANCE
			+ math.floor(r2 * (SPAWN_MAX_DISTANCE - SPAWN_MIN_DISTANCE + 1))
		x = math.floor(px + math.cos(angle) * distance)
		y = math.floor(py + math.sin(angle) * distance)
		zref = pz
	end
	if not far_from_players(server, x + 0.5, y + 0.5, dimension) then return total end

	local def = mob_defs["default:" .. kind] or {}
	local z = find_surface_z(world, x, y, zref, def.height)
	if not z then return total end

	-- 环境门槛（如鱿鱼需要水），不满足就放弃本次
	if not spawn_allowed_here(world, kind, x, y, z) then return total end

	spawn_mob(kind, x + 0.5, y + 0.5, z + 0.01, dimension)
	counts[kind] = (counts[kind] or 0) + 1
	return total + 1
end

local function replenish_world(server, world, dimension)
	-- 生物功能关闭：不补充任何生物。
	if not mobs_enabled() then return end

	local kinds = kinds_for_dimension(dimension)
	if #kinds == 0 then return end
	local counts, total = count_mobs(world, kinds)
	local per_player = openminer:get_config("default:mobs_max_per_player") or 8
	for _, player in ipairs(server:get_players()) do
		if player:is_online() and player:dimension() == dimension then
			-- 每个玩家最多补到 per_player 只，且每次只尝试 1 次（降低刷新密度）
			if total < per_player then
				total = try_spawn(server, world, player, dimension, counts, total, nil, nil)
			end
		end
	end
end

-- 保留原有挖掘概率作为补充生成；不再把挖掘当作主要生成来源。
local dug_labels = nil
local function get_dug_labels()
	if dug_labels then return dug_labels end
	local ok1, grass = pcall(function()
		return openminer.registry:get_block_from_string("default:grass"):label()
	end)
	local ok2, netherrack = pcall(function()
		return openminer.registry:get_block_from_string("default:netherrack"):label()
	end)
	dug_labels = {grass = (ok1 and grass) or "草方块", netherrack = (ok2 and netherrack) or "下界岩"}
	return dug_labels
end

openminer:add_listener(Event.BlockDigged, function(pos, block, player, world, client, server)
	-- 生物功能关闭：挖掘不再生成任何生物。
	if not mobs_enabled() then return end

	-- 挖掘生成：仅作为极低概率的"惊喜"，且只在草方块上触发。
	-- 之前的实现里 roll 23~34 / 94~100 完全不判方块类型（挖石头也刷怪），
	-- 导致玩家一挖矿就刷出一堆生物（用户反馈"大量刷新"）。
	local dim = world:dimension():id()
	local labels = get_dug_labels()
	local dug = block:label()
	local kind = nil
	local roll = math.random(1000)
	if dim == 1 then
		if roll <= 10 and dug == labels.netherrack then kind = "zombified_piglin" end
	elseif dim == 0 then
		if dug == labels.grass then
			if roll <= 6 then kind = "pig"
			elseif roll <= 12 then kind = "sheep"
			elseif roll <= 15 then kind = "creeper"
			elseif roll <= 18 then kind = "spider"
			end
		end
	end
	if kind then
		local counts, total = count_mobs(world, kinds_for_dimension(dim))
		try_spawn(server, world, player, dim, counts, total, kind, pos)
	end
end)

openminer:add_listener(Event.PlayerConnected, function(pos, player, client, server)
	-- 生物功能关闭：玩家连入不再补充生物（并顺手清理该维度残留的生物）。
	if not mobs_enabled() then
		local dimension = player:dimension()
		local world = server:get_world(dimension)
		if world then despawn_mobs_in_world(world) end
		return
	end
	local dimension = player:dimension()
	local world = server:get_world(dimension)
	if world then replenish_world(server, world, dimension) end
end)

-- 自然生成主循环：每 3 秒（60 tick）按真实地表补充生物，并清理 AI 状态。
-- 生物个体的游荡由实体自己的 on_tick 驱动（EntityTickController 每 tick 调用），
-- 存档重载后 C++ 会按类型重新打 EntityTickTag，因此持久化生物也会继续行动。
-- 这里不再逐实体调用 mob_on_tick，否则会与 on_tick 双重驱动（速度翻倍）。
--
-- mobs_enabled = false 时本监听器只做清理：
--   * 主要路径是 mob_on_tick 里的就地 despawn —— 它在 EntityTickController 中执行，
--     早于同 tick 的 NetworkController，所以客户端连 EntitySpawn 都收不到；
--   * 这里再加一层兜底扫描（每秒一次，避免每 tick 建 Lua 表），清理没有 tick 标记
--     （例如存档里类型回调已失效）的残留生物，并丢弃 AI 状态。
openminer:add_listener(Event.ServerTick, function(server)
	mob_ticks = mob_ticks + 1
	local worlds = {}
	for _, dimension in ipairs({0, 1}) do
		worlds[dimension] = server:get_world(dimension)
	end

	if not mobs_enabled() then
		-- 启动后前 2 秒每 tick 扫一次，之后每秒一次
		if mob_ticks <= 40 or mob_ticks % 20 == 0 then
			for _, dimension in ipairs({0, 1}) do
				local world = worlds[dimension]
				if world then despawn_mobs_in_world(world) end
			end
		end
		if next(mob_ai_state) ~= nil then mob_ai_state = {} end
		return
	end

	if mob_ticks % (openminer:get_config("default:mobs_spawn_interval") or 200) == 0 then
		for _, dimension in ipairs({0, 1}) do
			local world = worlds[dimension]
			if world then replenish_world(server, world, dimension) end
		end
	end

	-- 清理已 despawn 实体的 AI 状态；不 despawn 任何现存生物。
	if mob_ticks % 200 == 0 then
		local alive = {}
		for _, dimension in ipairs({0, 1}) do
			if worlds[dimension] then
				for _, entity in ipairs(worlds[dimension]:get_entities()) do alive[entity:id()] = true end
			end
		end
		for id in pairs(mob_ai_state) do
			if not alive[id] then mob_ai_state[id] = nil end
		end
	end
end)
