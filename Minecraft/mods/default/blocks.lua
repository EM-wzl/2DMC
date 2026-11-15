--
-- =====================================================================================
--
--  OpenMiner
--
--  Copyright (C) 2018-2020 Unarelith, Quentin Bazin <openminer@unarelith.net>
--  Copyright (C) 2019-2020 the OpenMiner contributors (see CONTRIBUTORS.md)
--
--  This file is part of OpenMiner.
--
--  OpenMiner is free software; you can redistribute it and/or
--  modify it under the terms of the GNU Lesser General Public
--  License as published by the Free Software Foundation; either
--  version 2.1 of the License, or (at your option) any later version.
--
--  OpenMiner is distributed in the hope that it will be useful,
--  but WITHOUT ANY WARRANTY; without even the implied warranty of
--  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
--  Lesser General Public License for more details.
--
--  You should have received a copy of the GNU Lesser General Public License
--  along with OpenMiner; if not, write to the Free Software Foundation, Inc.,
--  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
--
-- =====================================================================================
--

mod:block {
	id = "dirt",
	name = "泥土",
	tiles = "dirt.png",
	groups = {
		om_material_dirt = 1,
	},
}

mod:block {
	id = "cobblestone",
	name = "圆石",
	tiles = "cobblestone.png",

	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "grass",
	name = "草方块",
	tiles = {"grass_block_top.png", "dirt.png", "grass_block_side.png"},
	color_multiplier = {129, 191, 91, 255},
	groups = {
		om_material_dirt = 1,
	},

	item_drop = {
		id = mod:id()..":dirt",
		amount = 1
	}
}

mod:block {
	id = "oak_leaves",
	name = "橡木树叶",
	tiles = "oak_leaves.png",
	color_multiplier = {106, 173, 51, 255},
	hardness = 0,
	draw_type = "leaves",
	-- is_opaque = false, -- FIXME
}

mod:block {
	id = "oak_wood",
	name = "橡木原木",
	tiles = {"oak_log_top.png", "oak_log.png"},
	groups = {
		om_material_wood = 1,
	}
}

mod:block {
	id = "stone",
	name = "石头",
	tiles = "stone.png",
	hardness = 1.5,
	harvest_requirements = Capability.Pickaxe,
	item_drop = {
		id = mod:id()..":cobblestone",
		amount = 1
	},
}

mod:block {
	id = "sand",
	name = "沙子",
	tiles = "sand.png",
	groups = {
		om_material_sand = 1
	}
}

mod:block {
	id = "water",
	name = "水",

	tiles = "water.png",
	color_multiplier = {51, 115, 255, 217},

	draw_type = "liquid",
	is_opaque = false,
	is_collidable = false,

	fog_depth = 20.0,
	fog_color = {0, 128, 255},
}

mod:block {
	id = "glass",
	name = "玻璃",
	tiles = "glass.png",
	draw_type = "glass",
	is_opaque = false,
}

mod:block {
	id = "coal_ore",
	name = "煤矿石",
	tiles = "coal_ore.png",
	hardness = 3,
	harvest_requirements = Capability.Pickaxe,
	item_drop = {
		id = mod:id()..":coal",
		amount = 1
	},
}

mod:block {
	id = "oak_planks",
	name = "橡木木板",
	tiles = "oak_planks.png",

	hardness = 1,

	groups = {
		om_material_wood = 1,
		default_planks = 1
	}
}

mod:block {
	id = "glowstone",
	name = "荧石",
	tiles = "glowstone.png",
	is_light_source = true,
	groups = {
		om_material_stone = 1
	},
}

mod:block {
	id = "iron_ore",
	name = "铁矿石",
	tiles = "iron_ore.png",
	hardness = 3,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "dandelion",
	name = "蒲公英",
	tiles = "dandelion.png",
	hardness = 0,
	draw_type = "xshape",
	bounding_box = {0.25, 0.25, 0.0, 0.5, 0.5, 0.5},
}

mod:block {
	id = "tallgrass",
	name = "草方块",
	tiles = "grass.png",
	color_multiplier = {129, 191, 91, 255},
	hardness = 0,
	draw_type = "xshape",
}

mod:block {
	id = "stonebrick",
	name = "石砖",
	tiles = "stonebrick.png",
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "bricks",
	name = "红砖块",
	tiles = "bricks.png",
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "clay",
	name = "黏土",
	tiles = "clay.png",
	item_drop = {
		id = mod:id()..":clay_ball",
		amount = 4
	}
}

mod:block {
	id = "oak_slab",
	name = "橡木台阶",
	tiles = "oak_planks.png",
	groups = {
		om_material_wood = 1,
	},

	draw_type = "boundingbox",
	is_opaque = false,

	bounding_box = {0, 0, 0, 1, 1, 0.5}
}

-- ============================================================
--  传送门（Minecraft 风格：维度专属门 + 目标维度留返回门）
-- ============================================================
--
--  MC 的规则是「双向配对」，不是维度循环：
--    主世界的门  → 下界的门（紫色的 nether_portal）
--    下界的门    → 主世界的门
--    末地的门    → 主世界的门
--
--  所以从主世界进下界后，站在下界那扇紫色门旁右键就能原路返回，
--  而不是像之前那样按 (dim+1)%3 循环（那样在末地会莫名其妙跳到主世界）。
--
--  坐标换算（MC 的 1:8 缩放）：
--    下界/末地坐标 = (主世界坐标 - 64) / 8
--  本引擎没有做缩放，直接用同坐标，简单可预期。
--
local function portal_target_dimension(portal_kind)
	-- portal_kind: "overworld" | "overworld_end" | "nether" | "end"
	if portal_kind == "nether" then return 0 end
	if portal_kind == "end" then return 0 end
	if portal_kind == "overworld_end" then return 2 end  -- 主世界的末地门 -> 末地
	return 1 -- 主世界的门 -> 下界
end

-- 在目标维度找一扇已经存在的传送门（避免每次传送都造一扇新的）。
--
-- 以「玩家当前位置」为中心做粗略扫描（±32 格，步长 4）。
-- 注意 world:get_block(x,y,z) 用的是世界坐标，x/z 是水平轴、y 是垂直轴。
local function find_existing_portal(world, portal_block_id, cx, cy, cz)
	if not world then return nil end

	for dx = -32, 32, 4 do
		for dz = -32, 32, 4 do
			for dy = -24, 24, 4 do
				local bx, by, bz = cx + dx, cy + dy, cz + dz

				local ok, id = pcall(function() return world:get_block(bx, by, bz) end)
				if ok and id and id ~= 0 then
					local ok2, sid = pcall(function()
						return world:get_block_def(bx, by, bz):string_id()
					end)
					if ok2 and sid == portal_block_id then
						return {x = bx, y = by, z = bz}
					end
				end
			end
		end
	end

	return nil
end

-- 执行一次「维度专属传送门」的传送。
local function activate_portal(portal_kind, target_portal_id, frame_id, player, client, server)
	local target_dim = portal_target_dimension(portal_kind)

	local px = math.floor(player:x())
	local py = math.floor(player:y())
	local pz = math.floor(player:z())

	-- 在目标维度找已有的返回门（以玩家当前位置为中心）
	local target_world = server:get_world(target_dim)
	local existing = find_existing_portal(target_world, target_portal_id, px, py, pz)

	local dest_x, dest_y, dest_z
	if existing then
		dest_x, dest_y, dest_z = existing.x, existing.y, existing.z
	else
		-- 没有现成的门：在目标维度就地造一扇（4x5，底部留一格门框）
		dest_x, dest_y, dest_z = px, py, pz
		server:create_portal(target_dim, dest_x, dest_y, dest_z, target_portal_id, frame_id)
	end

	server:send_player_change_dimension(client.id, dest_x, dest_y, dest_z, target_dim, client)
end

-- 主世界的门（紫色外观），通往下界
mod:block {
	id = "portal",
	name = "传送门",
	tiles = "portal.png",

	draw_type = "glass",
	is_opaque = false,

	on_block_activated = function(pos, block, player, world, client, server)
		activate_portal("overworld", "default:nether_portal", "default:obsidian",
			player, client, server)
	end,
}

mod:block {
	id = "netherrack",
	name = "下界岩",
	tiles = "netherrack.png",
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "soul_sand",
	name = "灵魂沙",
	tiles = "soul_sand.png",
	groups = {
		om_material_sand = 1
	}
}

mod:block {
	id = "lava",
	name = "岩浆",
	tiles = "lava.png",

	draw_type = "liquid",
	is_light_source = true,
	is_opaque = false,
	is_collidable = false,

	fog_depth = 10.0,
	fog_color = {255, 128, 0},
}

mod:block {
	id = "cactus",
	name = "仙人掌",
	tiles = {"cactus_top.png", "cactus_bottom.png", "cactus_side.png"},
	hardness = 0,
	is_opaque = false,

	draw_type = "cactus",
	bounding_box = {1/16, 1/16, 0, 14/16, 14/16, 1};

	on_block_destroyed = function(pos, world)
		if world:get_block(pos.x, pos.y, pos.z + 1) == world:get_block(pos.x, pos.y, pos.z) then
			-- FIXME: Make something like "player:break_block(pos.x, pos.y, pos.z + 1)"
			world:set_block(pos.x, pos.y, pos.z + 1, 0)
		end
	end,
}

mod:block {
	id = "deadbush",
	name = "枯萎的灌木",
	tiles = "deadbush.png",
	draw_type = "xshape",
	hardness = 0,
}

mod:block {
	id = "debug",
	name = "调试方块",
	tiles = {
		"debug/f.png",
		"debug/g.png",
		"debug/j.png",
		"debug/l.png",
		"debug/p.png",
		"debug/r.png",
	},
}

mod:block {
	id = "obsidian",
	name = "黑曜石",
	tiles = "obsidian.png",

	hardness = 8,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "reeds",
	name = "甘蔗",
	tiles = "reeds.png",
	inventory_image = "reeds_item.png",

	draw_type = "xshape",
	hardness = 0,

	on_block_destroyed = function(pos, world)
		if world:get_block(pos.x, pos.y, pos.z + 1) == world:get_block(pos.x, pos.y, pos.z) then
			world:set_block(pos.x, pos.y, pos.z + 1, 0)
		end
	end,
}

mod:block {
	id = "redstone_lamp",
	name = "红石灯",
	tiles = "redstone_lamp_off.png",
	groups = {
		om_material_stone = 1
	},

	states = {
		{ is_light_source = true, tiles = "redstone_lamp_on.png" }
	},

	on_block_activated = function(pos, block, player, world, client, server)
		local next_state = math.abs(world:get_block_state(pos.x, pos.y, pos.z):id() - 1)
		world:set_block_state(pos.x, pos.y, pos.z, next_state)
	end
}

mod:block {
	id = "farmland",
	name = "耕地",
	tiles = {"farmland_dry.png", "dirt.png", "dirt.png"},
	is_opaque = false,
	groups = {
		om_material_dirt = 1,
		om_farmland = 1
	},

	states = {
		{ alt_tiles = {"farmland_wet.png", "dirt.png", "dirt.png"} },
	},

	draw_type = "boundingbox",
	bounding_box = {0, 0, 0, 1, 1, 15 / 16},
}

mod:block {
	id = "grass_path",
	name = "草径",
	tiles = {"grass_path_top.png", "dirt.png", "grass_path_side.png"},
	is_opaque = false,
	groups = {
		om_material_dirt = 1,
	},

	draw_type = "boundingbox",
	bounding_box = {0, 0, 0, 1, 1, 15 / 16},
}

mod:block {
	id = "nether_wart",
	name = "下界疣",
	tiles = "nether_wart_stage_0.png",
	draw_type = "xshape",
	inventory_image = "nether_wart.png",
	hardness = 0,

	placement_constraints = {
		[{0, 0, -1}] = {
			block = "default:soul_sand",
			is_whitelist = true
		}
	},

	bounding_box = {0, 0, 0, 1, 1, 4.0 / 16.0},
	draw_offset = {0, 0, -1.0 / 16.0},

	states = {
		{ tiles = "nether_wart_stage_1.png", bounding_box = {0, 0, 0, 1, 1, 8.0 / 16.0}, },  -- 1
		{ tiles = "nether_wart_stage_2.png", bounding_box = {0, 0, 0, 1, 1, 12.0 / 16.0}, },  -- 2
	},

	tick_randomly = true,
	tick_probability = 0.01,

	on_tick = function(pos, block, chunk, world)
		local block_param = world:get_data(pos.x, pos.y, pos.z)
		local current_state = block:param():get_param(BlockParamType.State, block_param)
		if current_state < 2 then
			world:set_data(pos.x, pos.y, pos.z, current_state + 1)
		end
	end,

	on_block_activated = function(pos, block, player, world, client, server)
		local block_param = world:get_data(pos.x, pos.y, pos.z)
		local current_state = block:param():get_param(BlockParamType.State, block_param)
		if current_state >= 2 then
			world:set_data(pos.x, pos.y, pos.z,
				block:param():set_param(BlockParamType.State, block_param, 0))

			-- FIXME: It should drop the item if 'default:use_item_drops' is enabled
			local item_stack = ItemStack.new("default:nether_wart", 1)
			mods["default"]:give_item_stack(player, item_stack)
		end
	end
}

mod:block {
	id = "seeds",
	name = "种子（开发中）",
	tiles = "wheat_stage_0.png",
	draw_type = "xshape",
	inventory_image = "seeds_wheat.png",
	hardness = 0,

	placement_constraints = {
		[{0, 0, -1}] = {
			block = "default:farmland",
			is_whitelist = true
		}
	},

	bounding_box = {0, 0, 0, 1, 1, 1.0 / 16.0},
	draw_offset = {0, 0, -1.0 / 16.0},

	states = {
		{ tiles = "wheat_stage_1.png", bounding_box = {0, 0, 0, 1, 1, 3.0 / 16.0}, },  -- 1
		{ tiles = "wheat_stage_2.png", bounding_box = {0, 0, 0, 1, 1, 5.0 / 16.0}, },  -- 2
		{ tiles = "wheat_stage_3.png", bounding_box = {0, 0, 0, 1, 1, 8.0 / 16.0}, },  -- 3
		{ tiles = "wheat_stage_4.png", bounding_box = {0, 0, 0, 1, 1, 10.0 / 16.0}, }, -- 4
		{ tiles = "wheat_stage_5.png", bounding_box = {0, 0, 0, 1, 1, 12.0 / 16.0}, }, -- 5
		{ tiles = "wheat_stage_6.png", bounding_box = {0, 0, 0, 1, 1, 14.0 / 16.0}, }, -- 6
		{ tiles = "wheat_stage_7.png", bounding_box = {0, 0, 0, 1, 1, 1}, },           -- 7
	},

	tick_randomly = true,
	tick_probability = 0.01,

	on_tick = function(pos, block, chunk, world)
		local block_param = world:get_data(pos.x, pos.y, pos.z)
		local current_state = block:param():get_param(BlockParamType.State, block_param)
		if current_state < 7 then
			world:set_data(pos.x, pos.y, pos.z, current_state + 1)
		end
	end,

	on_block_activated = function(pos, block, player, world, client, server)
		local block_param = world:get_data(pos.x, pos.y, pos.z)
		local current_state = block:param():get_param(BlockParamType.State, block_param)
		if current_state >= 7 then
			world:set_data(pos.x, pos.y, pos.z,
				block:param():set_param(BlockParamType.State, block_param, 0))

			-- FIXME: It should drop the item if 'default:use_item_drops' is enabled
			local item_stack = ItemStack.new("default:wheat", 1)
			mods["default"]:give_item_stack(player, item_stack)
		end
	end
}

mod:block {
	id = "torch",
	name = "火把（开发中）",
	tiles = "torch_on.png",

	is_light_source = true,
	is_collidable = false,
	is_opaque = false,

	inventory_image = "../blocks/torch_on.png",

	draw_type = "boundingbox",

	bounding_box = {7 / 16, 7 / 16, 0, 2 / 16, 2 / 16, 10 / 16},

	hardness = 0
}

mod:block {
	id = "stone_andesite",
	name = "安山岩",
	tiles = "stone_andesite.png",
	hardness = 1.5,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "stone_granite",
	name = "花岗岩",
	tiles = "stone_granite.png",
	hardness = 1.5,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "stone_diorite",
	name = "闪长岩",
	tiles = "stone_diorite.png",
	hardness = 1.5,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "ice",
	name = "冰",
	tiles = "ice.png",
	draw_type = "glass",
	is_opaque = false
}

mod:block {
	id = "snow",
	name = "雪",
	tiles = "snow.png",
}

mod:block {
	id = "bookshelf",
	name = "书架",
	tiles = {"oak_planks.png", "oak_planks.png", "bookshelf.png"},
}

mod:block {
	id = "stonebrick_mossy",
	name = "苔石砖",
	tiles = "stonebrick_mossy.png",
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "stonebrick_cracked",
	name = "裂纹石砖",
	tiles = "stonebrick_cracked.png",
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "stonebrick_carved",
	name = "切制石砖",
	tiles = "stonebrick_carved.png",
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "cobblestone_mossy",
	name = "苔石",
	tiles = "cobblestone_mossy.png",
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "coal_block",
	name = "煤炭块",
	tiles = "coal_block.png",
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "diamond_block",
	name = "钻石块",
	tiles = "diamond_block.png",
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "gold_block",
	name = "金块",
	tiles = "gold_block.png",
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "lapis_block",
	name = "青金石块",
	tiles = "lapis_block.png",
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "iron_block",
	name = "铁块",
	tiles = "iron_block.png",
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "redstone_block",
	name = "红石块",
	tiles = "redstone_block.png",
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "emerald_block",
	name = "绿宝石块",
	tiles = "emerald_block.png",
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "emerald_ore",
	name = "绿宝石矿石",
	tiles = "emerald_ore.png",
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "gold_ore",
	name = "金矿石",
	tiles = "gold_ore.png",
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "diamond_ore",
	name = "钻石矿石",
	tiles = "diamond_ore.png",
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "redstone_ore",
	name = "红石矿石",
	tiles = "redstone_ore.png",
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "lapis_ore",
	name = "青金石矿石",
	tiles = "lapis_ore.png",
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "red_sandstone_smooth",
	name = "平滑红砂岩",
	tiles = "red_sandstone_smooth.png",
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "sandstone_smooth",
	name = "平滑砂岩",
	tiles = "sandstone_smooth.png",
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "red_sandstone_carved",
	name = "切制红砂岩",
	tiles = "red_sandstone_carved.png",
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "sandstone_carved",
	name = "切制砂岩",
	tiles = "sandstone_carved.png",
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "red_sandstone",
	name = "红砂岩",
	tiles = {"red_sandstone_top.png", "red_sandstone_bottom.png", "red_sandstone_normal.png"},
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "sandstone",
	name = "砂岩",
	tiles = {"sandstone_top.png", "sandstone_bottom.png", "sandstone_normal.png"},
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "red_sand",
	name = "红沙",
	tiles = "red_sand.png",
}

mod:block {
	id = "slime",
	name = "黏液块",
	tiles = "slime.png",
	draw_type = "glass",
}

mod:block {
	id = "nether_bricks",
	name = "下界砖块",
	tiles = "nether_bricks.png",
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "red_nether_brick",
	name = "红色下界砖块",
	tiles = "red_nether_brick.png",
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "end_stone",
	name = "末地石",
	tiles = "end_stone.png",
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "end_bricks",
	name = "末地石砖",
	tiles = "end_bricks.png",
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}





mod:block {
	id = "hay_block",
	name = "干草捆",
	tiles = {"hay_block_top.png", "hay_block_top.png", "hay_block_side.png"},
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

dofile("blocks/workbench.lua")
dofile("blocks/furnace.lua")
dofile("blocks/door.lua")

-- ===== 下界矿物（追加在末尾，保证既有数字 ID 不变）=====
mod:block {
	id = "nether_gold_ore",
	name = "下界金矿",
	tiles = "nether_gold_ore.png",
	hardness = 3,
	harvest_requirements = Capability.Pickaxe,
	item_drop = {
		id = mod:id() .. ":gold_nugget",
		amount = 3,
	},
}

mod:block {
	id = "ancient_debris",
	name = "远古残骸",
	tiles = {"ancient_debris_top.png", "ancient_debris_top.png", "ancient_debris_side.png"},
	hardness = 8,
	harvest_requirements = Capability.Pickaxe,
}

-- ===== 下界扩展（仅追加，保持旧存档方块数字 ID 稳定）=====
mod:block {
	id = "nether_quartz_ore",
	name = "下界石英矿",
	tiles = "nether_quartz_ore.png",
	hardness = 3,
	harvest_requirements = Capability.Pickaxe,
	item_drop = {
		id = mod:id() .. ":nether_quartz",
		amount = 1,
	},
}

mod:block {
	id = "magma_block",
	name = "岩浆块",
	tiles = "magma_block.png",
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
	is_light_source = true,
}

mod:block {
	id = "basalt",
	name = "玄武岩",
	tiles = {"basalt_top.png", "basalt_top.png", "basalt_side.png"},
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "nether_wart_block",
	name = "下界疣块",
	tiles = "nether_wart_block.png",
	hardness = 1,
}

mod:block {
	id = "blackstone",
	name = "黑石",
	tiles = {"blackstone_top.png", "blackstone_top.png", "blackstone.png"},
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "quartz_block",
	name = "石英块",
	tiles = {"quartz_block_top.png", "quartz_block_bottom.png", "quartz_block_side.png"},
	hardness = 1,
	harvest_requirements = Capability.Pickaxe,
}

-- ===== 末地专属方块 =====
-- 紫珀块：末地城的主要建材（淡紫色，带斑点纹理）
mod:block {
	id = "purpur_block",
	name = "紫珀块",
	tiles = "purpur_block.png",
	hardness = 1.5,
	harvest_requirements = Capability.Pickaxe,
}

-- 紫珀柱：末地城的柱子，六面用竖条纹贴图（顶/底用紫珀块）
mod:block {
	id = "purpur_pillar",
	name = "紫珀柱",
	-- 六面顺序 {Top, Bottom, East, West, South, North}：顶/底为紫珀块，四侧为竖条纹柱子
	tiles = {"purpur_block.png", "purpur_block.png", "purpur_pillar.png", "purpur_pillar.png", "purpur_pillar.png", "purpur_pillar.png"},
	hardness = 1.5,
	harvest_requirements = Capability.Pickaxe,
}

-- 末地水晶：发光装饰方块（可放置，用于末地城顶部的"水晶"效果）。
-- 挖掉掉落末地碎片，让 4 个末地合成配方在生存模式里可闭环获取。
mod:block {
	id = "end_crystal_block",
	name = "末地水晶",
	tiles = "end_crystal.png",
	hardness = 0.5,
	is_light_source = true,
	draw_type = "glass",
	is_opaque = false,
	is_collidable = false,
	harvest_requirements = Capability.Pickaxe,
	item_drop = {
		id = mod:id()..":end_shard",
		amount = 2,
	},
}

-- 末地传送门框架：暗绿石框，带符文（用于搭建末地传送门）
mod:block {
	id = "end_portal_frame",
	name = "末地传送门框架",
	tiles = "end_portal_frame.png",
	hardness = 3,
	harvest_requirements = Capability.Pickaxe,
}

-- 紫颂植株：末地特有的 xshape 植物（可放置装饰）。
-- 挖掉掉落紫颂果，让末地食物在生存模式里可获取。
mod:block {
	id = "chorus_plant",
	name = "紫颂植株",
	tiles = "chorus_plant.png",
	hardness = 0.2,
	is_opaque = false,
	is_collidable = false,
	draw_type = "xshape",
	item_drop = {
		id = mod:id()..":chorus_fruit",
		amount = 1,
	},
}

-- ============================================================
--  维度专属传送门（Minecraft 风格）
--
--  放在文件末尾：方块的数字 ID 必须追加在所有既有方块之后，
--  否则旧存档里存的是数字 ID，方块顺序一变就会错乱。
-- ============================================================

-- 下界传送门（紫色）：只存在于下界，右键回到主世界。
-- 站在主世界的门里进下界后，服务端会在下界生成一扇这个方块，
-- 玩家右键它就能原路返回。
mod:block {
	id = "nether_portal",
	name = "下界传送门",
	tiles = "nether_portal.png",

	draw_type = "glass",
	is_opaque = false,
	is_collidable = false,

	on_block_activated = function(pos, block, player, world, client, server)
		activate_portal("nether", "default:portal", "default:obsidian",
			player, client, server)
	end,
}

-- 末地传送门（绿色带黑点）：末地的返回门，右键回到主世界。
mod:block {
	id = "end_portal",
	name = "末地传送门",
	tiles = "end_portal.png",

	draw_type = "glass",
	is_opaque = false,
	is_collidable = false,

	on_block_activated = function(pos, block, player, world, client, server)
		activate_portal("end", "default:portal", "default:obsidian",
			player, client, server)
	end,
}


-- 末地传送门（主世界版）：可合成、可放置在主世界，右键进入末地（维度 2）。
-- 与末地的返回门用同一套贴图；分开两个 id 是因为同一个方块在不同维度要
-- 走不同的目标维度，而本引擎的 on_block_activated 拿不到「当前维度」以外的
-- 上下文，用两个 id 最直观（对应 MC 里主世界要塞的末地门 vs 末地的返回门）。
mod:block {
	id = "end_portal_overworld",
	name = "末地传送门（主世界）",
	tiles = "end_portal.png",

	draw_type = "glass",
	is_opaque = false,
	is_collidable = false,

	on_block_activated = function(pos, block, player, world, client, server)
		activate_portal("overworld_end", "default:end_portal", "default:end_stone",
			player, client, server)
	end,
}
