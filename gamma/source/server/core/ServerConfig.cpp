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
#include <fstream>

#include <filesystem.hpp>

#include "common/core/Debug.hpp"
#include "server/core/ServerConfig.hpp"

namespace fs = ghc::filesystem;

// Server
u8 ServerConfig::maxPlayers = 5;
u16 ServerConfig::maxItemStackSize = 64;

// World
u8 ServerConfig::renderDistance = 8;

// Mod-defined options
std::unordered_map<std::string, ServerConfig::OptionValue> ServerConfig::options;

// 用户显式设置过的选项（配置文件里存在，或被 /option 命令改过）。
// 用来区分「mod 声明的默认值」和「玩家想要的配置」——见 ServerConfig.hpp 的说明。
std::unordered_map<std::string, ServerConfig::OptionValue> ServerConfig::userOptions;

bool ServerConfig::isUserDefined(const std::string &name) {
	return userOptions.find(name) != userOptions.end();
}

static sol::state lua;

// 把任意 Lua 值转成与 Lua state 无关的 OptionValue。
// config/server.lua 是在上面这个临时 sol::state 里解析的，若把它的
// sol::object 直接存下来，之后返回给游戏 Lua（另一个 state）会得到 nil。
static ServerConfig::OptionValue toOptionValue(const sol::object &value) {
	if (!value.valid())
		return std::monostate{};

	switch (value.get_type()) {
		case sol::type::boolean: return value.as<bool>();
		case sol::type::number:  return value.as<double>();
		case sol::type::string:  return value.as<std::string>();
		default:                 return std::monostate{};
	}
}

void ServerConfig::loadConfigFromFile(const char *file) {
	if (fs::exists(file)) {
		try {
			lua.safe_script_file(file);

			maxPlayers = lua["max_players"].get_or(maxPlayers);
			maxItemStackSize = lua["max_item_stack_size"].get_or(maxItemStackSize);

			renderDistance = lua["render_distance"].get_or(renderDistance);

			if (lua["mod_options"].valid() && lua["mod_options"].get_type() == sol::type::table) {
				for (auto &it : lua["mod_options"].get<sol::table>()) {
					const std::string key = it.first.as<std::string>();
					const OptionValue value = toOptionValue(it.second);

					// 配置文件里存在的键 = 玩家当前配置意图，
					// 优先于 mod 声明的默认值（mod 加载时会用 insert_or_assign）。
					options.insert_or_assign(key, value);
					userOptions.insert_or_assign(key, value);
				}
			}

			logInfo() << "Config file loaded successfully";
		}
		catch (sol::error &e) {
			logError() << e.what();
		}
	}
}

void ServerConfig::saveConfigToFile(const char *filename) {
	std::ofstream file{filename, std::ofstream::out | std::ofstream::trunc};
	file << "max_players = " << (u16)maxPlayers << std::endl;
	file << "max_item_stack_size = " << maxItemStackSize << std::endl;
	file << "render_distance = " << (u16)renderDistance << std::endl;
	file << "mod_options = {" << std::endl;

	// 只写「用户显式设置过」的选项。
	//
	// mod 声明的默认值不写进文件：它们每次启动都由 mod 重新声明，
	// 写进去会让陈旧的旧值在下次启动时压过新的默认值
	// （例如 mod 把生物上限从 40 改成 12，却因为文件里还留着 40 而不生效）。
	// 而用户改过的值必须写进去，否则重启就丢了。
	for (auto &it : userOptions) {
		file << "\t[\"" << it.first << "\"] = ";

		if (std::holds_alternative<bool>(it.second)) {
			file << (std::get<bool>(it.second) ? "true" : "false");
		}
		else if (std::holds_alternative<double>(it.second)) {
			file << std::get<double>(it.second);
		}
		else if (std::holds_alternative<std::string>(it.second)) {
			file << "\"" << std::get<std::string>(it.second) << "\"";
		}
		else {
			file << "nil";
		}

		file << "," << std::endl;
	}

	file << "}" << std::endl;
}

bool ServerConfig::assignOption(const std::string &name, const std::string &value) {
	auto it = options.find(name);
	if (it != options.end()) {
		try {
			sol::object object = lua.load("return " + value)().get<sol::object>();
			OptionValue converted = toOptionValue(object);
			if (!std::holds_alternative<std::monostate>(converted)) {
				it->second = converted;

				// /option 命令改过的值 = 玩家显式设置，
				// 后续 mod 加载默认值时不能覆盖它，且会被保存到 config/server.lua。
				userOptions.insert_or_assign(name, converted);
				return true;
			}
		}
		catch (sol::error &e) {
			logWarning() << e.what();
		}
	}
	else {
		logWarning() << "Can't assign option: '" + name + "' doesn't exist";
	}

	return false;
}
