--
-- =====================================================================================
--  gamma 流体系统（2DMC）
--
--  把控制台版（beta/beta1.8）的流体玩法移植到 3D：
--    * 水 7 级 / 岩浆 3 级流动，向下流动优先，失去支撑后逐级回退消失
--    * 静置机制：流动稳定后转为不 tick 的静态方块，稳态零开销
--    * 水接触岩浆 → 黑曜石（与控制台版规则一致）
--    * 桶取水/岩浆、水桶/岩浆桶放流体（客户端已支持持桶选中液体）
--
--  引擎约定（gamma/source 已核实）：
--    * z 轴为竖直方向（+z 向上），x/y 为水平方向
--    * 带 on_tick 的方块由 ServerChunk::tick 每个 tick 调用；
--      tick_randomly + tick_probability 则按概率随机调用（用于放慢岩浆）
--    * world:set_block 走 Chunk::setBlock，自动注册/注销 tick 列表；
--      但不会广播给客户端，改动后必须 server:send_block_update 同步
--    * 服务端 set_block 不触发 BlockPlaced/BlockDigged 事件，
--      所以流体自身状态变化后要手动 fluid_kick_neighbors 激活邻居
--    * 加载期 openminer.registry 尚不可用（引擎在 loadMods 之后才 setRegistry），
--      因此所有 ID 解析延迟到首次运行时调用（ensure_ids）
--
--  新方块全部带 ci_ignore 组（不进创造物品栏），且在 init.lua 最后加载，
--  数字 ID 追加在所有既有方块之后，旧存档兼容。
-- =====================================================================================
--

if not mods then mods = {} end

local MAX_LEVEL = { water = 7, lava = 3 }
-- 岩浆用随机刻放慢流速（约每 3 tick 一步），水每 tick 更新
local TICK_PROBABILITY = { water = nil, lava = 0.3 }

local ID_WATER, ID_LAVA, ID_OBSIDIAN  -- 延迟解析（ensure_ids）
local FLUID_INFO = {}  -- [blockId] = { kind = "water"|"lava", level = 0..N, still = true|nil }
local ACTIVE_ID = {}   -- [kind] = 活跃源方块 id（会 tick，静置后还原为静态源）
local FLOW_ID   = {}   -- [kind][level] = 流动方块 id
local STILL_ID  = {}   -- [kind][level] = 静置方块 id（不 tick）
local ids_ready = false

local ALL_DIRS  = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} }
local FLAT_DIRS = { {1,0}, {-1,0}, {0,1}, {0,-1} }

-- 首次运行时调用：从注册表解析所有流体方块数字 id
-- （加载期 openminer.registry 为 nil，不能在 dofile 时解析）
local function ensure_ids()
	if ids_ready then return end
	local R = openminer.registry

	ID_WATER    = R:get_block_from_string("default:water"):id()
	ID_LAVA     = R:get_block_from_string("default:lava"):id()
	ID_OBSIDIAN = R:get_block_from_string("default:obsidian"):id()

	ACTIVE_ID["water"] = R:get_block_from_string("default:water_active"):id()
	ACTIVE_ID["lava"]  = R:get_block_from_string("default:lava_active"):id()

	FLUID_INFO[ACTIVE_ID["water"]] = { kind = "water", level = 0 }
	FLUID_INFO[ACTIVE_ID["lava"]]  = { kind = "lava",  level = 0 }

	FLOW_ID["water"] = {}
	STILL_ID["water"] = {}
	FLOW_ID["lava"] = {}
	STILL_ID["lava"] = {}
	for lvl = 1, MAX_LEVEL.water do
		local fid = R:get_block_from_string("default:water_flow_" .. lvl):id()
		local sid = R:get_block_from_string("default:water_still_" .. lvl):id()
		FLOW_ID["water"][lvl] = fid
		STILL_ID["water"][lvl] = sid
		FLUID_INFO[fid] = { kind = "water", level = lvl }
		FLUID_INFO[sid] = { kind = "water", level = lvl, still = true }
	end
	for lvl = 1, MAX_LEVEL.lava do
		local fid = R:get_block_from_string("default:lava_flow_" .. lvl):id()
		local sid = R:get_block_from_string("default:lava_still_" .. lvl):id()
		FLOW_ID["lava"][lvl] = fid
		STILL_ID["lava"][lvl] = sid
		FLUID_INFO[fid] = { kind = "lava", level = lvl }
		FLUID_INFO[sid] = { kind = "lava", level = lvl, still = true }
	end

	ids_ready = true
end

-- 查询某方块作为 kind 流体的支撑层级：静态源/活跃源=0，流动/静置=级数，非流体=nil
local function level_of(kind, id)
	if not id or id == 0 then return nil end
	if kind == "water" and id == ID_WATER then return 0 end
	if kind == "lava" and id == ID_LAVA then return 0 end
	local f = FLUID_INFO[id]
	if f and f.kind == kind then return f.level end
	return nil
end

-- 激活 (x,y,z) 的邻居静置/静态流体：转成会 tick 的对应方块。
-- 由事件监听（玩家挖/放方块）与流体自身状态变化调用，是回退级联的驱动源。
function fluid_kick_neighbors(world, server, x, y, z)
	ensure_ids()
	for _, d in ipairs(ALL_DIRS) do
		local nx, ny, nz = x + d[1], y + d[2], z + d[3]
		local id = world:get_block(nx, ny, nz)
		local f = FLUID_INFO[id]
		local tid = nil
		if f then
			if f.still then tid = FLOW_ID[f.kind][f.level] end
		elseif id == ID_WATER then
			tid = ACTIVE_ID.water
		elseif id == ID_LAVA then
			tid = ACTIVE_ID.lava
		end
		if tid and tid ~= id then
			world:set_block(nx, ny, nz, tid)
			server:send_block_update(nx, ny, nz, tid)
		end
	end
end

-- 玩家放置方块后调用：若放的是静态水/岩浆（创造物品栏直接放置），转成活跃源立刻流动
function fluid_on_placed(world, server, x, y, z)
	ensure_ids()
	local id = world:get_block(x, y, z)
	local tid = nil
	if id == ID_WATER then tid = ACTIVE_ID.water
	elseif id == ID_LAVA then tid = ACTIVE_ID.lava end
	if tid then
		world:set_block(x, y, z, tid)
		server:send_block_update(x, y, z, tid)
	end
	fluid_kick_neighbors(world, server, x, y, z)
end

-- 桶取水/岩浆：block 为射线选中的液体方块；静态源取水后源仍在，流动/静置格被舀走
function fluid_bucket_fill(pos, block, player, world, client, server)
	ensure_ids()
	local id = world:get_block(pos.x, pos.y, pos.z)
	local static_kind = (id == ID_WATER and "water") or (id == ID_LAVA and "lava") or nil
	local f = FLUID_INFO[id]
	local kind = static_kind or (f and f.kind) or nil
	if not kind then return false end

	local filled = (kind == "water") and "default:bucket_water" or "default:bucket_lava"
	local inv = player:inventory()

	for y = 0, 3 do
		for x = 0, 8 do
			local st = inv:get_stack(x, y)
			if st:amount() > 0 and st:item():string_id() == "default:bucket_empty" then
				inv:set_stack(x, y, (st:amount() > 1) and "default:bucket_empty" or "", st:amount() - 1)
				inv:add_stack(filled, 1)
				if not static_kind then
					world:set_block(pos.x, pos.y, pos.z, 0)
					server:send_block_update(pos.x, pos.y, pos.z, 0)
					fluid_kick_neighbors(world, server, pos.x, pos.y, pos.z)
				end
				server:send_player_inv_update(client.id, client)
				return true
			end
		end
	end
	return false
end

-- 生成 kind 流体的 on_tick 闭包
-- 引擎调用签名：on_tick(pos, block, chunk, world, server)
--   （第 5 参 ServerCommandHandler 为本项目新增，见 ServerBlock.cpp）
local function make_tick(kind)
	return function(pos, block, chunk, world, server)
		ensure_ids()
		local x, y, z = pos.x, pos.y, pos.z
		local self = FLUID_INFO[world:get_block(x, y, z)]
		if not self then return end

		local level = self.level
		local max_level = MAX_LEVEL[kind]
		local changed = false
		local static_block_id = (kind == "water") and ID_WATER or ID_LAVA

		local function place(bx, by, bz, bid)
			world:set_block(bx, by, bz, bid)
			-- 读回校验：目标区块未加载时 set_block 是空操作，
			-- 返回 false 让调用方视为“放置未生效”，避免无限重试（如虚空瀑布）
			if world:get_block(bx, by, bz) ~= bid then return false end
			server:send_block_update(bx, by, bz, bid)
			return true
		end

		-- 1) 岩浆遇水熄灭：任意方向接触水 → 自身变黑曜石
		if kind == "lava" then
			for _, d in ipairs(ALL_DIRS) do
				local nid = world:get_block(x + d[1], y + d[2], z + d[3])
				local nf = FLUID_INFO[nid]
				if (nf and nf.kind == "water") or nid == ID_WATER then
					place(x, y, z, ID_OBSIDIAN)
					fluid_kick_neighbors(world, server, x, y, z)
					return
				end
			end
		end

		-- 2) 支撑校验（活跃源 level=0 跳过）：
		--    需要水平相邻存在层级严格更低的同类流体，或正上方是同类流体（水柱）
		if level > 0 then
			local best = nil
			if level_of(kind, world:get_block(x, y, z + 1)) then best = 1 end
			for _, d in ipairs(FLAT_DIRS) do
				local nl = level_of(kind, world:get_block(x + d[1], y + d[2], z))
				if nl and nl < level then
					if not best or nl + 1 < best then best = nl + 1 end
				end
			end
			if not best then
				-- 失去支撑 → 消失（回退级联由 kick 驱动）
				place(x, y, z, 0)
				fluid_kick_neighbors(world, server, x, y, z)
				return
			end
			if best < level then
				-- 找到更强水源 → 升级（层级变小）
				place(x, y, z, FLOW_ID[kind][best])
				fluid_kick_neighbors(world, server, x, y, z)
				return
			end
		end

		-- 3) 水把相邻岩浆烧成黑曜石（6 个方向）
		if kind == "water" then
			for _, d in ipairs(ALL_DIRS) do
				local nx, ny, nz = x + d[1], y + d[2], z + d[3]
				local nid = world:get_block(nx, ny, nz)
				local nf = FLUID_INFO[nid]
				if (nf and nf.kind == "lava") or nid == ID_LAVA then
					place(nx, ny, nz, ID_OBSIDIAN)
					changed = true
				end
			end
			if changed then
				fluid_kick_neighbors(world, server, x, y, z)
				return
			end
		end

		-- 4) 向下流动：下方为空气 → 生成最强流动层级
		--    （放置未生效 = 下方在未加载区块/世界底 → 视作被阻挡，继续横向铺开）
		if world:get_block(x, y, z - 1) == 0 then
			if place(x, y, z - 1, FLOW_ID[kind][1]) then return end
		end

		-- 5) 水平扩散：落地（下方不是同类流体）且未到最末级才铺开
		local below = world:get_block(x, y, z - 1)
		if level_of(kind, below) == nil and level < max_level then
			for _, d in ipairs(FLAT_DIRS) do
				local nx, ny = x + d[1], y + d[2]
				local nid = world:get_block(nx, ny, z)
				local nf = FLUID_INFO[nid]
				if nid == 0 then
					if place(nx, ny, z, FLOW_ID[kind][level + 1]) then changed = true end
				elseif nf and nf.kind == kind and nf.level > level + 1 then
					-- 邻居层级高于本源能提供的 → 降级（静置邻居借此重估）
					if place(nx, ny, z, FLOW_ID[kind][level + 1]) then changed = true end
				end
			end
		end

		-- 6) 静置：本 tick 无任何改动 → 转为不 tick 的静态方块（稳态零开销）
		if not changed then
			local sid = (level == 0) and static_block_id or STILL_ID[kind][level]
			place(x, y, z, sid)
		end
	end
end

-- 注册流体方块族（仅注册；ID 解析延迟到 ensure_ids）
local function register_fluid_family(kind, base_tile, fog_depth, fog_color, is_light_source)
	local max_level = MAX_LEVEL[kind]
	local prob = TICK_PROBABILITY[kind]
	local label = (kind == "water") and "水" or "岩浆"

	local function base_attrs()
		local t = {
			tiles = base_tile,
			draw_type = "liquid",
			is_opaque = false,
			fog_depth = fog_depth,
			fog_color = fog_color,
			groups = { ci_ignore = 1 },
		}
		if is_light_source then t.is_light_source = true end
		return t
	end

	local function apply_tick(t, with_tick)
		if with_tick then
			t.on_tick = make_tick(kind)
			if prob then
				t.tick_randomly = true
				t.tick_probability = prob
			end
		end
		return t
	end

	-- 活跃源
	local active = base_attrs()
	active.id = kind .. "_active"
	active.name = "活" .. label .. "（流体）"
	if kind == "water" then active.color_multiplier = {51, 115, 255, 217} end
	apply_tick(active, true)
	mod:block(active)

	-- 流动/静置各级
	for lvl = 1, max_level do
		local flow = base_attrs()
		flow.id = kind .. "_flow_" .. lvl
		flow.name = "流动" .. label .. "（" .. lvl .. "）"
		if kind == "water" then flow.color_multiplier = {51, 115, 255, 217 - (lvl - 1) * 18} end
		apply_tick(flow, true)
		mod:block(flow)

		local still = base_attrs()
		still.id = kind .. "_still_" .. lvl
		still.name = "静" .. label .. "（" .. lvl .. "）"
		if kind == "water" then still.color_multiplier = {51, 115, 255, 217 - (lvl - 1) * 18} end
		apply_tick(still, false)
		mod:block(still)
	end
end

register_fluid_family("water", "water.png", 20.0, {0, 128, 255}, false)
register_fluid_family("lava",  "lava.png",  10.0, {255, 128, 0}, true)
