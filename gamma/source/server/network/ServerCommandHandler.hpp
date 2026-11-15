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
#ifndef SERVERCOMMANDHANDLER_HPP_
#define SERVERCOMMANDHANDLER_HPP_

#include <unordered_map>

#include <entt/entt.hpp>
#include <sol/sol.hpp>

#include "common/core/Vector3.hpp"
#include "server/network/ChatCommandHandler.hpp"

struct BlockData;

struct AnimationComponent;
class ClientInfo;
class DrawableDef;
class EntityWrapper;
class Inventory;
class PlayerList;
class Registry;
class ScriptEngine;
class Server;
class ServerPlayer;
class ServerWorld;
class WorldController;

//=================================================================================================
// IMPORTANT: ServerCommandHandler is the only class that should be creating and sending packets.
// The only exceptions to this rule are ServerWorld::sendRequestedChunks and LuaGUI::show
//=================================================================================================

class ServerCommandHandler {
	public:
		ServerCommandHandler(ScriptEngine &scriptEngine, Server &server, WorldController &worldController, PlayerList &players, Registry &registry)
			: m_scriptEngine(scriptEngine), m_server(server), m_worldController(worldController), m_players(players), m_registry(registry) {}

		void sendServerTick(const ClientInfo *client = nullptr) const;
		void sendServerClosed(const std::string &message, ClientInfo *client = nullptr) const;
		void sendChunkUnload(s32 chunkX, s32 chunkY, s32 chunkZ, const ClientInfo *client = nullptr) const;
		void sendBlockDataUpdate(s32 x, s32 y, s32 z, const BlockData *blockData, const ClientInfo *client = nullptr) const;
		void sendBlockInvUpdate(s32 x, s32 y, s32 z, const Inventory &inventory, const ClientInfo *client = nullptr) const;
		void sendBlockUpdate(s32 x, s32 y, s32 z, u16 blockID) const;
		void sendPlayerPosUpdate(u16 clientID, bool isTeleportation = false, const ClientInfo *client = nullptr) const;
		void sendPlayerRotUpdate(u16 clientID, const ClientInfo *client = nullptr) const;
		void sendPlayerInvUpdate(u16 clientID, const ClientInfo *client = nullptr) const;
		void sendPlayerHealth(u16 clientID, u8 health, u8 maxHealth, const ClientInfo *client = nullptr) const;
		void sendPlayerGamemode(u16 clientID, u8 gamemode, const ClientInfo *client = nullptr) const;
		void sendPlayerChangeDimension(u16 clientID, s32 x, s32 y, s32 z, u16 dimension, const ClientInfo *client = nullptr) const;
		// 在指定维度放置一个 4x5 的 Minecraft 风格传送门（Minecraft 的 PortalForcer 等价物）。
		// 用于从主世界进下界/末地后，在目标维度留下一扇可原路返回的传送门。
		void createPortal(u16 dimension, s32 x, s32 y, s32 z, const std::string &portalBlockID, const std::string &frameBlockID) const;
		void sendChatMessage(u16 clientID, const std::string &message, const ClientInfo *client = nullptr) const;
		void sendEntitySpawn(entt::entity entityID, const ClientInfo *client = nullptr) const;
		void sendEntityDespawn(entt::entity entityID, const ClientInfo *client = nullptr) const;

		// 杀死一个实体（服务端权威）：触发 EntityKilled 事件（掉落）、
		// 广播 EntityDespawn 并从场景移除。
		// 供 Lua 调用（server:kill_entity(entity)），也用于 /kill <实体> 命令。
		// 返回 true 表示实体确实被杀死。
		bool killEntity(EntityWrapper &entity, ClientInfo &client);

		void setupCallbacks();

		void setPlayerPosition(u16 clientID, s32 x, s32 y, s32 z);

		// 命令系统（ChatCommandHandler）需要的查询/操作接口
		ServerPlayer *getPlayer(u16 clientID) const;
		ServerWorld &getWorld(u16 dimension);
		u16 worldCount() const;

		// 通过 Lua 的 mods["default"]:spawn_entity 生成实体（命令 /summon 用）。
		// 返回是否成功；失败只记日志，不抛异常。
		bool spawnEntity(const std::string &entityID, double x, double y, double z, u16 dimension) const;

		void stopServer() const;

		const Server &server() const { return m_server; }

		// 实体验证钩子（2DMC_ENTITYTEST，见 ServerWorld.cpp）需要访问 Lua 引擎加载临时脚本
		ScriptEngine &scriptEngine() { return m_scriptEngine; }

		static void initUsertype(sol::state &lua);

	private:
		ServerWorld &getWorldForClient(u16 clientID);

		ScriptEngine &m_scriptEngine;

		Server &m_server;
		WorldController &m_worldController;
		PlayerList &m_players;

		Registry &m_registry;

		ChatCommandHandler m_chatCommandHandler{*this};
};

#endif // SERVERCOMMANDHANDLER_HPP_
