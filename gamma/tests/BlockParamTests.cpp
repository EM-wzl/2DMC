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
// BlockParamTests.cpp
//
// 迁移自原 CxxTest 的 tests/BlockParamTests.hpp（提示词 3：CxxTest -> Catch2）。
// 只测纯逻辑（位分配/读写），不依赖 Registry / World，可独立运行。
//------------------------------------------------------------------------------
#include <catch2/catch_test_macros.hpp>

#include "common/world/Block.hpp"
#include "common/world/BlockParam.hpp"

// BlockParam 需要绑定到一个 Block（构造签名 BlockParam(Block&)），
// 所以每个用例都先造一个 Block 再取它的 param()。
static Block makeBlock() {
	return Block{1, "default:testblock"};
}

TEST_CASE("BlockParam: initially no bits allocated", "[blockparam]") {
	Block block = makeBlock();
	BlockParam &param = block.param();
	REQUIRE(param.getAllocatedSize() == 0);
}

TEST_CASE("BlockParam: allocateBits accumulates size", "[blockparam]") {
	Block block = makeBlock();
	BlockParam &param = block.param();
	param.allocateBits(BlockParam::Rotation, 5);
	REQUIRE(param.getAllocatedSize() == 5);

	param.allocateBits(BlockParam::State, 2);
	REQUIRE(param.getAllocatedSize() == 7);
}

TEST_CASE("BlockParam: setParam/getParam round-trip", "[blockparam]") {
	Block block = makeBlock();
	BlockParam &param = block.param();
	param.allocateBits(BlockParam::Rotation, 5);
	param.allocateBits(BlockParam::State, 2);

	u16 blockParam = 0;

	blockParam = param.setParam(BlockParam::Rotation, blockParam, 16);
	REQUIRE(blockParam == 16);

	blockParam = param.setParam(BlockParam::State, blockParam, 3);
	// Rotation 占低 5 位，State 从第 5 位开始
	REQUIRE(blockParam == (16 | (3 << 5)));

	REQUIRE(param.getParam(BlockParam::Rotation, blockParam) == 16);
	REQUIRE(param.getParam(BlockParam::State, blockParam) == 3);
}

TEST_CASE("BlockParam: overwriting one field keeps the other", "[blockparam]") {
	Block block = makeBlock();
	BlockParam &param = block.param();
	param.allocateBits(BlockParam::Rotation, 5);
	param.allocateBits(BlockParam::State, 2);

	u16 v = 0;
	v = param.setParam(BlockParam::Rotation, v, 7);
	v = param.setParam(BlockParam::State, v, 2);
	v = param.setParam(BlockParam::Rotation, v, 31);

	REQUIRE(param.getParam(BlockParam::Rotation, v) == 31);
	REQUIRE(param.getParam(BlockParam::State, v) == 2);
}

TEST_CASE("BlockParam: zero-size allocation is a no-op", "[blockparam]") {
	Block block = makeBlock();
	BlockParam &param = block.param();
	param.allocateBits(BlockParam::Rotation, 0);
	REQUIRE(param.getAllocatedSize() == 0);
}
