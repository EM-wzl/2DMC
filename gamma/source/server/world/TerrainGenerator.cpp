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

	generateMineshafts(chunk);

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
