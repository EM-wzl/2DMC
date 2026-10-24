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
#ifndef ENTITYCALLBACKCONTAINER_HPP_
#define ENTITYCALLBACKCONTAINER_HPP_

#include <sol/sol.hpp>

#include "common/core/IntTypes.hpp"

namespace sf { class Packet; }

struct EntityCallbackContainer {
	sol::unsafe_function collisionCallback;

	// 逐实体 tick 回调（mod:entity 定义的 on_tick），由 EntityTickController 每个服务端 tick 调用
	sol::unsafe_function tickCallback;
};

// 打在有 on_tick 回调的运行时实体上：让 EntityTickController 的 view
// 只遍历需要 Lua tick 的实体，无 on_tick 的实体（如 item_drop）零开销。
// 注意：不参与存档序列化（Scene::registerComponents 注册为 is_savable=false），
// 存档重载后需重新打标。含 1 字节成员使其成为非空组件：entt 对空类型的
// emplace_or_replace 返回 void，会导致 Scene::set 的元类型实例化编译失败
struct EntityTickTag { u8 _pad = 0; };

// 空实现：extend_meta_type 会实例化 save<EntityTickTag>（packet << component），
// 运行时因 is_savable=false 不会被调用
inline sf::Packet &operator<<(sf::Packet &packet, const EntityTickTag &) { return packet; }
inline sf::Packet &operator>>(sf::Packet &packet, EntityTickTag &) { return packet; }

#endif // ENTITYCALLBACKCONTAINER_HPP_
