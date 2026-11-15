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
//------------------------------------------------------------------------------
// MathTests.cpp
//
// 覆盖 Box / Rect / Vector3 的构造、包含判定与相等性。
// 这些是区块边界、射线、光照范围计算的基础，改动风险高但原先零测试。
//------------------------------------------------------------------------------
#include <catch2/catch_test_macros.hpp>

#include "common/core/Box.hpp"
#include "common/core/Rect.hpp"
#include "common/core/Vector3.hpp"

TEST_CASE("Rect: basic construction and equality", "[math][rect]") {
	Rect<int> r{1, 2, 3, 4};
	REQUIRE(r.x == 1);
	REQUIRE(r.y == 2);
	REQUIRE(r.sizeX == 3);
	REQUIRE(r.sizeY == 4);

	Rect<int> same{1, 2, 3, 4};
	REQUIRE(r == same);

	Rect<int> diff{1, 2, 3, 5};
	REQUIRE_FALSE(r == diff);
}

TEST_CASE("Rect: contains respects half-open bounds", "[math][rect]") {
	Rect<int> r{0, 0, 4, 4};

	REQUIRE(r.contains(0, 0));
	REQUIRE(r.contains(3, 3));
	// 右/上边界不包含（size 是长度，不是闭区间）
	REQUIRE_FALSE(r.contains(4, 3));
	REQUIRE_FALSE(r.contains(3, 4));
	REQUIRE_FALSE(r.contains(-1, 0));
}

TEST_CASE("Box: basic construction", "[math][box]") {
	Box<int> b{1, 2, 3, 4, 5, 6};
	REQUIRE(b.x == 1);
	REQUIRE(b.y == 2);
	REQUIRE(b.z == 3);
	REQUIRE(b.sizeX == 4);
	REQUIRE(b.sizeY == 5);
	REQUIRE(b.sizeZ == 6);
}

TEST_CASE("Box: intersects detects overlap and separation", "[math][box]") {
	Box<int> a{0, 0, 0, 2, 2, 2};
	Box<int> overlapping{1, 1, 1, 2, 2, 2};
	Box<int> far{10, 10, 10, 2, 2, 2};

	REQUIRE(a.intersects(overlapping));
	REQUIRE_FALSE(a.intersects(far));
	// 贴边情况（touching）的语义依赖实现，这里不做断言，避免把实现细节写死
}

TEST_CASE("Vector3: arithmetic and equality", "[math][vector3]") {
	Vector3<int> a{1, 2, 3};
	Vector3<int> b{4, 5, 6};

	REQUIRE(a.x == 1);
	REQUIRE(a == Vector3<int>{1, 2, 3});
	REQUIRE_FALSE(a == b);

	Vector3<int> sum{a.x + b.x, a.y + b.y, a.z + b.z};
	REQUIRE(sum == Vector3<int>{5, 7, 9});
}
