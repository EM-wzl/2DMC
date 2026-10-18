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

mod:block {
	id = "portal",
	name = "传送门",
	tiles = "portal.png",

	draw_type = "glass",
	is_opaque = false,

	on_block_activated = function(pos, block, player, world, client, server)
		local dim = (player:dimension() + 1) % 2
		local pos = {
			x = math.floor(player:x()),
			y = math.floor(player:y()),
			z = math.floor(player:z())
		}

		server:send_player_change_dimension(client.id, pos.x, pos.y, pos.z, dim, client)
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
