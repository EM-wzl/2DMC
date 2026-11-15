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
#ifndef SERVERINFO_HPP_
#define SERVERINFO_HPP_

#include <algorithm>
#include <vector>

#include "common/core/Exception.hpp"
#include "server/network/ClientInfo.hpp"

class ServerInfo {
	public:
		ClientInfo &addClient(const std::shared_ptr<sf::TcpSocket> &socket);
		ClientInfo *getClient(u16 id);
		void removeClient(u16 id);

		std::vector<ClientInfo> &clients() { return m_clients; }
		const std::vector<ClientInfo> &clients() const { return m_clients; }

	private:
		std::vector<ClientInfo> m_clients;

		// 下一个要分配的 client id。绝不复用已经分配过的 id：
		// 客户端在 PlayerSpawn / PlayerPosUpdate / ChatMessage 等包里按 client id
		// 区分玩家，服务端在 PlayerInvUpdate / ChunkData 里也按 id 查人。
		// 如果用 clients().size() + 1 分配，有人中途退出后再进来就会拿到一个
		// 仍在线玩家正在用的 id（局域网里玩家频繁进出时必现），两人的数据流会串。
		u16 m_nextClientID = 1;
};

#endif // SERVERINFO_HPP_
