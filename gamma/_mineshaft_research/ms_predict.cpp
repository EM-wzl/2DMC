// Throwaway predictor: replicates TerrainGenerator's mineshaft hash + the
// HeightmapChunk noise formula to predict tunnel layout near the origin.
// Usage: ms_predict.exe <seed> <cellMin> <cellMax>
#include "FastNoiseLite.hpp"
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>

using u64 = std::uint64_t;
using s32 = std::int32_t;
using s64 = std::int64_t;

static u64 msFinalize(u64 x) {
	x += 0x9E3779B97F4A7C15ULL;
	x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
	x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
	return x ^ (x >> 31);
}
static u64 msHash(s32 seed, s32 a, s32 b, u64 salt) {
	u64 h = msFinalize((u64)(s64)seed + 0x9E3779B97F4A7C15ULL);
	h = msFinalize(h ^ ((u64)(s64)a + 0x9E3779B97F4A7C15ULL));
	h = msFinalize(h ^ ((u64)(s64)b + 0x9E3779B97F4A7C15ULL));
	return msFinalize(h ^ salt);
}

static const u64 SaltRowOff      = 0x243F6A8885A308D3ULL;
static const u64 SaltRowDepth    = 0x13198A2E03707344ULL;
static const u64 SaltRowSeg      = 0xA4093822299F31D0ULL;
static const u64 SaltColOff      = 0xC0AC29B7C97C50DDULL;
static const u64 SaltColDepth    = 0x9216D5D9899FB199ULL;
static const u64 SaltColSeg      = 0xD3A264F921A6BE0AULL;
static const u64 SaltShaft       = 0x67C3323B7C4E5B15ULL;

static FastNoiseLite n1, n2, n3, n4;

static s32 landHeight(s32 wx, s32 wy) { // HeightmapChunk::generate() formula
	double a = n1.GetNoise<double>(wx, wy);
	double b = n2.GetNoise<double>(wx, wy);
	double c = n3.GetNoise<double>(wx, wy);
	double d = n4.GetNoise<double>(wx, wy);
	return s32((a + (b * c * (d * 2 - 1))) * 64 + 64);
}

int main(int argc, char **argv) {
	s32 seed = argc > 1 ? atoi(argv[1]) : 1337;
	s32 lo = argc > 2 ? atoi(argv[2]) : -3;
	s32 hi = argc > 3 ? atoi(argv[3]) : 3;

	for (FastNoiseLite *n : {&n1, &n2, &n3, &n4}) {
		n->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
		n->SetFractalType(FastNoiseLite::FractalType_FBm);
		n->SetFractalOctaves(4);
	}
	n1.SetFrequency(1 / 256.0f); n2.SetFrequency(1 / 256.0f);
	n3.SetFrequency(1 / 256.0f); n4.SetFrequency(1 / 1024.0f);
	n1.SetSeed(seed); n2.SetSeed(seed); n3.SetSeed(seed); n4.SetSeed(seed);

	printf("seed=%d cells [%d..%d]\n", seed, lo, hi);
	for (s32 r = lo; r <= hi; ++r) {
		s32 off  = 2 + s32(msHash(seed, r, 0, SaltRowOff) % 12);
		s32 dep  = 9 + s32(msHash(seed, r, 0, SaltRowDepth) % 31);
		s32 h    = landHeight(r * 16 + 8, r * 16 + off);
		s32 zRow = h - dep > 4 ? h - dep : 4;
		s32 cz   = zRow >= 0 ? zRow / 32 : (zRow - 31) / 32;
		printf("row r=%d: off=%d depth=%d h=%d zRow=%d (zChunk %d) segs:", r, off, dep, h, zRow, cz);
		for (s32 cx = lo; cx <= hi; ++cx) printf(" %d", (msHash(seed, cx, r, SaltRowSeg) % 5) != 0);
		printf("\n");
	}
	for (s32 c = lo; c <= hi; ++c) {
		s32 off  = 2 + s32(msHash(seed, c, 1, SaltColOff) % 12);
		s32 dep  = 9 + s32(msHash(seed, c, 1, SaltColDepth) % 31);
		s32 h    = landHeight(c * 16 + off, c * 16 + 8);
		s32 zCol = h - dep > 4 ? h - dep : 4;
		s32 cz   = zCol >= 0 ? zCol / 32 : (zCol - 31) / 32;
		printf("col c=%d: off=%d depth=%d h=%d zCol=%d (zChunk %d) segs:", c, off, dep, h, zCol, cz);
		for (s32 cy = lo; cy <= hi; ++cy) printf(" %d", (msHash(seed, c, cy, SaltColSeg) % 5) != 0);
		printf("\n");
	}
	printf("junctions (rowSeg&&colSeg, dz>=3 -> shaft prob 1/6):\n");
	for (s32 r = lo; r <= hi; ++r) {
		s32 roff = 2 + s32(msHash(seed, r, 0, SaltRowOff) % 12);
		s32 rdep = 9 + s32(msHash(seed, r, 0, SaltRowDepth) % 31);
		s32 rh   = landHeight(r * 16 + 8, r * 16 + roff);
		s32 rz   = rh - rdep > 4 ? rh - rdep : 4;
		for (s32 c = lo; c <= hi; ++c) {
			s32 coff = 2 + s32(msHash(seed, c, 1, SaltColOff) % 12);
			s32 cdep = 9 + s32(msHash(seed, c, 1, SaltColDepth) % 31);
			s32 ch   = landHeight(c * 16 + coff, c * 16 + 8);
			s32 czl  = ch - cdep > 4 ? ch - cdep : 4;
			bool rseg = (msHash(seed, c, r, SaltRowSeg) % 5) != 0;
			bool cseg = (msHash(seed, c, r, SaltColSeg) % 5) != 0;
			s32 dz = rz > czl ? rz - czl : czl - rz;
			bool shaft = rseg && cseg && dz >= 3 && (msHash(seed, c, r, SaltShaft) % 6) == 0;
			printf("  cell (%d,%d): rz=%d cz=%d dz=%d rseg=%d cseg=%d shaft=%d\n", c, r, rz, czl, dz, rseg, cseg, shaft);
		}
	}
	return 0;
}
