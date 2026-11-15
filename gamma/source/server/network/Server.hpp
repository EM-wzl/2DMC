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
#ifndef SERVER_HPP_
#define SERVER_HPP_

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <SFML/Network/SocketSelector.hpp>
#include <SFML/Network/TcpListener.hpp>
#include <SFML/System/Clock.hpp>

#include "common/network/Network.hpp"
#include "server/network/ServerInfo.hpp"

class Server {
	using ConnectionCallback = std::function<void(ClientInfo &, Network::Packet &packet)>;
	using CommandCallback = std::function<void(ClientInfo &, Network::Packet &packet)>;

	public:
		// 独立服务端与单机内嵌服务端共用的默认端口。
		static constexpr u16 DefaultPort = 4242;

		void init(u16 port = DefaultPort);

		void disconnectAllClients();
		void disconnectClient(ClientInfo &client);

		void handleGameEvents();

		void sendToAllClients(Network::Packet &packet) const;

		bool isRunning() const { return m_isRunning; }
		bool isSingleplayer() const { return m_isSingleplayer; }

		u16 port() const { return m_port; }

		// 枚举本机所有可用的 IPv4 地址（用于「开放局域网」提示玩家该填哪个 IP）。
		// 已过滤回环/链路本地地址并去重，首选出口网卡地址排在最前面。
		static std::vector<std::string> localIPv4Addresses();

		const ServerInfo &info() const { return m_info; }

		void setRunning(bool isRunning) { m_isRunning = isRunning; }
		void setSingleplayer(bool isSingleplayer) { m_isSingleplayer = isSingleplayer; }

		void setConnectionCallback(const ConnectionCallback &callback) { m_connectionCallback = callback; }
		void setCommandCallback(Network::Command command, const CommandCallback &callback) { m_commands[command] = callback; }

	private:
		void handleNewConnections();
		void handleClientMessages();

		// 已 accept 但还没收到 ClientConnect 握手的连接。
		// 开放到局域网后，端口扫描器/半开连接很常见；如果直接在 accept 之后
		// 阻塞等待握手，一个不说话的连接就能让整个服务端停摆（其他玩家掉线）。
		// 这里把它挂起，和普通客户端一样用 selector 轮询，超过时限就丢弃。
		struct PendingConnection {
			std::shared_ptr<sf::TcpSocket> socket;
			sf::Clock clock;
		};

		void acceptNewConnection();
		void handlePendingConnections();
		void finalizeConnection(PendingConnection &pending, Network::Packet &packet);

		bool m_isRunning = false;
		bool m_isSingleplayer = false;

		u16 m_port = 0;

		// 单机内嵌服务端里「主机玩家」的 client id（0 = 尚未连接）
		u16 m_hostClientID = 0;

		ServerInfo m_info;

		sf::TcpListener m_tcpListener;
		sf::SocketSelector m_selector;

		std::vector<PendingConnection> m_pendingConnections;

		ConnectionCallback m_connectionCallback;

		std::unordered_map<Network::Command, CommandCallback> m_commands;
};

#endif // SERVER_HPP_
