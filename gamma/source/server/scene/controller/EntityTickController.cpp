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
#include "server/scene/controller/EntityTickController.hpp"

#include <cstdlib>
#include <vector>

#include "common/core/Debug.hpp"
#include "common/core/Registry.hpp"
#include "common/scene/EntityCallbackContainer.hpp"
#include "common/scene/component/PositionComponent.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/scene/EntityWrapper.hpp"
#include "server/world/ServerWorld.hpp"

void EntityTickController::update(entt::registry &registry) {
	// 无 server/world（场景初始化早期）时直接跳过
	if (!m_server || !m_world) return;

	// TEMP: 实体验证日志（仅 2DMC_ENTITYTEST=1 时启用，验证后移除）：
	// 每秒（20 tick）记录一次 2dmctest:mover 的服务端位置，确认 on_tick 移动生效
	static const bool s_entityTestLogging = std::getenv("2DMC_ENTITYTEST") != nullptr;
	static u64 s_entityTestLogTick = 0;
	if (s_entityTestLogging && (s_entityTestLogTick++ % 20 == 0)) {
		registry.view<PositionComponent, std::string>().each([&](auto, auto &position, auto &id) {
			if (id == "2dmctest:mover")
				logInfo() << "[ENTITYTEST] mover pos:" << position.x << position.y << position.z;
		});
	}

	// 先收集实体再逐个调用：on_tick 里可能 spawn/despawn 实体，
	// 边遍历 entt view 边增删实体会使迭代器失效
	std::vector<entt::entity> entities;
	registry.view<PositionComponent, EntityTickTag>().each([&](auto entity, auto &, auto &) {
		entities.push_back(entity);
	});

	for (auto entity : entities) {
		if (!registry.valid(entity)) continue; // 本轮里被其他实体的 on_tick despawn 掉了

		auto *position = registry.try_get<PositionComponent>(entity);
		auto *id = registry.try_get<std::string>(entity);
		if (!position || !id) continue;

		auto &callbacks = Registry::getInstance().getEntityCallbackContainer(*id);
		if (!callbacks.tickCallback.valid()) continue;

		try {
			EntityWrapper entityWrapper{entity, registry};

			sol::state_view lua(callbacks.tickCallback.lua_state());
			sol::table pos = lua.create_table_with(
				"x", position->x,
				"y", position->y,
				"z", position->z
			);

			callbacks.tickCallback(entityWrapper, pos, m_world, *m_server);
		}
		catch (const sol::error &e) {
			logError() << ("EntityTickController: Failed to run on_tick for entity '" + *id + "': " + e.what()).c_str();
			// 出错实体停 tick，避免每 tick 刷错误日志；Lua 可对同类型新实体重新生成
			if (registry.valid(entity))
				registry.remove<EntityTickTag>(entity);
		}
	}
}
