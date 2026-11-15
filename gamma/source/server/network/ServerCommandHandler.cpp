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
#include "common/core/GameTime.hpp"
#include "common/core/Registry.hpp"
#include "common/core/Utils.hpp"
#include "common/scene/component/AnimationComponent.hpp"
#include "common/scene/component/DrawableDef.hpp"
#include "common/scene/component/NetworkComponent.hpp"
#include "common/scene/component/HealthComponent.hpp"
#include "common/scene/component/PositionComponent.hpp"
#include "common/world/BlockData.hpp"
#include "server/core/PlayerList.hpp"
#include "server/lua/ScriptEngine.hpp"
#include "server/network/Server.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/scene/EntityWrapper.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/ServerItem.hpp"
#include "server/world/WorldController.hpp"

void ServerCommandHandler::sendServerTick(const ClientInfo *client) const {
	Network::Packet packet;
	packet << Network::Command::ServerTick << (sf::Uint64)GameTime::getTicks();

	if (!client)
		m_server.sendToAllClients(packet);
	else
		client->tcpSocket->send(packet);
}

void ServerCommandHandler::sendServerClosed(const std::string &message, ClientInfo *client) const {
	Network::Packet packet;
	packet << Network::Command::ServerClosed << message;

	if (!client) {
		m_server.sendToAllClients(packet);
		m_server.disconnectAllClients();
	}
	else {
		client->tcpSocket->send(packet);
		m_server.disconnectClient(*client);
	}
}

void ServerCommandHandler::sendChunkUnload(s32 chunkX, s32 chunkY, s32 chunkZ, const ClientInfo *client) const {
	Network::Packet packet;
	packet << Network::Command::ChunkUnload << chunkX << chunkY << chunkZ;

	if (!client)
		m_server.sendToAllClients(packet);
	else
		client->tcpSocket->send(packet);
}

void ServerCommandHandler::sendBlockDataUpdate(s32 x, s32 y, s32 z, const BlockData *blockData, const ClientInfo *client) const {
	Network::Packet packet;
	packet << Network::Command::BlockDataUpdate << x << y << z << blockData->meta;

	if (!client)
		m_server.sendToAllClients(packet);
	else
		client->tcpSocket->send(packet);
}

void ServerCommandHandler::sendBlockInvUpdate(s32 x, s32 y, s32 z, const Inventory &inventory, const ClientInfo *client) const {
	Network::Packet packet;
	packet << Network::Command::BlockInvUpdate << x << y << z << inventory;

	if (!client)
		m_server.sendToAllClients(packet);
	else
		client->tcpSocket->send(packet);
}

void ServerCommandHandler::sendPlayerPosUpdate(u16 clientID, bool isTeleportation, const ClientInfo *client) const {
	const ServerPlayer *player = m_players.getPlayerFromClientID(clientID);
	if (player) {
		Network::Packet packet;
		packet << Network::Command::PlayerPosUpdate;
		packet << clientID;
		packet << player->x() << player->y() << player->z();
		packet << isTeleportation;

		if (!client)
			m_server.sendToAllClients(packet);
		else
			client->tcpSocket->send(packet);
	}
	else
		logError() << ("Failed to send position update for player " + std::to_string(clientID) + ": Player not found").c_str();
}

void ServerCommandHandler::sendPlayerRotUpdate(u16 clientID, const ClientInfo *client) const {
	const ServerPlayer *player = m_players.getPlayerFromClientID(clientID);
	if (player) {
		Network::Packet packet;
		packet << Network::Command::PlayerRotUpdate;
		packet << clientID;
		packet << player->cameraYaw() << player->cameraPitch();

		if (!client)
			m_server.sendToAllClients(packet);
		else
			client->tcpSocket->send(packet);
	}
	else
		logError() << ("Failed to send rotation update for player " + std::to_string(clientID) + ": Player not found").c_str();
}

void ServerCommandHandler::sendPlayerInvUpdate(u16 clientID, const ClientInfo *client) const {
	ServerPlayer *player = m_players.getPlayerFromClientID(clientID);
	if (player) {
		Network::Packet packet;
		packet << Network::Command::PlayerInvUpdate;
		packet << clientID << player->inventory() << player->heldItemSlot();

		if (!client)
			m_server.sendToAllClients(packet);
		else
			client->tcpSocket->send(packet);
	}
	else
		logError() << ("Failed to send inventory update for player " + std::to_string(clientID) + ": Player not found").c_str();
}

void ServerCommandHandler::sendPlayerHealth(u16 clientID, u8 health, u8 maxHealth, const ClientInfo *client) const {
	Network::Packet packet;
	packet << Network::Command::PlayerHealth << clientID << health << maxHealth;

	if (!client)
		m_server.sendToAllClients(packet);
	else
		client->tcpSocket->send(packet);
}

void ServerCommandHandler::sendPlayerGamemode(u16 clientID, u8 gamemode, const ClientInfo *client) const {
	Network::Packet packet;
	packet << Network::Command::PlayerGamemode << clientID << gamemode;

	if (!client)
		m_server.sendToAllClients(packet);
	else
		client->tcpSocket->send(packet);
}

void ServerCommandHandler::sendPlayerChangeDimension(u16 clientID, s32 x, s32 y, s32 z, u16 dimension, const ClientInfo *client) const {
	// 边界保护：dimension 来自 Lua，非法值会让 getWorld() 的 deque 下标访问越界（UB）。
	const u16 worldCount = m_worldController.getWorldCount();
	if (worldCount == 0 || dimension >= worldCount) {
		logError() << "Refusing to change dimension to out-of-range value"
			<< dimension << "|" << "world count:" << worldCount;
		return;
	}

	ServerPlayer *player = m_players.getPlayerFromClientID(clientID);
	if (player) {
		Network::Packet packet;
		packet << Network::Command::PlayerChangeDimension;
		packet << clientID << x << y << z << dimension;

		if (client) {
			client->tcpSocket->send(packet);

			// FIXME: sendPlayerChangeDimension shouldn't be exposed to Lua
			//        Instead, there should be a changePlayerDimension function that sends
			//        the packet above + the entities (instead of doing that here)
			//        also, it should change player dimension and position accordingly
			m_worldController.getWorld(dimension).scene().sendEntities(*client);

			player->setPosition(x + 0.5, y + 0.5, z + 0.5);
			player->setDimension(dimension);
			player->clearLoadedChunks();
		}
		else
			m_server.sendToAllClients(packet);
	}
	else
		logError() << ("Failed to send dimension change for player " + std::to_string(clientID) + ": Player not found").c_str();
}

// 杀死实体：触发 EntityKilled 事件（掉落由 Lua 侧配置）→ 广播 EntityDespawn → 从场景移除。
//
// 注意顺序：EntityKilled 必须在 registry.destroy() **之前**触发——销毁后
// EntityWrapper 里的 entt 句柄就失效了，Lua 侧读不到位置/类型。
bool ServerCommandHandler::killEntity(EntityWrapper &entity, ClientInfo &client) {
	entt::entity entityID = entity.id();
	if (!entity.isValid())
		return false;

	PositionComponent *position = entity.getPositionComponent();
	NetworkComponent *network = entity.getNetworkComponent();
	if (!position || !network)
		return false;

	// position->dimension 来自网络实体数据，必须校验范围：
	// WorldController::getWorld() 是 deque 下标访问，越界是未定义行为。
	const u16 worldCount = m_worldController.getWorldCount();
	if (worldCount == 0 || position->dimension >= worldCount) {
		logError() << "killEntity: entity has out-of-range dimension " << position->dimension
			<< " (world count " << worldCount << "); refusing";
		return false;
	}

	ServerWorld &world = m_worldController.getWorld(position->dimension);
	entt::registry &registry = world.scene().registry();

	// 二次确认：实体必须真的属于这个世界。若 entity 是在别的维度/别的
	// registry 里创建的，销毁它会在错误的 registry 上操作（entt 句柄在不同
	// registry 间不通用），轻则杀不死、重则破坏别的实体。
	if (!registry.valid(entityID)) {
		logError() << "killEntity: entity does not belong to dimension " << position->dimension;
		return false;
	}

	ServerPlayer *player = m_players.getPlayerFromClientID(client.id);

	try {
		// player 传指针：找不到击杀者时为 nullptr，sol2 会转成 Lua nil，
		// 这样 Lua 侧仍能处理"生物被环境杀死"的情况（只是拿不到玩家）。
		m_scriptEngine.luaCore().onEvent(LuaEventType::EntityKilled,
			glm::ivec3{position->x, position->y, position->z},
			entity, player, world, client, *this);
	}
	catch (const std::exception &e) {
		logError() << "[EntityKilled] " << e.what();
	}

	sendEntityDespawn(network->entityID);
	registry.destroy(entityID);
	return true;
}

void ServerCommandHandler::sendChatMessage(u16 clientID, const std::string &message, const ClientInfo *client) const {
	Network::Packet packet;
	packet << Network::Command::ChatMessage << clientID << message;

	if (!client)
		m_server.sendToAllClients(packet);
	else
		client->tcpSocket->send(packet);
}

// 在指定维度放置一个 Minecraft 风格的传送门（4 宽 × 5 高）：
//   - 外圈是门框方块（黑曜石之类）
//   - 内部 2×3 是传送门方块（可通行的"洞"）
//
// 用途：MC 里从主世界进下界后，下界会有一扇对应的下界传送门可以原路返回。
// 这里在目标维度就地生成一扇，并把方块变化广播给客户端。
//
// 注意 1：必须先确保目标区块已生成地形，否则放置的方块会在之后的
//         generateChunk() 里被地形覆盖（generateChunk 对未初始化 chunk 生效）。
// 注意 2：坐标换算必须和 World::getChunkAtBlockPos() 完全一致：
//           chunkX = x / CHUNK_WIDTH (16)
//           chunkY = y / CHUNK_DEPTH (16)   <- 是 DEPTH 不是 HEIGHT
//           chunkZ = z / CHUNK_HEIGHT (32)  <- 是 HEIGHT 不是 WIDTH
//         引擎里 y 是垂直轴、z 是"高度方向的 chunk 索引轴"。
void ServerCommandHandler::createPortal(u16 dimension, s32 x, s32 y, s32 z,
	const std::string &portalBlockID, const std::string &frameBlockID) const {
	const u16 worldCount = m_worldController.getWorldCount();
	if (worldCount == 0 || dimension >= worldCount) {
		logError() << "create_portal: out-of-range dimension" << dimension;
		return;
	}

	ServerWorld &world = m_worldController.getWorld(dimension);

	// 方块 ID 由 Lua 传入，拼错时 getBlockFromStringID 会抛 EXCEPTION。
	// 这个函数由 Lua 调用，异常会穿透到服务端主线程，必须在就地挡住。
	u16 portalID = 0, frameID = 0;
	try {
		portalID = Registry::getInstance().getBlockFromStringID(portalBlockID).id();
		frameID = Registry::getInstance().getBlockFromStringID(frameBlockID).id();
	}
	catch (const std::exception &e) {
		logError() << "create_portal: invalid block id: " << e.what();
		return;
	}

	// 传送门是 4(x) × 5(z)，底部再补一格门框当"站台"，
	// 所以实际占用的范围是 x..x+3, y-1..y, z..z+4。
	// 它可能横跨多个 chunk，必须把它们全部生成出来再放方块，
	// 否则后生成的 chunk 地形会覆盖已经放好的门。
	const s32 minChunkX = ((x) & -CHUNK_WIDTH) / CHUNK_WIDTH;
	const s32 maxChunkX = ((x + 3) & -CHUNK_WIDTH) / CHUNK_WIDTH;
	const s32 minChunkY = ((y - 1) & -CHUNK_DEPTH) / CHUNK_DEPTH;
	const s32 maxChunkY = (y & -CHUNK_DEPTH) / CHUNK_DEPTH;
	const s32 minChunkZ = ((z) & -CHUNK_HEIGHT) / CHUNK_HEIGHT;
	const s32 maxChunkZ = ((z + 4) & -CHUNK_HEIGHT) / CHUNK_HEIGHT;

	for (s32 cz = minChunkZ ; cz <= maxChunkZ ; ++cz)
		for (s32 cy = minChunkY ; cy <= maxChunkY ; ++cy)
			for (s32 cx = minChunkX ; cx <= maxChunkX ; ++cx) {
				ServerChunk &chunk = world.getOrCreateChunk(cx, cy, cz);
				if (!chunk.isInitialized())
					world.generateChunk(chunk);
			}

	// 放置门框 + 内部传送门方块
	auto place = [&](int bx, int by, int bz, u16 id) {
		world.setBlock(bx, by, bz, id);

		Network::Packet packet;
		packet << Network::Command::BlockUpdate << bx << by << bz << u32(id & 0xffff);
		m_server.sendToAllClients(packet);
	};

	for (int dz = 0 ; dz < 5 ; ++dz) {
		for (int dx = 0 ; dx < 4 ; ++dx) {
			const bool isFrame = (dx == 0 || dx == 3 || dz == 0 || dz == 4);
			place(x + dx, y, z + dz, isFrame ? frameID : portalID);
		}
	}

	// 底部一格门框：玩家站在这里，不会掉出世界
	for (int dx = 0 ; dx < 4 ; ++dx)
		place(x + dx, y - 1, z, frameID);

	logInfo() << "Created portal in dimension" << dimension
		<< "at" << x << "," << y << "," << z
		<< "| portal:" << portalBlockID << "|" << "frame:" << frameBlockID;
}

void ServerCommandHandler::sendEntitySpawn(entt::entity entityID, const ClientInfo *client) const {
	Network::Packet packet;
	packet << Network::Command::EntitySpawn << entityID;

	if (!client)
		m_server.sendToAllClients(packet);
	else
		client->tcpSocket->send(packet);
}

void ServerCommandHandler::sendEntityDespawn(entt::entity entityID, const ClientInfo *client) const {
	Network::Packet packet;
	packet << Network::Command::EntityDespawn << entityID;

	if (!client)
		m_server.sendToAllClients(packet);
	else
		client->tcpSocket->send(packet);
}

void ServerCommandHandler::setupCallbacks() {
	m_server.setConnectionCallback([this](ClientInfo &client, Network::Packet &connectionPacket) {
		std::string username;
		connectionPacket >> username;

		if (!utils::regexMatch(username, "^[A-Za-z0-9_]+$")) {
			sendServerClosed("Invalid username", &client);
			return;
		}

		// 只有单机主机玩家才走「接管存档角色」路径；局域网玩家必须按普通玩家处理，
		// 否则第二个玩家会顶掉主机玩家的角色（见 PlayerList::connectPlayer）。
		ServerPlayer *player = m_players.connectPlayer(username, client, client.isSingleplayer);
		if (!player) {
			sendServerClosed("User is already online", &client);
			return;
		}

		// Try to find a valid spawn point (WIP)
		if (player->isNewPlayer()) {
			// FIXME: Default dimension hardcoded here
			ServerWorld &world = m_worldController.getWorld(0);
			Heightmap &heightmap = world.heightmap();

			bool hasFoundPosition = false;
			for (s32 spawnChunkX = 0 ; spawnChunkX < 16 && !hasFoundPosition ; ++spawnChunkX) {
				for (s32 spawnChunkY = 0 ; spawnChunkY < 16 && !hasFoundPosition ; ++spawnChunkY) {
					for(int y = 0 ; y < CHUNK_DEPTH && !hasFoundPosition ; y++) {
						for(int x = 0 ; x < CHUNK_WIDTH ; x++) {
							int maxChunkZ = heightmap.getHighestChunkAt(x + spawnChunkX * CHUNK_WIDTH, y + spawnChunkY * CHUNK_DEPTH);
							int worldZ = heightmap.getHighestBlockAt(x + spawnChunkX * CHUNK_WIDTH, y + spawnChunkY * CHUNK_DEPTH) + 1;
							int z = math::pmod(worldZ, CHUNK_WIDTH);

							world.generateChunk(world.getOrCreateChunk(spawnChunkX - 1, spawnChunkY,     maxChunkZ));
							world.generateChunk(world.getOrCreateChunk(spawnChunkX + 1, spawnChunkY,     maxChunkZ));
							world.generateChunk(world.getOrCreateChunk(spawnChunkX,     spawnChunkY - 1, maxChunkZ));
							world.generateChunk(world.getOrCreateChunk(spawnChunkX,     spawnChunkY + 1, maxChunkZ));
							world.generateChunk(world.getOrCreateChunk(spawnChunkX,     spawnChunkY,     maxChunkZ - 1));
							world.generateChunk(world.getOrCreateChunk(spawnChunkX,     spawnChunkY,     maxChunkZ + 1));

							ServerChunk &chunk = world.getOrCreateChunk(spawnChunkX, spawnChunkY, maxChunkZ);
							world.generateChunk(chunk);

							const BlockState *blockBelow = chunk.getBlockState(x, y, z - 1);
							const Block &blockFeet = m_registry.getBlock(chunk.getBlock(x, y, z));
							const Block &blockHead = m_registry.getBlock(chunk.getBlock(x, y, z + 1));

							if (blockFeet.id() == 0 && blockHead.id() == 0
							&& blockBelow && blockBelow->isCollidable()
							&& blockBelow->drawType() != BlockDrawType::Leaves)
							{
								player->setPosition(x + spawnChunkX * CHUNK_WIDTH + .5, y + spawnChunkY * CHUNK_DEPTH + .5, worldZ + .2);
								hasFoundPosition = true;
								break;
							}
						}
					}
				}
			}

			if (!hasFoundPosition)
				logError() << "Can't find a good position for the player";

			player->setHeldItemSlot(0);
		}

		// Send the registry
		Network::Packet packet;
		packet << Network::Command::RegistryData;
		m_registry.serialize(packet);
		client.tcpSocket->send(packet);

		// Send already connected players to the new client
		// 注意：PlayerSpawn 的第一个字段是 u16 clientID（客户端按此解析），
		// 不是玩家名字符串。此前误发 it.first（std::string），导致该包畸形，
		// 客户端解析越界（上游遗留 bug，多人同服时必现）。
		//
		// 跳过刚连接的玩家自己：connectPlayer() 已经把他加进 m_players，
		// 若不跳过，客户端会收到两次自己的 PlayerSpawn（下面 322 行还会
		// 广播一次），表现为重复创建 PlayerBox / 名字闪一下。
		for (auto &it : m_players) {
			if (it.second.clientID() == client.id)
				continue;

			Network::Packet spawnPacket;
			spawnPacket << Network::Command::PlayerSpawn << it.second.clientID();
			spawnPacket << it.second.x() << it.second.y() << it.second.z() << it.second.dimension() << it.second.name();
			spawnPacket << it.second.cameraYaw() << it.second.cameraPitch();
			client.tcpSocket->send(spawnPacket);
		}

		// Triggers the 'PlayerConnected' Lua event
		if (player->isNewPlayer())
			m_scriptEngine.luaCore().onEvent(LuaEventType::PlayerConnected, glm::ivec3{player->x(), player->y(), player->z()}, player, client, *this);

		// Send inventory
		sendPlayerInvUpdate(client.id, &client);

		// 生命值 / 游戏模式（/heal /life /kill /gamemode 命令的客户端状态）
		sendPlayerHealth(client.id, player->health(), player->maxHealth(), &client);
		sendPlayerGamemode(client.id, player->gamemode(), &client);

		// Send spawn packet to all clients for this player
		Network::Packet spawnPacket;
		spawnPacket << Network::Command::PlayerSpawn << client.id;
		spawnPacket << player->x() << player->y() << player->z() << player->dimension() << player->name();
		spawnPacket << player->cameraYaw() << player->cameraPitch();
		m_server.sendToAllClients(spawnPacket);

		// Send entities to the client
		m_worldController.getWorld(player->dimension()).scene().sendEntities(client);
	});

	m_server.setCommandCallback(Network::Command::ClientDisconnect, [this](ClientInfo &client, Network::Packet &) {
		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (player) {
			player->clearLoadedChunks();
			m_players.disconnectPlayer(client.playerName);
		}
	});

	m_server.setCommandCallback(Network::Command::ChunkUnload, [this](ClientInfo &client, Network::Packet &packet) {
		s32 cx, cy, cz;
		packet >> cx >> cy >> cz;

		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (player) {
			player->removeLoadedChunk(Vector3i{cx, cy, cz});
		}
		else
			logError() << ("Failed to unload chunk for player " + std::to_string(client.id) + ": Player not found").c_str();
	});

	m_server.setCommandCallback(Network::Command::PlayerInvUpdate, [this](ClientInfo &client, Network::Packet &packet) {
		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (player) {
			packet >> player->inventory();
		}
		else
			logError() << ("Failed to update inventory of player " + std::to_string(client.id) + ": Player not found").c_str();
	});

	m_server.setCommandCallback(Network::Command::PlayerPosUpdate, [this](ClientInfo &client, Network::Packet &packet) {
		double x, y, z;
		packet >> x >> y >> z;

		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (player) {
			player->setPosition(x, y, z);
		}
		else
			logError() << ("Failed to update position of player " + std::to_string(client.id) + ": Player not found").c_str();
	});

	m_server.setCommandCallback(Network::Command::PlayerRotUpdate, [this](ClientInfo &client, Network::Packet &packet) {
		float yaw, pitch;
		packet >> yaw >> pitch;

		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (player) {
			player->setRotation(yaw, pitch);
		}
		else
			logError() << ("Failed to update rotation of player " + std::to_string(client.id) + ": Player not found").c_str();
	});

	m_server.setCommandCallback(Network::Command::PlayerPlaceBlock, [this](ClientInfo &client, Network::Packet &packet) {
		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (player) {
			s32 x, y, z;
			u32 block;
			packet >> x >> y >> z >> block;

			ServerWorld &world = getWorldForClient(client.id);
			world.setData(x, y, z, u16(block >> 16));
			world.setBlock(x, y, z, u16(block & 0xffff));

			const BlockState *blockState = world.getBlockState(x, y, z);
			if (!blockState) return;

			m_scriptEngine.luaCore().onEvent(LuaEventType::BlockPlaced, glm::ivec3{x, y, z}, *blockState, *player, world, client, *this);

			Network::Packet answer;
			answer << Network::Command::BlockUpdate << x << y << z << block;
			m_server.sendToAllClients(answer);
		}
		else
			logError() << ("Failed to place block using player " + std::to_string(client.id) + ": Player not found").c_str();
	});

	m_server.setCommandCallback(Network::Command::PlayerAttackEntity, [this](ClientInfo &client, Network::Packet &packet) {
		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (!player) {
			logError() << ("Failed to attack entity using player " + std::to_string(client.id) + ": Player not found").c_str();
			return;
		}

		entt::entity networkID;
		packet >> networkID;

		ServerWorld &world = getWorldForClient(client.id);
		entt::registry &registry = world.scene().registry();

		// 包里带的是**网络实体 ID**（客户端从 EntitySpawn 广播里拿到的那个），
		// 不是本地 entt 句柄。必须按 NetworkComponent::entityID 反查真实实体，
		// 否则客户端本地句柄会被当成服务端实体 ID 使用，攻击落到别的实体
		// （item_drop 或另一只生物）上，表现为"打不动 / 打到 1 血锁住"。
		entt::entity entityID = entt::null;
		registry.view<NetworkComponent>().each([&](auto entity, auto &network) {
			if (network.entityID == networkID)
				entityID = entity;
		});
		if (entityID == entt::null || !registry.valid(entityID))
			return;

		EntityWrapper wrapper{entityID, registry};
		if (wrapper.getMaxHealth() <= 0)
			return; // 非生物（无 HealthComponent）

		// 已经 0 血的实体（理论上会在下面立刻销毁，这里只是防御）
		if (wrapper.isDead())
			return;

		// 攻击距离校验：防止客户端隔空打怪（服务端权威）
		auto *position = registry.try_get<PositionComponent>(entityID);
		if (position) {
			double dx = position->x - player->x();
			double dy = position->y - player->y();
			double dz = position->z - player->z();
			if (dx * dx + dy * dy + dz * dz > 36.0) // 6 格
				return;
		}

		// 伤害值：手持物品的 attack 值（工具/剑），空手为 1
		// 注意：Player::m_heldItemSlot 默认是 -1，heldItemStack() 会把它转成
		// u16 再调 Inventory::getStack 的 m_items.at() → 越界抛异常 → 服务端崩溃。
		// heldItemStack() 现在内部已做槽位保护（越界返回空栈），这里再判 amount。
		int damage = 1;
		const ItemStack &held = player->heldItemStack();
		if (held.amount() > 0) {
			const Item &item = held.item();
			damage = std::max(1, (int)item.getGroupValue("group:om_weapon"));
		}

		wrapper.damage(damage);

		// 立刻把新血量广播给所有客户端（HealthComponent::isUpdated 也会让
		// NetworkController 下一 tick 再发一次，这里发是为了即时反馈）。
		// 注意：EntityHealth 包里必须用 NetworkComponent::entityID（网络 ID），
		// 不能用本地 entt 句柄——客户端按网络 ID 在 m_entityMap 里查实体，
		// 用本地句柄会查不到，客户端 HUD 血量就永远停在旧值（"1 血锁住"）。
		auto *health = registry.try_get<HealthComponent>(entityID);
		if (health) {
			Network::Packet answer;
			answer << Network::Command::EntityHealth << networkID << health->health << health->maxHealth << health->hurtTimer;
			m_server.sendToAllClients(answer);
		}

		// 血量归零 → 生物死亡：广播 EntityDespawn 并从场景移除
		if (wrapper.isDead()) {
			killEntity(wrapper, client);
		}
	});

	m_server.setCommandCallback(Network::Command::PlayerDigBlock, [this](ClientInfo &client, Network::Packet &packet) {
		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (player) {
			s32 x, y, z;
			packet >> x >> y >> z;

			ServerWorld &world = getWorldForClient(client.id);
			const BlockState *blockState = world.getBlockState(x, y, z);
			if (!blockState) return;
			world.setBlock(x, y, z, 0);

			m_scriptEngine.luaCore().onEvent(LuaEventType::BlockDigged, glm::ivec3{x, y, z}, *blockState, *player, world, client, *this);

			Network::Packet answer;
			answer << Network::Command::BlockUpdate << x << y << z << u32(0);
			m_server.sendToAllClients(answer);
		}
		else
			logError() << ("Failed to dig block using player " + std::to_string(client.id) + ": Player not found").c_str();
	});

	m_server.setCommandCallback(Network::Command::PlayerHeldItemChanged, [this](ClientInfo &client, Network::Packet &packet) {
		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (player) {
			u8 hotbarSlot;
			u16 itemID;
			packet >> hotbarSlot >> itemID;
			if (player->inventory().getStack(hotbarSlot, 0).item().id() != itemID)
				logWarning() << "PlayerHeldItemChanged:" << "Desync of item ID between client and server";

			player->setHeldItemSlot(hotbarSlot);
		}
		else
			logError() << ("Failed to change held item of player " + std::to_string(client.id) + ": Player not found").c_str();
	});

	m_server.setCommandCallback(Network::Command::PlayerReady, [this](ClientInfo &client, Network::Packet &) {
		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (player) {
			player->setReady(true);
		}
		else
			logError() << ("Failed to change held item of player " + std::to_string(client.id) + ": Player not found").c_str();
	});

	// 客户端请求局域网信息（ESC 菜单「开放局域网」用）：
	// 服务端回传实际监听端口 + 本机所有 IPv4 地址。
	//
	// 安全考虑：只允许「单机主机玩家」查询。本机 IP 列表属于内网信息，
	// 不应让任意局域网访客拿到（他们本来就知道该连哪个地址）。
	m_server.setCommandCallback(Network::Command::LanInfoRequest, [this](ClientInfo &client, Network::Packet &) {
		if (!client.isSingleplayer) {
			logWarning() << "LanInfoRequest denied for non-host client" << client.id;
			return;
		}

		Network::Packet answer;
		answer << Network::Command::LanInfo << m_server.port();

		std::vector<std::string> addresses = Server::localIPv4Addresses();
		answer << (u32)addresses.size();
		for (const std::string &addr : addresses)
			answer << addr;

		if (client.tcpSocket->send(answer) != sf::Socket::Done)
			logWarning() << "Failed to send LAN info to client" << client.id;

		logInfo() << "LAN info requested by client" << client.id
			<< "| port:" << m_server.port()
			<< "| addresses:" << addresses.size();
	});

	m_server.setCommandCallback(Network::Command::PlayerChunkPosUpdate, [this](ClientInfo &client, Network::Packet &packet) {
		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (player) {
			s32 chunkX, chunkY, chunkZ;
			packet >> chunkX >> chunkY >> chunkZ;

			ServerWorld &world = getWorldForClient(client.id);
			world.updatePlayerChunks(*player, chunkX, chunkY, chunkZ);
		}
		else
			logError() << ("Failed to update chunk position of player " + std::to_string(client.id) + ": Player not found").c_str();
	});

	m_server.setCommandCallback(Network::Command::BlockActivated, [this](ClientInfo &client, Network::Packet &packet) {
		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (player) {
			s32 x, y, z;
			packet >> x >> y >> z >> client.screenWidth >> client.screenHeight >> client.guiScale;

			ServerWorld &world = getWorldForClient(client.id);

			u16 id = world.getBlock(x, y, z);
			ServerBlock &block = (ServerBlock &)(m_registry.getBlock(id));
			bool hasBeenActivated = block.onBlockActivated({x, y, z}, *player, world, client, *this);

			if (hasBeenActivated)
				m_scriptEngine.luaCore().onEvent(LuaEventType::BlockActivated, glm::ivec3{x, y, z}, block, *player, world, client, *this);
		}
		else
			logError() << ("Failed to activate block using player '" + client.playerName + "': Player not found").c_str();
	});

	m_server.setCommandCallback(Network::Command::BlockInvUpdate, [this](ClientInfo &client, Network::Packet &packet) {
		Vector3<s32> pos;
		packet >> pos.x >> pos.y >> pos.z;

		BlockData *data = getWorldForClient(client.id).getBlockData(pos.x, pos.y, pos.z);
		if (data)
			packet >> data->inventory;
		else
			logError() << "BlockInvUpdate: No block data found at" << pos.x << pos.y << pos.z;
	});

	m_server.setCommandCallback(Network::Command::BlockDataUpdate, [this](ClientInfo &client, Network::Packet &packet) {
		Vector3<s32> pos;
		packet >> pos.x >> pos.y >> pos.z;

		BlockData *data = getWorldForClient(client.id).getBlockData(pos.x, pos.y, pos.z);
		if (data) {
			packet >> data->meta;
		}
		else
			logError() << "BlockDataUpdate: No block data found at" << pos.x << pos.y << pos.z;
	});

	m_server.setCommandCallback(Network::Command::ItemActivated, [this](ClientInfo &client, Network::Packet &packet) {
		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (player) {
			s32 x, y, z;
			packet >> x >> y >> z >> client.screenWidth >> client.screenHeight >> client.guiScale;

			ServerWorld &world = getWorldForClient(client.id);

			u16 id = world.getBlock(x, y, z);
			ServerBlock &block = (ServerBlock &)(m_registry.getBlock(id));

			ServerItem &item = (ServerItem &)player->heldItemStack().item();
			if (item.canBeActivated()) {
				bool hasBeenActivated = item.onItemActivated({x, y, z}, block, *player, world, client, *this);

				if (hasBeenActivated)
					m_scriptEngine.luaCore().onEvent(LuaEventType::ItemActivated, glm::ivec3{x, y, z}, block, *player, world, client, *this);
			}
		}
		else
			logError() << ("Failed to activate item using player '" + client.playerName + "': Player not found").c_str();
	});

	m_server.setCommandCallback(Network::Command::ChatMessage, [this](ClientInfo &client, Network::Packet &packet) {
		std::string message;
		packet >> message;

		if (message[0] != '/' || (message.length() > 1 && message[1] == '/')) {
			if (message[0] == '/' && message.length() > 1 && message[1] == '/')
				sendChatMessage(client.id, message.substr(1));
			else
				sendChatMessage(client.id, message);
		}
		else {
			m_chatCommandHandler.parseCommand(message.substr(1), client);
		}
	});

	m_server.setCommandCallback(Network::Command::KeyPressed, [this](ClientInfo &client, Network::Packet &packet) {
		u16 keyID;
		packet >> keyID >> client.screenWidth >> client.screenHeight >> client.guiScale;

		m_registry.getKey(keyID).callback()(keyID, client);
	});
}

void ServerCommandHandler::setPlayerPosition(u16 clientID, s32 x, s32 y, s32 z) {
	ServerPlayer *player = m_players.getPlayerFromClientID(clientID);
	if (player)
		player->setPosition(x, y, z);
	else
		logError() << ("Failed to set position for player " + std::to_string(clientID) + ": Player not found").c_str();
}

inline ServerWorld &ServerCommandHandler::getWorldForClient(u16 clientID) {
	ServerPlayer *player = m_players.getPlayerFromClientID(clientID);
	if (!player)
		throw EXCEPTION("Player instance not found for client", clientID);

	return m_worldController.getWorld(player->dimension());
}

void ServerCommandHandler::stopServer() const {
	m_server.setRunning(false);
}

ServerPlayer *ServerCommandHandler::getPlayer(u16 clientID) const {
	return m_players.getPlayerFromClientID(clientID);
}

ServerWorld &ServerCommandHandler::getWorld(u16 dimension) {
	return m_worldController.getWorld(dimension);
}

u16 ServerCommandHandler::worldCount() const {
	return m_worldController.getWorldCount();
}

// 命令 /summon 的实体生成：转调 Lua 的 mods["default"]:spawn_entity，
// 复用 LuaEntityLoader::spawnEntity 的完整初始化（组件、tick 标记、网络同步）。
// 这里走一段 Lua 脚本而不是 C++ 直接取 sol::function：spawn_entity 是注册在
// LuaMod usertype 上的成员方法，用脚本调用最稳妥（避免 usertype 索引语义差异）。
bool ServerCommandHandler::spawnEntity(const std::string &entityID, double x, double y, double z, u16 dimension) const {
	try {
		sol::state &lua = m_scriptEngine.lua();

		sol::object mods = lua["mods"];
		if (!mods.valid() || mods.get_type() != sol::type::table) {
			logError() << "[Command] /summon: Lua 'mods' table is not available";
			return false;
		}

		if (!mods.as<sol::table>().valid()) {
			logError() << "[Command] /summon: Lua 'mods' table is not available";
			return false;
		}

		lua["_summon_id"] = entityID;
		lua["_summon_x"] = x;
		lua["_summon_y"] = y;
		lua["_summon_z"] = z;
		lua["_summon_dim"] = dimension;

		// 注意：必须用 safe_script。lua.script 是未保护的，Lua 出错时
		// LuaJIT 的 lj_err_throw 会以 Windows SEH 异常逃逸，C++ catch 抓不到，
		// 会直接终止服务端进程。
		auto result = lua.safe_script(R"lua(
			local mod = mods["default"]
			if not mod then error("mod 'default' is not loaded") end
			mod:spawn_entity(_summon_id, {
				position = {_summon_x, _summon_y, _summon_z},
				dimension = _summon_dim,
			})
		)lua", sol::script_pass_on_error);

		if (!result.valid()) {
			sol::error err = result;
			logError() << ("[Command] /summon failed: " + std::string{err.what()}).c_str();
			return false;
		}

		return true;
	}
	catch (const sol::error &e) {
		logError() << ("[Command] /summon failed: " + std::string{e.what()}).c_str();
		return false;
	}
	catch (const std::exception &e) {
		logError() << ("[Command] /summon failed: " + std::string{e.what()}).c_str();
		return false;
	}
}

// 服务端 Lua 修改方块后手动同步客户端（流体系统用，见 mods/default/fluids.lua）。
// 包格式与 PlayerPlaceBlock 的广播一致：u32 = (param << 16) | blockID，
// 客户端收到后 setBlock + setData。
void ServerCommandHandler::sendBlockUpdate(s32 x, s32 y, s32 z, u16 blockID) const {
	Network::Packet packet;
	packet << Network::Command::BlockUpdate << x << y << z << u32(blockID & 0xffff);
	m_server.sendToAllClients(packet);
}

// Please update 'docs/lua-api-cpp.md' if you change this
void ServerCommandHandler::initUsertype(sol::state &lua) {
	lua.new_usertype<ServerCommandHandler>("ServerCommandHandler",
		"send_player_change_dimension", &ServerCommandHandler::sendPlayerChangeDimension,
		"send_chat_message", &ServerCommandHandler::sendChatMessage,

		// 在目标维度放置 MC 风格传送门（4x5），供 mods/default 的传送门逻辑调用
		"create_portal", &ServerCommandHandler::createPortal,

		// 杀死实体（触发 EntityKilled 事件 -> 掉落物），供 Lua / 命令使用
		"kill_entity", [](ServerCommandHandler &self, EntityWrapper &entity, ClientInfo &client) {
			return self.killEntity(entity, client);
		},

		// 流体系统（mods/default/fluids.lua）用：同步方块修改与玩家背包
		"send_block_update", &ServerCommandHandler::sendBlockUpdate,
		"send_player_inv_update", &ServerCommandHandler::sendPlayerInvUpdate,

		// 枚举所有已连接玩家（含未就绪的），供实体 AI 选目标；下标从 1 开始
		"get_players", [](ServerCommandHandler &self, sol::this_state s) {
			sol::state_view state(s);
			sol::table players = state.create_table();

				unsigned i = 1;
				for (auto &it : self.m_players)
					players[i++] = &it.second;

				return players;
			},

		// 取指定维度的 ServerWorld（生物自然生成/命令用）
		//
		// 边界保护：getWorld() 是 std::deque 的下标访问，传入越界的维度
		// 会是未定义行为（可能直接崩溃）。这里把越界维度夹到合法范围，
		// 并记一条警告，避免 Lua 侧传错维度把整个服务端带崩。
		"get_world", [](ServerCommandHandler &self, u16 dimension) -> ServerWorld & {
			const u16 count = self.m_worldController.getWorldCount();
			if (count == 0) {
				logError() << "get_world called with no world loaded";
				dimension = 0;
			}
			else if (dimension >= count) {
				logWarning() << "get_world called with out-of-range dimension"
					<< dimension << "|" << "count:" << count;
				dimension = 0;
			}
			return self.m_worldController.getWorld(dimension);
		}
	);
}
