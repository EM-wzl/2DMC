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

mod:biome {
	id = "grassland",
	name = "草原",

	params = {
		temperature = 0.5,
		precipitation = 0.3
	},

	blocks = {
		top = "default:grass",
		ground = "default:dirt",
		deep = "default:stone",
		beach = "default:sand",
		liquid = "default:water",
		portal = "default:portal",
		portal_frame = "default:obsidian",
	},

	trees = {
		{
			type = "default:oak",
			probability = 0.01041666667
		}
	},

	-- Currently, all ores are defined per-biome. In a later update,
	-- we could see ores defined mostly by worldtype, and some extras
	-- added per biome. In addition, given the large world depth, we
	-- could decide whether ores at a point very far down should
	-- still depend on the surface biome, or only on the worldtype.
	ores = {
		{
			block = "default:coal_ore",
			probability = 0.001,
			size = 2,
			gen_type = "flood_fill",
		},
		{
			block = "default:iron_ore",
			probability = 0.003,
			size = 8,
			gen_type = "random_walk",
		}
	},

	flora = {
		{
			block = "default:tallgrass",
			spawns_on = "default:grass",
			probability = 0.25
		},
		{
			block = "default:dandelion",
			spawns_on = "default:grass",
			probability = 0.025
		}
	}
}

mod:biome {
	id = "desert",
	name = "沙漠",

	params = {
		temperature = -0.3,
		precipitation = -0.7
	},

	blocks = {
		top = "default:sand",
		ground = "default:sand",
		deep = "default:stone",
		beach = "default:sand",
		liquid = "default:water",
		portal = "default:portal",
		portal_frame = "default:obsidian",
	},

	trees = {
		{
			type = "default:tree_cactus",
			probability = 0.001,
		}
	},

	flora = {
		{
			block = "default:deadbush",
			spawns_on = "default:sand",
			probability = 0.005,
		}
	},
}

mod:biome {
	id = "netherland",
	name = "下界荒地",

	params = {
		temperature = -0.3,
		precipitation = -1
	},

	blocks = {
		top = "default:netherrack",
		ground = "default:netherrack",
		deep = "default:netherrack",
		beach = "default:soul_sand",
		liquid = "default:lava",
		portal = "default:portal",
		portal_frame = "default:obsidian",
	},

	ores = {
		{
			block = "default:nether_quartz_ore",
			probability = 0.005,
			size = 6,
			gen_type = "flood_fill",
		},
		{
			block = "default:nether_gold_ore",
			probability = 0.004,
			size = 4,
			gen_type = "flood_fill",
		},
		{
			block = "default:glowstone",
			probability = 0.003,
			size = 5,
			gen_type = "flood_fill",
		},
		{
			block = "default:magma_block",
			probability = 0.002,
			size = 4,
			gen_type = "random_walk",
		},
		{
			block = "default:ancient_debris",
			probability = 0.0005,
			size = 1,
			gen_type = "random_walk",
		},
	},
}

-- 新群系仅追加，不改变既有群系注册顺序。
mod:biome {
	id = "nether_wastes",
	name = "下界荒地",
	params = {temperature = -0.1, precipitation = -1},
	blocks = {
		top = "default:netherrack",
		ground = "default:netherrack",
		deep = "default:netherrack",
		beach = "default:soul_sand",
		liquid = "default:lava",
		portal = "default:portal",
		portal_frame = "default:obsidian",
	},
	ores = {
		{block = "default:nether_quartz_ore", probability = 0.006, size = 6, gen_type = "flood_fill"},
		{block = "default:nether_gold_ore", probability = 0.004, size = 4, gen_type = "flood_fill"},
		{block = "default:glowstone", probability = 0.004, size = 5, gen_type = "flood_fill"},
		{block = "default:magma_block", probability = 0.002, size = 4, gen_type = "random_walk"},
	},
	flora = {
		{
			block = "default:nether_wart",
			spawns_on = "default:soul_sand",
			probability = 0.035,
		},
	},
}

mod:biome {
	id = "soul_sand_valley",
	name = "灵魂沙峡谷",
	params = {temperature = 0.3, precipitation = -1},
	blocks = {
		top = "default:soul_sand",
		ground = "default:soul_sand",
		deep = "default:netherrack",
		beach = "default:soul_sand",
		liquid = "default:lava",
		portal = "default:portal",
		portal_frame = "default:obsidian",
	},
	ores = {
		{block = "default:nether_quartz_ore", probability = 0.002, size = 4, gen_type = "flood_fill"},
		{block = "default:basalt", probability = 0.003, size = 5, gen_type = "random_walk"},
		{block = "default:magma_block", probability = 0.002, size = 4, gen_type = "random_walk"},
	},
	flora = {
		{
			block = "default:nether_wart",
			spawns_on = "default:soul_sand",
			probability = 0.06,
		},
	},
}

mod:biome {
	id = "basalt_deltas",
	name = "玄武岩三角洲",
	params = {temperature = 0.7, precipitation = -1},
	blocks = {
		top = "default:basalt",
		ground = "default:basalt",
		deep = "default:blackstone",
		beach = "default:magma_block",
		liquid = "default:lava",
		portal = "default:portal",
		portal_frame = "default:obsidian",
	},
	ores = {
		{block = "default:magma_block", probability = 0.005, size = 5, gen_type = "flood_fill"},
		{block = "default:basalt", probability = 0.004, size = 5, gen_type = "random_walk"},
		{block = "default:nether_quartz_ore", probability = 0.001, size = 3, gen_type = "flood_fill"},
	},
}

-- ===== 末地群系 =====
-- 末地荒地：整片末地石，无树无花无液体；矿石以末地石砖/紫珀为主（结构由 C++ 生成）
mod:biome {
	id = "end_barrens",
	name = "末地荒地",
	params = {temperature = 0.5, precipitation = -1},
	blocks = {
		top = "default:end_stone",
		ground = "default:end_stone",
		deep = "default:end_stone",
		beach = "default:end_stone",
		liquid = "_:air", -- 末地无液体；引擎内置空气方块（id 0）
		portal = "default:portal",
		portal_frame = "default:obsidian",
	},
	-- 末地矿石：紫珀/末地石砖以"矿脉"形式零散出现（主体结构由 generateEndCity 生成）
	ores = {
		{block = "default:purpur_block", probability = 0.004, size = 4, gen_type = "flood_fill"},
		{block = "default:end_bricks", probability = 0.003, size = 5, gen_type = "random_walk"},
		{block = "default:end_crystal_block", probability = 0.0008, size = 2, gen_type = "flood_fill"},
	},
}

-- 末地高地：稍微起伏的末地石地形，紫颂植株点缀
mod:biome {
	id = "end_highlands",
	name = "末地高地",
	params = {temperature = 0.3, precipitation = -1},
	blocks = {
		top = "default:end_stone",
		ground = "default:end_stone",
		deep = "default:end_stone",
		beach = "default:end_stone",
		liquid = "_:air", -- 末地无液体；引擎内置空气方块（id 0）
		portal = "default:portal",
		portal_frame = "default:obsidian",
	},
	flora = {
		{
			block = "default:chorus_plant",
			spawns_on = "default:end_stone",
			probability = 0.02,
		},
	},
	ores = {
		{block = "default:purpur_block", probability = 0.006, size = 5, gen_type = "flood_fill"},
		{block = "default:end_bricks", probability = 0.002, size = 4, gen_type = "random_walk"},
	},
}