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
#ifndef ENTITYTICKCONTROLLER_HPP_
#define ENTITYTICKCONTROLLER_HPP_

#include "common/scene/controller/AbstractController.hpp"

class ServerCommandHandler;
class ServerWorld;

// 服务端逐实体 Lua tick：对每个带 EntityTickTag 的实体，若其类型定义了 on_tick
// 则每个服务端 tick 调用一次 on_tick(entity, pos, world, server)。
class EntityTickController : public AbstractController {
	public:
		void update(entt::registry &registry) override;

		void setServer(ServerCommandHandler *server) { m_server = server; }
		void setWorld(ServerWorld *world) { m_world = world; }

	private:
		ServerCommandHandler *m_server = nullptr;
		ServerWorld *m_world = nullptr;
};

#endif // ENTITYTICKCONTROLLER_HPP_
