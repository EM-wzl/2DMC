/*
 * =====================================================================================
 *
 *  OpenMiner
 *
 *  Copyright (C) 2018-2020 Unarelith, Quentin Bazin <openminer@unarelith.net>
 *  Copyright (C) 2019-2020 the OpenMiner contributors (see CONTRIBUTORS.md)
 *
 *  This file is part of OpenMiner.
 *
 *  OpenMiner is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  OpenMiner is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with OpenMiner; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * =====================================================================================
 */
#include "common/core/Registry.hpp"
#include "common/world/Biome.hpp"
#include "common/world/PlacementEntry.hpp"
#include "common/world/Tree.hpp"
#include "server/core/ServerConfig.hpp"
#include "server/lua/LuaMod.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/scene/EntityWrapper.hpp"
#include "server/world/ServerPlayer.hpp"
#include "server/world/WorldController.hpp"

void LuaMod::commit() {
	while (!m_defs.empty()) {
		auto &it = m_defs.front();

		switch (it.first) {
			case DefinitionType::Block:           m_blockLoader.loadBlock(it.second);            break;
			case DefinitionType::Item:            m_itemLoader.loadItem(it.second);              break;
			case DefinitionType::CraftingRecipe:  m_recipeLoader.loadCraftingRecipe(it.second);  break;
			case DefinitionType::SmeltingRecipe:  m_recipeLoader.loadSmeltingRecipe(it.second);  break;
			case DefinitionType::Sky:             m_skyLoader.loadSky(it.second);                break;
			case DefinitionType::Tree:            m_biomeLoader.loadTree(it.second);             break;
			case DefinitionType::Biome:           m_biomeLoader.loadBiome(it.second);            break;
			case DefinitionType::Dimension:       m_dimensionLoader.loadDimension(it.second);    break;
			case DefinitionType::Key:             m_keyLoader.loadKey(it.second);                break;
			case DefinitionType::Entity:          m_entityLoader.loadEntity(it.second);          break;
			default: break;
		}

		m_defs.pop();
	}
}

#define DEF_FUNC(type) [](LuaMod &self, const sol::table &table) { \
	self.m_defs.emplace(type, table); \
}

void LuaMod::initUsertype(sol::state &lua) {
	lua.new_usertype<LuaMod>("LuaMod",
		"id",              &LuaMod::id,
		"path",            &LuaMod::path,

		"spawn_entity",    &LuaMod::spawnEntity,
		"despawn_entity",  &LuaMod::despawnEntity,

		"give_item_stack", &LuaMod::giveItemStack,

		"block",           DEF_FUNC(DefinitionType::Block),
		"item",            DEF_FUNC(DefinitionType::Item),
		"crafting_recipe", DEF_FUNC(DefinitionType::CraftingRecipe),
		"smelting_recipe", DEF_FUNC(DefinitionType::SmeltingRecipe),
		"sky",             DEF_FUNC(DefinitionType::Sky),
		"tree",            DEF_FUNC(DefinitionType::Tree),
		"biome",           DEF_FUNC(DefinitionType::Biome),
		"dimension",       DEF_FUNC(DefinitionType::Dimension),
		"key",             DEF_FUNC(DefinitionType::Key),
		"entity",          DEF_FUNC(DefinitionType::Entity),

		"option", [&] (LuaMod &self, const std::string &name, sol::object defaultValue) {
			// 用带 mod 前缀的完整键查找/插入。
			//
			// 优先级规则（重要，改错了会导致「玩家改了配置没效果」）：
			//   1. **玩家显式设置过的值优先**（config/server.lua 里存在该键，
			//      或玩家用 /option 命令改过）——mod 的默认值不能覆盖它。
			//      否则玩家在 config/server.lua 里把 mobs_enabled 改成 true
			//      会被 mod 里声明的 false 覆盖，功能永远开不起来。
			//   2. 玩家没设置过的键，mod 声明的默认值用 insert_or_assign 写入，
			//      这样 mod 改了默认值能立即生效（不会残留上次的旧值）。
			//
			// 执行顺序保证：config 先加载（标记 userOptions）→ mod 加载（受此规则约束）。
			const std::string key = self.m_id + ":" + name;

			// 玩家已显式设置：不覆盖，保留玩家值。
			if (ServerConfig::isUserDefined(key))
				return;

			if (defaultValue.valid()) {
				switch (defaultValue.get_type()) {
					case sol::type::boolean: ServerConfig::options.insert_or_assign(key, defaultValue.as<bool>()); break;
					case sol::type::number:  ServerConfig::options.insert_or_assign(key, defaultValue.as<double>()); break;
					case sol::type::string:  ServerConfig::options.insert_or_assign(key, defaultValue.as<std::string>()); break;
					default: break;
				}
			}
		}
	);
}

void LuaMod::spawnEntity(const std::string &entityID, const sol::table &table) {
	m_entityLoader.spawnEntity(entityID, table);
}

void LuaMod::despawnEntity(EntityWrapper &entity) {
	PositionComponent *position = entity.getPositionComponent();
	NetworkComponent *network = entity.getNetworkComponent();
	if (position && network) {
		m_worldController.server()->sendEntityDespawn(network->entityID);
		m_worldController.getWorld(position->dimension).scene().registry().destroy(entity.id());
	}
	else
		// FIXME: Maybe improve this message with the type of entity that caused the error
		logError() << ("In mod '" + m_id + "': Failed to despawn entity: Missing position and network components").c_str();
}

ItemStack LuaMod::giveItemStack(ServerPlayer &player, ItemStack *itemStack) {
	if (!itemStack) {
		logError() << ("In mod '" + m_id + "': Failed to add stack to player").c_str();
		return ItemStack::Empty;
	}

	// FIXME: This should probably be moved to a mod
	ItemStack stackRet = player.inventory().addStack(itemStack->item().stringID(), itemStack->amount(), 9, 24, true);
	if (stackRet.amount() != 0)
		stackRet = player.inventory().addStack(stackRet.item().stringID(), stackRet.amount(), 0, 9, true, true);

	m_worldController.server()->sendPlayerInvUpdate(player.clientID(), player.client());

	return stackRet;
}

