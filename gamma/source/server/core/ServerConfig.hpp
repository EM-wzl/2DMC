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
#ifndef SERVERCONFIG_HPP_
#define SERVERCONFIG_HPP_

#include <string>
#include <unordered_map>
#include <variant>

#include <sol/sol.hpp>

#include "common/core/IntTypes.hpp"

namespace ServerConfig {
	// Server
	extern u8 maxPlayers;
	extern u16 maxItemStackSize;

	// World
	extern u8 renderDistance;

	// Mod-defined options.
	//
	// 刻意不存 sol::object：config/server.lua 是在 ServerConfig 自己的临时
	// sol::state 里解析的，把那个 state 的 sol::object 直接返回给游戏 Lua 会
	// 得到 nil/无效对象（跨 state 引用）。因此这里存纯 C++ 值，
	// get_config 再在调用方的 Lua state 里重建对象。
	using OptionValue = std::variant<std::monostate, bool, double, std::string>;
	extern std::unordered_map<std::string, OptionValue> options;

	// 「用户显式设置」的选项（通过 /option 命令或直接改 config/server.lua）。
	//
	// 为什么需要它：
	//   * options 里同时存在「mod 声明的默认值」和「用户改过的值」，
	//     两者用同一个 map 存，无法区分谁该赢。
	//   * 之前的实现让 mod 默认值永远覆盖用户配置 → 玩家改 config/server.lua
	//     完全没效果（mobs_enabled 改成 true 也会被 mod 里的 false 覆盖）。
	//   * 而更早的实现让配置文件里的历史值永远赢 → mod 改了默认值也不生效
	//     （生物上限从 40 改成 12 无效，导致生物刷屏）。
	//
	// 现在的规则：
	//   1. config/server.lua 里存在的键 = 用户当前配置意图，优先于 mod 默认值。
	//   2. /option 命令运行时改的值同样算用户意图，并且会被保存到文件。
	//   3. 保存时只写「用户设置过的」+「mod 当前默认值」，不写运行期临时值，
	//      这样文件里不会积累陈旧历史值。
	extern std::unordered_map<std::string, OptionValue> userOptions;

	// 该键是否已被用户显式设置（配置文件里存在，或被 /option 改过）
	bool isUserDefined(const std::string &name);

	void loadConfigFromFile(const char *file);
	void saveConfigToFile(const char *file);

	bool assignOption(const std::string &name, const std::string &value);
}

#endif // SERVERCONFIG_HPP_
