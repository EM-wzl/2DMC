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
#include "common/scene/component/HealthComponent.hpp"
#include "server/scene/EntityWrapper.hpp"

std::string EntityWrapper::getType() const {
	auto *type = m_registry.try_get<std::string>(m_id);
	return type ? *type : "";
}

int EntityWrapper::getHealth() const {
	auto *health = m_registry.try_get<HealthComponent>(m_id);
	return health ? (int)health->health : 0;
}

int EntityWrapper::getMaxHealth() const {
	auto *health = m_registry.try_get<HealthComponent>(m_id);
	return health ? (int)health->maxHealth : 0;
}

void EntityWrapper::damage(int amount) {
	auto *health = m_registry.try_get<HealthComponent>(m_id);
	if (!health || amount <= 0) return;

	// 正常扣血：血量可以降到 0，随后由调用方移除实体（生物会死亡）。
	// 之前为了"便于观察 AI"把下限锁在 1，用户反馈这是 bug（打到 1 血就锁住），
	// 现改为正常击杀。
	int next = (int)health->health - amount;
	if (next < 0) next = 0;
	health->health = (u16)next;

	// 受击闪红：0.3 秒（约 6 tick），随 EntityHealth 包同步给所有客户端
	health->hurtTimer = 6;
	health->isUpdated = true;
}

bool EntityWrapper::isDead() const {
	auto *health = m_registry.try_get<HealthComponent>(m_id);
	return health && health->health == 0;
}

std::tuple<double, double, double> EntityWrapper::getPosition() const {
	auto *position = getPositionComponent();
	if (!position) return {0.0, 0.0, 0.0};
	return {position->x, position->y, position->z};
}

void EntityWrapper::setPosition(double x, double y, double z) {
	auto *position = getPositionComponent();
	if (!position) return;

	position->x = x;
	position->y = y;
	position->z = z;
	position->isUpdated = true; // 触发 NetworkController 下一 tick 广播 EntityPosition
}

// Please update 'docs/lua-api-cpp.md' if you change this
void EntityWrapper::initUsertype(sol::state &lua) {
	lua.new_usertype<EntityWrapper>("EntityWrapper",
		"id", &EntityWrapper::id,

		"position", &EntityWrapper::getPositionComponent,
		"network", &EntityWrapper::getNetworkComponent,
		"item_stack", &EntityWrapper::getItemStack,

		// "type" 是属性（entity.type），"get_type" 是方法（entity:get_type()）。
		// entities.lua 用的是方法形式，之前只注册了属性名，导致
		// "attempt to call method 'get_type' (a nil value)"，生物 AI 全部不执行。
		"type", &EntityWrapper::getType,
		"get_type", &EntityWrapper::getType,
		"get_position", &EntityWrapper::getPosition,
		"set_position", &EntityWrapper::setPosition,
		"valid", &EntityWrapper::isValid,

		// 血量（HUD/Lua 用）
		"get_health", &EntityWrapper::getHealth,
		"get_max_health", &EntityWrapper::getMaxHealth,
		"damage", &EntityWrapper::damage
	);
}
