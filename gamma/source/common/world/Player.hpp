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
#ifndef PLAYER_HPP_
#define PLAYER_HPP_

#include "common/core/Box.hpp"
#include "common/core/ISerializable.hpp"
#include "common/inventory/Inventory.hpp"

class Player : public ISerializable {
	public:
		Player();

		const std::string &name() const { return m_name; }

		// West, East, South, North
		// Same order as enum BlockFace in TilesDef.hpp
		u8 getDirection() const;
		u8 getOppositeDirection() const;

		Vector3i getCurrentChunk() const;

		void serialize(sf::Packet &packet) const override;
		void deserialize(sf::Packet &packet) override;

		double x() const { return m_x; }
		double y() const { return m_y; }
		double z() const { return m_z; }

		u16 dimension() const { return m_dimension; }

		float cameraYaw()   const { return m_viewAngleH; }
		float cameraPitch() const { return m_viewAngleV; }
		float cameraRoll()  const { return m_viewAngleRoll; }

		u16 clientID() const { return m_clientID; }

		// 生命值与游戏模式（服务端权威；不写入存档，旧存档兼容）
		u8 health() const { return m_health; }
		u8 maxHealth() const { return m_maxHealth; }
		u8 gamemode() const { return m_gamemode; } // 0 = 创造, 1 = 生存

		void setHealth(u8 health) { m_health = health; }
		void setMaxHealth(u8 maxHealth) { m_maxHealth = maxHealth; }
		void setGamemode(u8 gamemode) { m_gamemode = gamemode; }

		Inventory &inventory() { return m_inventory; }

		void setName(const std::string &name) { m_name = name; }
		void setPosition(double x, double y, double z) { m_x = x; m_y = y; m_z = z; }
		void setRotation(float yaw, float pitch) { m_viewAngleH = yaw; m_viewAngleV = pitch; }
		void setDimension(u16 dimension) { m_dimension = dimension; }
		void setClientID(u16 clientID) { m_clientID = clientID; }

		const FloatBox &hitbox() const { return m_hitbox; }

		// m_heldItemSlot 默认 -1（尚未收到 PlayerHeldItemChanged 时）。
		// 直接传给 getStack 会转成 u16(65535) 再 m_items.at() → 越界抛异常，
		// 曾导致"攻击生物时服务端崩溃"。这里做槽位范围保护，越界时返回空栈。
		const ItemStack &heldItemStack() {
			if (m_heldItemSlot < 0 || m_heldItemSlot >= 9)
				return ItemStack::Empty;
			return m_inventory.getStack(m_heldItemSlot, 0);
		}
		s8 heldItemSlot() const { return m_heldItemSlot; }
		void setHeldItemSlot(u8 heldItemSlot) { m_heldItemSlot = heldItemSlot; }

		static void initUsertype(sol::state &lua);

	protected:
		std::string m_name;

		double m_x = 0;
		double m_y = 0;
		double m_z = 0;

		u16 m_dimension = 0;

		float m_viewAngleH = -90;
		float m_viewAngleV = 0;
		float m_viewAngleRoll = 0;

		u16 m_clientID = 0;

		// 生命值与游戏模式（beta 命令系统移植：/heal /life /kill /gamemode）
		// 刻意不参与 serialize/deserialize：旧存档没有这两个字段，追加会破坏
		// 存档兼容性；每次进入世界按默认值初始化即可。
		u8 m_health = 20;
		u8 m_maxHealth = 20;
		u8 m_gamemode = 1;

		Inventory m_inventory{9, 4};

		FloatBox m_hitbox;

		s8 m_heldItemSlot = -1;
};

#endif // PLAYER_HPP_
