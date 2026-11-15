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
#include <cstdlib>

#include "common/core/EngineConfig.hpp"
#include "common/core/GameClock.hpp"
#include "common/core/GameTime.hpp"
#include "common/core/Registry.hpp"
#include "common/world/Dimension.hpp"
#include "common/world/EntityPath.hpp"
#include "common/scene/EntityCallbackContainer.hpp"
#include "common/scene/component/NetworkComponent.hpp"
#include "common/scene/component/PositionComponent.hpp"
#include "server/core/PlayerList.hpp"
#include "server/core/ServerConfig.hpp"
#include "server/lua/ScriptEngine.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/scene/EntityWrapper.hpp"
#include "server/world/ServerPlayer.hpp"
#include "server/world/ServerWorld.hpp"

// TEMP: 实体引擎验证钩子（仅 2DMC_ENTITYTEST=1 时由 ServerWorld::update 调用，验证后移除）。
// 注册一个带 on_tick 的测试实体类型 2dmctest:mover（每 tick 沿 x 轴 +0.05），
// 生成在维度 0 出生点附近，用于验证逐实体 tick 与位置写入/同步链路。
// 不触碰 ServerApplication 的 2DMC_AUTOTEST 流体钩子。详见 _entity_research/entity_api_report.md。
static void runEntityTestHook(ServerWorld &world, ServerCommandHandler &server) {
	auto &registry = Registry::getInstance();

	// 测试实体模型：无组件，spawn_entity 会补齐 Position/Network/类型组件
	registry.registerEntity("2dmctest:mover");
	registry.addEntityCallbackContainer("2dmctest:mover");

	sol::state_view lua(server.scriptEngine().lua());

	try {
		registry.getEntityCallbackContainer("2dmctest:mover").tickCallback = lua.script(R"lua(
			return function(entity, pos, world, server)
				local x, y, z = entity:get_position()
				entity:set_position(x + 0.05, y, z)
			end
		)lua");

		lua["_2DMC_ENTITYTEST_SPAWN_Z"] = world.heightmap().getHighestBlockAt(8, 8) + 2;
		lua.script(R"lua(
			mods["default"]:spawn_entity("2dmctest:mover", {
				position = {8.5, 8.5, _2DMC_ENTITYTEST_SPAWN_Z},
				dimension = 0,
			})
		)lua");

		// TEMP(第二批补充): 额外生成 2 只 default:pig（真实 on_tick 游荡 AI），
		// z 取地表 +1.01，与 entities.lua find_surface_z 的落点一致
		//（脚部格为空气、正下方为固体，AI 的地面探测才能成立）。
		// 移除：删除本块即可（与 mover 钩子同属 TEMP 验证代码）。
		lua["_2DMC_ENTITYTEST_MOB_Z"] = (double)world.heightmap().getHighestBlockAt(10, 8) + 1.01;
		lua.script(R"lua(
			mods["default"]:spawn_entity("default:pig", {
				position = {10.5, 8.5, _2DMC_ENTITYTEST_MOB_Z},
				dimension = 0,
			})
			mods["default"]:spawn_entity("default:pig", {
				position = {8.5, 10.5, _2DMC_ENTITYTEST_MOB_Z},
				dimension = 0,
			})
		)lua");

		logInfo() << "[ENTITYTEST] test entity '2dmctest:mover' spawned, on_tick moves it along +x";
		logInfo() << "[ENTITYTEST] 2x default:pig spawned (real wandering AI on_tick test)";

		// TEMP(第二批诊断): 验证完成后随本 TEMP 钩子一并移除。
		// ① pig 回调容器在钩子时刻的状态；② 生成的 pig 实体是否带 EntityTickTag；
		// ③ 用 safe_script 显式暴露 Lua 侧错误
		lua["_diag_world"] = &world;
		auto &pigCallbacks = Registry::getInstance().getEntityCallbackContainer("default:pig");
		logInfo() << "[ENTITYTEST] pig tickCallback valid (hook time): " << (pigCallbacks.tickCallback.valid() ? 1 : 0)
			<< ", collisionCallback valid: " << (pigCallbacks.collisionCallback.valid() ? 1 : 0);

		entt::registry &sceneReg = world.scene().registry();
		sceneReg.view<PositionComponent, std::string>().each([&](auto e, auto &, auto &sid) {
			if (sid.rfind("default:pig", 0) == 0)
				logInfo() << "[ENTITYTEST] pig entity has_tick_tag: " << (sceneReg.all_of<EntityTickTag>(e) ? 1 : 0);
			if (sid == "2dmctest:mover")
				logInfo() << "[ENTITYTEST] mover entity has_tick_tag: " << (sceneReg.all_of<EntityTickTag>(e) ? 1 : 0);
		});

		auto diag_result = lua.safe_script(R"lua(
			local w = _diag_world
			local function grid(cx, cy)
				local t = {}
				for dz = 79, 82 do
					local row = {}
					for dy = -1, 1 do
						for dx = -1, 1 do
							row[#row+1] = w:get_block(cx+dx, cy+dy, dz)
						end
					end
					t[#t+1] = "z=" .. dz .. " [" .. table.concat(row, ",") .. "]"
				end
				return table.concat(t, " ")
			end
			__diag_pig1 = grid(10, 8)
			__diag_pig2 = grid(8, 10)
		)lua", sol::script_pass_on_error);
		if (!diag_result.valid()) {
			sol::error err = diag_result;
			logError() << "[ENTITYTEST] diag script error: " << err.what();
		}
		else {
			logInfo() << "[ENTITYTEST] diag pig1(10,8): " << lua["_diag_pig1"].get_or(std::string("<nil>"));
			logInfo() << "[ENTITYTEST] diag pig2(8,10): " << lua["_diag_pig2"].get_or(std::string("<nil>"));
		}

		lua["__pig_ticks"] = 0;
		lua["__orig_pig_tick"] = pigCallbacks.tickCallback;
		auto wrap_result = lua.safe_script(R"lua(
			local orig = __orig_pig_tick
			__wrapped_pig_tick = function(entity, pos, world, server)
				__pig_ticks = __pig_ticks + 1
				return orig(entity, pos, world, server)
			end
		)lua", sol::script_pass_on_error);
		if (!wrap_result.valid()) {
			sol::error err = wrap_result;
			logError() << "[ENTITYTEST] wrap script error: " << err.what();
		}
		else {
			pigCallbacks.tickCallback = lua["__wrapped_pig_tick"];
			logInfo() << "[ENTITYTEST] pig tickCallback wrapped for counting";
		}
	}
	catch (const sol::error &e) {
		logError() << "[ENTITYTEST] hook failed:" << e.what();
	}
}

ServerWorld::ServerWorld(PlayerList &players, const Dimension &dimension, s32 seed)
	: m_players(players),
	  m_dimension(dimension),
	  m_heightmap(seed),
	  m_terrainGenerator(m_heightmap, dimension, seed),
	  m_scene(players),
	  m_seed(seed)
{
	// 实体 on_tick 回调需要访问所在 world（Lua 侧用于方块查询/寻路）
	m_scene.setWorld(this);
}

void ServerWorld::update(bool doTick) {
	World::update();

	processSendRequests();

	if (doTick) {
		// TEMP: 实体引擎验证钩子（2DMC_ENTITYTEST=1 时启用，验证后移除），
		// 仅维度 0 的第一个 tick 触发一次；见文件顶部 runEntityTestHook
		static bool entityTestHookDone = false;
		if (!entityTestHookDone && m_dimension.id() == 0 && m_server && std::getenv("2DMC_ENTITYTEST")) {
			entityTestHookDone = true;
			runEntityTestHook(*this, *m_server);
		}

		for (auto &it : m_chunks) // TODO: Loop on a subset of this hashmap
			it.second->tick();

		m_scene.update();
	}
}

void ServerWorld::updatePlayerChunks(ServerPlayer &player, s32 cx, s32 cy, s32 cz) {
	Vector3i currentChunk{cx, cy, cz};

	player.sentChunks.clear();
	m_chunkSendRequestQueue.emplace(currentChunk, player, GameClock::getInstance().getTicks(true));

	std::list<Vector3i> chunksToRemove;
	for (auto &chunkPos : player.loadedChunks()) {
		glm::dvec3 chunkWorldPos{
			chunkPos.x * CHUNK_WIDTH + CHUNK_WIDTH / 2,
			chunkPos.y * CHUNK_DEPTH + CHUNK_DEPTH / 2,
			chunkPos.z * CHUNK_HEIGHT + CHUNK_HEIGHT / 2
		};

		glm::dvec3 playerPos{
			player.x(),
			player.y(),
			player.z()
		};

		if (glm::length(playerPos - chunkWorldPos) >= (ServerConfig::renderDistance + 3) * CHUNK_WIDTH) {
			m_server->sendChunkUnload(chunkPos.x, chunkPos.y, chunkPos.z, player.client());
			chunksToRemove.emplace_back(chunkPos);

			// logDebug() << "Chunk at" << chunkPos.x << chunkPos.y << chunkPos.z << "unloaded for player" << player.name();
		}
	}

	for (auto &chunkPos : chunksToRemove)
		player.removeLoadedChunk(chunkPos);
}

void ServerWorld::createChunkNeighbours(ServerChunk &chunk) {
	Vector3i surroundingChunks[6] = {
		{chunk.x() - 1, chunk.y(),     chunk.z()},
		{chunk.x() + 1, chunk.y(),     chunk.z()},
		{chunk.x(),     chunk.y() - 1, chunk.z()},
		{chunk.x(),     chunk.y() + 1, chunk.z()},
		{chunk.x(),     chunk.y(),     chunk.z() - 1},
		{chunk.x(),     chunk.y(),     chunk.z() + 1},
	};

	for (u8 i = 0 ; i < 6 ; ++i) {
		// Check if this neighbour already exists, if yes then skip it
		ServerChunk *neighbour = (ServerChunk *)getChunk(surroundingChunks[i].x, surroundingChunks[i].y, surroundingChunks[i].z);
		if (neighbour) {
			// Assign surrounding chunk pointers
			chunk.setSurroundingChunk(i, neighbour);
			neighbour->setSurroundingChunk((i % 2 == 0) ? i + 1 : i - 1, &chunk);

			continue;
		}

		// Create our neighbour
		auto it = m_chunks.emplace(
			Vector3i{
				surroundingChunks[i].x,
				surroundingChunks[i].y,
				surroundingChunks[i].z
			},
			new ServerChunk{
				surroundingChunks[i].x,
				surroundingChunks[i].y,
				surroundingChunks[i].z,
				*this
			}
		);

		// Get the created neighbour
		neighbour = it.first->second.get();

		// Assign surrounding chunk pointers
		chunk.setSurroundingChunk(i, neighbour);
		neighbour->setSurroundingChunk((i % 2 == 0) ? i + 1 : i - 1, &chunk);
	}
}

ServerChunk &ServerWorld::getOrCreateChunk(s32 cx, s32 cy, s32 cz) {
	ServerChunk *chunk = (ServerChunk *)getChunk(cx, cy, cz);
	if (!chunk) {
		auto it = m_chunks.emplace(Vector3i{cx, cy, cz}, new ServerChunk(cx, cy, cz, *this));
		chunk = it.first->second.get();
	}

	// Create our neighbours
	createChunkNeighbours(*chunk);

	return *chunk;
}

Chunk *ServerWorld::getChunk(int cx, int cy, int cz) const {
	auto it = m_chunks.find({cx, cy, cz});
	if (it == m_chunks.end())
		return nullptr;

	return it->second.get();
}

bool ServerWorld::generateChunk(ServerChunk &chunk) {
	if (chunk.isInitialized()) return false;

	createChunkNeighbours(chunk);

	m_terrainGenerator.generate(chunk);
	chunk.setInitialized(true);

	return true;
}

void ServerWorld::processSendRequests() {
	// Debug variables
	// u64 chunksGenerated = 0;
	// u64 chunksSent = 0;
	// u64 chunksTooOld = 0;

	// if (!m_chunkSendRequestQueue.empty())
	// 	logDebug() << "Processing send requests...";

	u64 start = GameClock::getInstance().getTicks(true);
	u64 now = start;
	while (now - start < 500 && !m_chunkSendRequestQueue.empty()) {
		auto &[chunkPos, player, timestamp] = m_chunkSendRequestQueue.front();

		bool isTooOld = now - timestamp >= 10000;
		if (!isTooOld && player.sentChunks.find(chunkPos) == player.sentChunks.end()) {
			glm::dvec3 chunkWorldPos{
				chunkPos.x * CHUNK_WIDTH + CHUNK_WIDTH / 2,
				chunkPos.y * CHUNK_DEPTH + CHUNK_DEPTH / 2,
				chunkPos.z * CHUNK_HEIGHT + CHUNK_HEIGHT / 2
			};

			glm::dvec3 playerPos{
				player.x(),
				player.y(),
				player.z()
			};

			// bool addChunkBackToQueue = false;
			if (glm::length(playerPos - chunkWorldPos) < (ServerConfig::renderDistance + 2) * CHUNK_WIDTH) {
				if (player.isOnline() && !player.isChunkLoaded(chunkPos)) {
					ServerChunk &chunk = getOrCreateChunk(chunkPos.x, chunkPos.y, chunkPos.z);

					// chunksGenerated +=
					generateChunk(chunk);
					generateChunk((ServerChunk &)*chunk.getSurroundingChunk(Chunk::West));
					generateChunk((ServerChunk &)*chunk.getSurroundingChunk(Chunk::East));
					generateChunk((ServerChunk &)*chunk.getSurroundingChunk(Chunk::South));
					generateChunk((ServerChunk &)*chunk.getSurroundingChunk(Chunk::North));
					generateChunk((ServerChunk &)*chunk.getSurroundingChunk(Chunk::Bottom));
					generateChunk((ServerChunk &)*chunk.getSurroundingChunk(Chunk::Top));

					if (!chunk.areAllNeighboursInitialized())
						logWarning() << "All neighbours of chunk" << chunkPos << "aren't initialized when they should be";
					else {
						chunk.lightmap().updateLights();
						chunk.setReadyToSend();

						addChunkToProcess(&chunk);
						player.addLoadedChunk(chunkPos);

						// ++chunksSent;
					}
				}

				// logDebug() << "OK for chunk" << chunkPos.x << chunkPos.y << chunkPos.z << ":" << glm::length(playerPos - chunkWorldPos) << "<" << (int)ServerConfig::renderDistance * CHUNK_WIDTH;

				auto addChunkToQueue = [this](Vector3i pos, s8 dx, s8 dy, s8 dz, ServerPlayer &player) {
					pos.x += dx; pos.y += dy; pos.z += dz;
					m_chunkSendRequestQueue.emplace(pos, player, GameClock::getInstance().getTicks(true));
				};

				addChunkToQueue(chunkPos,  1,  0,  0, player);
				addChunkToQueue(chunkPos, -1,  0,  0, player);
				addChunkToQueue(chunkPos,  0,  1,  0, player);
				addChunkToQueue(chunkPos,  0, -1,  0, player);
				addChunkToQueue(chunkPos,  0,  0,  1, player);
				addChunkToQueue(chunkPos,  0,  0, -1, player);
			}

			// if (addChunkBackToQueue)
			// 	m_chunkSendRequestQueue.emplace(chunkPos, player, timestamp);
			// else
				player.sentChunks.emplace(chunkPos);
		}

		// if (isTooOld)
		// 	++chunksTooOld;

		m_chunkSendRequestQueue.pop();

		now = GameClock::getInstance().getTicks(true);
	}

	// if (now - start > 0) {
	// 	logDebug() << "Done in" << now - start << "ms for dim" << m_dimension.id()
	// 		<< "| Gen:" << chunksGenerated
	// 		<< "| Sent:" << chunksSent
	// 		// << "| BTQ:" << chunksBackToQueue
	// 		<< "| Old:" << chunksTooOld;
	// 		// << "| QLen:" << startQueueSize << "->" << m_chunkSendRequestQueue.size();
	// }
}

// Please update 'docs/lua-api-cpp.md' if you change this
void ServerWorld::initUsertype(sol::state &lua) {
	lua.new_usertype<ServerWorld>("ServerWorld",
		sol::base_classes, sol::bases<World>(),

		"dimension", &ServerWorld::dimension,

		// 地表高度查询（Lua 生成生物/传送用；返回该列最高地形方块的 z）
		"heightmap", &ServerWorld::heightmap,

		// 光照查询（Lua 生物 AI 用）：返回 (x,y,z) 处方块接收到的最大光照强度 0..15，
		// 取阳光与火把光中的较大者。区块未加载或尚未初始化时返回 -1
		// （与「全黑 = 0」区分开，Lua 侧可据此判断该位置是否可查询）。
		"get_light", [](ServerWorld &self, int x, int y, int z) -> int {
			Chunk *chunk = self.getChunkAtBlockPos(x, y, z);
			if (!chunk || !chunk->isInitialized())
				return -1;

			// 方块坐标 → 区块内局部坐标（C++ 对负数取模为负，用位与等价于取模 2 的幂）
			const int lx = x & (CHUNK_WIDTH - 1);
			const int ly = y & (CHUNK_DEPTH - 1);
			const int lz = z & (CHUNK_HEIGHT - 1);

			const ChunkLightmap &lightmap = chunk->lightmap();
			const u8 sunlight = lightmap.getSunlight(lx, ly, lz);
			const u8 torchlight = lightmap.getTorchlight(lx, ly, lz);

			return (int)(sunlight > torchlight ? sunlight : torchlight);
		},

		// 是否白天（Lua 生物 AI 用）：仅主世界（dimension 0）有昼夜循环，
		// 下界/末地恒为 false。0..12000 tick 为白天，12000..24000 为夜晚。
		"is_daytime", [](ServerWorld &self) -> bool {
			if (self.dimension().id() != 0)
				return false;

			return (u32)(GameTime::getTicks() % GameTime::dayLength) < 12000;
		},

		// 游戏总 tick 数（Lua 侧做 AI 冷却/计时用；20 tick = 1 秒）
		"get_ticks", [](ServerWorld &self) {
			return GameTime::getTicks();
		},

		// 视线检测（Lua 生物 AI 用）：from→to 之间是否没有可碰撞方块阻挡。
		// 未加载区块视为「不挡」，详见 EntityPath.hpp 顶部注释。
		"path_clear", [](ServerWorld &self, double fx, double fy, double fz,
		                              double tx, double ty, double tz) {
			return clearPath(self, Vector3d{fx, fy, fz}, Vector3d{tx, ty, tz});
		},

		// 枚举本维度所有实体（网络实体，下标从 1 开始），供实体 AI 选目标
		"get_entities", [](ServerWorld &self, sol::this_state s) {
			sol::state_view state(s);
			sol::table entities = state.create_table();

				entt::registry &registry = self.scene().registry();
				unsigned i = 1;
				registry.view<PositionComponent, NetworkComponent>().each([&](auto entity, auto &, auto &) {
					entities[i++] = EntityWrapper{entity, registry};
				});

				return entities;
			},

			"get_entities_by_type", [](ServerWorld &self, const std::string &type, sol::this_state s) {
				sol::state_view state(s);
				sol::table entities = state.create_table();

				entt::registry &registry = self.scene().registry();
				unsigned i = 1;
				registry.view<PositionComponent, NetworkComponent, std::string>().each([&](auto entity, auto &, auto &, auto &id) {
					if (id == type)
						entities[i++] = EntityWrapper{entity, registry};
				});

				return entities;
			}
	);
}
