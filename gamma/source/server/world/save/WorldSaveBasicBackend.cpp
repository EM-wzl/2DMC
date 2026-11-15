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
#include <cstring>
#include <fstream>
#include <vector>

#include <zlib.h>

#include <filesystem.hpp>

#include "common/core/Debug.hpp"
#include "common/core/GameTime.hpp"
#include "common/core/Registry.hpp"
#include "common/network/Network.hpp"
#include "common/scene/component/ComponentType.hpp"
#include "common/scene/component/NetworkComponent.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/ServerWorld.hpp"
#include "server/world/save/WorldSaveBasicBackend.hpp"

namespace fs = ghc::filesystem;

using namespace entt::literals;

void WorldSaveBasicBackend::load(const std::string &name) {
	logInfo() << ("Loading '" + name + "'...").c_str();

	std::ifstream file("saves/" + name + ".dat", std::ofstream::binary);

	// Note: Registry should be saved, otherwise blocks won't match since they're stored using
	//       their integer ID (not string ID), so we need to add a check for that

	if (file.is_open()) {
		file.seekg(0, file.end);
		std::size_t length = file.tellg();
		file.seekg(0, file.beg);

		char *buffer = new char[length];
		file.read(buffer, length);

		Network::Packet save;

		// ------------------------------------------------------------------
		// 存档格式演进（都可读，写永远用最新的 OMZ2）：
		//   OMZ2  [魔数 OMZ2][u32 原始长度][zlib( u32维度数 + 3个维度数据 + 玩家 + ticks )]
		//   OMZ1  [魔数 OMZ1][u32 原始长度][zlib( 2个维度数据 + 玩家 + ticks )]
		//   裸格式 无魔数，直接是 sf::Packet（2 个维度数据 + 玩家 + ticks）。
		//
		// OMZ1 与裸格式都是"加末地维度之前"的 2 维度存档，加载时固定按
		// 2 个维度读，第 3 个维度跳过（避免把玩家数据误读成 chunkCount）。
		// ------------------------------------------------------------------
		static constexpr char kMagicV2[4] = {'O', 'M', 'Z', '2'};
		static constexpr char kMagicV1[4] = {'O', 'M', 'Z', '1'};
		const bool isV2 = (length >= 8 && std::memcmp(buffer, kMagicV2, 4) == 0);
		const bool isV1 = (length >= 8 && std::memcmp(buffer, kMagicV1, 4) == 0);
		const bool isCompressed = isV2 || isV1;

		if (isCompressed) {
			u32 rawSize = 0;
			std::memcpy(&rawSize, buffer + 4, sizeof(u32));

			// rawSize 来自文件内容，损坏存档里可能是垃圾大值：
			// 直接 std::vector<char> raw(rawSize) 会抛 std::bad_alloc。
			// 目前最大的合法存档远小于 256 MiB，留这个余量足够。
			constexpr u32 kMaxRawSize = 256u * 1024u * 1024u;
			if (rawSize == 0 || rawSize > kMaxRawSize || rawSize > length * 64u) {
				logError() << "Save '" << name << "' declares implausible raw size " << rawSize;
				delete[] buffer;
				return;
			}

			std::vector<char> raw(rawSize);
			uLongf destLen = rawSize;
			int rc = uncompress((Bytef *)raw.data(), &destLen,
				(const Bytef *)buffer + 8, (uLong)(length - 8));

			if (rc == Z_OK && destLen == rawSize) {
				save.append(raw.data(), rawSize);
			}
			else {
				logError() << "Failed to decompress world save '" << name
					<< "' (zlib rc=" << rc << ", expected " << rawSize
					<< " bytes, got " << destLen << ")";
				delete[] buffer;
				return;
			}
		}
		else {
			// 旧格式：原样使用
			save.append(buffer, length);
		}

		delete[] buffer;

		// ------------------------------------------------------------------
		// 维度数量兼容：末地维度是后来加的，m_worldList 现在是 3 个。
		//   - OMZ2（新格式）：压缩数据开头有 u32 维度数，按它循环。
		//   - OMZ1 / 无魔数（旧格式）：存档只写了 2 个维度，固定按 2 个读，
		//     否则第 3 次循环会把后面的玩家列表/时间字段当成 chunkCount
		//     误读，导致生成海量空区块或存档数据错位。
		// 多余的维度（如旧档遇上新版本）跳过读取，用默认种子，保持可玩。
		// ------------------------------------------------------------------
		u32 savedDimensionCount = 2; // OMZ1 / 裸格式都是加末地之前的 2 维度存档
		if (isV2)
			save >> savedDimensionCount;

		u32 dimIndex = 0;
		for (auto &world : m_worldList) {
			if (dimIndex++ >= savedDimensionCount) {
				logInfo() << "Save has fewer dimensions than the mod defines; skipping extra dimension"
					<< (dimIndex - 1);
				continue;
			}

			// 读之前先确认 packet 还有数据；损坏存档里 chunkCount 可能是垃圾值，
			// 若不加检查会生成海量空区块甚至耗尽内存。
			if (save.endOfPacket()) {
				logError() << "Save truncated before dimension " << dimIndex - 1 << " data";
				return;
			}

			unsigned int chunkCount = 0;
			s32 seed = 0;
			save >> chunkCount >> seed;

			// 合理上限：一个维度最多几万个已生成区块；超过就认为存档损坏。
			constexpr unsigned int kMaxChunkCount = 100000u;
			if (chunkCount > kMaxChunkCount) {
				logError() << "Save has implausible chunk count " << chunkCount
					<< " (dimension " << dimIndex - 1 << "); refusing to load";
				return;
			}

			world.setSeed(seed);

			// logInfo() << "Loading dimension" << world.dimension().id() << "| Chunk count:" << chunkCount;

			for (unsigned int i = 0 ; i < chunkCount ; ++i) {
				if (save.endOfPacket()) {
					logError() << "Save truncated before chunk " << i << " (dimension " << dimIndex - 1 << ")";
					return;
				}
				int cx = 0, cy = 0, cz = 0;
				save >> cx >> cy >> cz;

				ServerChunk &chunk = world.getOrCreateChunk(cx, cy, cz);
				for (u8 z = 0 ; z < Chunk::height ; ++z) {
					for (u8 y = 0 ; y < Chunk::depth ; ++y) {
						for (u8 x = 0 ; x < Chunk::width ; ++x) {
							bool hasMetadata = false;
							save >> hasMetadata;

							if (hasMetadata) {
								BlockData *blockData = chunk.addBlockData(x, y, z);
								save >> blockData->inventory >> blockData->meta;
							}

							u32 data = 0;
							u8 light = 0;
							save >> data >> light;

							// 方块 ID 必须在当前注册表范围内。损坏存档里这个值是垃圾
							// （实测 61488），直接传给 Registry::getBlock(id) 会走
							// vector::at() 抛 out_of_range，进而让整个 load 失败。
							// 超出范围时把该格当作空气，跳过它的 ticking 注册。
							const u16 blockID = u16(data & 0xffff);
							const bool validBlock = blockID < Registry::getInstance().blocks().size();

							if (validBlock) {
								chunk.setBlockRaw(x, y, z, blockID);
								chunk.setData(x, y, z, u16(data >> 16));
								chunk.lightmap().setLightData(x, y, z, light);

								const ServerBlock &block = (ServerBlock &)Registry::getInstance().getBlock(blockID);
								if (block.canUpdate())
									chunk.addTickingBlock(x, y, z, block);
							}
							else {
								logWarning() << "Save has invalid block ID " << blockID
									<< " in dimension " << dimIndex - 1 << "; treating block as air";
								chunk.setBlockRaw(x, y, z, 0);
							}
						}
					}
				}

				chunk.setInitialized(true);
				chunk.setModified(true);
			}

			loadEntities(save, world);
		}

		u16 playerCount = 0;
		if (save.endOfPacket()) {
			logError() << "Save truncated before player list";
			return;
		}
		save >> playerCount;
		// 玩家数上限（与 maxPlayers 配置一致且留余量）；损坏存档里的垃圾值
		// 会让循环里 save >> username 读出一个巨大的字符串长度，触发
		// std::bad_alloc -> std::terminate（实测 0xC0000409 崩溃）。
		if (playerCount > 1024) {
			logError() << "Save has implausible player count " << playerCount << "; refusing to load";
			return;
		}
		for (u16 i = 0 ; i < playerCount ; ++i) {
			if (save.endOfPacket()) {
				logError() << "Save truncated inside player list";
				return;
			}
			std::string username;
			save >> username;

			auto &player = m_playerList.addPlayer(username, false);
			player.deserialize(save);
		}

		sf::Uint64 ticks = 0;
		save >> ticks;
		GameTime::setTicks(ticks);
	}

	// logInfo() << "Loading done.";
}

void WorldSaveBasicBackend::save(const std::string &name) {
	logInfo() << ("Saving '" + name + "'...").c_str();

	fs::create_directory("saves");

	std::ofstream file("saves/" + name + ".dat", std::ofstream::binary | std::ofstream::trunc);

	Network::Packet save;

	// 维度数量写在压缩数据的开头，供 load() 判断该读几个维度。
	// 与末地维度（第 3 个）兼容的关键：旧格式没有这个字段，load() 会
	// 按 2 个维度读，绝不会把玩家列表误当成第 3 个维度的 chunkCount。
	save << (u32)m_worldList.size();

	for (auto &world : m_worldList) {
		Network::Packet chunks;
		unsigned int chunkCount = 0;
		for (auto &it : world.chunks()) {
			if (!it.second->isInitialized() || !it.second->hasBeenModified()) continue;

			const Vector3i &chunkpos = it.first;
			const Chunk::DataArray &data = it.second->data();
			chunks << chunkpos.x << chunkpos.y << chunkpos.z;

			for (u8 z = 0 ; z < Chunk::height ; ++z) {
				for (u8 y = 0 ; y < Chunk::depth ; ++y) {
					for (u8 x = 0 ; x < Chunk::width ; ++x) {
						BlockData *blockData = it.second->getBlockData(x, y, z);
						if (blockData)
							chunks << true << blockData->inventory << blockData->meta;
						else
							chunks << false;

						chunks << u32(data[z][y][x])
							<< u8(it.second->lightmap().getLightDataRaw(x, y, z));
					}
				}
			}

			++chunkCount;
		}

		// logInfo() << "Saving dimension" << world.dimension().id() << "| Chunk count:" << chunkCount;

		save << chunkCount << world.seed();
		save.append(chunks.getData(), chunks.getDataSize());

		saveEntities(save, world);
	}

	save << (u16)m_playerList.size();

	for (auto &it : m_playerList) {
		save << it.second.name();
		save << it.second;
	}

	save << (sf::Uint64)GameTime::getTicks();

	// ------------------------------------------------------------------
	// 写入：zlib 压缩 + 魔数 "OMZ2"（压缩数据开头带 u32 维度数）
	//   旧格式（裸 packet）体积大且不可压缩；这里改为
	//   [魔数 4B][u32 原始长度][zlib 数据]，读档时按魔数自动识别。
	//   压缩失败时回退为写裸数据（保证绝不丢存档）。
	// ------------------------------------------------------------------
	const u32 rawSize = (u32)save.getDataSize();
	uLongf bound = compressBound(rawSize);
	std::vector<char> compressed(bound);

	int rc = compress2((Bytef *)compressed.data(), &bound,
		(const Bytef *)save.getData(), rawSize, Z_BEST_SPEED);

	if (rc == Z_OK) {
		static constexpr char kMagic[4] = {'O', 'M', 'Z', '2'};
		file.write(kMagic, 4);
		file.write((const char *)&rawSize, sizeof(u32));
		file.write(compressed.data(), (std::streamsize)bound);

		logInfo() << ("Saved '" + name + "': " + std::to_string(rawSize)
			+ " -> " + std::to_string((std::size_t)bound) + " bytes (zlib)").c_str();
	}
	else {
		// 压缩失败：回退为旧格式，绝不丢存档
		logWarning() << "zlib compress failed (rc=" << rc << "), writing uncompressed save";
		file.write((const char *)save.getData(), save.getDataSize());
	}

	// logInfo() << "Saving done.";
}

#include "common/scene/EntityCallbackContainer.hpp"
#include "common/scene/component/AnimationComponent.hpp"
#include "common/scene/component/ComponentType.hpp"
#include "common/scene/component/DrawableDef.hpp"
#include "common/scene/component/PositionComponent.hpp"
#include "common/scene/component/HealthComponent.hpp"
#include "common/scene/component/RotationComponent.hpp"

void WorldSaveBasicBackend::loadEntities(sf::Packet &save, ServerWorld &world) {
	entt::registry &registry = world.scene().registry();
	registry.clear();

	// 边界检查：存档损坏/被截断时，sf::Packet 的 operator>> 在末尾只会把
	// m_isValid 置 false，并**不会抛异常，也不会改写目标变量**。
	// 若这里不初始化、也不检查 packet 状态，entityCount 就会保持栈上的垃圾值，
	// 循环次数变成随机数，后续 registry.create()/emplace() 在无效数据上
	// 触发断言或 abort（实测损坏存档会让服务端以 0xC0000409 崩溃）。
	u32 entityCount = 0;
	if (save.endOfPacket()) {
		logError() << "Save truncated while reading entity count (dimension "
			<< world.dimension().id() << ")";
		return;
	}
	save >> entityCount;
	if (entityCount > 100000) {
		logError() << "Save has implausible entity count " << entityCount
			<< " (dimension " << world.dimension().id() << "); refusing to load";
		return;
	}

	// logDebug() << "Loading" << componentCount << "components in dimension" << world.dimension().id();

	for (u32 i = 0 ; i < entityCount ; ++i) {
		// 截断存档保护：sf::Packet 的 operator>> 在 EOF 时不改写目标变量，
		// 所以每个变量都必须初始化，且每层循环开头都要检查 endOfPacket()，
		// 否则会用栈上的垃圾值当计数/类型，导致 emplace 到无效 ComponentType
		// 上触发 abort。
		if (save.endOfPacket()) {
			logError() << "Save truncated inside entity list (dimension "
				<< world.dimension().id() << ")";
			return;
		}
		entt::entity entityID = entt::null;
		save >> entityID;

		entt::entity entity = registry.create();
		m_entityMap.emplace(entityID, entity);
		registry.emplace<NetworkComponent>(entity, entity);

		// logDebug() << "Creating entity" << std::underlying_type_t<entt::entity>(entityID);

		u32 componentCount = 0;
		if (save.endOfPacket()) {
			logError() << "Save truncated before component count (dimension "
				<< world.dimension().id() << ")";
			return;
		}
		save >> componentCount;
		if (componentCount > 64) {
			logError() << "Save has implausible component count " << componentCount
				<< " (dimension " << world.dimension().id() << "); refusing to load";
			return;
		}

		for (u32 j = 0 ; j < componentCount ; ++j) {
			if (save.endOfPacket()) {
				logError() << "Save truncated inside component list (dimension "
					<< world.dimension().id() << ")";
				return;
			}
			// 用一个不可能的哨兵值初始化；endOfPacket() 已保证正常路径下
			// operator>> 会写入真实值。
			ComponentType type = static_cast<ComponentType>(0xff);
			save >> type;

			// logDebug() << "Loading component" << (u16)type << "for entity" << std::underlying_type_t<entt::entity>(entityID);

			if (type == ComponentType::Position) {
				save >> registry.emplace<PositionComponent>(entity);
			}
			else if (type == ComponentType::Rotation) {
				save >> registry.emplace<RotationComponent>(entity);
			}
			else if (type == ComponentType::Animation) {
				save >> registry.emplace<AnimationComponent>(entity);
			}
			else if (type == ComponentType::Drawable) {
				save >> registry.emplace<DrawableDef>(entity);
			}
			else if (type == ComponentType::ItemStack) {
				save >> registry.emplace<ItemStack>(entity);
			}
			else if (type == ComponentType::Hitbox) {
				save >> registry.emplace<DoubleBox>(entity);
			}
			else if (type == ComponentType::EntityID) {
				save >> registry.emplace<std::string>(entity);
			}
			else if (type == ComponentType::Health) {
				save >> registry.emplace<HealthComponent>(entity);
			}
			else
				logWarning() << "Unknown component with type" << (int)type;
		}

		// EntityTickTag 不参与存档（is_savable=false），重载后按实体类型重新打标，
		// 否则带 on_tick 的生物/测试实体在存档重载后会永久静止。
		// （EntityTickController 只遍历带该标记的实体；ServerTick 驱动的 AI 不依赖它，
		//   但 on_tick 是公开 API，重载后仍应恢复。）
		auto *entityType = registry.try_get<std::string>(entity);
		if (entityType && Registry::getInstance().hasEntityCallbacks(*entityType)
		&& Registry::getInstance().getEntityCallbackContainer(*entityType).tickCallback.valid())
			registry.emplace<EntityTickTag>(entity);
	}
}

void WorldSaveBasicBackend::saveEntities(sf::Packet &save, ServerWorld &world) {
	entt::registry &registry = world.scene().registry();

	auto view = registry.view<NetworkComponent>();
	save << (u32)view.size();

	view.each([&] (auto entity, auto &network) {
		u32 componentCount = 0;
		Network::Packet entityPacket;
		registry.visit(entity, [&] (const auto &component_type) {
			const auto &type = entt::resolve(component_type);
			if (type.prop("is_savable"_hs).value().template cast<bool>()) {
				entityPacket << type.prop("type"_hs).value().template cast<ComponentType>();

				const auto &component = type.func("get"_hs).invoke({}, std::ref(registry), entity);
				Network::Packet packet = type.func("save"_hs).invoke({}, component).template cast<Network::Packet>();
				entityPacket.append(packet.getData(), packet.getDataSize());

				// logDebug() << "Serializing component" << type.prop("name"_hs).value().template cast<std::string>() << "for entity" << std::underlying_type_t<entt::entity>(entity) << "of size" << packet.getDataSize();

				++componentCount;
			}
		});

		// logDebug() << "Saving" << componentCount << "components for entity" << std::underlying_type_t<entt::entity>(entity);

		save << network.entityID << componentCount;
		save.append(entityPacket.getData(), entityPacket.getDataSize());
	});

	// logDebug() << "Saving" << view.size() << "entities in dimension" << world.dimension().id();
}
