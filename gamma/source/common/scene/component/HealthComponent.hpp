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
#ifndef HEALTHCOMPONENT_HPP_
#define HEALTHCOMPONENT_HPP_

#include "common/core/IntTypes.hpp"
#include "common/core/ISerializable.hpp"
#include "common/network/Network.hpp"

// 生物血量（服务端权威，随 EntityHealth (0x56) 包同步到客户端）。
// 本项目按 2DMC 控制台版规则：生物有血量、会被打掉血、HUD 显示，
// 但血量下限锁 1，不会死亡/掉落（"打不死"设计）。
struct HealthComponent : public ISerializable {
	HealthComponent() = default;
	HealthComponent(u16 health_, u16 maxHealth_)
		: health(health_), maxHealth(maxHealth_) {}

	u16 health = 20;
	u16 maxHealth = 20;

	// 受击闪红计时（客户端用；服务端也会写，随包同步让所有客户端一起闪）
	u8 hurtTimer = 0;

	bool isUpdated = true;

	void serialize(sf::Packet &packet) const override { packet << health << maxHealth << hurtTimer; }
	void deserialize(sf::Packet &packet) override { packet >> health >> maxHealth >> hurtTimer; }

	Network::Command packetType = Network::Command::EntityHealth;
};

#endif // HEALTHCOMPONENT_HPP_
