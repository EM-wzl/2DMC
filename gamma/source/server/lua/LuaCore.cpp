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
#include "server/core/ServerConfig.hpp"
#include "server/lua/LuaCore.hpp"
#include "server/lua/ServerModLoader.hpp"

void LuaCore::addListener(LuaEventType eventType, const sol::function &listener) {
	m_listeners.emplace(eventType, listener);
}
void LuaCore::initUsertype(sol::state &lua) {
	lua["Event"] = lua.create_table_with(
		"BlockPlaced", LuaEventType::BlockPlaced,
		"BlockDigged", LuaEventType::BlockDigged,
		"BlockActivated", LuaEventType::BlockActivated,

		"ItemActivated", LuaEventType::ItemActivated,

		"PlayerConnected", LuaEventType::PlayerConnected,

		// 生物被击杀（掉落表在 Lua 侧配置）
		"EntityKilled", LuaEventType::EntityKilled,

		"ServerTick", LuaEventType::ServerTick
	);

	lua.new_usertype<LuaCore>("LuaCore",
		"registry", &LuaCore::m_registry,
		"mod_loader", &LuaCore::m_modLoader,

		"add_listener", &LuaCore::addListener,
		// 在调用方的 Lua state 里重建选项值：ServerConfig::options 存的是
		// 与 state 无关的纯 C++ 值（见 ServerConfig.hpp 的说明）。
		// 注意：必须把 self（LuaCore&）作为第一个参数显式列出，否则 Lua 用
		// 冒号语法 openminer:get_config("x") 时 self 会占掉 option 的位置，
		// 导致 option 变成空串（曾表现为 "Option \"\" doesn't exist"）。
		"get_config", [](LuaCore &, const std::string &option, sol::this_state s) {
			auto it = ServerConfig::options.find(option);
			if (it == ServerConfig::options.end()) {
				logWarning() << "Option" << option << "doesn't exist";
				return sol::object{};
			}

			sol::state_view state(s);
			const ServerConfig::OptionValue &value = it->second;
			if (std::holds_alternative<bool>(value))
				return sol::object{state, sol::in_place_type<bool>, std::get<bool>(value)};
			if (std::holds_alternative<double>(value))
				return sol::object{state, sol::in_place_type<double>, std::get<double>(value)};
			if (std::holds_alternative<std::string>(value))
				return sol::object{state, sol::in_place_type<std::string>, std::get<std::string>(value)};
			return sol::object{};
		}
	);
}
