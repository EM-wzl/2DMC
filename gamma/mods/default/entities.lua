--
-- =====================================================================================
--  entities.lua — 生物系统（pig / zombie）
--
--  基于 gamma 实体 API 的能力边界实现（详见 gamma/_mobs_research/mobs_report.md）：
--    * visual 仅支持 "inventorycube"（按 block_id 引用一个已注册方块的六面贴图渲染），
--      实体必须有 is_rotatable = true 才会被客户端绘制（RenderingController 的视图要求
--      DrawableComponent + PositionComponent + RotationComponent 三者齐全）
--    * 无生命值/伤害系统；Lua 不能写实体位置（PositionComponent 无 sol usertype）、
--      不能枚举世界中的实体、没有 per-entity tick 回调 —— 因此没有真实的移动/追逐 AI
--    * 可用的周期性驱动只有方块 on_tick 与事件监听器（BlockPlaced/BlockDigged/
--      BlockActivated/ItemActivated/PlayerConnected）
--    * 实体随存档持久化；despawn_entity 只能在 on_collision 内使用
--
--  因此本文件实现到 API 上限：
--    - pig / zombie 两个实体，贴图取自原版资源包（经 _mobs_research/make_mob_textures.py 转换）
--    - 客户端动画层"伪游荡"：translation 动画由客户端逐帧累加（min/max 往返），
--      pig 水平踱步 + 轻微起伏，zombie 沿另一轴踱步 + 绕竖直轴缓慢转身
--    - 生成：BlockDigged（有 world 对象，可做草方块表面检测，概率生成）+
--      PlayerConnected（该事件无 world 对象，生成在玩家脚底平面，见限制说明）
--    - 交互：on_collision 限频聊天消息（pig 哼哼 / zombie 抓握，无伤害可施加）
-- =====================================================================================
--

-- ---------------------------------------------------------------------------
-- 1. 技术方块：inventorycube visual 需要引用一个已注册方块来获得六面贴图。
--    tiles 六元素与面的对应（TilesDef::getTextureForFace，size=6 行）：
--    {Top, Bottom, East, West, South, North} —— 脸部贴图放南/北面。
--    这些方块不会被放置到世界中（不参与存档方块 ID 映射，无兼容性影响）。
-- ---------------------------------------------------------------------------
mod:block {
	id = "pig_block",
	name = "Pig",
	tiles = {"pig_top.png", "pig_bottom.png", "pig_side.png", "pig_side.png", "pig_face.png", "pig_face.png"},
}

mod:block {
	id = "zombie_block",
	name = "Zombie",
	tiles = {"zombie_top.png", "zombie_bottom.png", "zombie_side.png", "zombie_side.png", "zombie_face.png", "zombie_face.png"},
}

-- ---------------------------------------------------------------------------
-- 2. 实体定义
--    size/origin 让立方体水平居中于生成点、底面贴地；hitbox 与视觉盒一致
--    （Box 构造为 {x, y, z, sizeX, sizeY, sizeZ}，前三项是相对实体位置的偏移）。
--    动画在客户端逐帧应用，速度与帧率相关（60fps 下标定）。
-- ---------------------------------------------------------------------------
local oink_counter = 0
local zombie_hit_counter = 0

mod:entity {
	id = "pig",

	properties = {
		visual = {
			type = "inventorycube",
			size = 0.75,
			origin = {0.375, 0.375, 0},
			block_id = "default:pig_block",
		},

		is_rotatable = true,

		animation = {
			-- 水平踱步：±0.5 格往返，60fps 下约 0.09 格/秒
			{type = "translation", delta = {0.0015, 0, 0}, min = -0.5, max = 0.5, loop = true},
			-- 轻微起伏
			{type = "translation", delta = {0, 0, -0.0008}, min = -0.06, max = 0, loop = true},
		},

		hitbox = {-0.375, -0.375, 0, 0.75, 0.75, 0.75},
	},

	on_collision = function(entity, player, server)
		-- on_collision 在玩家与实体相交期间每 tick 触发，限频到约 2 秒一次
		oink_counter = oink_counter + 1
		if oink_counter % 40 == 0 then
			server:send_chat_message(0, "Pig: Oink oink!", player:client())
		end
	end,
}

mod:entity {
	id = "zombie",

	properties = {
		visual = {
			type = "inventorycube",
			size = 0.9,
			origin = {0.45, 0.45, 0},
			block_id = "default:zombie_block",
		},

		is_rotatable = true,

		animation = {
			-- 沿另一水平轴踱步（与 pig 区分）
			{type = "translation", delta = {0, 0.0012, 0}, min = -0.5, max = 0.5, loop = true},
			-- 缓慢转身（绕竖直轴，60fps 下约 4.8 度/秒）
			{type = "rotation", axis = {0, 0, 1}, angle = 0.08},
		},

		hitbox = {-0.45, -0.45, 0, 0.9, 0.9, 0.9},
	},

	on_collision = function(entity, player, server)
		zombie_hit_counter = zombie_hit_counter + 1
		if zombie_hit_counter % 30 == 0 then
			-- 引擎没有生命值/伤害系统，无法造成真实伤害，仅作交互提示
			server:send_chat_message(0, "Zombie grabs you! (no damage system in this engine)", player:client())
		end
	end,
}

-- ---------------------------------------------------------------------------
-- 3. 自然生成
--    触发点只有事件监听器；会话内计数上限防止无限累积（实体随存档持久化，
--    重启后计数清零会再补一批，见报告的已知限制）。
-- ---------------------------------------------------------------------------
mod:option("mobs_enabled", true)
mod:option("mobs_max_pigs", 12)
mod:option("mobs_max_zombies", 8)

local spawn_counts = {pig = 0, zombie = 0}

local function can_spawn(kind)
	if not openminer:get_config("default:mobs_enabled") then
		return false
	end
	local cap = openminer:get_config(kind == "pig" and "default:mobs_max_pigs" or "default:mobs_max_zombies")
	return spawn_counts[kind] < (cap or 8)
end

local function spawn_mob(kind, x, y, z, dimension)
	mods["default"]:spawn_entity("default:" .. kind, {
		position = {x, y, z},
		dimension = dimension,
	})
	spawn_counts[kind] = spawn_counts[kind] + 1
end

-- 从 zref 附近竖直向下找可站立表面：第一个"固体 + 上方两格空气"的方块顶面 z。
-- 未加载区块 get_block 返回 0（空气），扫描会自然终止于下限。
local function find_surface_z(world, x, y, zref)	for z = zref + 6, zref - 8, -1 do
		if world:get_block(x, y, z) ~= 0 then
			if world:get_block(x, y, z + 1) == 0 and world:get_block(x, y, z + 2) == 0 then
				return z + 1
			end
			return nil
		end
	end
	return nil
end

-- 挖方块时概率在附近草方块表面生成生物（此事件提供 world 对象，可做表面/草方块检测）
openminer:add_listener(Event.BlockDigged, function(pos, block, player, world, client, server)
	local roll = math.random(100)
	if roll <= 12 and can_spawn("pig") then
		local sx = pos.x + math.random(-5, 5)
		local sy = pos.y + math.random(-5, 5)
		local sz = find_surface_z(world, sx, sy, pos.z)
		-- 注：openminer.registry 在 mod 加载期为 nil（服务端在 loadMods 后才 setRegistry），
		-- 因此草方块用运行时 get_block_def 字符串比对，而不是加载期取数字 ID
		if sz and world:get_block_def(sx, sy, sz - 1):string_id() == "default:grass" then
			spawn_mob("pig", sx + 0.5, sy + 0.5, sz + 0.01, world:dimension():id())
		end
	elseif roll >= 94 and can_spawn("zombie") then
		local sx = pos.x + math.random(-5, 5)
		local sy = pos.y + math.random(-5, 5)
		local sz = find_surface_z(world, sx, sy, pos.z)
		if sz then
			spawn_mob("zombie", sx + 0.5, sy + 0.5, sz + 0.01, world:dimension():id())
		end
	end
end)

-- 玩家连入时在玩家周围生成一小批生物。
-- 限制：PlayerConnected 监听器只拿到 (pos, player, client, server)，没有 world 对象，
-- 无法做表面检测 —— 生物直接生成在玩家脚底同一水平面上（pos.z + 0.01），
-- 地形起伏时可能嵌进地表，此为 API 限制（详见报告）。
openminer:add_listener(Event.PlayerConnected, function(pos, player, client, server)
	if not openminer:get_config("default:mobs_enabled") then
		return
	end

	local dimension = player:dimension()
	for i = 1, 2 do
		if can_spawn("pig") then
			spawn_mob("pig",
				pos.x + math.random(-5, 5) + 0.5,
				pos.y + math.random(-5, 5) + 0.5,
				pos.z + 0.01, dimension)
		end
	end
	if can_spawn("zombie") then
		spawn_mob("zombie",
			pos.x + math.random(-4, 4) + 0.5,
			pos.y + math.random(-4, 4) + 0.5,
			pos.z + 0.01, dimension)
	end
end)
