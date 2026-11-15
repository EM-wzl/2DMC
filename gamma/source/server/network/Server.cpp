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
#ifdef _WIN32
	// winsock2.h 必须在任何可能引入 windows.h 的头文件之前包含
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <winsock2.h>
	#include <ws2tcpip.h>
#else
	#include <arpa/inet.h>
	#include <ifaddrs.h>
	#include <netinet/in.h>
	#include <sys/socket.h>
	#include <unistd.h>
#endif

#include <algorithm>
#include <cstring>

#include "server/core/ServerConfig.hpp"
#include "server/network/Server.hpp"

namespace {
	// 过滤掉回环 / 未指定 / 链路本地（169.254.x.x，通常是 DHCP 失败后的自动地址，
	// 局域网内其它机器无法访问）以及重复项。
	void addUsableAddress(std::vector<std::string> &addresses, const char *address) {
		if (!address || address[0] == '\0')
			return;

		std::string str{address};
		if (str == "0.0.0.0" || str == "127.0.0.1")
			return;
		if (str.rfind("169.254.", 0) == 0)
			return;

		if (std::find(addresses.begin(), addresses.end(), str) == addresses.end())
			addresses.emplace_back(str);
	}
}

std::vector<std::string> Server::localIPv4Addresses() {
	std::vector<std::string> addresses;

#ifdef _WIN32
	// 用主机名反查本机全部 IPv4 地址（GetAdaptersAddresses 需要额外链接 iphlpapi，
	// 而 gethostname + getaddrinfo 只依赖已链接的 ws2_32，跨编译器更稳）。
	char hostname[256] = {0};
	if (gethostname(hostname, int(sizeof(hostname) - 1)) == 0) {
		addrinfo hints;
		std::memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;

		addrinfo *result = nullptr;
		if (getaddrinfo(hostname, nullptr, &hints, &result) == 0) {
			for (addrinfo *it = result ; it ; it = it->ai_next) {
				char buffer[INET_ADDRSTRLEN] = {0};
				auto *sin = reinterpret_cast<sockaddr_in *>(it->ai_addr);
				if (inet_ntop(AF_INET, &sin->sin_addr, buffer, sizeof(buffer)) != nullptr)
					addUsableAddress(addresses, buffer);
			}
			freeaddrinfo(result);
		}
	}
#else
	ifaddrs *interfaces = nullptr;
	if (getifaddrs(&interfaces) == 0) {
		for (ifaddrs *it = interfaces ; it ; it = it->ifa_next) {
			if (!it->ifa_addr || it->ifa_addr->sa_family != AF_INET)
				continue;

			char buffer[INET_ADDRSTRLEN] = {0};
			auto *sin = reinterpret_cast<sockaddr_in *>(it->ifa_addr);
			if (inet_ntop(AF_INET, &sin->sin_addr, buffer, sizeof(buffer)) != nullptr)
				addUsableAddress(addresses, buffer);
		}
		freeifaddrs(interfaces);
	}
#endif

	// 兜底：枚举失败时至少给出 SFML 认为的本机地址。
	// 注意：SFML 2.6 的 getLocalAddress() 通过 connect(UDP, 127.0.0.1) + getsockname
	// 实现，因此在部分系统上会返回 127.0.0.1，只能作为最后手段。
	if (addresses.empty()) {
		std::string localAddress = sf::IpAddress::getLocalAddress().toString();
		addUsableAddress(addresses, localAddress.c_str());
	}

	return addresses;
}

void Server::init(u16 port) {
	if (m_tcpListener.listen(port) != sf::Socket::Done)
		throw EXCEPTION("Network error: Listen failed");

	m_port = m_tcpListener.getLocalPort();

	m_tcpListener.setBlocking(false);

	m_selector.add(m_tcpListener);
}

void Server::disconnectAllClients() {
	// 不能用 range-for 遍历 clients()：disconnectClient() 会 erase 当前元素，
	// 迭代器立即失效（多客户端同时在线时关闭服务器会崩）。
	while (!m_info.clients().empty())
		disconnectClient(m_info.clients().front());
}

void Server::disconnectClient(ClientInfo &client) {
	// client 在 m_info.removeClient() 后即失效，先取出需要的字段
	u16 clientID = client.id;

	Network::Packet packet;
	packet << Network::Command::ClientDisconnect << clientID;
	sendToAllClients(packet);

	// 用 find 而非 operator[]：后者在键不存在时会插入新元素，可能触发
	// unordered_map rehash；而此时外层可能正在遍历 m_commands，迭代器会失效。
	auto it = m_commands.find(Network::Command::ClientDisconnect);
	if (it != m_commands.end())
		it->second(client, packet);

	m_selector.remove(*client.tcpSocket);
	m_info.removeClient(clientID);

	// 单机模式下主机（本地客户端）离开 = 关闭这个世界：
	// 即使还有局域网玩家在线也要停服，否则 ServerApplication::run() 的
	// 世界保存分支不会执行，主机玩家的进度会丢。
	if (m_isSingleplayer && (clientID == m_hostClientID || m_info.clients().size() == 0)) {
		m_tcpListener.close();
		m_isRunning = false;
	}
}

void Server::handleGameEvents() {
	bool hasReadySocket = m_selector.wait(sf::milliseconds(10));

	// 新连接和已有客户端的消息都要处理：原来用 if/else 二选一，局域网里有人
	// 频繁加入时，已有玩家的包会被新连接事件挤掉。
	if (hasReadySocket && m_selector.isReady(m_tcpListener))
		handleNewConnections();

	if (hasReadySocket)
		handleClientMessages();

	// 挂起的握手连接也要每帧推进（它们还没进 m_selector 的正式客户端列表）。
	handlePendingConnections();
}

void Server::handleNewConnections() {
	// 一次可能积压多个连接（局域网里几个人同时加入），循环 accept 直到取空。
	while (true) {
		std::shared_ptr<sf::TcpSocket> clientSocket = std::make_shared<sf::TcpSocket>();
		sf::Socket::Status status = m_tcpListener.accept(*clientSocket);
		if (status != sf::Socket::Done) {
			if (status != sf::Socket::NotReady)
				logWarning() << "Warning: Connection accept failed.";
			return;
		}

		// 不在 accept 之后阻塞等握手：开放到局域网后端口扫描器/半开连接很常见，
		// 阻塞等待会让一个不说话的连接卡住整个服务端。这里只挂起，后续每帧轮询。
		clientSocket->setBlocking(false);
		m_pendingConnections.emplace_back();
		m_pendingConnections.back().socket = clientSocket;
	}
}

void Server::handlePendingConnections() {
	// 反向遍历以便安全删除；握手超时 5 秒（正常客户端连上后立刻发 ClientConnect）。
	for (std::size_t i = m_pendingConnections.size() ; i > 0 ; --i) {
		PendingConnection &pending = m_pendingConnections[i - 1];

		Network::Packet packet;
		sf::Socket::Status status = pending.socket->receive(packet);

		if (status == sf::Socket::Done) {
			finalizeConnection(pending, packet);
			m_pendingConnections.erase(m_pendingConnections.begin() + long(i - 1));
		}
		else if (status == sf::Socket::Disconnected || status == sf::Socket::Error
		      || pending.clock.getElapsedTime() > sf::seconds(5)) {
			logWarning() << "Dropped new connection: no valid handshake received";
			m_pendingConnections.erase(m_pendingConnections.begin() + long(i - 1));
		}
	}
}

void Server::finalizeConnection(PendingConnection &pending, Network::Packet &packet) {
	Network::Command command;
	packet >> command;

	if (command != Network::Command::ClientConnect) {
		logError() << "Network error: Expected 'ClientConnect' packet for new clients.";
		return;
	}

	if (m_info.clients().size() >= ServerConfig::maxPlayers) {
		Network::Packet outPacket;
		outPacket << Network::Command::ClientRefused;
		pending.socket->send(outPacket);
		return;
	}

	ClientInfo &client = m_info.addClient(pending.socket);
	m_selector.add(*client.tcpSocket);

	// 单机内嵌服务端：只有本机（回环地址）的第一个客户端才是「单机玩家」。
	// 客户端 Client::connect() 会校验 ClientOk 里的 singleplayer 标志与自身
	// 状态一致，否则抛 "The server is not valid"。若把 true 发给所有客户端，
	// 局域网玩家（多人模式）永远连不上——这是「开放局域网」的核心修复。
	bool isLoopback = pending.socket->getRemoteAddress() == sf::IpAddress::LocalHost;
	bool isHostClient = m_isSingleplayer && isLoopback && m_hostClientID == 0;
	if (isHostClient)
		m_hostClientID = client.id;

	client.isSingleplayer = isHostClient;

	Network::Packet outPacket;
	outPacket << Network::Command::ClientOk << client.id << isHostClient;
	client.tcpSocket->send(outPacket);

	if (m_connectionCallback)
		m_connectionCallback(client, packet);
}

void Server::handleClientMessages() {
	for (size_t i = 0 ; i < m_info.clients().size() ; ++i) {
		// 注意：不能用引用持有 clients()[i]。disconnectClient() 会
		// m_info.removeClient() → vector::erase 使该元素失效，
		// 之后 do/while 再用 client 就是悬垂引用（上游遗留 bug，
		// 表现为玩家断开/重连时服务端静默退出）。这里只取 id 与 socket 副本。
		u16 clientID = m_info.clients()[i].id;
		auto tcpSocket = m_info.clients()[i].tcpSocket;
		if (!tcpSocket || !m_selector.isReady(*tcpSocket))
			continue;

		bool clientRemoved = false;
		Network::Packet packet;
		sf::Socket::Status status;
		int packetsProcessed = 0;
		do {
			status = tcpSocket->receive(packet);
			if (status == sf::Socket::Done) {
				Network::Command command;
				packet >> command;

				// logDebug() << "TCP message received:" << Network::commandToString(command);

				if (m_isRunning) {
					ClientInfo *client = m_info.getClient(clientID);
					if (!client) {
						clientRemoved = true;
						break;
					}

					// ClientDisconnect 由 disconnectClient() 统一处理：它内部会触发
					// 断开回调、广播 ClientDisconnect 包、移除客户端。这里若先执行
					// it.second(*client, packet) 会导致断开回调被执行两次
					// （表现为 "xxx is offline" 打两遍、玩家状态被重复清理）。
					if (command == Network::Command::ClientDisconnect) {
						disconnectClient(*client);
						clientRemoved = true;
						break;
					}

					// 命令回调里可能抛异常（Registry::getBlockFromStringID 查未注册
					// 方块、getKey 用 at()、getWorldForClient 越界等），而这些回调
					// 的入参来自网络包，必须当成不可信输入。异常若穿透到
					// ServerApplication::run() 的外层 catch 之外就会 terminate，
					// 让一个畸形包把整个服务端打死。这里就地挡住并跳过该包。
					for (auto &it : m_commands) {
						if (command == it.first) {
							try {
								it.second(*client, packet);
							}
							catch (const std::exception &e) {
								logError() << "Command handler threw for command "
									<< Network::commandToString(command) << ": " << e.what();
								break;
							}
							catch (...) {
								logError() << "Command handler threw (unknown) for command "
									<< Network::commandToString(command);
								break;
							}
						}
					}
				}
			}
			else if (status == sf::Socket::Disconnected) {
				ClientInfo *client = m_info.getClient(clientID);
				if (client) {
					disconnectClient(*client);
					clientRemoved = true;
				}
			}
		}
		while (!clientRemoved
			&& (status == sf::Socket::Done || status == sf::Socket::Disconnected)
			&& packetsProcessed++ < 100);

		// 该客户端已被移除：vector 收缩，下标 i 现在指向下一个元素，
		// 回退一次让 for 的 ++i 抵消（i==0 时先跳过本轮，由循环头 ++ 归零）。
		if (clientRemoved) {
			if (i == 0)
				continue;
			--i;
		}
	}
}

void Server::sendToAllClients(Network::Packet &packet) const {
	for (const ClientInfo &client : m_info.clients()) {
		client.tcpSocket->send(packet);
	}
}
