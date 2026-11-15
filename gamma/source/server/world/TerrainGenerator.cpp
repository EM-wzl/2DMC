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
#include "common/core/EngineConfig.hpp"
#include "common/core/Registry.hpp"
#include "common/world/Dimension.hpp"
#include "common/world/World.hpp"
#include "server/world/ServerChunk.hpp"
#include "server/world/TerrainGenerator.hpp"

#include "common/core/Debug.hpp"

#include <glm/gtc/noise.hpp>
#include <FastNoiseLite.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace {
	// =========================================================================
	// 3D mineshaft generation (overworld only)
	//
	// The layout is a pure function of (seed, world coordinates):
	//  - one lattice cell per chunk column (16x16, aligned with the chunk grid);
	//  - straight corridors along x (row bands) and along y (column bands),
	//    each with a fixed y/x offset and a fixed z level derived from the
	//    surface height at the band's anchor point (surface - 8..40 blocks);
	//  - corridors exist per cell (~80%), so they run as broken abandoned
	//    tunnels but always line up at chunk borders;
	//  - where a row and a column corridor cross at different levels, ~1/6 of
	//    such junctions get a vertical shaft connecting the two levels.
	// Every chunk carves only the cells it owns, so no cross-chunk writes are
	// needed and neighbouring chunks agree at their borders by construction.
	// =========================================================================

	constexpr int MS_MIN_Z = 4;         // absolute bottom protection (gamma has no bedrock layer)
	constexpr int MS_SURFACE_CRUST = 5; // never carve into the top 5 blocks of a terrain column

	constexpr u64 SaltRowOff      = 0x243F6A8885A308D3ULL;
	constexpr u64 SaltRowDepth    = 0x13198A2E03707344ULL;
	constexpr u64 SaltRowSeg      = 0xA4093822299F31D0ULL;
	constexpr u64 SaltRowSupport  = 0x082EFA98EC4E6C89ULL;
	constexpr u64 SaltRowSupport2 = 0x452821E638D01377ULL;
	constexpr u64 SaltRowSlab     = 0xBE5466CF34E90C6CULL;
	constexpr u64 SaltColOff      = 0xC0AC29B7C97C50DDULL;
	constexpr u64 SaltColDepth    = 0x9216D5D9899FB199ULL;
	constexpr u64 SaltColSeg      = 0xD3A264F921A6BE0AULL;
	constexpr u64 SaltColSupport  = 0x26A11C2D1F9D7E35ULL;
	constexpr u64 SaltColSupport2 = 0x5D0F72D6380B5E1FULL;
	constexpr u64 SaltColSlab     = 0xF7A70A5E5B7A3F07ULL;
	constexpr u64 SaltShaft       = 0x67C3323B7C4E5B15ULL;

	u64 msFinalize(u64 x) {
		x += 0x9E3779B97F4A7C15ULL;
		x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
		x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
		return x ^ (x >> 31);
	}

	u64 msHash(s32 seed, s32 a, s32 b, u64 salt) {
		u64 h = msFinalize((u64)(s64)seed + 0x9E3779B97F4A7C15ULL);
		h = msFinalize(h ^ ((u64)(s64)a + 0x9E3779B97F4A7C15ULL));
		h = msFinalize(h ^ ((u64)(s64)b + 0x9E3779B97F4A7C15ULL));
		return msFinalize(h ^ salt);
	}

	// Nil-tolerant block id resolution (0 = not registered, treated as air).
	u16 msBlockID(const char *stringID) {
		Registry &registry = Registry::getInstance();
		return registry.hasBlock(stringID) ? registry.getBlockFromStringID(stringID).id() : u16(0);
	}

	// =========================================================================
	// Nether decoration helpers (default:nether only)
	//
	// A smooth 2D value noise sampled on the world block grid. A per-column hash
	// would give salt-and-pepper holes; interpolating a lattice instead gives
	// contiguous blobs, which is what lava basins and soul sand patches need.
	// Every sample depends only on (seed, world x, world y), so neighbouring
	// chunks agree at their borders without any cross-chunk writes.
	// =========================================================================

	s32 nfFloorDiv(s32 value, s32 divisor) {
		return (value >= 0) ? value / divisor : -((-value + divisor - 1) / divisor);
	}

	// Top 24 bits are plenty for a [0, 1) sample.
	float nfLattice(s32 seed, s32 gx, s32 gy, u64 salt) {
		return float(msHash(seed, gx, gy, salt) >> 40) * (1.0f / 16777216.0f);
	}

	float nfValueNoise2d(s32 seed, s32 wx, s32 wy, int cellSize, u64 salt) {
		const s32 gx = nfFloorDiv(wx, cellSize);
		const s32 gy = nfFloorDiv(wy, cellSize);
		const float fx = float(wx - gx * cellSize) / float(cellSize);
		const float fy = float(wy - gy * cellSize) / float(cellSize);
		const float sx = fx * fx * (3.0f - 2.0f * fx);
		const float sy = fy * fy * (3.0f - 2.0f * fy);

		const float v00 = nfLattice(seed, gx,     gy,     salt);
		const float v10 = nfLattice(seed, gx + 1, gy,     salt);
		const float v01 = nfLattice(seed, gx,     gy + 1, salt);
		const float v11 = nfLattice(seed, gx + 1, gy + 1, salt);

		const float top = v00 + (v10 - v00) * sx;
		const float bottom = v01 + (v11 - v01) * sx;
		return top + (bottom - top) * sy;
	}

	// Vertical bands used by the nether features (world z).
	constexpr int NF_LAVA_MIN_Z = 6;
	constexpr int NF_LAVA_MAX_Z = 18;
	constexpr int NF_GLOW_MIN_Z = 20;
	constexpr int NF_GLOW_MAX_Z = 30;

	// Blob sizes (blocks) and density thresholds.
	constexpr int NF_LAKE_REGION_CELL = 24;      // large-scale mask: which areas may hold a basin
	constexpr float NF_LAKE_REGION_THRESHOLD = 0.52f;
	constexpr int NF_LAKE_CELL = 9;
	constexpr int NF_LAKE_LEVEL_CELL = 13;
	constexpr float NF_LAKE_THRESHOLD = 0.60f;
	constexpr int NF_SOUL_ONE_IN = 30;
	constexpr int NF_BASALT_ONE_IN = 40;
	constexpr int NF_GLOWSTONE_ONE_IN = 200;

	constexpr u64 SaltNetherLake          = 0x3C6EF372FE94F82BULL;
	constexpr u64 SaltNetherLakeRegion    = 0x8A2D4B6E1F0C9D37ULL;
	constexpr u64 SaltNetherLakeLevel     = 0x5BE0CD19137E2179ULL;
	constexpr u64 SaltNetherLakeDepth     = 0x1F83D9ABFB41BD6BULL;
	constexpr u64 SaltNetherSoul          = 0x9B05688C2B3E6C1FULL;
	constexpr u64 SaltNetherBasalt        = 0x510E527FADE682D1ULL;
	constexpr u64 SaltNetherBasaltHeight  = 0x2B3E6C1F9B05688CULL;
	constexpr u64 SaltNetherBasaltTop     = 0xADE682D1510E527FULL;
	constexpr u64 SaltNetherGlowstone     = 0xCBBB9D5DC1059ED8ULL;
	constexpr u64 SaltNetherGlowstoneZ    = 0x629A292A367CD507ULL;
	constexpr u64 SaltNetherGlowstoneSize = 0x9159015A3070DD17ULL;

	// =========================================================================
	// End structures (default:end only): end city + end gateway
	//
	// Both are pure functions of (seed, region cell): every chunk independently
	// rebuilds the same geometry from the same anchor, so the pieces line up at
	// chunk borders without any cross-chunk writes. The anchors stay well inside
	// their cell (margin >= structure radius), so a structure never leaks into a
	// neighbouring cell and no cell has to know about its neighbours.
	// =========================================================================
	constexpr int EC_CITY_CELL          = 128; // one end city per 128x128 region
	constexpr int EC_CITY_MARGIN        = 16;  // anchor inset inside the cell
	constexpr int EC_CITY_MAX_RADIUS    = 5;   // max half footprint (9x9 tower)
	constexpr int EC_GATEWAY_CELL       = 256; // one end gateway per 256x256 region
	constexpr int EC_GATEWAY_MARGIN     = 48;  // anchor inset inside the cell
	constexpr int EC_GATEWAY_MAX_RADIUS = 5;   // max half footprint (9x9 base)
	constexpr int EC_MAX_PILLAR         = 32;  // cap for foundation columns

	constexpr u64 SaltEndCityRegion     = 0x2F8A6C1D4B7E9305ULL;
	constexpr u64 SaltEndGatewayRegion  = 0x7C4B1E93A6D2580FULL;

	// ---------------------------------------------------------------------
	// Nether fortress: one per NF_FORT_CELL region (96x96), 7x7 tower on a
	// 3-wide bridge. Margin >= max radius so structures never leak across
	// cell borders (same invariant as the end structures above).
	// ---------------------------------------------------------------------
	constexpr int NF_FORT_CELL       = 96;  // one fortress per 96x96 region
	constexpr int NF_FORT_MARGIN     = 20;  // anchor inset inside the cell
	constexpr int NF_FORT_MAX_RADIUS = 18;  // max bridge half-length + tower half
	constexpr int NF_FORT_TOWER_HEIGHT = 12;
	constexpr int NF_FORT_MAX_PILLAR = 40;  // cap for foundation columns

	constexpr u64 SaltNetherFortRegion = 0xB7E151628AED2A6AULL;

	// Village: one per VIL_CELL region (160x160).
	constexpr int VIL_CELL       = 160;
	constexpr int VIL_MARGIN     = 40;
	constexpr int VIL_MAX_RADIUS = 30;
	constexpr u64 SaltVillageRegion = 0xD1B54A32D192ED03ULL;

	// Nether tunnels / lava basins.
	constexpr int NT_CELL       = 64;   // one big lava basin per 64x64 region
	constexpr int NT_MARGIN     = 20;
	constexpr int NT_MAX_RADIUS = 16;
	constexpr u64 SaltNetherTunnelRegion = 0xA54FF53A5F1D36F1ULL;
	constexpr u64 SaltNetherLavaBasin    = 0x6A09E667F3BCC909ULL;

	struct EcAnchor { s32 x; s32 y; };

	// Positive modulo for negative world coordinates (same semantics as math::pmod,
	// but local to this file so no extra include is needed).
	s32 ecPmod(s32 value, s32 m) {
		const s32 r = value % m;
		return (r < 0) ? r + m : r;
	}

	// Deterministic anchor of an end structure inside its region cell.
	// Shared by the generator and by the gateway/city overlap check so both
	// always agree on where a structure is.
	EcAnchor ecAnchorInCell(s32 seed, s32 cellX, s32 cellY, int cellSize, int margin, u64 salt) {
		const u64 h = msHash(seed, cellX, cellY, salt);
		const u64 span = u64(cellSize - 2 * margin);
		const s32 ax = cellX * cellSize + margin + s32((h >> 8) % span);
		const s32 ay = cellY * cellSize + margin + s32((h >> 24) % span);
		return EcAnchor{ax, ay};
	}
}

TerrainGenerator::TerrainGenerator(Heightmap &heightmap, const Dimension &dimension, s32 seed)
	: m_biomeSampler(dimension, seed), m_heightmap(heightmap), m_seed(seed), m_dimensionStringID(dimension.stringID())
{
	m_caveNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
	m_caveNoise.SetFractalType(FastNoiseLite::FractalType::FractalType_FBm);
	m_caveNoise.SetFractalOctaves(2);
	m_caveNoise.SetFrequency(1.0 / 128.0);

	setSeed(seed);
}

void TerrainGenerator::generate(ServerChunk &chunk) {
	fastNoiseGeneration(chunk);

	// 主世界村庄（房屋/水井/道路）。对非 overworld 维度是空操作。
	generateVillages(chunk);

	// 下界专属地形装饰（岩浆湖/玄武岩柱/萤石簇/灵魂沙斑块）。
	// 对非 nether 维度是空操作，不改变主世界生成结果。
	generateNetherFeatures(chunk);

	// 下界隧道网络 + 大型岩浆湖。对非 nether 维度是空操作。
	generateNetherTunnels(chunk);

	// 下界要塞（桥廊 + 塔楼）。对非 nether 维度是空操作。
	generateNetherFortress(chunk);

	generateMineshafts(chunk);

	// 末地专属结构（末地城 + 末地折跃门）。对非 end 维度是空操作。
	generateEndCity(chunk);

	// TEMP: mineshaft verification instrumentation, enabled with 2DMC_MINESHAFT_CHECK=1.
	static const bool mineshaftCheck = [] {
		const char *env = std::getenv("2DMC_MINESHAFT_CHECK");
		return env && *env && std::strcmp(env, "0") != 0;
	}();
	if (mineshaftCheck)
		logMineshaftCheck(chunk);
}

void TerrainGenerator::fastNoiseGeneration(ServerChunk &chunk) {
	HeightmapChunk &heightmap = m_heightmap.getOrCreateChunk(chunk.x(), chunk.y());

	Random_t rand;
	rand.seed(chunk.x() + chunk.y() * CHUNK_WIDTH + chunk.z() * CHUNK_WIDTH * CHUNK_HEIGHT + 1337);

	Chunk *topChunk = chunk.getSurroundingChunk(Chunk::Top);
	for(s8 y = 0 ; y < CHUNK_DEPTH ; y++) {
		for(s8 x = 0 ; x < CHUNK_WIDTH ; x++) {
			u16 biomeIndex = m_biomeSampler.getBiomeIndexAt(x + chunk.x() * CHUNK_WIDTH, y + chunk.y() * CHUNK_DEPTH);
			const Biome &biome = Registry::getInstance().getBiome(biomeIndex);

			// Land height
			double h = heightmap.landHeightAt(x, y);

			// Land blocks
			for(s8 z = 0 ; z < CHUNK_HEIGHT ; z++) {
				// Are we above "ground" level?
				if(z + chunk.z() * CHUNK_HEIGHT > h) {
					// If we are not yet up to sea level, fill with water blocks
					if (z + chunk.z() * CHUNK_HEIGHT < SEALEVEL) {
						chunk.setBlockRaw(x, y, z, biome.getLiquidBlockID());
					}
					// Otherwise we are in the air
					else if (chunk.getBlock(x, y, z) == 0 && z == CHUNK_HEIGHT - 1) {
						// Add sunlight at the top of the chunk if possible
						chunk.lightmap().addSunlight(x, y, z, 15);
					}
				}
				else {
					bool isGeneratingTopBlock = z + chunk.z() * CHUNK_HEIGHT == h && z + chunk.z() * CHUNK_HEIGHT > SEALEVEL - 1;
					if (isGeneratingTopBlock)
						chunk.setBlockRaw(x, y, z, biome.getTopBlockID());
					else if (z + chunk.z() * CHUNK_HEIGHT <= SEALEVEL - 1 && h < SEALEVEL && z + chunk.z() * CHUNK_HEIGHT > h - 3)
						chunk.setBlockRaw(x, y, z, biome.getBeachBlockID());
					else if (z + chunk.z() * CHUNK_HEIGHT > h - 3)
						chunk.setBlockRaw(x, y, z, biome.getGroundBlockID());
					else
						chunk.setBlockRaw(x, y, z, biome.getDeepBlockID());

					// Caves
					generateCaves(chunk, x, y, z);

					// Populate ores.
					generateOres(chunk, x, y, z, biome, rand);

					// Generate trees, flora and portals
					if (isGeneratingTopBlock && chunk.getBlock(x, y, z)) {
						// Try to place a tree
						bool placedTree = tryPlaceTree(chunk, x, y, z + 1, biome, rand);

						// Otherwise try to place flora.
						bool placedFlora = false;
						if (!placedTree)
							placedFlora = tryPlaceFlora(chunk, x, y, z + 1, biome, rand);

						// Or a portal
						if (!placedTree && !placedFlora)
							tryPlacePortal(chunk, x, y, z + 1, biome, rand);
					}
				}

				if (topChunk && topChunk->isInitialized()) {
					u8 sunlightLevel = topChunk->lightmap().getSunlight(x, y, 0);
					if (sunlightLevel) {
						chunk.lightmap().addSunlight(x, y, CHUNK_HEIGHT - 1, sunlightLevel);
					}
				}
			}
		}
	}
}

inline bool TerrainGenerator::tryPlaceTree(ServerChunk &chunk, int x, int y, int z, const Biome &biome, Random_t &rand) const {
	if (chunk.getBlock(x, y, z - 1) == biome.getTopBlockID()) {
		for (const PlacementEntry::Tree &treePlacement : biome.getTrees()) {
			if (!rand.get<bool>(treePlacement.probability))
				continue;

			const Tree &tree = Registry::getInstance().getTree(treePlacement.treeID);

			// Trunk
			int h = rand.get(tree.trunkMinHeight(), tree.trunkMaxHeight());
			for (int i = 0; i < h; i++) {
				chunk.setBlockRaw(x, y, z + i, tree.getLogBlockID());
			}

			// Leaves
			if (tree.hasLeaves()) {
				for (int iz = -3; iz <= 3; iz++) {
					for (int iy = -3; iy <= 3; iy++) {
						for (int ix = -3; ix <= 3; ix++) {
							u16 block = chunk.getBlock(x + ix, y + iy, z + h + iz);
							if (ix * ix + iy * iy + iz * iz < 8 + rand.get(0, 1) && !block) {
								chunk.setBlockRaw(x + ix, y + iy, z + h + iz, tree.getLeavesBlockID());

								// FIXME: This is a temporary fix for the second part of #41
								chunk.lightmap().setSunlight(x + ix, y + iy, z + h + iz, 0);
							}
						}
					}
				}
			}

			return true;
		}
	}

	return false;
}

inline bool TerrainGenerator::tryPlaceFlora(ServerChunk &chunk, int x, int y, int z, const Biome &biome, Random_t &rand) const {
	for (const PlacementEntry::Flora &flora : biome.getFlora()) {
		if (chunk.getBlock(x, y, z - 1) != flora.spawnsOnBlockID)
			continue;

		if (!rand.get<bool>(flora.probability))
			continue;

		chunk.setBlockRaw(x, y, z, flora.blockID);

		return true;
	}

	return false;
}

inline bool TerrainGenerator::tryPlacePortal(ServerChunk &chunk, int x, int y, int z, const Biome &biome, Random_t &rand) const {
	// FIXME: This is a temporary portal generation
	//        This code should be replaced by a proper "feature" implementation
	//        which will also allow making stuff like villages easier
	if (chunk.getBlock(x, y, z - 1) == biome.getTopBlockID() && rand.get<bool>(0.0002)) {
		for (int ix = 0 ; ix < 4 ; ++ix) {
			for (int iz = 0 ; iz < 5 ; ++iz) {
				if (ix == 0 || iz == 0 || ix == 3 || iz == 4)
					chunk.setBlockRaw(x + ix, y, z + iz, biome.getPortalFrameBlockID());
				else
					chunk.setBlockRaw(x + ix, y, z + iz, biome.getPortalBlockID());
			}
		}

		return true;
	}

	return false;
}

inline void TerrainGenerator::generateOres(ServerChunk &chunk, int x, int y, int z, const Biome &biome, Random_t &rand) const {
	// TODO: Like trees, ores should be able to seamlessly cross chunk boundaries.
	// This could be achieved either by setting up a generation pipeline with stages,
	// processing neighboring chunks' ores every time, or generating them with noise.
	for (const PlacementEntry::Ore &ore : biome.getOres()) {
		if (!rand.get<bool>(ore.probability))
			continue;

		if (ore.genType == PlacementEntry::Ore::Gen::RandomWalk)
			randomWalkOrePlace(chunk, x, y, z, rand, ore.blockID, biome.getDeepBlockID(), ore.size);
		else if (ore.genType == PlacementEntry::Ore::Gen::FloodFill)
			oreFloodFill(chunk, x, y, z, biome.getDeepBlockID(), ore.blockID, ore.size, rand);

		break;
	}
}

inline void TerrainGenerator::randomWalkOrePlace(ServerChunk &chunk, int x, int y, int z, Random_t &rand, u16 oreBlock, u16 deepBlock, int size) const {
	if (size <= 0 || chunk.getBlock(x, y, z) != deepBlock)
		return;
	chunk.setBlockRaw(x, y, z, oreBlock);
	int direction = rand.get(1, 6);
	switch(direction) {
		case 1:
			randomWalkOrePlace(chunk, x + 1, y, z, rand, oreBlock, deepBlock, size - 1);
			break;
		case 2:
			randomWalkOrePlace(chunk, x - 1, y, z, rand, oreBlock, deepBlock, size - 1);
			break;
		case 3:
			randomWalkOrePlace(chunk, x, y + 1, z, rand, oreBlock, deepBlock, size - 1);
			break;
		case 4:
			randomWalkOrePlace(chunk, x, y - 1, z, rand, oreBlock, deepBlock, size - 1);
			break;
		case 5:
			randomWalkOrePlace(chunk, x, y, z + 1, rand, oreBlock, deepBlock, size - 1);
			break;
		case 6:
			randomWalkOrePlace(chunk, x, y, z - 1, rand, oreBlock, deepBlock, size - 1);
			break;
	}
}

void TerrainGenerator::oreFloodFill(ServerChunk &chunk, int x, int y, int z, u16 toReplace, u16 replaceWith, int depth, Random_t &rand) const {
	if (depth < 0) return;
	if (chunk.getBlock(x, y, z) == replaceWith) return;
	if (chunk.getBlock(x, y, z) == toReplace)
		chunk.setBlockRaw(x, y, z, replaceWith);

	oreFloodFill(chunk, x + 1, y, z, toReplace, replaceWith, depth - 1, rand);
	oreFloodFill(chunk, x - 1, y, z, toReplace, replaceWith, depth - 1, rand);
	oreFloodFill(chunk, x, y + 1, z, toReplace, replaceWith, depth - 1, rand);
	oreFloodFill(chunk, x, y - 1, z, toReplace, replaceWith, depth - 1, rand);
	oreFloodFill(chunk, x, y, z + 1, toReplace, replaceWith, depth - 1, rand);
	oreFloodFill(chunk, x, y, z - 1, toReplace, replaceWith, depth - 1, rand);

	if (rand.get<bool>(1.f / 15.f))
		oreFloodFill(chunk, x + 1, y + 1, z + 1, toReplace, replaceWith, depth - 1, rand);
	if (rand.get<bool>(1.f / 15.f))
		oreFloodFill(chunk, x + 1, y + 1, z - 1, toReplace, replaceWith, depth - 1, rand);
	if (rand.get<bool>(1.f / 15.f))
		oreFloodFill(chunk, x + 1, y - 1, z + 1, toReplace, replaceWith, depth - 1, rand);
	if (rand.get<bool>(1.f / 15.f))
		oreFloodFill(chunk, x + 1, y - 1, z - 1, toReplace, replaceWith, depth - 1, rand);
	if (rand.get<bool>(1.f / 15.f))
		oreFloodFill(chunk, x - 1, y + 1, z + 1, toReplace, replaceWith, depth - 1, rand);
	if (rand.get<bool>(1.f / 15.f))
		oreFloodFill(chunk, x - 1, y + 1, z - 1, toReplace, replaceWith, depth - 1, rand);
	if (rand.get<bool>(1.f / 15.f))
		oreFloodFill(chunk, x - 1, y - 1, z + 1, toReplace, replaceWith, depth - 1, rand);
	if (rand.get<bool>(1.f / 15.f))
		oreFloodFill(chunk, x - 1, y - 1, z - 1, toReplace, replaceWith, depth - 1, rand);
}

inline void TerrainGenerator::generateCavesOld(ServerChunk &chunk, s8 x, s8 y, s8 z, int h, HeightmapChunk &heightmap) const {
	float n2 = noise2d(-(x + chunk.x() * CHUNK_WIDTH) / 256.0, (y + chunk.y() * CHUNK_DEPTH) / 256.0, 8, 0.3f) * 4;
	float r2 = noise3d_abs(-float(x + chunk.x() * CHUNK_WIDTH) / 512.0f, float(z + chunk.z() * CHUNK_HEIGHT) / 512.0f, float(y + chunk.y() * CHUNK_DEPTH) / 512.0f, 4, 0.1f);
	float r3 = noise3d_abs(-float(x + chunk.x() * CHUNK_WIDTH) / 512.0f, float(z + chunk.z() * CHUNK_HEIGHT) / 128.0f, float(y + chunk.y() * CHUNK_DEPTH) / 512.0f, 4, 1.f);

	float r4 = n2 * 5 + r2 * r3 * 20;
	if (r4 > 6 && r4 < 8 && h > SEALEVEL) {
		chunk.setBlockRaw(x, y, z - 1, 0);
		chunk.setBlockRaw(x, y, z, 0);
		chunk.setBlockRaw(x, y, z + 1, 0);

		s32 landHeight = heightmap.landHeightAt(x, y);
		if (landHeight == z + 1 || landHeight == z || landHeight == z - 1)
			heightmap.setLandHeight(x, y, z - 2);
	}
}

inline void TerrainGenerator::generateCaves(ServerChunk &chunk, int x, int y, int z) {
	int rx = x + chunk.x() * CHUNK_WIDTH;
	int ry = y + chunk.y() * CHUNK_DEPTH;
	int rz = z + chunk.z() * CHUNK_HEIGHT;

	// Density map (not textured image)
	double n1 = m_caveNoise.GetNoise<double>(rx, ry, rz);
	double n2 = m_caveNoise.GetNoise<double>(rx, ry + 88.0, rz);
	double finalNoise = n1 * n1 + n2 * n2;

	if (finalNoise < 0.02) {
		// FIXME: Update heightmap
		chunk.setBlockRaw(x, y, z, 0);
	}
}

inline float TerrainGenerator::noise2d(double x, double y, int octaves, float persistence) {
	float sum = 0.f;
	float strength = 1.0f;
	float scale = 1.0f;

	for (int i = 0 ; i < octaves ; i++) {
		sum += strength * glm::simplex(glm::vec2{x, y} * scale);
		scale *= 2.0f;
		strength *= persistence;
	}

	return sum;
}

inline float TerrainGenerator::noise3d_abs(double x, double y, double z, int octaves, float persistence) {
	float sum = 0.f;
	float strength = 1.0f;
	float scale = 1.0f;

	for (int i = 0 ; i < octaves ; i++) {
		sum += strength * fabsf(glm::simplex(glm::vec3{x, y, z} * scale));
		scale *= 2.0f;
		strength *= persistence;
	}

	return sum;
}

//=============================================================================
// Nether-only terrain decoration
//=============================================================================

void TerrainGenerator::generateNetherFeatures(ServerChunk &chunk) const {
	// Strictly a nether feature: the overworld and custom dimensions must keep
	// generating exactly the same terrain as before.
	if (m_dimensionStringID != "default:nether")
		return;

	const u16 netherrackId = msBlockID("default:netherrack");
	const u16 soulSandId   = msBlockID("default:soul_sand");
	const u16 basaltId     = msBlockID("default:basalt");
	const u16 blackstoneId = msBlockID("default:blackstone");
	const u16 glowstoneId  = msBlockID("default:glowstone");
	const u16 lavaId       = msBlockID("default:lava");

	// Lava basins are the core feature; without netherrack/lava there is
	// nothing to carve. Each optional block id is nil-checked separately below.
	if (!netherrackId || !lavaId)
		return;

	const s32 cx = chunk.x();
	const s32 cy = chunk.y();
	const s32 zChunkBase = chunk.z() * CHUNK_HEIGHT;

	HeightmapChunk &heightmap = m_heightmap.getOrCreateChunk(cx, cy);

	const auto inChunk = [](int lx, int ly, int lz) {
		return lx >= 0 && lx < CHUNK_WIDTH && ly >= 0 && ly < CHUNK_DEPTH && lz >= 0 && lz < CHUNK_HEIGHT;
	};

	// Replace a block only when it currently holds `expected`. Air, cave voids,
	// ores and any other existing structure are therefore left untouched.
	const auto replaceIf = [&](int lx, int ly, int lz, u16 expected, u16 id) {
		if (!inChunk(lx, ly, lz))
			return;
		if (chunk.getBlock(lx, ly, lz) != expected)
			return;
		chunk.setBlockRaw(lx, ly, lz, id);
	};

	for (int ly = 0 ; ly < CHUNK_DEPTH ; ++ly) {
		for (int lx = 0 ; lx < CHUNK_WIDTH ; ++lx) {
			const s32 wx = cx * CHUNK_WIDTH + lx;
			const s32 wy = cy * CHUNK_DEPTH + ly;

			// Terrain surface of this column (same heightmap the base pass used).
			const s32 surfaceZ = heightmap.landHeightAt(s8(lx), s8(ly));
			const int surfaceLocal = int(surfaceZ) - int(zChunkBase);

			// --- Lava lakes ---------------------------------------------------
			// Two smooth noises: a large one gates which regions may hold a
			// basin (~half the nether), a smaller one shapes the pool so a
			// chunk is never entirely molten. Both are sampled in world block
			// coordinates, so pools stay contiguous across chunk borders.
			const float lakeRegion = nfValueNoise2d(m_seed, wx, wy, NF_LAKE_REGION_CELL, SaltNetherLakeRegion);
			if (lakeRegion > NF_LAKE_REGION_THRESHOLD) {
				const float lakeShape = nfValueNoise2d(m_seed, wx, wy, NF_LAKE_CELL, SaltNetherLake);
				if (lakeShape > NF_LAKE_THRESHOLD) {
					// Flat-ish pool surface between z 8 and 17, always leaving a
					// netherrack crust above it and staying inside z 6..18.
					int lakeTop = NF_LAVA_MIN_Z + 2
						+ int(nfValueNoise2d(m_seed, wx, wy, NF_LAKE_LEVEL_CELL, SaltNetherLakeLevel) * 8.0f);
					lakeTop = std::min(lakeTop, NF_LAVA_MAX_Z);
					lakeTop = std::min(lakeTop, surfaceZ - 3);

					if (lakeTop >= NF_LAVA_MIN_Z) {
						const int lakeDepth = 2 + int(msHash(m_seed, wx, wy, SaltNetherLakeDepth) % 3);
						const int lakeBottom = std::max(NF_LAVA_MIN_Z, lakeTop - lakeDepth + 1);

						for (int wz = lakeBottom ; wz <= lakeTop ; ++wz)
							replaceIf(lx, ly, wz - int(zChunkBase), netherrackId, lavaId);
					}
				}
			}

			// --- Basalt columns (basalt deltas style) -------------------------
			// Roughly one column in NF_BASALT_ONE_IN, 2..6 blocks tall, standing
			// on the surface; only air is filled so nothing existing is erased.
			if (basaltId && (msHash(m_seed, wx, wy, SaltNetherBasalt) % NF_BASALT_ONE_IN) == 0) {
				if (surfaceLocal >= 0 && surfaceLocal < CHUNK_HEIGHT
					&& chunk.getBlock(lx, ly, surfaceLocal) == netherrackId) {
					const int columnHeight = 2 + int(msHash(m_seed, wx, wy, SaltNetherBasaltHeight) % 5);
					const bool capBlackstone = blackstoneId
						&& (msHash(m_seed, wx, wy, SaltNetherBasaltTop) % 3) == 0;

					for (int i = 1 ; i <= columnHeight ; ++i) {
						const int lz = surfaceLocal + i;
						if (lz >= CHUNK_HEIGHT)
							break;
						if (chunk.getBlock(lx, ly, lz) != 0)
							break;

						const bool isTop = (i == columnHeight);
						chunk.setBlockRaw(lx, ly, lz, (isTop && capBlackstone) ? blackstoneId : basaltId);
					}
				}
			}

			// --- Glowstone clusters -------------------------------------------
			// Very sparse floating lights in the upper band (z 20..30), shaped
			// as a small vertical/plus cluster of 2..5 blocks. Air only.
			if (glowstoneId && (msHash(m_seed, wx, wy, SaltNetherGlowstone) % NF_GLOWSTONE_ONE_IN) == 0) {
				const int clusterBase = NF_GLOW_MIN_Z
					+ int(msHash(m_seed, wx, wy, SaltNetherGlowstoneZ) % (NF_GLOW_MAX_Z - NF_GLOW_MIN_Z + 1));
				const int clusterSize = 2 + int(msHash(m_seed, wx, wy, SaltNetherGlowstoneSize) % 4);

				// Relative offsets: a short hanging strand with a couple of side
				// bulbs, taking the first `clusterSize` entries.
				static const int clusterOffsets[5][3] = {
					{0, 0, 0}, {0, 0, 1}, {0, 0, 2}, {1, 0, 0}, {-1, 0, 0}
				};

				for (int i = 0 ; i < clusterSize ; ++i) {
					const int gx = lx + clusterOffsets[i][0];
					const int gy = ly + clusterOffsets[i][1];
					const int lz = clusterBase + clusterOffsets[i][2] - int(zChunkBase);

					if (!inChunk(gx, gy, lz))
						continue;
					if (chunk.getBlock(gx, gy, lz) != 0)
						continue;
					chunk.setBlockRaw(gx, gy, lz, glowstoneId);
				}
			}

			// --- Soul sand patches --------------------------------------------
			// Smooth 2D patches covering roughly one column in NF_SOUL_ONE_IN.
			// Only the surface netherrack block is swapped, so the crust below
			// (and anything built on top) is preserved.
			if (soulSandId && (msHash(m_seed, wx, wy, SaltNetherSoul) % NF_SOUL_ONE_IN) == 0) {
				if (surfaceLocal >= 0 && surfaceLocal < CHUNK_HEIGHT)
					replaceIf(lx, ly, surfaceLocal, netherrackId, soulSandId);
			}
		}
	}
}

//=============================================================================
// 3D mineshaft generation
//=============================================================================

void TerrainGenerator::generateMineshafts(ServerChunk &chunk) const {
	// Mineshafts are an overworld feature (skip nether and custom dimensions).
	if (m_dimensionStringID != "default:overworld")
		return;

	const u16 woodId  = msBlockID("default:oak_wood");
	const u16 plankId = msBlockID("default:oak_planks");
	const u16 slabId  = msBlockID("default:oak_slab");
	const u16 waterId = msBlockID("default:water");
	const u16 lavaId  = msBlockID("default:lava");

	const s32 cx = chunk.x();
	const s32 cy = chunk.y();
	const s32 zChunkBase = chunk.z() * CHUNK_HEIGHT;

	HeightmapChunk &heightmap = m_heightmap.getOrCreateChunk(cx, cy);

	// Every mineshaft write stays inside this chunk: carveCell() rejects any
	// coordinate outside [0, CHUNK_*). Cross-chunk consistency comes from the
	// pure world-coordinate layout, not from touching neighbour chunks.
	auto carveCell = [&](int lx, int ly, int lz, u16 id) {
		if (lx < 0 || lx >= CHUNK_WIDTH || ly < 0 || ly >= CHUNK_DEPTH || lz < 0 || lz >= CHUNK_HEIGHT)
			return;

		const s32 wz = zChunkBase + lz;

		// Never dig into the world floor protection or the surface crust.
		if (wz < MS_MIN_Z)
			return;
		if (wz > heightmap.landHeightAt(s8(lx), s8(ly)) - MS_SURFACE_CRUST)
			return;

		const u16 current = chunk.getBlock(lx, ly, lz);

		// Skip cells filled with water or lava.
		if ((waterId && current == waterId) || (lavaId && current == lavaId))
			return;

		if (current != id)
			chunk.setBlockRaw(lx, ly, lz, id);
	};

	const auto localZ = [zChunkBase](s32 wz) { return wz - zChunkBase; };

	// --- Row corridor: straight tunnel along +x inside row band cy -------------
	// Same (seed, cy) => same offset and level on both sides of a border.
	const s32 rowOff = 2 + s32(msHash(m_seed, cy, 0, SaltRowOff) % 12);
	const s32 rowDepth = 9 + s32(msHash(m_seed, cy, 0, SaltRowDepth) % 31); // surface - 8..40
	const HeightmapChunk &rowAnchor = m_heightmap.getOrCreateChunk(cy, cy);
	const s32 rowZ = std::max(MS_MIN_Z, rowAnchor.landHeightAt(8, s8(rowOff)) - rowDepth);
	const bool rowSeg = (msHash(m_seed, cx, cy, SaltRowSeg) % 5) != 0; // ~80% of cells

	if (rowSeg) {
		const int sup0 = int(msHash(m_seed, cx, cy, SaltRowSupport) % 4);
		const int supPeriod = 4 + int(msHash(m_seed, cx, cy, SaltRowSupport2) % 3); // every 4..6 blocks

		for (int lx = 0 ; lx < CHUNK_WIDTH ; ++lx) {
			// Tunnel section: 3 wide, 2 high, carved to air.
			for (int dy = -1 ; dy <= 1 ; ++dy) {
				for (int dz = 0 ; dz < 2 ; ++dz)
					carveCell(lx, rowOff + dy, localZ(rowZ + dz), 0);
			}

			const bool isSupport = lx >= sup0 && (lx - sup0) % supPeriod == 0;
			if (isSupport) {
				// Oak log pillars on both sides + oak plank floor section.
				for (int dy = -1 ; dy <= 1 ; ++dy)
					carveCell(lx, rowOff + dy, localZ(rowZ - 1), plankId);
				if (woodId) {
					carveCell(lx, rowOff - 1, localZ(rowZ), woodId);
					carveCell(lx, rowOff - 1, localZ(rowZ + 1), woodId);
					carveCell(lx, rowOff + 1, localZ(rowZ), woodId);
					carveCell(lx, rowOff + 1, localZ(rowZ + 1), woodId);
				}
			}
			else if (slabId && (msHash(m_seed, cx * CHUNK_WIDTH + lx, cy, SaltRowSlab) % 4) == 0) {
				// Occasional wood-slab platform sections.
				for (int dy = -1 ; dy <= 1 ; ++dy)
					carveCell(lx, rowOff + dy, localZ(rowZ), slabId);
			}
		}
	}

	// --- Column corridor: straight tunnel along +y inside column band cx -------
	const s32 colOff = 2 + s32(msHash(m_seed, cx, 1, SaltColOff) % 12);
	const s32 colDepth = 9 + s32(msHash(m_seed, cx, 1, SaltColDepth) % 31); // surface - 8..40
	const HeightmapChunk &colAnchor = m_heightmap.getOrCreateChunk(cx, cx);
	const s32 colZ = std::max(MS_MIN_Z, colAnchor.landHeightAt(s8(colOff), 8) - colDepth);
	const bool colSeg = (msHash(m_seed, cx, cy, SaltColSeg) % 5) != 0; // ~80% of cells

	if (colSeg) {
		const int sup0 = int(msHash(m_seed, cx, cy, SaltColSupport) % 4);
		const int supPeriod = 4 + int(msHash(m_seed, cx, cy, SaltColSupport2) % 3); // every 4..6 blocks

		for (int ly = 0 ; ly < CHUNK_DEPTH ; ++ly) {
			// Tunnel section: 3 wide, 2 high, carved to air.
			for (int dx = -1 ; dx <= 1 ; ++dx) {
				for (int dz = 0 ; dz < 2 ; ++dz)
					carveCell(colOff + dx, ly, localZ(colZ + dz), 0);
			}

			const bool isSupport = ly >= sup0 && (ly - sup0) % supPeriod == 0;
			if (isSupport) {
				// Oak log pillars on both sides + oak plank floor section.
				for (int dx = -1 ; dx <= 1 ; ++dx)
					carveCell(colOff + dx, ly, localZ(colZ - 1), plankId);
				if (woodId) {
					carveCell(colOff - 1, ly, localZ(colZ), woodId);
					carveCell(colOff - 1, ly, localZ(colZ + 1), woodId);
					carveCell(colOff + 1, ly, localZ(colZ), woodId);
					carveCell(colOff + 1, ly, localZ(colZ + 1), woodId);
				}
			}
			else if (slabId && (msHash(m_seed, cx, cy * CHUNK_DEPTH + ly, SaltColSlab) % 4) == 0) {
				// Occasional wood-slab platform sections.
				for (int dx = -1 ; dx <= 1 ; ++dx)
					carveCell(colOff + dx, ly, localZ(colZ), slabId);
			}
		}
	}

	// --- Junction shaft: connects the two corridor levels ----------------------
	// The junction column (colOff, rowOff) belongs to both corridors. When the
	// levels differ by 3 or more and the ~1/6 hash says so, carve a vertical
	// shaft between them (differences of 1-2 are already open).
	if (rowSeg && colSeg) {
		const s32 zLo = std::min(rowZ, colZ);
		const s32 zHi = std::max(rowZ, colZ);

		if (zHi - zLo >= 3 && (msHash(m_seed, cx, cy, SaltShaft) % 6) == 0) {
			for (s32 wz = zLo + 2 ; wz <= zHi - 1 ; ++wz)
				carveCell(colOff, rowOff, localZ(wz), 0);
		}
	}
}

// TEMP: per-chunk mineshaft block census for the 2DMC_MINESHAFT_CHECK=1 path.
void TerrainGenerator::logMineshaftCheck(ServerChunk &chunk) const {
	const u16 woodId  = msBlockID("default:oak_wood");
	const u16 plankId = msBlockID("default:oak_planks");
	const u16 slabId  = msBlockID("default:oak_slab");

	if (!woodId && !plankId && !slabId)
		return;

	u32 woodCount = 0;
	u32 plankCount = 0;
	u32 slabCount = 0;
	int sampleX = -1, sampleY = -1, sampleZ = -1;

	for (int z = 0 ; z < CHUNK_HEIGHT ; ++z) {
		for (int y = 0 ; y < CHUNK_DEPTH ; ++y) {
			for (int x = 0 ; x < CHUNK_WIDTH ; ++x) {
				const u16 id = u16(chunk.data(x, y, z) & 0xffff);

				if (woodId && id == woodId) {
					++woodCount;
					if (sampleX < 0) {
						sampleX = chunk.x() * CHUNK_WIDTH + x;
						sampleY = chunk.y() * CHUNK_DEPTH + y;
						sampleZ = chunk.z() * CHUNK_HEIGHT + z;
					}
				}
				else if (plankId && id == plankId) ++plankCount;
				else if (slabId && id == slabId) ++slabCount;
			}
		}
	}

	if (woodCount | plankCount | slabCount) {
		logInfo() << "[MINESHAFT_CHECK] chunk (" << chunk.x() << ", " << chunk.y() << ", " << chunk.z()
			<< ") oak_wood=" << woodCount << " oak_planks=" << plankCount << " oak_slab=" << slabCount
			<< " sample=(" << sampleX << ", " << sampleY << ", " << sampleZ << ")";
	}
}

//=============================================================================
// End-only structures: end city + end gateway
//=============================================================================

void TerrainGenerator::generateEndCity(ServerChunk &chunk) const {
	// TEMP(2DMC_ENDCITY_CHECK): verification instrumentation, removed after the
	// task-17 validation run. It lets the structure be exercised in a dimension
	// that can actually be loaded (the engine currently crashes on dimension
	// switch, so the real "end" dimension cannot be reached at runtime).
	static const bool endCityCheck = [] {
		const char *env = std::getenv("2DMC_ENDCITY_CHECK");
		return env && *env && std::strcmp(env, "0") != 0;
	}();
	const bool forceForCheck = endCityCheck && m_dimensionStringID == "default:overworld";

	// TEMP(2DMC_ENDCITY_CHECK): count what this pass writes so the verification
	// run can prove the geometry actually reached the chunk. Logged, not chatted:
	// server-side chat crashes when no client is attached.
	static u32 s_checkBricks = 0;
	static u32 s_checkObsidian = 0;
	static u32 s_checkStone = 0;
	static u32 s_checkChunks = 0;
	static bool s_checkSummaryDone = false;

	// Strictly an end feature: every other dimension keeps generating exactly the
	// same terrain as before (the dimension itself may not exist yet - we only
	// compare the runtime string id, so this compiles and is a no-op without it).
	if (m_dimensionStringID != "default:end" && !forceForCheck)
		return;

	const u16 endStoneId  = msBlockID("default:end_stone");
	const u16 endBricksId = msBlockID("default:end_bricks");
	const u16 obsidianId  = msBlockID("default:obsidian");
	// purpur_block is optional in this modpack; fall back to end bricks when absent.
	const u16 purpurId    = msBlockID("default:purpur_block");

	// Without end stone/end bricks there is nothing to build from.
	if (!endStoneId || !endBricksId)
		return;

	const u16 bodyId = endBricksId;                 // walls / floors / tower shell
	const u16 roofId = purpurId ? purpurId : bodyId; // top spire (purpur when available)

	const s32 cx = chunk.x();
	const s32 cy = chunk.y();
	const s32 zChunkBase = chunk.z() * CHUNK_HEIGHT;

	HeightmapChunk &heightmap = m_heightmap.getOrCreateChunk(cx, cy);

	const auto inChunk = [](int lx, int ly, int lz) {
		return lx >= 0 && lx < CHUNK_WIDTH && ly >= 0 && ly < CHUNK_DEPTH && lz >= 0 && lz < CHUNK_HEIGHT;
	};

	// Write a block only inside this chunk. `replaceSolidOnly` keeps existing air
	// (caves, the sky above the island) intact so a structure never seals a void.
	const auto put = [&](int lx, int ly, int lz, u16 id, bool replaceSolidOnly) {
		if (!inChunk(lx, ly, lz))
			return;
		if (replaceSolidOnly && chunk.getBlock(lx, ly, lz) == 0)
			return;
		chunk.setBlockRaw(lx, ly, lz, id);
	};

	// Build a horizontal rectangle outline in absolute world coordinates.
	const auto ringXZ = [&](int minX, int minY, int maxX, int maxY, int wz, u16 id, bool solidOnly) {
		for (int wy = minY ; wy <= maxY ; ++wy) {
			for (int wx = minX ; wx <= maxX ; ++wx) {
				const bool isEdge = (wx == minX || wx == maxX || wy == minY || wy == maxY);
				if (!isEdge)
					continue;
				put(wx - cx * CHUNK_WIDTH, wy - cy * CHUNK_DEPTH, wz - zChunkBase, id, solidOnly);
			}
		}
	};

	// Filled horizontal rectangle (floor plates).
	const auto fillXZ = [&](int minX, int minY, int maxX, int maxY, int wz, u16 id, bool solidOnly) {
		for (int wy = minY ; wy <= maxY ; ++wy)
			for (int wx = minX ; wx <= maxX ; ++wx)
				put(wx - cx * CHUNK_WIDTH, wy - cy * CHUNK_DEPTH, wz - zChunkBase, id, solidOnly);
	};

	// -----------------------------------------------------------------------
	// 1. End city: one tower per EC_CITY_CELL region.
	//
	//    Footprint 9x9, height 16..24. Hollow shell of end bricks, a floor plate
	//    every 4 blocks, 2x2 windows on all four faces between floors, a purpur
	//    (or end brick) stepped spire on top, and a foundation column of end stone
	//    going down to the terrain so the tower does not float.
	// -----------------------------------------------------------------------
	const s32 cityCellX = nfFloorDiv(cx * CHUNK_WIDTH, EC_CITY_CELL);
	const s32 cityCellY = nfFloorDiv(cy * CHUNK_DEPTH, EC_CITY_CELL);

	for (s32 gx = cityCellX - 1 ; gx <= cityCellX + 1 ; ++gx) {
		for (s32 gy = cityCellY - 1 ; gy <= cityCellY + 1 ; ++gy) {
			// Only the cells that can actually reach into this chunk are considered;
			// the anchor is inset by EC_CITY_MARGIN so radius EC_CITY_MAX_RADIUS can
			// never cross a cell border.
			const EcAnchor anchor = ecAnchorInCell(m_seed, gx, gy, EC_CITY_CELL,
				EC_CITY_MARGIN, SaltEndCityRegion);

			const int ax = anchor.x;
			const int ay = anchor.y;

			// Quick reject: does the tower footprint intersect this chunk at all?
			const int chunkMinX = cx * CHUNK_WIDTH;
			const int chunkMinY = cy * CHUNK_DEPTH;
			if (ax + EC_CITY_MAX_RADIUS < chunkMinX || ax - EC_CITY_MAX_RADIUS >= chunkMinX + CHUNK_WIDTH)
				continue;
			if (ay + EC_CITY_MAX_RADIUS < chunkMinY || ay - EC_CITY_MAX_RADIUS >= chunkMinY + CHUNK_DEPTH)
				continue;

			// Deterministic per-tower parameters.
			const u64 h = msHash(m_seed, gx, gy, SaltEndCityRegion ^ 0xA5A5A5A5A5A5A5A5ULL);
			const int half = 3 + int(h % 2);                  // half footprint: 3 or 4 -> 7x7 or 9x9
			const int towerHeight = 16 + int((h >> 4) % 9);   // 16..24
			const int floorStep = 4;                          // floor plate every 4 blocks

			// Skip towers that overlap an end gateway anchor: gateways win, so the
			// two features never intersect (deterministic, no cross-chunk state).
			const s32 gwCellX = nfFloorDiv(ax, EC_GATEWAY_CELL);
			const s32 gwCellY = nfFloorDiv(ay, EC_GATEWAY_CELL);
			bool blockedByGateway = false;
			for (s32 wx = gwCellX - 1 ; wx <= gwCellX + 1 && !blockedByGateway ; ++wx) {
				for (s32 wy = gwCellY - 1 ; wy <= gwCellY + 1 && !blockedByGateway ; ++wy) {
					const EcAnchor gw = ecAnchorInCell(m_seed, wx, wy, EC_GATEWAY_CELL,
						EC_GATEWAY_MARGIN, SaltEndGatewayRegion);
					const int dx = gw.x - ax;
					const int dy = gw.y - ay;
					if (dx * dx + dy * dy <= (EC_GATEWAY_MAX_RADIUS + half + 2) * (EC_GATEWAY_MAX_RADIUS + half + 2))
						blockedByGateway = true;
				}
			}
			if (blockedByGateway)
				continue;

			// Surface height at the tower centre decides where the tower starts.
			const s32 surfaceZ = heightmap.landHeightAt(
				s8(ecPmod(ax, CHUNK_WIDTH)), s8(ecPmod(ay, CHUNK_DEPTH)));
			const int baseZ = int(surfaceZ) + 1; // first block above the ground
			const int topZ = baseZ + towerHeight;

			// --- Foundation: end stone column from the terrain up to the base ----
			// The tower may straddle a slope; sample each corner column and drop a
			// column of end stone down to (local) terrain, capped at EC_MAX_PILLAR.
			for (int oy = -half ; oy <= half ; ++oy) {
				for (int ox = -half ; ox <= half ; ++ox) {
					if (ox != -half && ox != half && oy != -half && oy != half)
						continue; // only the shell ring needs support

					const int wx = ax + ox;
					const int wy = ay + oy;
					const s32 colSurface = heightmap.landHeightAt(
						s8(ecPmod(wx, CHUNK_WIDTH)), s8(ecPmod(wy, CHUNK_DEPTH)));

					const int groundZ = int(colSurface) + 1;
					const int bottom = std::max(baseZ - EC_MAX_PILLAR, std::min(groundZ, baseZ));
					for (int wz = bottom ; wz < baseZ ; ++wz)
						put(wx - cx * CHUNK_WIDTH, wy - cy * CHUNK_DEPTH, wz - zChunkBase, endStoneId, false);
				}
			}

			// --- Tower shell + floors + windows ----------------------------------
			for (int wz = baseZ ; wz <= topZ ; ++wz) {
				const bool isFloor = ((wz - baseZ) % floorStep) == 0;
				const bool isWindowBand = ((wz - baseZ) % floorStep) == 2; // one band above each floor

				// Floor plate (solid) on floor levels, otherwise just the shell ring.
				if (isFloor) {
					fillXZ(ax - half, ay - half, ax + half, ay + half, wz, bodyId, false);
					continue;
				}

				ringXZ(ax - half, ay - half, ax + half, ay + half, wz, bodyId, false);

				// 2x2 windows on all four faces, centred on each wall, in the band
				// right above a floor. The shell ring is punched back to air.
				if (isWindowBand && wz < topZ) {
					const int off = half - 1; // window centre offset from the tower centre
					for (int i = 0 ; i < 2 ; ++i) {
						for (int j = 0 ; j < 2 ; ++j) {
							// +x / -x faces
							put(ax + half - cx * CHUNK_WIDTH, ay + off + i - cy * CHUNK_DEPTH,
								wz - zChunkBase - j, 0, false);
							put(ax - half - cx * CHUNK_WIDTH, ay + off + i - cy * CHUNK_DEPTH,
								wz - zChunkBase - j, 0, false);
							// +y / -y faces
							put(ax + off + i - cx * CHUNK_WIDTH, ay + half - cy * CHUNK_DEPTH,
								wz - zChunkBase - j, 0, false);
							put(ax + off + i - cx * CHUNK_WIDTH, ay - half - cy * CHUNK_DEPTH,
								wz - zChunkBase - j, 0, false);
						}
					}
				}
			}

			// --- Stepped spire on top -------------------------------------------
			int spireZ = topZ + 1;
			for (int step = half ; step >= 0 ; --step) {
				fillXZ(ax - step, ay - step, ax + step, ay + step, spireZ, roofId, false);
				++spireZ;
			}

			// A single end brick finial so the tip reads as a spire, not a flat cap.
			put(ax - cx * CHUNK_WIDTH, ay - cy * CHUNK_DEPTH, spireZ - zChunkBase, bodyId, false);
		}
	}

	// -----------------------------------------------------------------------
	// 2. End gateway: one obsidian frame per EC_GATEWAY_CELL region.
	//
	//    A 4x5 vertical obsidian frame standing on an end stone base pad; the
	//    inside stays hollow (air) so it reads as a portal frame. Gateways are
	//    placed on the terrain surface and are skipped when they would collide
	//    with an end city anchor.
	// -----------------------------------------------------------------------
	const s32 gwCellX = nfFloorDiv(cx * CHUNK_WIDTH, EC_GATEWAY_CELL);
	const s32 gwCellY = nfFloorDiv(cy * CHUNK_DEPTH, EC_GATEWAY_CELL);

	for (s32 gx = gwCellX - 1 ; gx <= gwCellX + 1 ; ++gx) {
		for (s32 gy = gwCellY - 1 ; gy <= gwCellY + 1 ; ++gy) {
			const EcAnchor anchor = ecAnchorInCell(m_seed, gx, gy, EC_GATEWAY_CELL,
				EC_GATEWAY_MARGIN, SaltEndGatewayRegion);

			const int ax = anchor.x;
			const int ay = anchor.y;

			const int chunkMinX = cx * CHUNK_WIDTH;
			const int chunkMinY = cy * CHUNK_DEPTH;
			if (ax + EC_GATEWAY_MAX_RADIUS < chunkMinX || ax - EC_GATEWAY_MAX_RADIUS >= chunkMinX + CHUNK_WIDTH)
				continue;
			if (ay + EC_GATEWAY_MAX_RADIUS < chunkMinY || ay - EC_GATEWAY_MAX_RADIUS >= chunkMinY + CHUNK_DEPTH)
				continue;

			// Skip gateways that overlap an end city anchor (cities win here).
			const s32 cityCellX2 = nfFloorDiv(ax, EC_CITY_CELL);
			const s32 cityCellY2 = nfFloorDiv(ay, EC_CITY_CELL);
			bool blockedByCity = false;
			for (s32 wx = cityCellX2 - 1 ; wx <= cityCellX2 + 1 && !blockedByCity ; ++wx) {
				for (s32 wy = cityCellY2 - 1 ; wy <= cityCellY2 + 1 && !blockedByCity ; ++wy) {
					const EcAnchor city = ecAnchorInCell(m_seed, wx, wy, EC_CITY_CELL,
						EC_CITY_MARGIN, SaltEndCityRegion);
					const int dx = city.x - ax;
					const int dy = city.y - ay;
					if (dx * dx + dy * dy <= (EC_CITY_MAX_RADIUS + EC_GATEWAY_MAX_RADIUS + 2) * (EC_CITY_MAX_RADIUS + EC_GATEWAY_MAX_RADIUS + 2))
						blockedByCity = true;
				}
			}
			if (blockedByCity)
				continue;

			const s32 surfaceZ = heightmap.landHeightAt(
				s8(ecPmod(ax, CHUNK_WIDTH)), s8(ecPmod(ay, CHUNK_DEPTH)));
			const int baseZ = int(surfaceZ) + 1;

			// Base pad: 9x9 end stone slab, one block above the terrain, so the
			// frame always stands on solid ground.
			fillXZ(ax - 4, ay - 4, ax + 4, ay + 4, baseZ - 1, endStoneId, true);

			// Frame: 4 wide (x) by 5 tall (z), 1 block thick (y). The 2x3 opening
			// in the middle stays air. Frame bottom sits on the pad.
			for (int wz = baseZ ; wz <= baseZ + 4 ; ++wz) {
				for (int ox = 0 ; ox < 4 ; ++ox) {
					const bool isEdge = (ox == 0 || ox == 3 || wz == baseZ || wz == baseZ + 4);
					if (!isEdge)
						continue;
					put(ax + ox - cx * CHUNK_WIDTH, ay - cy * CHUNK_DEPTH, wz - zChunkBase, obsidianId, false);
				}
			}
		}
	}

	// TEMP(2DMC_ENDCITY_CHECK): report what this chunk actually received. This is
	// the end-to-end proof that the geometry reached the world (logged, because
	// server-side chat crashes without an attached client).
	if (endCityCheck) {
		u32 bricks = 0, obs = 0, stone = 0;
		for (int z = 0 ; z < CHUNK_HEIGHT ; ++z) {
			for (int y = 0 ; y < CHUNK_DEPTH ; ++y) {
				for (int x = 0 ; x < CHUNK_WIDTH ; ++x) {
					const u16 id = chunk.getBlock(x, y, z);
					if (id == endBricksId) ++bricks;
					else if (id == obsidianId) ++obs;
					else if (id == endStoneId) ++stone;
				}
			}
		}
		if (bricks | obs | stone) {
			logInfo() << "[ENDCITY_CHECK] chunk (" << cx << ", " << cy << ", " << chunk.z()
				<< ") end_bricks=" << bricks << " obsidian=" << obs << " end_stone=" << stone;
		}
	}
}


// ===========================================================================
// Nether fortress (default:nether only)
//
// One fortress per NF_FORT_CELL region: a raised bridge deck with parapets,
// supported by columns down to the terrain, plus a square tower with windows
// and a glowstone beacon on top. Purely a function of (seed, world coords),
// so chunks can be generated in any order and still match.
// ===========================================================================
void TerrainGenerator::generateNetherFortress(ServerChunk &chunk) const {
	if (m_dimensionStringID != "default:nether")
		return;

	const u16 brickId = msBlockID("default:nether_bricks");
	if (!brickId)
		return;
	const u16 rackId   = msBlockID("default:netherrack");
	const u16 glowId   = msBlockID("default:glowstone");
	const u16 basaltId = msBlockID("default:basalt");

	const s32 cx = chunk.x();
	const s32 cy = chunk.y();
	const s32 zChunkBase = chunk.z() * CHUNK_HEIGHT;

	const auto inChunk = [](int lx, int ly, int lz) {
		return lx >= 0 && lx < CHUNK_WIDTH && ly >= 0 && ly < CHUNK_DEPTH && lz >= 0 && lz < CHUNK_HEIGHT;
	};

	// replaceSolidOnly: don't punch into existing air (keeps lava lakes / caves).
	const auto put = [&](int lx, int ly, int lz, u16 id, bool replaceSolidOnly) {
		if (!inChunk(lx, ly, lz))
			return;
		if (replaceSolidOnly && chunk.getBlock(lx, ly, lz) == 0)
			return;
		chunk.setBlockRaw(lx, ly, lz, id);
	};

	// Absolute -> local conversion helpers for this chunk.
	const int chunkMinX = cx * CHUNK_WIDTH;
	const int chunkMinY = cy * CHUNK_DEPTH;

	const s32 cellX = nfFloorDiv(chunkMinX, NF_FORT_CELL);
	const s32 cellY = nfFloorDiv(chunkMinY, NF_FORT_CELL);

	for (s32 gx = cellX - 1 ; gx <= cellX + 1 ; ++gx) {
		for (s32 gy = cellY - 1 ; gy <= cellY + 1 ; ++gy) {
			const EcAnchor anchor = ecAnchorInCell(m_seed, gx, gy, NF_FORT_CELL, NF_FORT_MARGIN,
				SaltNetherFortRegion);
			const int ax = anchor.x;
			const int ay = anchor.y;

			// Reject cells whose whole footprint can't reach this chunk.
			if (ax + NF_FORT_MAX_RADIUS < chunkMinX || ax - NF_FORT_MAX_RADIUS >= chunkMinX + CHUNK_WIDTH)
				continue;
			if (ay + NF_FORT_MAX_RADIUS < chunkMinY || ay - NF_FORT_MAX_RADIUS >= chunkMinY + CHUNK_DEPTH)
				continue;

			const u64 h = msHash(m_seed, gx, gy, SaltNetherFortRegion ^ 0x5D5D5D5D5D5D5D5DULL);
			const int deckZ   = 64 + int(h % 9);          // bridge deck height 64..72
			const bool alongX = ((h >> 8) & 1) != 0;      // bridge runs along X or Y
			const int deckLen = 21 + int((h >> 12) % 10); // 21..30
			const int halfLen = deckLen / 2;
			const int halfW   = 1;                        // deck is 3 wide

			// --- Bridge deck + parapets + support columns -------------------
			for (int t = -halfLen ; t <= halfLen ; ++t) {
				for (int w = -halfW ; w <= halfW ; ++w) {
					const int wx = alongX ? ax + t : ax + w;
					const int wy = alongX ? ay + w : ay + t;
					const int lx = wx - chunkMinX;
					const int ly = wy - chunkMinY;

					// Deck floor (replace air too: the bridge is a built structure).
					put(lx, ly, deckZ - zChunkBase, brickId, false);

					// Parapets on both edges (1 block above the deck).
					if (w != 0) {
						put(lx, ly, deckZ + 1 - zChunkBase, brickId, false);
					}

					// Support column down to terrain, every 5 blocks along the span.
					if ((t % 5 == 0) && w == 0) {
						for (int z = deckZ - 1 ; z >= deckZ - NF_FORT_MAX_PILLAR ; --z) {
							if (z < zChunkBase)
								break;
							const int lz = z - zChunkBase;
							if (!inChunk(lx, ly, lz))
								continue;
							if (chunk.getBlock(lx, ly, lz) != 0) {
								// Reached terrain: cap with basalt and stop.
								if (basaltId)
									put(lx, ly, lz, basaltId, false);
								break;
							}
							put(lx, ly, lz, brickId, false);
						}
					}
				}
			}

			// --- Tower at the bridge centre ---------------------------------
			const int half = 3;                        // 7x7 footprint
			const int towerTop = deckZ + NF_FORT_TOWER_HEIGHT;
			for (int oy = -half ; oy <= half ; ++oy) {
				for (int ox = -half ; ox <= half ; ++ox) {
					const int wx = ax + ox;
					const int wy = ay + oy;
					const int lx = wx - chunkMinX;
					const int ly = wy - chunkMinY;

					const bool isEdge = (ox == -half || ox == half || oy == -half || oy == half);

					// Foundation column below the deck, down to terrain.
					for (int z = deckZ - 1 ; z >= deckZ - NF_FORT_MAX_PILLAR ; --z) {
						if (z < zChunkBase)
							break;
						const int lz = z - zChunkBase;
						if (!inChunk(lx, ly, lz))
							continue;
						if (chunk.getBlock(lx, ly, lz) != 0)
							break;
						put(lx, ly, lz, brickId, false);
					}

					if (!isEdge)
						continue;

					// Shell walls with 2x2 windows every 4 blocks.
					for (int z = deckZ ; z <= towerTop ; ++z) {
						const int lz = z - zChunkBase;
						// 每隔 4 层在墙面开一行窗（四角不挖，保证塔楼结构完整）。
						// 注意括号：&& 优先级高于 ||，原来少括号会把"非四角"判断拆错。
						const bool isCorner = (ox == -half || ox == half) && (oy == -half || oy == half);
						const bool windowRow = ((z - deckZ) % 4 == 2) && !isCorner;
						if (windowRow)
							continue;
						put(lx, ly, lz, brickId, false);
					}
				}
			}

			// Glowstone beacon on the tower roof.
			if (glowId) {
				put(ax - chunkMinX, ay - chunkMinY, towerTop + 1 - zChunkBase, glowId, false);
			}

			// Netherrack skirt around the base so the fortress reads as "carved in".
			if (rackId) {
				for (int oy = -half - 1 ; oy <= half + 1 ; ++oy) {
					for (int ox = -half - 1 ; ox <= half + 1 ; ++ox) {
						const int wx = ax + ox;
						const int wy = ay + oy;
						const int lx = wx - chunkMinX;
						const int ly = wy - chunkMinY;
						put(lx, ly, deckZ - 1 - zChunkBase, rackId, true);
					}
				}
			}
		}
	}
}


// ===========================================================================
// Nether tunnels + large lava basins (default:nether only)
//
// Carves winding tunnels through the netherrack using the existing 3D noise,
// lines them with magma / quartz, and drops one big lava basin per
// NT_CELL region. Only replaces solid blocks, so existing decoration
// (glowstone clusters, soul sand patches) is left untouched.
// ===========================================================================
void TerrainGenerator::generateNetherTunnels(ServerChunk &chunk) const {
	if (m_dimensionStringID != "default:nether")
		return;

	const u16 rackId    = msBlockID("default:netherrack");
	const u16 lavaId    = msBlockID("default:lava");
	const u16 magmaId   = msBlockID("default:magma_block");
	const u16 basaltId  = msBlockID("default:basalt");
	const u16 obsidId   = msBlockID("default:obsidian");
	const u16 quartzId  = msBlockID("default:nether_quartz_ore");

	if (!rackId)
		return;

	const s32 cx = chunk.x();
	const s32 cy = chunk.y();
	const s32 zChunkBase = chunk.z() * CHUNK_HEIGHT;

	const auto inChunk = [](int lx, int ly, int lz) {
		return lx >= 0 && lx < CHUNK_WIDTH && ly >= 0 && ly < CHUNK_DEPTH && lz >= 0 && lz < CHUNK_HEIGHT;
	};

	// 1. Tunnels: carve along a few deterministic "worm" paths per chunk.
	//
	//    NOTE: an earlier version sampled a 3D noise field for every block in
	//    the 30..60 band (about 32k simplex evaluations per chunk), which made
	//    nether chunk generation extremely slow (seconds per chunk) and stalled
	//    the server. Tunnels are now generated from a handful of hashed seeds,
	//    which is O(path length) instead of O(chunk volume).
	const int chunkMinX = cx * CHUNK_WIDTH;
	const int chunkMinY = cy * CHUNK_DEPTH;

	constexpr int NT_TUNNELS_PER_CHUNK = 2;
	constexpr int NT_TUNNEL_STEPS      = 48;

	for (int t = 0 ; t < NT_TUNNELS_PER_CHUNK ; ++t) {
		const u64 h0 = msHash(m_seed, cx * 7 + t, cy * 7 + t, SaltNetherTunnelRegion);
		// Start anywhere in the chunk, at a hashed height in 30..60.
		float px = float(chunkMinX + int(h0 % u64(CHUNK_WIDTH)));
		float py = float(chunkMinY + int((h0 >> 8) % u64(CHUNK_DEPTH)));
		float pz = float(30 + int((h0 >> 16) % 31));

		// Initial direction, then a gentle random walk (deterministic).
		float ang = float((h0 >> 24) % 360) * 3.14159265f / 180.f;
		float dz = 0.f;

		for (int s = 0 ; s < NT_TUNNEL_STEPS ; ++s) {
			const u64 hs = msHash(m_seed, t, s, SaltNetherTunnelRegion ^ u64(cx) ^ (u64(cy) << 20));
			ang += float(int(hs % 41) - 20) * 0.01f;   // -0.2..0.2 rad turn
			dz  += float(int((hs >> 8) % 21) - 10) * 0.01f; // -0.1..0.1 vertical drift
			if (dz > 0.6f) dz = 0.6f;
			if (dz < -0.6f) dz = -0.6f;

			px += std::cos(ang) * 1.0f;
			py += std::sin(ang) * 1.0f;
			pz += dz;
			if (pz < 30.f) pz = 30.f;
			if (pz > 60.f) pz = 60.f;

			const int bx = int(px);
			const int by = int(py);
			const int bz = int(pz);
			const int radius = 2 + int((hs >> 16) % 2); // 2..3

			// Carve a small sphere-ish blob, only where the chunk already has
			// solid blocks (never punches into existing air / lava).
			for (int oz = -radius ; oz <= radius ; ++oz) {
				for (int oy = -radius ; oy <= radius ; ++oy) {
					for (int ox = -radius ; ox <= radius ; ++ox) {
						if (ox * ox + oy * oy + oz * oz > radius * radius)
							continue;

						const int wx = bx + ox;
						const int wy = by + oy;
						const int wz = bz + oz;
						const int lx = wx - chunkMinX;
						const int ly = wy - chunkMinY;
						const int lz = wz - zChunkBase;

						if (!inChunk(lx, ly, lz))
							continue;
						if (chunk.getBlock(lx, ly, lz) == 0)
							continue; // already air

						const u64 hh = msHash(m_seed, wx, wy, SaltNetherTunnelRegion ^ u64(wz));
						if (quartzId && (hh % 131) == 0)
							chunk.setBlockRaw(lx, ly, lz, quartzId); // ore vein on the wall
						else if (magmaId && (hh % 173) == 0)
							chunk.setBlockRaw(lx, ly, lz, magmaId);
						else
							chunk.setBlockRaw(lx, ly, lz, 0);
					}
				}
			}
		}
	}

	// 2. Large lava basins: one per NT_CELL region.
	if (!lavaId)
		return;

	const s32 cellX = nfFloorDiv(chunkMinX, NT_CELL);
	const s32 cellY = nfFloorDiv(chunkMinY, NT_CELL);

	for (s32 gx = cellX - 1 ; gx <= cellX + 1 ; ++gx) {
		for (s32 gy = cellY - 1 ; gy <= cellY + 1 ; ++gy) {
			const EcAnchor anchor = ecAnchorInCell(m_seed, gx, gy, NT_CELL, NT_MARGIN,
				SaltNetherLavaBasin);
			const int ax = anchor.x;
			const int ay = anchor.y;

			if (ax + NT_MAX_RADIUS < chunkMinX || ax - NT_MAX_RADIUS >= chunkMinX + CHUNK_WIDTH)
				continue;
			if (ay + NT_MAX_RADIUS < chunkMinY || ay - NT_MAX_RADIUS >= chunkMinY + CHUNK_DEPTH)
				continue;

			const u64 h = msHash(m_seed, gx, gy, SaltNetherLavaBasin ^ 0x9E3779B97F4A7C15ULL);
			const int radius  = 8 + int((h >> 8) % 7);   // 8..14
			const int lakeZ   = 40 + int((h >> 16) % 11); // 40..50
			const int depth   = 3 + int((h >> 24) % 3);   // 3..5

			for (int oy = -radius ; oy <= radius ; ++oy) {
				for (int ox = -radius ; ox <= radius ; ++ox) {
					const int d2 = ox * ox + oy * oy;
					if (d2 > radius * radius)
						continue;

					const int wx = ax + ox;
					const int wy = ay + oy;
					const int lx = wx - chunkMinX;
					const int ly = wy - chunkMinY;

					// Lava body.
					for (int z = lakeZ - depth ; z <= lakeZ ; ++z) {
						const int lz = z - zChunkBase;
						if (!inChunk(lx, ly, lz))
							continue;
						chunk.setBlockRaw(lx, ly, lz, lavaId);
					}

					// Basin floor: obsidian.
					if (obsidId) {
						const int lz = lakeZ - depth - 1 - zChunkBase;
						if (inChunk(lx, ly, lz))
							chunk.setBlockRaw(lx, ly, lz, obsidId);
					}

					// Rim: magma / basalt on the outermost ring.
					const int rimD2 = (radius - 1) * (radius - 1);
					if (d2 >= rimD2) {
						const int lz = lakeZ + 1 - zChunkBase;
						if (inChunk(lx, ly, lz)) {
							const u16 rimId = (magmaId && ((ox + oy) & 1)) ? magmaId : (basaltId ? basaltId : rackId);
							chunk.setBlockRaw(lx, ly, lz, rimId);
						}
					}
				}
			}
		}
	}
}


// ===========================================================================
// Villages (default:overworld only)
//
// One village per VIL_CELL region: a few small houses, a well, and gravel
// paths connecting them, all snapped to the terrain surface via the heightmap.
// Deterministic per (seed, cell) so any chunk order produces the same village.
// ===========================================================================
void TerrainGenerator::generateVillages(ServerChunk &chunk) const {
	if (m_dimensionStringID != "default:overworld")
		return;

	const u16 cobbleId = msBlockID("default:cobblestone");
	const u16 planksId = msBlockID("default:oak_planks");
	const u16 glassId  = msBlockID("default:glass");
	const u16 gravelId = msBlockID("default:gravel");
	const u16 waterId  = msBlockID("default:water");
	const u16 dirtId   = msBlockID("default:dirt");

	if (!cobbleId || !planksId)
		return;

	const s32 cx = chunk.x();
	const s32 cy = chunk.y();
	const s32 zChunkBase = chunk.z() * CHUNK_HEIGHT;
	const int chunkMinX = cx * CHUNK_WIDTH;
	const int chunkMinY = cy * CHUNK_DEPTH;

	HeightmapChunk &heightmap = m_heightmap.getOrCreateChunk(cx, cy);

	const auto inChunk = [](int lx, int ly, int lz) {
		return lx >= 0 && lx < CHUNK_WIDTH && ly >= 0 && ly < CHUNK_DEPTH && lz >= 0 && lz < CHUNK_HEIGHT;
	};

	const auto put = [&](int wx, int wy, int wz, u16 id, bool replaceSolidOnly) {
		const int lx = wx - chunkMinX;
		const int ly = wy - chunkMinY;
		const int lz = wz - zChunkBase;
		if (!inChunk(lx, ly, lz))
			return;
		if (replaceSolidOnly && chunk.getBlock(lx, ly, lz) == 0)
			return;
		chunk.setBlockRaw(lx, ly, lz, id);
	};

	const auto surfaceAt = [&](int wx, int wy) -> int {
		const s32 lx = s32(ecPmod(wx, CHUNK_WIDTH));
		const s32 ly = s32(ecPmod(wy, CHUNK_DEPTH));
		return int(heightmap.landHeightAt(s8(lx), s8(ly)));
	};

	const s32 cellX = nfFloorDiv(chunkMinX, VIL_CELL);
	const s32 cellY = nfFloorDiv(chunkMinY, VIL_CELL);

	for (s32 gx = cellX - 1 ; gx <= cellX + 1 ; ++gx) {
		for (s32 gy = cellY - 1 ; gy <= cellY + 1 ; ++gy) {
			const EcAnchor anchor = ecAnchorInCell(m_seed, gx, gy, VIL_CELL, VIL_MARGIN,
				SaltVillageRegion);
			const int ax = anchor.x;
			const int ay = anchor.y;

			if (ax + VIL_MAX_RADIUS < chunkMinX || ax - VIL_MAX_RADIUS >= chunkMinX + CHUNK_WIDTH)
				continue;
			if (ay + VIL_MAX_RADIUS < chunkMinY || ay - VIL_MAX_RADIUS >= chunkMinY + CHUNK_DEPTH)
				continue;

			const u64 h = msHash(m_seed, gx, gy, SaltVillageRegion ^ 0x2545F4914F6CDD1DULL);
			const int baseZ = surfaceAt(ax, ay) + 1;

			// --- Well at the village centre ---------------------------------
			{
				const int r = 1; // 3x3
				for (int oy = -r ; oy <= r ; ++oy) {
					for (int ox = -r ; ox <= r ; ++ox) {
						const int wx = ax + ox;
						const int wy = ay + oy;
						const bool isEdge = (ox == -r || ox == r || oy == -r || oy == r);

						if (isEdge) {
							// Cobblestone rim, 2 blocks tall.
							put(wx, wy, baseZ, cobbleId, false);
							put(wx, wy, baseZ + 1, cobbleId, false);
						}
						else if (waterId) {
							// Water in the middle, sunk 2 blocks.
							put(wx, wy, baseZ - 1, waterId, false);
							put(wx, wy, baseZ - 2, waterId, false);
						}
					}
				}
			}

			// --- Houses -----------------------------------------------------
			const int houseCount = 3 + int((h >> 8) % 3); // 3..5
			for (int i = 0 ; i < houseCount ; ++i) {
				const u64 hh = msHash(m_seed, gx * 31 + i, gy * 31 + i, SaltVillageRegion ^ 0xC3C3C3C3C3C3C3C3ULL);

				// Place houses on a ring around the well, spaced deterministically.
				const float ang = float((hh >> 4) % 360) * 3.14159265f / 180.f;
				const int dist = 8 + int((hh >> 12) % 8); // 8..15
				const int hx = ax + int(std::cos(ang) * float(dist));
				const int hy = ay + int(std::sin(ang) * float(dist));

				const int hw = 2 + int((hh >> 20) % 2);   // half width: 2 or 3 -> 5x5 or 7x7
				const int wallH = 3 + int((hh >> 24) % 2); // 3..4
				const int hz = surfaceAt(hx, hy) + 1;

				// Path from the well to the house (gravel / dirt).
				const u16 pathId = gravelId ? gravelId : dirtId;
				if (pathId) {
					int px = ax;
					int py = ay;
					while (px != hx || py != hy) {
						if (px != hx)
							px += (hx > px) ? 1 : -1;
						else if (py != hy)
							py += (hy > py) ? 1 : -1;
						const int pz = surfaceAt(px, py) + 1;
						put(px, py, pz, pathId, true);
					}
				}

				// Walls (cobblestone), roof (planks), windows (glass), door (air).
				for (int oy = -hw ; oy <= hw ; ++oy) {
					for (int ox = -hw ; ox <= hw ; ++ox) {
						const int wx = hx + ox;
						const int wy = hy + oy;
						const bool isEdge = (ox == -hw || ox == hw || oy == -hw || oy == hw);

						// Floor
						put(wx, wy, hz - 1, cobbleId, false);

						if (isEdge) {
							for (int z = hz ; z < hz + wallH ; ++z) {
								// Door on one side, windows on others.
								const bool isDoor = (oy == -hw) && (ox == 0);
								const bool isWindow = !isDoor && ((ox == -hw || ox == hw) ? (oy == 0) : (ox == 0)) && (z == hz + 1);
								if (isDoor)
									put(wx, wy, z, 0, false); // air = opening
								else if (isWindow && glassId)
									put(wx, wy, z, glassId, false);
								else
									put(wx, wy, z, cobbleId, false);
							}
						}
					}
				}

				// Roof: planks slab on top of the walls.
				for (int oy = -hw - 1 ; oy <= hw + 1 ; ++oy) {
					for (int ox = -hw - 1 ; ox <= hw + 1 ; ++ox) {
						put(hx + ox, hy + oy, hz + wallH, planksId, false);
					}
				}
			}
		}
	}
}
