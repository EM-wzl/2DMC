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
#include <entt/entt.hpp>

#include "common/core/ApplicationStateStack.hpp"
#include "common/core/Debug.hpp"
#include "common/core/GameTime.hpp"
#include "common/core/Registry.hpp"
#include "common/scene/component/AnimationComponent.hpp"
#include "common/scene/component/DrawableDef.hpp"
#include "common/scene/component/NetworkComponent.hpp"
#include "common/scene/component/HealthComponent.hpp"
#include "common/scene/component/PositionComponent.hpp"
#include "common/scene/component/RotationComponent.hpp"

#include "client/core/Config.hpp"
#include "client/core/GameConfig.hpp"
#include "client/graphics/Camera.hpp"
#include "client/network/Client.hpp"
#include "client/network/ClientCommandHandler.hpp"
#include "client/scene/component/DrawableComponent.hpp"
#include "client/states/ConnectionErrorState.hpp"
#include "client/states/LuaGUIState.hpp"
#include "client/world/ClientPlayer.hpp"
#include "client/world/ClientWorld.hpp"

void ClientCommandHandler::sendPlayerInvUpdate() {
	Network::Packet invPacket;
	invPacket << Network::Command::PlayerInvUpdate;
	invPacket << m_player.inventory();
	m_client.send(invPacket);
}

void ClientCommandHandler::sendPlayerPosUpdate() {
	Network::Packet packet;
	packet << Network::Command::PlayerPosUpdate;
	packet << m_player.x();
	packet << m_player.y();
	packet << m_player.z();
	m_client.send(packet);
}

void ClientCommandHandler::sendPlayerRotUpdate() {
	Network::Packet packet;
	packet << Network::Command::PlayerRotUpdate;
	packet << m_player.cameraYaw();
	packet << m_player.cameraPitch();
	m_client.send(packet);
}

void ClientCommandHandler::sendPlayerDigBlock(const glm::ivec4 &selectedBlock) {
	Network::Packet packet;
	packet << Network::Command::PlayerDigBlock
		<< s32(selectedBlock.x)
		<< s32(selectedBlock.y)
		<< s32(selectedBlock.z);
	m_client.send(packet);
}

void ClientCommandHandler::sendPlayerPlaceBlock(s32 x, s32 y, s32 z, u32 block) {
	Network::Packet packet;
	packet << Network::Command::PlayerPlaceBlock << x << y << z << block;
	m_client.send(packet);
}

void ClientCommandHandler::sendPlayerHeldItemChanged(u8 hotbarSlot, u16 itemID) {
	Network::Packet packet;
	packet << Network::Command::PlayerHeldItemChanged
		<< hotbarSlot << itemID;
	m_client.send(packet);
}

void ClientCommandHandler::sendPlayerReady() {
	Network::Packet packet;
	packet << Network::Command::PlayerReady;
	m_client.send(packet);
}

void ClientCommandHandler::sendPlayerChunkPosUpdate() const {
	Network::Packet packet;
	const Vector3i &chunkPos = m_player.getCurrentChunk();
	packet << Network::Command::PlayerChunkPosUpdate
		<< s32(chunkPos.x) << s32(chunkPos.y) << s32(chunkPos.z);
	m_client.send(packet);
}

void ClientCommandHandler::sendPlayerAttackEntity(entt::entity entityID) {
	// entityID 是客户端本地的 entt 实体句柄，**不能**直接发给服务端：
	// 客户端与服务端的 entt::registry 是两套独立分配器，本地句柄的数值和
	// 服务端的网络实体 ID 没有任何对应关系（只在双方创建顺序完全一致时才
	// 偶然相等，任何一次 despawn/回收都会让两边错位）。
	// 之前直接发本地句柄，结果攻击经常落到"服务端另一个恰好存在的实体"上：
	// 要么是无 HealthComponent 的 item_drop（静默 return，不掉血），要么是
	// 别的生物（血掉在别人身上），表现为"打到 1 血就锁住、再也打不死"。
	// 正确做法：用 NetworkComponent::entityID（服务端广播 EntitySpawn 时用的
	// 那个 ID）作为攻击目标。
	auto &registry = m_world.scene().registry();
	NetworkComponent *network = registry.try_get<NetworkComponent>(entityID);
	if (!network)
		return; // 目标已被 EntityDespawn 移除（HUD 缓存的句柄失效），不发送

	Network::Packet packet;
	packet << Network::Command::PlayerAttackEntity << network->entityID;
	m_client.send(packet);
}

void ClientCommandHandler::sendBlockActivated(const glm::ivec4 &selectedBlock) {
	Network::Packet packet;
	packet << Network::Command::BlockActivated
		<< s32(selectedBlock.x)
		<< s32(selectedBlock.y)
		<< s32(selectedBlock.z)
		<< u16(Config::screenWidth) << u16(Config::screenHeight) << u8(Config::guiScale);
	m_client.send(packet);
}

void ClientCommandHandler::sendBlockInvUpdate(Inventory &inventory) {
	Network::Packet packet;
	packet << Network::Command::BlockInvUpdate;
	packet << s32(inventory.blockPos().x) << s32(inventory.blockPos().y) << s32(inventory.blockPos().z);
	packet << inventory;
	m_client.send(packet);
}

void ClientCommandHandler::sendItemActivated(const glm::ivec4 &selectedBlock) {
	Network::Packet packet;
	packet << Network::Command::ItemActivated
		<< s32(selectedBlock.x)
		<< s32(selectedBlock.y)
		<< s32(selectedBlock.z)
		<< u16(Config::screenWidth) << u16(Config::screenHeight) << u8(Config::guiScale);
	m_client.send(packet);
}

// 请求服务端下发局域网信息（端口 + 本机 IP 列表）
void ClientCommandHandler::requestLanInfo() {
	Network::Packet packet;
	packet << Network::Command::LanInfoRequest;
	m_client.send(packet);

	m_lanInfoRequested = true;
}

void ClientCommandHandler::sendChatMessage(const std::string &message) {
	Network::Packet packet;
	packet << Network::Command::ChatMessage;
	packet << message;
	m_client.send(packet);
}

void ClientCommandHandler::sendKeyPressed(u16 keyID) {
	Network::Packet packet;
	packet << Network::Command::KeyPressed << keyID
		<< u16(Config::screenWidth) << u16(Config::screenHeight) << u8(Config::guiScale);
	m_client.send(packet);
}

template<typename ComponentType>
static void addComponentCommandCallback(Network::Command command, Client &client, ClientCommandHandler::EntityMap &entityMap, ClientWorld &world) {
	client.setCommandCallback(command, [&, command](Network::Packet &packet) {
		entt::entity entityID;
		packet >> entityID;

		auto it = entityMap.find(entityID);
		if (it != entityMap.end()) {
			// logDebug() << "Received component" << Network::commandToString(command);
			auto &component = world.scene().registry().get_or_emplace<ComponentType>(it->second);
			component.deserialize(packet);
		}
		else
			logError() << Network::commandToString(command) + ": Entity ID" << std::underlying_type_t<entt::entity>(entityID) << "is invalid";
	});
}

void ClientCommandHandler::setupCallbacks() {
	m_client.setCommandCallback(Network::Command::ClientDisconnect, [this](Network::Packet &packet) {
		u16 clientID;
		packet >> clientID;

		// 玩家断开：先从 Client::m_players 移除，再销毁 m_playerBoxes 里的对象。
		//
		// 原实现写反了：先 addPlayer()（往 m_players 里加引用）再 erase() 掉
		// m_playerBoxes 里的对象 —— m_players 存的是 Player&，对象被销毁后
		// 引用立即悬垂，HUD 遍历 players() 读名字时会访问已释放内存
		// （表现为崩溃或乱码名字）。这里改成先 removePlayer() 再 erase。
		m_client.removePlayer(clientID);

		auto it = m_playerBoxes.find(clientID);
		if (it != m_playerBoxes.end())
			m_playerBoxes.erase(it);
	});

	m_client.setCommandCallback(Network::Command::ServerTick, [this](Network::Packet &packet) {
		if (!m_isSingleplayer) { // FIXME
			sf::Uint64 ticks;
			packet >> ticks;
			GameTime::setTicks(ticks);
		}
	});

	m_client.setCommandCallback(Network::Command::ServerClosed, [this](Network::Packet &packet) {
		std::string message;
		packet >> message;

		GameConfig::isGameRunning = false;

		ApplicationStateStack::getInstance().clear();
		ApplicationStateStack::getInstance().push<ConnectionErrorState>(message, m_client.serverAddress().toString(), m_client.serverPort(), nullptr);
	});

	m_client.setCommandCallback(Network::Command::RegistryData, [this](Network::Packet &packet) {
		// FIXME: This is a quick fix for concurrency between client and server in singleplayer
		if (!m_isSingleplayer)
			Registry::getInstance().deserialize(packet);

		m_isRegistryInitialized = true;
	});

	m_client.setCommandCallback(Network::Command::ChunkData, [this](Network::Packet &packet) {
		m_world.receiveChunkData(packet);
	});

	m_client.setCommandCallback(Network::Command::ChunkUnload, [this](Network::Packet &packet) {
		s32 chunkX, chunkY, chunkZ;
		packet >> chunkX >> chunkY >> chunkZ;
		m_world.removeChunk({chunkX, chunkY, chunkZ});
	});

	m_client.setCommandCallback(Network::Command::BlockUpdate, [this](Network::Packet &packet) {
		s32 x, y, z;
		u32 block;
		packet >> x >> y >> z >> block;
		m_world.setBlock(x, y, z, u16(block & 0xffff));
		m_world.setData(x, y, z, u16((block >> 16) & 0xffff));
	});

	m_client.setCommandCallback(Network::Command::PlayerInvUpdate, [this](Network::Packet &packet) {
		u16 clientId;
		packet >> clientId;

		if (clientId == m_client.id()) {
			packet >> m_player.inventory();

			s8 heldItemSlot;
			packet >> heldItemSlot;

			if (m_player.heldItemSlot() == -1) {
				m_player.setHeldItemSlot(heldItemSlot);
			}
		}
		else
			packet >> m_playerBoxes.at(clientId).inventory();
	});

	m_client.setCommandCallback(Network::Command::LanInfo, [this](Network::Packet &packet) {
		u16 port = 0;
		u32 count = 0;
		packet >> port >> count;

		m_lanPort = port;
		m_lanAddresses.clear();
		for (u32 i = 0; i < count; ++i) {
			std::string addr;
			packet >> addr;
			m_lanAddresses.emplace_back(std::move(addr));
		}
		m_lanInfoRequested = false;

		logInfo() << "LAN info received: port" << m_lanPort << "| addresses:" << m_lanAddresses.size();
	});

	m_client.setCommandCallback(Network::Command::PlayerHealth, [this](Network::Packet &packet) {
		u16 clientId;
		u8 health, maxHealth;
		packet >> clientId >> health >> maxHealth;

		if (clientId == m_client.id()) {
			m_player.setHealth(health);
			m_player.setMaxHealth(maxHealth);
		}
		else {
			auto it = m_playerBoxes.find(clientId);
			if (it != m_playerBoxes.end()) {
				it->second.setHealth(health);
				it->second.setMaxHealth(maxHealth);
			}
		}
	});

	m_client.setCommandCallback(Network::Command::PlayerGamemode, [this](Network::Packet &packet) {
		u16 clientId;
		u8 gamemode;
		packet >> clientId >> gamemode;

		if (clientId == m_client.id()) {
			m_player.setGamemode(gamemode);
			Config::isFlyModeEnabled = (gamemode == 0);
		}
		else {
			auto it = m_playerBoxes.find(clientId);
			if (it != m_playerBoxes.end())
				it->second.setGamemode(gamemode);
		}
	});

	m_client.setCommandCallback(Network::Command::PlayerPosUpdate, [this](Network::Packet &packet) {
		double x, y, z;
		u16 clientId;
		bool isTeleportation;
		packet >> clientId;
		packet >> x >> y >> z;
		packet >> isTeleportation;

		if (clientId != m_client.id()) {
			auto it = m_playerBoxes.find(clientId);
			if (it != m_playerBoxes.end())
				it->second.setPosition(x, y, z);
		}
		else if (isTeleportation) {
			m_player.setPosition(x, y, z);
		}
	});

	m_client.setCommandCallback(Network::Command::PlayerRotUpdate, [this](Network::Packet &packet) {
		float yaw, pitch;
		u16 clientId;
		packet >> clientId;
		packet >> yaw >> pitch;

		if (clientId != m_client.id()) {
			auto it = m_playerBoxes.find(clientId);
			if (it != m_playerBoxes.end())
				it->second.Player::setRotation(yaw, pitch);
		}
	});

	m_client.setCommandCallback(Network::Command::PlayerSpawn, [this](Network::Packet &packet) {
		u16 clientId;
		Vector3d pos;
		u16 dimension;
		std::string username;
		float cameraYaw, cameraPitch;
		packet >> clientId >> pos.x >> pos.y >> pos.z >> dimension
			>> username >> cameraYaw >> cameraPitch;

		if (clientId != m_client.id()) {
			m_playerBoxes.emplace(clientId, PlayerBox{m_player.camera()});
			Player &player = m_playerBoxes.at(clientId);
			player.setPosition(pos.x, pos.y, pos.z);
			player.setRotation(cameraYaw, cameraPitch);
			player.setDimension(dimension);
			player.setClientID(clientId);
			player.setName(username);
			m_client.addPlayer(player);
		}
		else {
			m_player.setPosition(pos.x, pos.y, pos.z);
			m_player.setRotation(cameraYaw, cameraPitch);
			m_player.setDimension(dimension);
			m_player.updateCamera();
		}
	});

	m_client.setCommandCallback(Network::Command::PlayerChangeDimension, [this](Network::Packet &packet) {
		u16 clientId, dimension;
		s32 x, y, z;
		packet >> clientId >> x >> y >> z >> dimension;

		if (clientId == m_client.id()) {
			m_player.setDimension(dimension);
			m_player.setPosition(x + 0.5, y + 0.5, z + 0.5);
			m_world.clear();
			m_world.changeDimension(dimension);
			m_entityMap.clear();
			sendPlayerChunkPosUpdate();
		}
	});

	m_client.setCommandCallback(Network::Command::BlockGUIData, [this](Network::Packet &packet) {
		if (!LuaGUIState::isActive)
			ApplicationStateStack::getInstance().push<LuaGUIState>(*this, m_player, m_world, packet, &ApplicationStateStack::getInstance().top());
	});

	m_client.setCommandCallback(Network::Command::BlockInvUpdate, [this](Network::Packet &packet) {
		Vector3<s32> pos;
		packet >> pos.x >> pos.y >> pos.z;

		BlockData *data = m_world.getBlockData(pos.x, pos.y, pos.z);
		if (!data)
			data = m_world.addBlockData(pos.x, pos.y, pos.z);

		if (data)
			packet >> data->inventory;
	});

	m_client.setCommandCallback(Network::Command::BlockDataUpdate, [this](Network::Packet &packet) {
		Vector3<s32> pos;
		packet >> pos.x >> pos.y >> pos.z;

		Chunk *chunk = m_world.getChunkAtBlockPos(pos.x, pos.y, pos.z);
		if (chunk) {
			BlockData *data = chunk->getBlockData(pos.x & (CHUNK_WIDTH - 1), pos.y & (CHUNK_DEPTH - 1), pos.z & (CHUNK_HEIGHT - 1));
			if (!data)
				data = m_world.addBlockData(pos.x, pos.y, pos.z);

			if (data) {
				packet >> data->meta;
			}
		}
	});

	m_client.setCommandCallback(Network::Command::EntitySpawn, [this](Network::Packet &packet) {
		entt::entity entityID;
		packet >> entityID;

		auto &registry = m_world.scene().registry();

		auto it = m_entityMap.find(entityID);
		if (it == m_entityMap.end()) {
			entt::entity entity = registry.create();
			m_entityMap.emplace(entityID, entity);
			registry.emplace<NetworkComponent>(entity, entityID);
			// logDebug() << "Entity spawned:" << std::underlying_type_t<entt::entity>(entityID);
		}
		else if (registry.get<NetworkComponent>(it->second).entityID != entityID) {
			logError() << "EntitySpawn: Entity ID" << std::underlying_type_t<entt::entity>(entityID) << "is invalid";
		}
	});

	m_client.setCommandCallback(Network::Command::EntityDespawn, [this](Network::Packet &packet) {
		entt::entity entityID;
		packet >> entityID;

		auto it = m_entityMap.find(entityID);
		if (it != m_entityMap.end()) {
			m_world.scene().registry().destroy(it->second);
			// 必须从映射里移除：否则该网络 ID 之后被复用时，
			// EntitySpawn 会以为实体已存在而跳过创建（表现为生物"隐身"/打不死）
			m_entityMap.erase(it);
		}
		else
			logError() << "EntityDespawn: Entity ID" << std::underlying_type_t<entt::entity>(entityID) << "is invalid";
	});

	addComponentCommandCallback<PositionComponent>(Network::Command::EntityPosition, m_client, m_entityMap, m_world);
	addComponentCommandCallback<RotationComponent>(Network::Command::EntityRotation, m_client, m_entityMap, m_world);
	addComponentCommandCallback<AnimationComponent>(Network::Command::EntityAnimation, m_client, m_entityMap, m_world);
	addComponentCommandCallback<DrawableDef>(Network::Command::EntityDrawableDef, m_client, m_entityMap, m_world);
	addComponentCommandCallback<HealthComponent>(Network::Command::EntityHealth, m_client, m_entityMap, m_world);
}
