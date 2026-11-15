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
#include "server/network/ServerInfo.hpp"

ClientInfo &ServerInfo::addClient(const std::shared_ptr<sf::TcpSocket> &socket) {
	// 单调递增分配，跳过 0（0 在聊天包等协议里表示「系统消息」）以及仍在使用的 id。
	// 上游用 clients().size() + 1，在局域网里有人退出后再加入时会和在线玩家撞 id。
	while (m_nextClientID == 0 || getClient(m_nextClientID))
		++m_nextClientID;

	m_clients.emplace_back(m_nextClientID, socket);
	++m_nextClientID;

	return m_clients.back();
}

ClientInfo *ServerInfo::getClient(u16 id) {
	auto it = std::find_if(m_clients.begin(), m_clients.end(), [id] (ClientInfo &client) { return client.id == id; });
	if (it == m_clients.end())
		return nullptr;

	return &*it;
}

void ServerInfo::removeClient(u16 id) {
	auto it = std::find_if(m_clients.begin(), m_clients.end(), [id] (ClientInfo &client) { return client.id == id; });
	if (it != m_clients.end())
		m_clients.erase(it);
}
