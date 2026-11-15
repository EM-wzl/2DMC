-- ===========================================================================
--  生物掉落表（mob drops）
--
--  引擎在生物血量归零时触发 `Event.EntityKilled` 事件
--  （见 source/server/network/ServerCommandHandler.cpp 的 PlayerAttackEntity
--   处理，以及 LuaEventType::EntityKilled）。这里监听它并按表掉落物品。
--
--  掉落物以 item_drop 实体的形式掉落在生物死亡的位置，玩家走过去捡起
--  （item_drop.lua 里 on_collision 会把物品塞进玩家背包）。
--
--  表格式：
--      { "物品 id", 最小数量, 最大数量 }
--  数量为 0 表示该物品按概率掉落（见 dropChance）。
-- ===========================================================================

-- 每种生物的掉落表
--
-- 注意：键名必须与 mod:entity 的 id 完全一致（见 entities.lua）。
-- 当前实体清单（19 种）：
--   主世界被动：pig / sheep / cow / chicken / rabbit_brown / ocelot / wolf
--               / axolotl_brown / snow_golem / squid
--   主世界敌对：zombie / skeleton / creeper / spider / slime / bat
--   下界：zombified_piglin / pig_zombie / ghast
local mob_drop_table = {
	-- 主世界 被动
	pig = {
		{"default:porkchop_raw", 1, 3},
	},
	cow = {
		{"default:beef_raw", 1, 3},
		{"default:leather", 0, 2},
	},
	sheep = {
		{"default:mutton_raw", 1, 2},
		-- 注：本工程没有独立的羊毛物品（items.lua 里只有 string），
		-- 所以羊毛暂不掉落，避免掉落表里出现无法解析的物品 id。
	},
	chicken = {
		{"default:chicken_raw", 1, 1},
		{"default:feather", 0, 2},
	},
	-- 兔子：实体 id 是 rabbit_brown（贴图按毛色拆分），不是 rabbit
	rabbit_brown = {
		{"default:rabbit_raw", 0, 1},
		{"default:rabbit_hide", 0, 1},
	},
	-- 主世界 敌对
	zombie = {
		{"default:rotten_flesh", 0, 2},
	},
	skeleton = {
		{"default:bone", 0, 2},
		{"default:arrow", 0, 2},
	},
	creeper = {
		{"default:gunpowder", 0, 2},
	},
	spider = {
		{"default:string", 0, 2},
		{"default:spider_eye", 0, 1},
	},
	slime = {
		{"default:slimeball", 0, 2},
	},
	squid = {
		{"default:string", 0, 2},
	},
	ocelot = {
		{"default:leather", 0, 1},
	},
	-- 下界
	zombified_piglin = {
		{"default:rotten_flesh", 0, 1},
	},
	-- 僵尸猪人（pig_zombie）与僵尸猪灵是两种不同实体，掉落一致
	pig_zombie = {
		{"default:rotten_flesh", 0, 1},
	},
	ghast = {
		{"default:gunpowder", 0, 2},
	},
	-- 无掉落（MC 里也不掉东西，或本工程缺少对应物品）
	wolf = {},
	bat = {},
	axolotl_brown = {},
	snow_golem = {
		{"default:snowball", 0, 15},
	},
}

-- 掉落概率（0..1）：最小数量为 0 的物品按这个概率掉
local dropChance = 0.75

-- 生物被击杀 -> 掉落
openminer:add_listener(Event.EntityKilled, function(pos, entity, player, world, client, server)
	local ok, entity_type = pcall(function() return entity:get_type() end)
	if not ok or not entity_type then return end

	-- entity_type 形如 "default:pig"，去掉 mod 前缀
	local mob = entity_type:match("([^:]+)$")
	if not mob then return end

	local drops = mob_drop_table[mob]
	if not drops or #drops == 0 then return end

	local count = 0
	for _, drop in ipairs(drops) do
		local item_id, min_amount, max_amount = drop[1], drop[2], drop[3]

		local amount
		if min_amount >= max_amount then
			amount = min_amount
		elseif min_amount == 0 then
			-- 概率掉落
			if math.random() > dropChance then
				amount = 0
			else
				amount = math.random(1, max_amount)
			end
		else
			amount = math.random(min_amount, max_amount)
		end

		if amount > 0 then
			local spawned = pcall(function()
				mods["default"]:spawn_entity("default:item_drop", {
					position = {pos.x + 0.5, pos.y + 0.5, pos.z + 0.5},
					dimension = world:dimension():id(),
					-- item_stack 用**数组**形式 {物品 id, 数量}
					-- （引擎 LuaEntityLoader 读的是 table[1] / table[2]，不是键名）
					item_stack = {item_id, amount},
				})
			end)
			if spawned then
				count = count + 1
			end
		end
	end

	if count > 0 then
		-- client 可能为空（环境杀死），send_chat_message 接受 nil client
		server:send_chat_message(0, "击杀了 " .. mob .. "，掉落 " .. count .. " 组物品", client)
	end
end)
