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
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with OpenMiner; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * =====================================================================================
 */
#ifndef ENTITYPATH_HPP_
#define ENTITYPATH_HPP_

#include <algorithm>
#include <cmath>

#include "common/core/IntTypes.hpp"
#include "common/core/Vector3.hpp"
#include "common/world/BlockState.hpp"
#include "common/world/World.hpp"

// 视线检测：沿 from→to 的线段步进采样，判断是否有可碰撞方块挡住视线。
//
// 用途：Lua 侧生物 AI 用它判断「能不能看见玩家」，从而决定追击/逃跑/攻击。
// 与之等价的 C++ 语义也被路径寻找（寻路）复用。
//
// 实现要点：
//   * 采样密度固定为每格 8 个点（步长 0.125）。现有方块的碰撞盒最小单位是
//     1/8 格（半砖 0.5、台阶 0.125 级别），步长不大于它才不会「穿角」漏检。
//   * 每个采样点先定位到所在方块，再把点换算成该方块内的局部坐标（0..1），
//     与该方块真实的 boundingBox() 做点包含判定。这样做的好处是台阶、半砖、
//     栅栏等「非满格方块」只在真正实心的那部分挡视线，而不是整格都算障碍。
//   * 未加载区块的 getBlockState 返回 nullptr。这里把 nullptr 视为「不挡」：
//     区块没加载并不代表那里有墙，若当成障碍，生物在区块边界附近会莫名其妙
//     失去视线（而且这种丢视线是随机的、跟玩家移动导致的加载状态相关）。
//     代价是：穿过未加载区块的视线会被判为通畅，可能让生物「透视」到远处；
//     但生物的目标选择本身应受距离限制，实际不会跨未加载区域锁定玩家。
//
// 复杂度：采样数 = ceil(distance * 8)，每步一次区块查找 + 一次方块状态查询。
// 建议调用方把距离限制在 32 格以内（约 256 次采样），不要在每 tick 里对
// 所有实体×所有玩家无节制调用。
inline bool clearPath(const World &world, const Vector3d &from, const Vector3d &to) {
	Vector3d delta{to.x - from.x, to.y - from.y, to.z - from.z};
	const double distance = delta.length();

	const int steps = (int)std::ceil(distance * 8.0);
	if (steps <= 0)
		return true; // 起终点重合（或距离极近），之间不可能有障碍

	for (int i = 0 ; i <= steps ; ++i) {
		const double t = (double)i / (double)steps;

		const double px = from.x + delta.x * t;
		const double py = from.y + delta.y * t;
		const double pz = from.z + delta.z * t;

		// floor 而非强转 int：负坐标下 int 截断朝零，会把 -0.5 变成 0，位置判错
		const int bx = (int)std::floor(px);
		const int by = (int)std::floor(py);
		const int bz = (int)std::floor(pz);

		const BlockState *state = world.getBlockState(bx, by, bz);
		if (!state || !state->isCollidable())
			continue;

		// 点在该方块内的局部坐标（0..1）
		const double lx = px - (double)bx;
		const double ly = py - (double)by;
		const double lz = pz - (double)bz;

		const FloatBox &box = state->boundingBox();
		if (lx >= box.x && lx <= box.x + box.sizeX
		 && ly >= box.y && ly <= box.y + box.sizeY
		 && lz >= box.z && lz <= box.z + box.sizeZ)
			return false;
	}

	return true;
}

#endif // ENTITYPATH_HPP_
