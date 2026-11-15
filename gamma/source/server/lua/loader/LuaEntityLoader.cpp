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
#include "common/core/Debug.hpp"
#include "common/core/Registry.hpp"
#include "common/scene/EntityCallbackContainer.hpp"
#include "common/scene/component/AnimationComponent.hpp"
#include "common/scene/component/DrawableDef.hpp"
#include "common/scene/component/NetworkComponent.hpp"
#include "common/scene/component/HealthComponent.hpp"
#include "common/scene/component/PositionComponent.hpp"
#include "common/scene/component/RotationComponent.hpp"
#include "server/lua/LuaMod.hpp"
#include "server/lua/loader/LuaEntityLoader.hpp"
#include "server/world/WorldController.hpp"

void LuaEntityLoader::loadEntity(const sol::table &table) const {
	m_stringID = m_mod.id() + ":" + table["id"].get<std::string>();

	entt::registry &registry = Registry::getInstance().entityRegistry();
	entt::entity entity = Registry::getInstance().registerEntity(m_stringID);

	tryLoadCallbacks(table);

	sol::optional<sol::table> properties = table["properties"];
	if (properties != sol::nullopt) {
		tryLoadVisual(properties.value(), registry, entity);
		tryLoadRotation(properties.value(), registry, entity);
		tryLoadAnimation(properties.value(), registry, entity);
		tryLoadHitbox(properties.value(), registry, entity);
	}
}

void LuaEntityLoader::spawnEntity(const std::string &entityID, const sol::table &table) const {
	Vector3f pos;
	u16 dim;

	sol::optional<sol::table> position = table["position"];
	sol::optional<u16> dimension = table["dimension"];
	if (position != sol::nullopt && dimension != sol::nullopt) {
		pos = {position.value()[1], position.value()[2], position.value()[3]};
		dim = dimension.value();
	}
	else {
		logError() << ("In mod '" + m_mod.id() + "': Cannot spawn entity '" + entityID + "' without a position and a dimension").c_str();
		return;
	}

	entt::registry &modelRegistry = Registry::getInstance().entityRegistry();
	entt::entity modelEntity = Registry::getInstance().getEntityFromStringID(entityID);
	if (modelEntity != entt::null) {
		ServerWorld &world = m_worldController.getWorld(dim);
		Scene &scene = world.scene();
		entt::registry &registry = scene.registry();
		entt::entity entity = scene.createEntityFromModel(modelRegistry, modelEntity);

		registry.emplace<PositionComponent>(entity, pos.x, pos.y, pos.z, dim);
		registry.emplace<NetworkComponent>(entity, entity);
		registry.emplace<std::string>(entity, entityID);

		// 类型定义了 on_tick 的实体打上 tick 标记，EntityTickController 只遍历这些实体
		if (Registry::getInstance().getEntityCallbackContainer(entityID).tickCallback.valid())
			registry.emplace<EntityTickTag>(entity);

		// 生物血量：取该实体类型 max_health（mod:entity 顶层字段，缺省 0 = 非生物）。
		// item_drop 等没有 max_health 的实体不会带 HealthComponent。
		// 注意：必须先用 hasEntityCallbacks 守住，否则 item_drop 这类没有回调
		// 容器的类型会触发 getEntityCallbackContainer() 的 .at() 抛异常。
		if (Registry::getInstance().hasEntityCallbacks(entityID)) {
			u16 typeMaxHealth = Registry::getInstance().getEntityCallbackContainer(entityID).maxHealth;
			if (typeMaxHealth > 0)
				registry.emplace<HealthComponent>(entity, typeMaxHealth, typeMaxHealth);
		}

		// FIXME: This code is too specific to the item drop entity
		ItemStack *itemStack = tryLoadItemStack(table, registry, entity);
		auto *drawable = registry.try_get<DrawableDef>(entity);
		if (itemStack && drawable) {
			auto &cube = drawable->getInventoryCubeDef();
			cube.blockID = itemStack->item().stringID();
		}
	}
	else
		logError() << ("In mod '" + m_mod.id() + "': Cannot find entity with id '" + entityID + "'").c_str();
}

void LuaEntityLoader::tryLoadCallbacks(const sol::table &table) const {
	auto &entityCallbackContainer = Registry::getInstance().addEntityCallbackContainer(m_stringID);

	sol::optional<sol::unsafe_function> onCollision = table["on_collision"];
	if (onCollision != sol::nullopt) {
		entityCallbackContainer.collisionCallback = onCollision.value();
	}

	// 逐实体 tick 回调：由 EntityTickController 每个服务端 tick 调用
	// on_tick(entity, pos, world, server)，pos 为 {x, y, z} 表
	sol::optional<sol::unsafe_function> onTick = table["on_tick"];
	if (onTick != sol::nullopt) {
		entityCallbackContainer.tickCallback = onTick.value();
	}

	// 生物最大生命值：mod:entity 表顶层的 max_health（缺省 0 = 不是生物）
	entityCallbackContainer.maxHealth = (u16)table["max_health"].get_or(0);
}

void LuaEntityLoader::tryLoadVisual(const sol::table &table, entt::registry &registry, entt::entity entity) const {
	// 支持两种写法：
	//   visual = {type="inventorycube", size=..., origin=..., block_id=...}   —— 单部件（旧）
	//   parts  = { {size=...,origin=...,block_id=...}, {...} }                —— 多部件（身体/头/腿）
	// 多部件是为了让生物有"身子"，而不是只显示一个方块。
	auto loadCube = [](DrawableDef &drawable, const sol::table &def) {
		auto &cube = drawable.addInventoryCube();
		cube.size = def["size"].get_or(1.0f);

		sol::optional<sol::table> origin = def["origin"];
		if (origin != sol::nullopt)
			cube.origin = {origin.value()[1], origin.value()[2], origin.value()[3]};

		sol::optional<sol::table> offset = def["offset"];
		if (offset != sol::nullopt)
			cube.offset = {offset.value()[1], offset.value()[2], offset.value()[3]};

		sol::optional<std::string> blockID = def["block_id"];
		if (blockID != sol::nullopt)
			cube.blockID = blockID.value();
	};

	sol::optional<sol::table> parts = table["parts"];
	if (parts != sol::nullopt) {
		DrawableDef &drawable = registry.emplace<DrawableDef>(entity);
		for (size_t i = 1; i <= parts.value().size(); ++i) {
			sol::optional<sol::table> part = parts.value()[i];
			if (part != sol::nullopt)
				loadCube(drawable, part.value());
		}
		return;
	}

	sol::optional<sol::table> visual = table["visual"];
	if (visual != sol::nullopt) {
		std::string type = visual.value()["type"];
		if (type == "inventorycube") {
			DrawableDef &drawable = registry.emplace<DrawableDef>(entity);
			loadCube(drawable, visual.value());
		}
		else
			logError() << ("For entity '" + m_stringID + "': Visual type '" + type + "' unknown").c_str();
	}
}

void LuaEntityLoader::tryLoadRotation(const sol::table &table, entt::registry &registry, entt::entity entity) const {
	bool isRotatable = table["is_rotatable"].get_or(false);
	if (isRotatable)
		registry.emplace<RotationComponent>(entity);

}

void LuaEntityLoader::tryLoadAnimation(const sol::table &table, entt::registry &registry, entt::entity entity) const {
	sol::optional<sol::table> animation = table["animation"];
	if (animation != sol::nullopt) {
		auto &animationComponent = registry.emplace<AnimationComponent>(entity);
		for (auto &it : animation.value()) {
			sol::table anim = it.second.as<sol::table>();
			std::string type = anim["type"];
			if (type == "rotation") {
				sol::table axis = anim["axis"];
				animationComponent.addRotation(axis[1], axis[2], axis[3], anim["angle"]);
			}
			else if (type == "translation") {
				sol::table delta = anim["delta"];
				animationComponent.addTranslation(delta[1], delta[2], delta[3], anim["min"], anim["max"], anim["loop"].get_or(true));
			}
			else
				logError() << ("For entity '" + m_stringID + "': Animation type '" + type + "' unknown").c_str();
		}
	}
}

void LuaEntityLoader::tryLoadHitbox(const sol::table &table, entt::registry &registry, entt::entity entity) const {
	sol::optional<sol::table> hitboxTable = table["hitbox"];
	if (hitboxTable != sol::nullopt) {
		registry.emplace<DoubleBox>(entity,
			hitboxTable.value()[1], hitboxTable.value()[2], hitboxTable.value()[3],
			hitboxTable.value()[4], hitboxTable.value()[5], hitboxTable.value()[6]
		);
	}
}

ItemStack *LuaEntityLoader::tryLoadItemStack(const sol::table &table, entt::registry &registry, entt::entity entity) const {
	sol::optional<sol::table> itemStack = table["item_stack"];
	if (itemStack != sol::nullopt) {
		return &registry.emplace<ItemStack>(entity, itemStack.value()[1], itemStack.value()[2].get_or(1));
	}

	return nullptr;
}
