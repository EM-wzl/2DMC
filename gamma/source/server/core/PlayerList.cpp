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
#include "server/core/PlayerList.hpp"

ServerPlayer &PlayerList::addPlayer(const std::string &name, bool isNewPlayer) {
	m_players.emplace(name, ServerPlayer{name, isNewPlayer});
	return m_players.at(name);
}

ServerPlayer *PlayerList::connectPlayer(const std::string &name, ClientInfo &client, bool isSingleplayer) {
	ServerPlayer *player = nullptr;

	// isSingleplayer 表示「这个客户端是单机主机玩家」（只有本机回环连接的第一个
	// 客户端才是，见 Server::handleNewConnections），不是「服务端是单机模式」——
	// 局域网访客永远是 false，必须按普通玩家处理。
	if (isSingleplayer && !m_players.empty()) {
		// 单机主机接管存档里的角色：
		// 1) 优先精确匹配同名角色（主机用自己的用户名连自己的存档，最常见）；
		auto it = m_players.find(name);
		if (it != m_players.end() && !it->second.isOnline()) {
			player = &it->second;
			logInfo() << name << "is online";
		}
		// 2) 否则若存档里只有一个角色，沿用旧行为把它改名为当前用户名；
		else if (it == m_players.end() && m_players.size() == 1 && !m_players.begin()->second.isOnline()) {
			std::string oldname = m_players.begin()->second.name();
			m_players.emplace(name, m_players.begin()->second);
			m_players.erase(oldname);

			player = &m_players.at(name);
			player->setName(name);

			logInfo() << name << "is online";
		}
		// 3) 存档里有多个角色（之前开放过局域网，存档里多了访客角色）：
		//    继续走下面的普通连接路径（新建/复用同名角色），而不是像上游那样
		//    直接返回 nullptr 拒绝——否则主机再也进不了自己的世界。
	}

	if (!player) {
		auto it = m_players.find(name);
		if (it != m_players.end()) {
			if (!it->second.isOnline()) {
				player = &it->second;
				logInfo() << name << "is online";
			}
			else {
				logWarning() << name << "failed to connect: already online";
				return nullptr;
			}
		}
		else {
			player = &addPlayer(name, true);
			logInfo() << name << "is online (first connection)";
		}
	}

	player->setClient(&client);
	player->setClientID(client.id);

	client.playerName = name;

	return player;
}

void PlayerList::disconnectPlayer(const std::string &name) {
	auto it = m_players.find(name);
	if (it != m_players.end()) {
		it->second.setClient(nullptr);
		it->second.setNewPlayer(false);
		it->second.clearLoadedChunks();
		logInfo() << name << "is offline";
	}
}

const ServerPlayer *PlayerList::getPlayer(const std::string &name) const {
	auto it = m_players.find(name);
	if (it == m_players.end())
		return nullptr;

	return &it->second;
}

ServerPlayer *PlayerList::getPlayer(const std::string &name) {
	auto it = m_players.find(name);
	if (it == m_players.end())
		return nullptr;

	return &it->second;
}

ServerPlayer *PlayerList::getPlayerFromClientID(u16 clientID) {
	for (auto &it : m_players) {
		if (it.second.clientID() == clientID)
			return &it.second;
	}

	return nullptr;
}
