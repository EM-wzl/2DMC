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
#include <cmath>
#include <cstdlib>
#include <functional>
#include <sstream>
#include <unordered_map>

#include "common/core/GameTime.hpp"
#include "common/core/Registry.hpp"
#include "common/inventory/Inventory.hpp"
#include "common/world/Block.hpp"
#include "server/core/ServerConfig.hpp"
#include "server/lua/ScriptEngine.hpp"
#include "server/network/ChatCommandHandler.hpp"
#include "server/network/ClientInfo.hpp"
#include "server/network/Server.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/world/ServerPlayer.hpp"
#include "server/world/ServerWorld.hpp"
#include "server/world/WorldController.hpp"

namespace {
	// 物品名解析：支持 "stone"（补全为 default:stone）与 "mod:item" 全名。
	// 返回空字符串表示未找到。
	std::string resolveItemID(const std::string &name) {
		Registry &registry = Registry::getInstance();

		if (name.find(':') != std::string::npos) {
			if (registry.hasItem(name))
				return name;
		}
		else {
			std::string full = "default:" + name;
			if (registry.hasItem(full))
				return full;
		}

		return "";
	}

	// 生物名解析：英文 id 与中文别名都映射到 default:<kind>。
	std::string resolveEntityID(const std::string &name) {
		static const std::unordered_map<std::string, std::string> aliases = {
			{"pig", "pig"},                           {"猪", "pig"},
			{"zombie", "zombie"},                     {"僵尸", "zombie"},
			{"sheep", "sheep"},                       {"羊", "sheep"},
			{"spider", "spider"},                     {"蜘蛛", "spider"},
			{"creeper", "creeper"},                   {"苦力怕", "creeper"},
			{"zombified_piglin", "zombified_piglin"}, {"僵尸猪灵", "zombified_piglin"},
			{"pigman", "zombified_piglin"},           {"猪灵", "zombified_piglin"},
		};

		auto it = aliases.find(name);
		if (it != aliases.end())
			return "default:" + it->second;

		if (name.find(':') != std::string::npos)
			return name;

		return "";
	}

	// 在 (x,y) 附近找可站立的地表 z（脚部两格空气），找不到返回 false。
	bool findSurface(ServerWorld &world, s32 x, s32 y, s32 zref, double &outZ) {
		for (s32 z = zref + 8 ; z >= zref - 16 ; --z) {
			u16 block = world.getBlock(x, y, z);
			if (block != 0) {
				if (world.getBlock(x, y, z + 1) == 0 && world.getBlock(x, y, z + 2) == 0) {
					outZ = z + 1 + 0.01;
					return true;
				}
				return false;
			}
		}
		return false;
	}
}

void ChatCommandHandler::parseCommand(const std::string &str, ClientInfo &client) const {
	std::stringstream sstream;
	sstream << str;

	std::vector<std::string> command;
	std::string line;
	while (std::getline(sstream, line, ' ')) {
		if (!line.empty())
			command.emplace_back(line);
	}

	if (!command.empty()) {
		auto it = m_commands.find(command.at(0));
		if (it != m_commands.end()) {
			(this->*it->second)(command, client);
		}
		else {
			m_server.sendChatMessage(0, "未知命令: " + command.at(0), &client);
			m_server.sendChatMessage(0, "输入 /help 查看全部命令", &client);
		}
	}
}

void ChatCommandHandler::helpCommand(const std::vector<std::string> &command, ClientInfo &client) const {
	if (command.size() > 1) {
		m_server.sendChatMessage(0, "用法: /help", &client);
		return;
	}

	m_server.sendChatMessage(0, "===== 命令列表 =====", &client);
	m_server.sendChatMessage(0, "/help — 显示本列表", &client);
	m_server.sendChatMessage(0, "/give <物品> [数量] — 给予物品（1-64）", &client);
	m_server.sendChatMessage(0, "/heal — 恢复满生命值", &client);
	m_server.sendChatMessage(0, "/life <0-20> — 设置生命上限", &client);
	m_server.sendChatMessage(0, "/kill — 自杀并回到地表", &client);
	m_server.sendChatMessage(0, "/gamemode [0|1] — 切换/设置游戏模式（0 创造，1 生存）", &client);
	m_server.sendChatMessage(0, "/clear — 清空背包", &client);
	m_server.sendChatMessage(0, "/summon <生物> [数量] — 召唤生物（猪/僵尸/羊/蜘蛛/苦力怕/僵尸猪灵）", &client);
	m_server.sendChatMessage(0, "/cheat — 获取一套下界装备（别名 /EM_wzl）", &client);
	m_server.sendChatMessage(0, "/time [set|add] <值> — 设置/增加时间", &client);
	m_server.sendChatMessage(0, "/tp <x> <y> <z> — 传送", &client);
	m_server.sendChatMessage(0, "/tps — 查看服务器 TPS", &client);
	m_server.sendChatMessage(0, "/publish — 开放到局域网并显示 IP:端口（别名 /openlan、/lan）", &client);
	m_server.sendChatMessage(0, "/option <名称> [值] — 读取/设置服务器选项", &client);
	m_server.sendChatMessage(0, "/stop — 关闭服务器", &client);
}

void ChatCommandHandler::optionCommand(const std::vector<std::string> &command, ClientInfo &client) const {
	if (command.size() < 2 || command.size() > 3) {
		m_server.sendChatMessage(0, "用法: /option <name> [<value>]", &client);
	}
	else {
		std::string name = command.at(1);

		// 配置键在 ServerConfig::options 里是带 mod 前缀的完整形式
		// （如 "default:mobs_enabled"），但玩家习惯输入裸名。
		// 先精确匹配；不中就尝试补 "default:" 前缀；仍不中则做后缀匹配。
		if (ServerConfig::options.find(name) == ServerConfig::options.end()) {
			std::string prefixed = "default:" + name;
			if (ServerConfig::options.find(prefixed) != ServerConfig::options.end())
				name = prefixed;
			else {
				for (const auto &it : ServerConfig::options) {
					const std::string &key = it.first;
					if (key.size() > name.size()
					&& key.compare(key.size() - name.size(), name.size(), name) == 0
					&& key[key.size() - name.size() - 1] == ':') {
						name = key;
						break;
					}
				}
			}
		}

		auto it = ServerConfig::options.find(name);
		if (it != ServerConfig::options.end()) {
			const ServerConfig::OptionValue &value = it->second;
			if (command.size() < 3) {
				if (std::holds_alternative<bool>(value))
					m_server.sendChatMessage(0, std::string("Value: ") + (std::get<bool>(value) ? "true" : "false"), &client);
				else if (std::holds_alternative<double>(value))
					m_server.sendChatMessage(0, "Value: " + std::to_string(std::get<double>(value)), &client);
				else if (std::holds_alternative<std::string>(value))
					m_server.sendChatMessage(0, "Value: " + std::get<std::string>(value), &client);
				else
					m_server.sendChatMessage(0, "Value: nil", &client);
			}
			else {
				std::string value = command.at(2);
				if (ServerConfig::assignOption(name, value))
					m_server.sendChatMessage(0, "Value: " + value, &client);
				else
					m_server.sendChatMessage(0, "无效的选项值", &client);
			}
		}
		else {
			m_server.sendChatMessage(0, "选项 '" + name + "' 不存在", &client);
		}
	}
}

void ChatCommandHandler::stopCommand(const std::vector<std::string> &command, ClientInfo &client) const {
	if (command.size() > 1)
		m_server.sendChatMessage(0, "用法: /stop", &client);
	else {
		m_server.sendChatMessage(0, "正在关闭服务器……", &client);
		m_server.stopServer();
	}
}

void ChatCommandHandler::timeCommand(const std::vector<std::string> &command, ClientInfo &client) const {
	std::string subcommand;
	std::string param;
	if (command.size() == 2) {
		subcommand = command.at(1);
		if (subcommand != "set" && subcommand != "add" && subcommand != "help") {
			subcommand = "set";
			param = command.at(1);
		}
		else {
			subcommand = "help";
		}
	}
	else if (command.size() == 3) {
		subcommand = command.at(1);
		param = command.at(2);
	}
	else {
		subcommand = "help";
	}
	s64 ticks = 0;
	bool gotTicks = false;
	if (!param.empty()) {
		if (subcommand == "set") {
			static const std::unordered_map<std::string, u64> values = {
				{"day",       1000},
				{"noon",      6000},
				{"sunset",   11000},
				{"night",    13000},
				{"midnight", 18000},
				{"sunrise",   1000},
			};

			if (auto it = values.find(param) ; it != values.end()) {
				ticks = it->second;
				gotTicks = true;
			}
		}
		if (!gotTicks) {
			size_t dot_pos = param.find(':');
			if (dot_pos != std::string::npos) {
				bool minus_sign = param[0] == '-';
				if (minus_sign) {
					param = param.substr(1);
					--dot_pos;
				}
				try {
					u64 hours = std::stoull(param);
					u64 mins = std::stoull(param.substr(dot_pos + 1));
					ticks = (hours * 1000 + mins * 1000 / 60);
					if (subcommand == "set") {
						ticks = (ticks + 18000) % 24000;
					}
					else if (minus_sign) {
						ticks = -ticks;
					}
					gotTicks = true;
				}
				catch (...) {
					m_server.sendChatMessage(0, "无效的时间", &client);
					return;
				}
			}
			else {
				try {
					ticks = std::stoull(param);
					gotTicks = true;
				}
				catch (...) {
					m_server.sendChatMessage(0, "无效的时间", &client);
					return;
				}
			}
		}
	}

	if (gotTicks && subcommand == "set") {
		GameTime::setTicks(ticks);
		m_server.sendChatMessage(0, "时间已设置为 " + std::to_string(ticks), &client);
	}
	else if (gotTicks && subcommand == "add") {
		GameTime::setTicks(GameTime::getTicks() + ticks);
		m_server.sendChatMessage(0, "时间增加了 " + std::to_string(ticks), &client);
	}
	else {
		m_server.sendChatMessage(0, "用法: /time [set|add] <值>", &client);
	}
}

void ChatCommandHandler::tpCommand(const std::vector<std::string> &command, ClientInfo &client) const {
	if (command.size() != 4) {
		m_server.sendChatMessage(0, "用法: /tp x y z", &client);
	}
	else {
		try {
			s32 x = std::stoi(command.at(1));
			s32 y = std::stoi(command.at(2));
			s32 z = std::stoi(command.at(3));

			m_server.setPlayerPosition(client.id, x, y, z);
			m_server.sendPlayerPosUpdate(client.id, true);

			m_server.sendChatMessage(0, "已传送到 " + std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(z), &client);
		}
		catch (...) {
			m_server.sendChatMessage(0, "无效的坐标", &client);
		}
	}
}

void ChatCommandHandler::tpsCommand(const std::vector<std::string> &command, ClientInfo &client) const {
	if (command.size() > 1)
		m_server.sendChatMessage(0, "用法: /tps", &client);
	else
		m_server.sendChatMessage(0, "TPS: " + std::to_string(GameTime::getTicksPerSecond()), &client);
}

// 开放局域网（类似 Minecraft 的 Open to LAN）。
// 服务端本来就 listen 在 0.0.0.0（SFML TcpListener::listen 默认 IpAddress::Any），
// 所以这里不需要真的"开放"什么——缺的只是把 IP:端口 告诉玩家。
void ChatCommandHandler::publishCommand(const std::vector<std::string> &command, ClientInfo &client) const {
	if (command.size() > 1) {
		m_server.sendChatMessage(0, "用法: /publish（别名 /openlan、/lan）", &client);
		return;
	}

	u16 port = m_server.server().port();
	if (port == 0) {
		m_server.sendChatMessage(0, "错误: 服务端未在监听，无法开放到局域网", &client);
		return;
	}

	std::vector<std::string> addresses = Server::localIPv4Addresses();

	std::stringstream logLine;
	logLine << "LAN: ";
	if (addresses.empty())
		logLine << "<unknown-ip>";
	else
		logLine << addresses.front();
	logLine << ":" << port;
	logInfo() << logLine.str().c_str();

	if (addresses.empty()) {
		m_server.sendChatMessage(0, "已开放到局域网，但未能获取本机 IP 地址", &client);
		m_server.sendChatMessage(0, "请手动用 ipconfig 查看本机 IPv4 地址，端口: " + std::to_string(port), &client);
	}
	else {
		m_server.sendChatMessage(0, "已开放到局域网: " + addresses.front() + ":" + std::to_string(port), &client);

		if (addresses.size() > 1) {
			std::string all;
			for (std::size_t i = 0 ; i < addresses.size() ; ++i) {
				if (i > 0)
					all += "  ";
				all += addresses[i] + ":" + std::to_string(port);
			}
			m_server.sendChatMessage(0, "本机全部地址: " + all, &client);
		}
	}

	m_server.sendChatMessage(0, "朋友在「多人游戏」里输入上面的 IP:端口 即可加入", &client);

	if (!m_server.server().isSingleplayer())
		m_server.sendChatMessage(0, "（当前是独立服务器，本来就对外开放）", &client);
}

// ---------------------------------------------------------------------------
// 控制台版命令移植
// ---------------------------------------------------------------------------

void ChatCommandHandler::giveCommand(const std::vector<std::string> &command, ClientInfo &client) const {
	if (command.size() < 2 || command.size() > 3) {
		m_server.sendChatMessage(0, "用法: /give <物品> [数量]（数量 1-64）", &client);
		return;
	}

	ServerPlayer *player = m_server.getPlayer(client.id);
	if (!player) {
		m_server.sendChatMessage(0, "无法获取玩家数据", &client);
		return;
	}

	u16 amount = 1;
	if (command.size() == 3) {
		try {
			int value = std::stoi(command.at(2));
			if (value <= 0 || value > 64) {
				m_server.sendChatMessage(0, "错误: 数量必须在 1-64 之间", &client);
				return;
			}
			amount = (u16)value;
		}
		catch (...) {
			m_server.sendChatMessage(0, "错误: 数量必须是数字", &client);
			return;
		}
	}

	std::string itemID = resolveItemID(command.at(1));
	if (itemID.empty()) {
		m_server.sendChatMessage(0, "错误: 未知物品 '" + command.at(1) + "'", &client);
		return;
	}

	ItemStack rest = player->inventory().addStack(itemID, amount);
	u16 given = amount - rest.amount();

	m_server.sendPlayerInvUpdate(client.id, &client);

	if (given == 0)
		m_server.sendChatMessage(0, "错误: 背包已满", &client);
	else
		m_server.sendChatMessage(0, "已获得 " + std::to_string(given) + " 个 " + itemID, &client);
}

void ChatCommandHandler::healCommand(const std::vector<std::string> &command, ClientInfo &client) const {
	if (command.size() > 1) {
		m_server.sendChatMessage(0, "用法: /heal", &client);
		return;
	}

	ServerPlayer *player = m_server.getPlayer(client.id);
	if (!player) {
		m_server.sendChatMessage(0, "无法获取玩家数据", &client);
		return;
	}

	player->setHealth(player->maxHealth());
	m_server.sendPlayerHealth(client.id, player->health(), player->maxHealth());
	m_server.sendChatMessage(0, "生命值已恢复", &client);
}

void ChatCommandHandler::lifeCommand(const std::vector<std::string> &command, ClientInfo &client) const {
	if (command.size() != 2) {
		m_server.sendChatMessage(0, "用法: /life <0-20>", &client);
		return;
	}

	ServerPlayer *player = m_server.getPlayer(client.id);
	if (!player) {
		m_server.sendChatMessage(0, "无法获取玩家数据", &client);
		return;
	}

	try {
		int value = std::stoi(command.at(1));
		if (value < 0 || value > 20) {
			m_server.sendChatMessage(0, "错误: 生命上限必须在 0-20 之间", &client);
			return;
		}

		player->setMaxHealth((u8)value);
		if (player->health() > player->maxHealth())
			player->setHealth(player->maxHealth());

		m_server.sendPlayerHealth(client.id, player->health(), player->maxHealth());
		m_server.sendChatMessage(0, "生命上限已设置为 " + std::to_string(value), &client);
	}
	catch (...) {
		m_server.sendChatMessage(0, "错误: 生命上限必须是数字", &client);
	}
}

void ChatCommandHandler::killCommand(const std::vector<std::string> &command, ClientInfo &client) const {
	if (command.size() > 1) {
		m_server.sendChatMessage(0, "用法: /kill", &client);
		return;
	}

	ServerPlayer *player = m_server.getPlayer(client.id);
	if (!player) {
		m_server.sendChatMessage(0, "无法获取玩家数据", &client);
		return;
	}

	player->setHealth(0);

	// 没有独立的出生点系统：重生到当前列的地表上方（与 /tp 一致的语义）。
	ServerWorld &world = m_server.getWorld(player->dimension());
	s32 x = (s32)std::floor(player->x());
	s32 y = (s32)std::floor(player->y());
	s32 z = world.heightmap().getHighestBlockAt(x, y) + 2;

	m_server.setPlayerPosition(client.id, x, y, z);
	m_server.sendPlayerPosUpdate(client.id, true);
	m_server.sendPlayerHealth(client.id, 0, player->maxHealth());

	player->setHealth(player->maxHealth());
	m_server.sendPlayerHealth(client.id, player->health(), player->maxHealth());

	m_server.sendChatMessage(0, "你死了，已在附近地表重生", &client);
}

void ChatCommandHandler::gamemodeCommand(const std::vector<std::string> &command, ClientInfo &client) const {
	if (command.size() > 2) {
		m_server.sendChatMessage(0, "用法: /gamemode [0|1]（0 创造，1 生存）", &client);
		return;
	}

	ServerPlayer *player = m_server.getPlayer(client.id);
	if (!player) {
		m_server.sendChatMessage(0, "无法获取玩家数据", &client);
		return;
	}

	u8 mode = player->gamemode() == 0 ? 1 : 0;
	if (command.size() == 2) {
		try {
			int value = std::stoi(command.at(1));
			if (value != 0 && value != 1) {
				m_server.sendChatMessage(0, "错误: 游戏模式只能是 0（创造）或 1（生存）", &client);
				return;
			}
			mode = (u8)value;
		}
		catch (...) {
			m_server.sendChatMessage(0, "错误: 游戏模式必须是 0 或 1", &client);
			return;
		}
	}

	player->setGamemode(mode);
	m_server.sendPlayerGamemode(client.id, mode);
	m_server.sendChatMessage(0, std::string("游戏模式已切换为 ") + (mode == 0 ? "创造" : "生存"), &client);
}

void ChatCommandHandler::clearCommand(const std::vector<std::string> &command, ClientInfo &client) const {
	if (command.size() > 1) {
		m_server.sendChatMessage(0, "用法: /clear", &client);
		return;
	}

	ServerPlayer *player = m_server.getPlayer(client.id);
	if (!player) {
		m_server.sendChatMessage(0, "无法获取玩家数据", &client);
		return;
	}

	Inventory &inventory = player->inventory();
	for (u16 y = 0 ; y < inventory.height() ; ++y)
		for (u16 x = 0 ; x < inventory.width() ; ++x)
			inventory.clearStack(x, y);

	m_server.sendPlayerInvUpdate(client.id, &client);
	m_server.sendChatMessage(0, "背包已清空", &client);
}

void ChatCommandHandler::summonCommand(const std::vector<std::string> &command, ClientInfo &client) const {
	if (command.size() < 2 || command.size() > 3) {
		m_server.sendChatMessage(0, "用法: /summon <生物> [数量]（1-20）", &client);
		return;
	}

	ServerPlayer *player = m_server.getPlayer(client.id);
	if (!player) {
		m_server.sendChatMessage(0, "无法获取玩家数据", &client);
		return;
	}

	std::string entityID = resolveEntityID(command.at(1));
	if (entityID.empty()) {
		m_server.sendChatMessage(0, "错误: 未知生物 '" + command.at(1) + "'", &client);
		m_server.sendChatMessage(0, "可用: pig/zombie/sheep/spider/creeper/zombified_piglin（或中文名）", &client);
		return;
	}

	int amount = 1;
	if (command.size() == 3) {
		try {
			amount = std::stoi(command.at(2));
		}
		catch (...) {
			amount = 0;
		}
		if (amount <= 0 || amount > 20) {
			m_server.sendChatMessage(0, "错误: 数量必须在 1-20 之间", &client);
			return;
		}
	}

	ServerWorld &world = m_server.getWorld(player->dimension());
	s32 spawned = 0;
	for (int i = 0 ; i < amount ; ++i) {
		s32 x = (s32)std::floor(player->x()) + (std::rand() % 9 - 4);
		s32 y = (s32)std::floor(player->y()) + (std::rand() % 9 - 4);
		double z = 0;

		if (!findSurface(world, x, y, (s32)std::floor(player->z()), z))
			continue;

		if (m_server.spawnEntity(entityID, x + 0.5, y + 0.5, z, player->dimension()))
			++spawned;
	}

	m_server.sendChatMessage(0, "已召唤 " + std::to_string(spawned) + " 只 " + entityID, &client);
}

void ChatCommandHandler::cheatCommand(const std::vector<std::string> &command, ClientInfo &client) const {
	if (command.size() > 1) {
		m_server.sendChatMessage(0, "用法: /cheat", &client);
		return;
	}

	ServerPlayer *player = m_server.getPlayer(client.id);
	if (!player) {
		m_server.sendChatMessage(0, "无法获取玩家数据", &client);
		return;
	}

	// 控制台版的作弊套装：全套下界合金 + 剑 + 64 金苹果。
	// 当前 gamma 只有下界工具（没有盔甲物品），缺失的条目会安全跳过。
	struct CheatItem { const char *id; u16 amount; };
	static const CheatItem items[] = {
		{"default:netherite_helmet", 1},
		{"default:netherite_chestplate", 1},
		{"default:netherite_leggings", 1},
		{"default:netherite_boots", 1},
		{"default:netherite_sword", 1},
		{"default:netherite_pickaxe", 1},
		{"default:netherite_axe", 1},
		{"default:apple_golden", 64},
	};

	u16 added = 0;
	for (const CheatItem &item : items) {
		if (!Registry::getInstance().hasItem(item.id))
			continue;

		ItemStack rest = player->inventory().addStack(item.id, item.amount);
		added += item.amount - rest.amount();
	}

	m_server.sendPlayerInvUpdate(client.id, &client);
	m_server.sendChatMessage(0, "作弊已生效：下界装备 + 金苹果（共 " + std::to_string(added) + " 件）", &client);
}

void ChatCommandHandler::dimensionCommand(const std::vector<std::string> &command, ClientInfo &client) const {
	if (command.size() != 2) {
		m_server.sendChatMessage(0, "用法: /dimension <0|1|2>  （0 主世界 / 1 下界 / 2 末地）", &client);
		return;
	}

	int dim = -1;
	try {
		dim = std::stoi(command.at(1));
	}
	catch (...) {
		m_server.sendChatMessage(0, "无效的维度编号", &client);
		return;
	}

	// 目标维度必须在注册表范围内，否则 getWorld() 的下标访问会越界。
	const u16 worldCount = (u16)m_server.worldCount();
	if (dim < 0 || dim >= (int)worldCount) {
		m_server.sendChatMessage(0, "维度编号超出范围（当前有 " + std::to_string(worldCount) + " 个维度）", &client);
		return;
	}

	ServerPlayer *player = m_server.getPlayer(client.id);
	if (!player) {
		m_server.sendChatMessage(0, "无法获取玩家数据", &client);
		return;
	}

	// 用玩家当前坐标切维度；服务端会把目标维度的区块发给客户端，
	// 客户端 ClientWorld::changeDimension() 会清掉旧维度的 chunk 再加载新的。
	const s32 x = (s32)player->x();
	const s32 y = (s32)player->y();
	const s32 z = (s32)player->z();

	m_server.sendPlayerChangeDimension(client.id, x, y, z, (u16)dim, &client);
	m_server.sendPlayerPosUpdate(client.id, true);
	m_server.sendChatMessage(0, "已传送到维度 " + std::to_string(dim), &client);
}
