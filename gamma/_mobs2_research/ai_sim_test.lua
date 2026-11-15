-- ai_sim_test.lua — entities.lua 第二批逻辑的 LuaJIT mock 单测
-- 运行：gamma\build\luajit.exe gamma\_mobs2_research\ai_sim_test.lua
-- 覆盖：游荡 AI（走/停、步长、撞墙换向、临崖换向、状态清理不抛错）、
--        BlockDigged 六种生成分支与 label 识别、每类型/总数上限、PlayerConnected、on_collision。
-- 说明：math.random 用脚本队列替换使分支可确定性复现；mock 只模拟 entities.lua
--        调用到的 API 面（方法名与引擎 usertype 一致，来源于源码检视）。

local failures = 0
local function check(cond, msg)
	if cond then
		print("PASS: " .. msg)
	else
		failures = failures + 1
		print("FAIL: " .. msg)
	end
end
local function near(a, b, eps) return math.abs(a - b) < (eps or 1e-6) end

-- ---------------------------------------------------------------- mocks
local registered, listeners, options, spawned = {}, {}, {}, {}
local cfg = {
	["default:mobs_enabled"] = true,
	["default:mobs_max_total"] = 40,
	["default:mobs_max_per_type"] = 12,
}

mod = {
	block = function(self, t) end,
	entity = function(self, t) registered[t.id] = t end,
	option = function(self, id, v) options[id] = v end,
}
openminer = {
	get_config = function(self, k) return cfg[k] end,
	add_listener = function(self, ev, fn) listeners[ev] = fn end,
	registry = {
		get_block_from_string = function(self, s)
			local label = (s == "default:grass") and "草方块" or "下界岩"
			return {label = function() return label end}
		end,
	},
}
Event = {BlockDigged = "BlockDigged", PlayerConnected = "PlayerConnected"}
mods = {default = {
	spawn_entity = function(self, id, t)
		spawned[#spawned + 1] = {id = id, position = t.position, dimension = t.dimension}
	end,
}}

-- 确定性随机队列（忽略参数直接出队，参数不匹配由用例自身保证）
local queue
local orig_random = math.random
math.random = function(a, b)
	if not queue or #queue == 0 then error("rng queue exhausted", 2) end
	local v = queue[1]
	table.remove(queue, 1)
	return v
end
local function set_rng(vals)
	queue = {}
	for i, v in ipairs(vals) do queue[i] = v end
end

dofile("D:/cpp/2DMC/gamma/mods/default/entities.lua")

local function make_entity(typ, id, x, y, z)
	local e = {etype = typ, eid = id, x = x, y = y, z = z, last_set = nil}
	function e:get_type() return e.etype end
	function e:id() return e.eid end
	function e:get_position() return e.x, e.y, e.z end
	function e:set_position(nx, ny, nz)
		e.last_set = {nx, ny, nz}
		e.x, e.y, e.z = nx, ny, nz
	end
	function e:valid() return true end
	return e
end

-- blocks(x,y,z) -> 方块 id；ents 为 get_entities/get_entities_by_type 的返回
local function make_world(dim, blocks, ents)
	ents = ents or {}
	local w = {}
	function w:dimension() return {id = function() return dim end} end
	function w:get_block(x, y, z) return blocks(x, y, z) end
	function w:get_block_def(x, y, z)
		local id = blocks(x, y, z)
		return {string_id = function() return id == 1 and "default:grass" or "default:dirt" end}
	end
	function w:get_entities() return ents end
	function w:get_entities_by_type(typ)
		local out = {}
		for _, e in ipairs(ents) do
			if e:get_type() == typ then out[#out + 1] = e end
		end
		return out
	end
	return w
end

local chat_log
local function make_server()
	chat_log = {}
	return {send_chat_message = function(self, chan, msg, client) chat_log[#chat_log + 1] = msg end}
end

local pig_tick = registered.pig.on_tick
local flat_world_blocks = function(x, y, z) return (z == 31) and 1 or 0 end
-- 注意：mob_ai_state 是 entities.lua 的文件级局部表（按 entity id 键控），
-- 跨用例不重置 —— 因此各用例必须使用全局唯一的实体 id，避免状态泄漏。

-- ---------------------------------------------------------------- 1. 走/停与步长
do
	set_rng({50, 1, 1, 60, 1, 1, 60, 1, 1, 60})  -- 初始待机50；随后一直走(dir=DIRS[1]={1,0})
	local e = make_entity("default:pig", 1, 8.5, 8.5, 32.01)
	local w = make_world(0, flat_world_blocks, {e})
	for t = 1, 200 do pig_tick(e, {x = e.x, y = e.y, z = e.z}, w, make_server()) end
	check(e.x > 10.0, "AI: 200 tick 内发生真实游走 (x=" .. string.format("%.3f", e.x) .. ")")
	-- 重选方向后当 tick 即移动：t=50..200 每 tick 都走 = 151 步 × 0.02
	check(near(e.x - 8.5, 151 * 0.02), "AI: 行走步长恰为 0.02/tick 且方向稳定 (位移=" ..
		string.format("%.4f", e.x - 8.5) .. ")")
	check(near(e.y, 8.5) and near(e.z, 32.01), "AI: 水平游走不改变 y/z")
end

-- ---------------------------------------------------------------- 2. 撞墙换向
do
	local blocks = function(x, y, z)
		if z == 31 then return 1 end
		if z == 32 and x == 10 then return 1 end  -- x=10 竖墙
		return 0
	end
	local q = {50, 1, 1, 1000}
	for _ = 1, 40 do q[#q + 1] = 1 end  -- 撞墙后每 tick 换向都取 dir=1（持续朝墙）
	set_rng(q)
	local e = make_entity("default:pig", 2, 9.5, 8.5, 32.01)
	local w = make_world(0, blocks, {e})
	for t = 1, 100 do pig_tick(e, {x = e.x, y = e.y, z = e.z}, w, make_server()) end
	check(e.x < 10.001 and e.x > 9.9, "AI: 前方实心方块被探测到，实体止步于墙边界（含浮点容差，未入墙格）(x=" ..
		string.format("%.4f", e.x) .. ")")
end

-- ---------------------------------------------------------------- 3. 临崖换向
do
	local blocks = function(x, y, z)
		return (z == 31 and x <= 9) and 1 or 0  -- x>=10 无地面
	end
	local q = {50, 1, 1, 1000}
	for _ = 1, 40 do q[#q + 1] = 1 end
	set_rng(q)
	local e = make_entity("default:pig", 3, 9.5, 8.5, 32.01)
	local w = make_world(0, blocks, {e})
	for t = 1, 100 do pig_tick(e, {x = e.x, y = e.y, z = e.z}, w, make_server()) end
	check(e.x < 10.001 and e.x > 9.9, "AI: 前下方无地面（悬崖）被探测到，实体不越出边界 (x=" ..
		string.format("%.4f", e.x) .. ")")
end

-- ---------------------------------------------------------------- 4. 状态清理不抛错、存活实体不受影响
do
	set_rng({50, 50, 1, 1, 5000, 1, 1, 5000})  -- e1/e2 初始化；t=50 双双转走，长 timer 避开窗口
	local e1 = make_entity("default:pig", 10, 8.5, 8.5, 32.01)
	local e2 = make_entity("default:pig", 11, 20.5, 8.5, 32.01)
	local w = make_world(0, flat_world_blocks, {e1, e2})
	for t = 1, 60 do  -- 两只都建立状态并开走
		pig_tick(e1, {x = e1.x, y = e1.y, z = e1.z}, w, make_server())
		pig_tick(e2, {x = e2.x, y = e2.y, z = e2.z}, w, make_server())
	end
	w2 = make_world(0, flat_world_blocks, {e1})  -- e2 消失
	local ok = true
	for t = 1, 400 do  -- 跨过 200 的清理边界
		local s, err = pcall(pig_tick, e1, {x = e1.x, y = e1.y, z = e1.z}, w2, make_server())
		if not s then ok = false; print("  err: " .. tostring(err)) break end
	end
	check(ok, "AI: 实体消失后的状态清理不抛错")
end

-- ---------------------------------------------------------------- 5. BlockDigged 各分支
local dug_block = {label = function() return "草方块" end}
local netherrack_block = {label = function() return "下界岩" end}
local stone_block = {label = function() return "石头" end}
local player_mock = {dimension = function() return 0 end, client = function() return 0 end}
local pos0 = {x = 0, y = 0, z = 32}
local grass_surface = function(x, y, z) return (z == 31) and 1 or 0 end  -- (0,0,31)=grass
local nether_surface = function(x, y, z) return (z == 31) and 2 or 0 end

local function dig(world, block, rng)
	spawned = {}
	set_rng(rng)
	listeners.Event_BlockDigged = listeners[Event.BlockDigged]
	listeners.Event_BlockDigged(pos0, block, player_mock, world, {}, make_server())
	return spawned
end

local w_over = make_world(0, grass_surface, {})
do
	local s = dig(w_over, dug_block, {1, 0, 0})  -- roll=1 → pig；偏移 0,0
	check(#s == 1 and s[1].id == "default:pig" and s[1].dimension == 0
		and near(s[1].position[1], 0.5) and near(s[1].position[3], 32.01),
		"生成: 挖草方块 12% 概率生成 pig 于草方块表面")
end
do
	local s = dig(w_over, dug_block, {14, 0, 0})  -- roll=14 → sheep
	check(#s == 1 and s[1].id == "default:sheep", "生成: 挖草方块 10% 概率生成 sheep")
end
do
	local s = dig(w_over, dug_block, {23, 0, 0})  -- roll=23 → spider
	check(#s == 1 and s[1].id == "default:spider", "生成: 任意挖掘 6% 概率生成 spider")
end
do
	local s = dig(w_over, dug_block, {29, 0, 0})  -- roll=29 → creeper
	check(#s == 1 and s[1].id == "default:creeper", "生成: 任意挖掘 6% 概率生成 creeper")
end
do
	local s = dig(w_over, dug_block, {94, 0, 0})  -- roll=94 → zombie
	check(#s == 1 and s[1].id == "default:zombie", "生成: 任意挖掘 7% 概率生成 zombie")
end
do
	local s = dig(w_over, stone_block, {1})  -- 挖的不是草方块 → 无生成
	check(#s == 0, "生成: 挖非草方块不触发 pig/sheep 分支")
end
do
	local w_nether = make_world(1, nether_surface, {})
	local s = dig(w_nether, netherrack_block, {1, 0, 0})  -- 下界挖下界岩 → zombified_piglin
	check(#s == 1 and s[1].id == "default:zombified_piglin" and s[1].dimension == 1,
		"生成: 下界挖下界岩 8% 概率生成 zombified_piglin（维度 1）")
end
do
	local w_nether = make_world(1, nether_surface, {})
	local s = dig(w_nether, stone_block, {1})  -- 下界挖非下界岩 → 无生成
	check(#s == 0, "生成: 下界挖非下界岩不生成")
end
do
	local w_nether = make_world(1, nether_surface, {})
	local s = dig(w_nether, netherrack_block, {50, 0, 0})  -- roll=50 > 8 → 无生成
	check(#s == 0, "生成: 下界挖掘概率未命中不生成")
end

-- ---------------------------------------------------------------- 6. 上限
do
	local twelve = {}
	for i = 1, 12 do twelve[i] = make_entity("default:pig", 100 + i, 0, 0, 32.01) end
	local w_full = make_world(0, grass_surface, twelve)
	local s = dig(w_full, dug_block, {1})
	check(#s == 0, "上限: pig 达 12 只（每类型上限）后不再生成")
end
do
	local many = {}
	for i = 1, 12 do many[#many + 1] = make_entity("default:pig", 100 + i, 0, 0, 32.01) end
	for i = 1, 12 do many[#many + 1] = make_entity("default:zombie", 200 + i, 0, 0, 32.01) end
	for i = 1, 12 do many[#many + 1] = make_entity("default:sheep", 300 + i, 0, 0, 32.01) end
	for i = 1, 4 do many[#many + 1] = make_entity("default:spider", 400 + i, 0, 0, 32.01) end
	local w_total = make_world(0, grass_surface, many)  -- 总数 40
	local s = dig(w_total, dug_block, {1})
	check(#s == 0, "上限: 总数达 40 只（总数上限）后不再生成")
end
do
	local s
	spawned = {}
	cfg["default:mobs_enabled"] = false
	set_rng({1, 0, 0})
	listeners[Event.BlockDigged](pos0, dug_block, player_mock, w_over, {}, make_server())
	s = spawned
	cfg["default:mobs_enabled"] = true
	check(#s == 0, "上限: mobs_enabled=false 时完全关闭生成")
end

-- ---------------------------------------------------------------- 7. PlayerConnected
do
	spawned = {}
	-- 此前用例已把会话计数推到上限附近，这里放宽 mock 配置以隔离验证连入生成本身
	cfg["default:mobs_max_total"] = 1000
	cfg["default:mobs_max_per_type"] = 1000
	set_rng({0, 0, 1, 1, -1, 2, 3, -4})
	listeners[Event.PlayerConnected]({x = 0, y = 0, z = 32}, player_mock, {}, make_server())
	check(#spawned == 4
		and spawned[1].id == "default:pig" and spawned[2].id == "default:pig"
		and spawned[3].id == "default:sheep" and spawned[4].id == "default:zombie"
		and spawned[1].dimension == 0,
		"生成: 主世界玩家连入生成 2 猪 + 1 羊 + 1 僵尸（脚底平面）")
end
do
	spawned = {}
	local nether_player = {dimension = function() return 1 end}
	set_rng({0, 0})
	listeners[Event.PlayerConnected]({x = 0, y = 0, z = 40}, nether_player, {}, make_server())
	check(#spawned == 1 and spawned[1].id == "default:zombified_piglin" and spawned[1].dimension == 1,
		"生成: 下界玩家连入生成 1 只 zombified_piglin")
end

-- ---------------------------------------------------------------- 8. on_collision 限频聊天
do
	local srv = make_server()
	local e = make_entity("default:pig", 7, 0, 0, 32.01)
	for i = 1, 40 do registered.pig.on_collision(e, player_mock, srv) end
	check(#chat_log == 1 and chat_log[1] == "Pig: Oink oink!",
		"交互: on_collision 40 tick 限频一次，pig 文案正确")
	local e2 = make_entity("default:creeper", 8, 0, 0, 32.01)
	for i = 1, 40 do registered.creeper.on_collision(e2, player_mock, srv) end
	check(#chat_log == 2 and chat_log[2] == "Creeper: Sssss... (no explosion/damage system in this engine)",
		"交互: creeper 文案正确（无爆炸/伤害系统，仅提示）")
end

-- ----------------------------------------------------------------
math.random = orig_random
print(failures == 0 and "\nALL TESTS PASSED" or ("\n" .. failures .. " TEST(S) FAILED"))
os.exit(failures == 0 and 0 or 1)
