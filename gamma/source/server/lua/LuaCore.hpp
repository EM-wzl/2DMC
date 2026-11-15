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
#ifndef LUACORE_HPP_
#define LUACORE_HPP_

#include <functional>
#include <iostream>

#include <sol/sol.hpp>

#include "common/core/Debug.hpp"

class Registry;
class ServerModLoader;

enum class LuaEventType {
	BlockPlaced,
	BlockDigged,
	BlockActivated,

	ItemActivated,

	PlayerConnected,

	// 生物被玩家击杀。参数：(pos, entity, player, world, client, server)。
	// 掉落表在 Lua 侧（entities.lua）配置，引擎只负责在血量归零时触发。
	EntityKilled,

	// 每个服务端 tick（20tps）触发一次，参数为 ServerCommandHandler。
	// 生物自然生成/游荡 AI 依赖它，避免依赖实体的 on_tick 标记（存档重载后不恢复）。
	ServerTick,
};

class LuaCore {
	public:
		void addListener(LuaEventType eventType, const sol::function &listener);

		// 用 protected_function 调用监听器：Lua 错误作为返回值处理，
		// 不会以 Windows SEH 异常逃逸到 C++（LuaJIT 的 lj_err_throw 无法被
		// C++ catch 捕获，曾导致服务端在 Lua 报错时直接崩溃）。
		template<typename... Args>
		void onEvent(LuaEventType eventType, Args &&...args) {
			for (auto &it : m_listeners) {
				if (it.first == eventType) {
					sol::protected_function listener = it.second;
					sol::protected_function_result result = listener(std::forward<Args>(args)...);
					if (!result.valid()) {
						sol::error err = result;
						logError() << ("Lua event error: " + std::string{err.what()}).c_str();
					}
				}
			}
		}

		void setRegistry(Registry *registry) { m_registry = registry; }
		void setModLoader(ServerModLoader *modLoader) { m_modLoader = modLoader; }

		static void initUsertype(sol::state &lua);

	private:
		Registry *m_registry = nullptr;
		ServerModLoader *m_modLoader = nullptr;

		std::unordered_multimap<LuaEventType, sol::protected_function> m_listeners;
};

#endif // LUACORE_HPP_
