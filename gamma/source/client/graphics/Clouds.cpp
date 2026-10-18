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
#include <cmath>
#include <vector>

#include <SDL.h>

#include "common/core/Color.hpp"
#include "common/core/Debug.hpp"

#include "client/graphics/BgfxView.hpp"
#include "client/graphics/Camera.hpp"
#include "client/graphics/Clouds.hpp"
#include "client/graphics/RenderTarget.hpp"
#include "client/graphics/Vertex.hpp"

namespace {
	// 云片尺寸（方块）与高度
	constexpr float CELL_SIZE = 16.f;
	constexpr float CLOUD_Y   = 128.f;

	// 以玩家为中心生成的格子半径（每边 2*HALF+1 个格子）
	constexpr int HALF = 48;

	// 云的漂移速度（方块/秒，朝 +X 方向）
	constexpr float DRIFT_SPEED = 0.6f;

	// 云片颜色（略带灰的白色，半透明）
	constexpr u8 CLOUD_R = 245, CLOUD_G = 245, CLOUD_B = 250, CLOUD_A = 190;

	// 最坏情况顶点数（每个格子 6 个顶点）
	constexpr u32 MAX_VERTEX_COUNT = u32(2 * HALF + 1) * u32(2 * HALF + 1) * 6;

	// 确定性哈希 -> [0, 1)
	inline float hashf(int x, int y) {
		u32 h = u32(x) * 374761393u + u32(y) * 668265263u;
		h = (h ^ (h >> 13)) * 1274126177u;
		h ^= (h >> 16);
		return float(h & 0xFFFFu) / 65535.f;
	}

	// 双线性插值的值噪声 -> [0, 1]
	inline float smoothNoise(float x, float y) {
		int x0 = (int)std::floor(x);
		int y0 = (int)std::floor(y);
		float fx = x - float(x0);
		float fy = y - float(y0);
		fx = fx * fx * (3.f - 2.f * fx);
		fy = fy * fy * (3.f - 2.f * fy);

		float a = hashf(x0, y0);
		float b = hashf(x0 + 1, y0);
		float c = hashf(x0, y0 + 1);
		float d = hashf(x0 + 1, y0 + 1);

		return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
	}

	// 两个尺度叠加，得到成团的云（而非零散噪点）
	inline float cloudDensity(int ci, int cj) {
		return smoothNoise(float(ci) * 0.10f, float(cj) * 0.10f) * 0.65f
		     + smoothNoise(float(ci) * 0.25f, float(cj) * 0.25f) * 0.35f;
	}

	inline bool isCloud(int ci, int cj) {
		return cloudDensity(ci, cj) > 0.52f;
	}
}

Clouds::Clouds(Camera &camera) : m_camera(camera) {
	m_shader.loadFromFile("basic");

	m_vbo.setupDefaultLayout();

	SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 1, 1, 32, SDL_PIXELFORMAT_RGBA32);
	if (surface) {
		SDL_FillRect(surface, nullptr, SDL_MapRGBA(surface->format, 255, 255, 255, 255));
		m_whiteTexture.loadFromSurface(surface);
		SDL_FreeSurface(surface);
	}
	else {
		logError() << "Failed to create the 1x1 white texture used by the clouds";
	}
}

void Clouds::draw(RenderTarget &target, RenderStates states) const {
	if (!m_whiteTexture.getSize().x)
		return;

	// 由于 C++ 浮点的精度问题，世界物体在绘制时统一减去相机位置，
	// 并把相机临时搬到原点（见 ClientWorld/ChunkRenderer 的说明）。
	const Vector3d camPos = m_camera.getDPosition();

	// 云的漂移随时间推进；把「漂移 + 相机位置」合并成一个连续量 A，
	// relX = ci*CELL_SIZE + A，网格只在跨过格子边界时重建。
	const double t = SDL_GetTicks() / 1000.0;
	const double A = double(DRIFT_SPEED) * t - camPos.x;

	const int baseCi = (int)std::floor(-A / double(CELL_SIZE));
	const int baseCj = (int)std::floor(camPos.z / double(CELL_SIZE));

	if (baseCi != m_baseCi || baseCj != m_baseCj) {
		rebuild(baseCi, baseCj);
		m_baseCi = baseCi;
		m_baseCj = baseCj;
	}

	if (m_vertexCount == 0)
		return;

	logInfo() << "CLOUDS-DEBUG: verts" << m_vertexCount << "baseCi" << baseCi << "baseCj" << baseCj
	          << "fracX" << fracX << "fracZ" << fracZ << "camY" << camPos.y;

	const double fracX = -A - double(baseCi) * double(CELL_SIZE);
	const double fracZ = camPos.z - double(baseCj) * double(CELL_SIZE);

	states.shader = &m_shader;
	states.texture = &m_whiteTexture;
	states.isCullFaceEnabled = false;
	states.isDepthTestEnabled = true;

	// 平移到相机相对坐标（x/z 用格内小数部分，y 用相机高度）
	states.transform.translate(float(-fracX), float(-camPos.y), float(-fracZ));

	target.draw(m_vbo, 0, m_vertexCount, states);
}

void Clouds::rebuild(int baseCi, int baseCj) const {
	if (!m_vbo.isValid())
		m_vbo.init(nullptr, MAX_VERTEX_COUNT * sizeof(Vertex), true);

	std::vector<Vertex> vertices;
	vertices.reserve(2048);

	const float color[4] = {
		CLOUD_R / 255.f, CLOUD_G / 255.f, CLOUD_B / 255.f, CLOUD_A / 255.f
	};

	for (int cj = baseCj - HALF ; cj <= baseCj + HALF ; ++cj) {
		for (int ci = baseCi - HALF ; ci <= baseCi + HALF ; ++ci) {
			if (!isCloud(ci, cj))
				continue;

			const float x0 = float(ci - baseCi) * CELL_SIZE;
			const float x1 = x0 + CELL_SIZE;
			const float z0 = float(cj - baseCj) * CELL_SIZE;
			const float z1 = z0 + CELL_SIZE;

			// 一个平铺的四边面片（从下方可见，因此关闭了背面剔除）
			const float corners[4][3] = {
				{x0, CLOUD_Y, z0},
				{x1, CLOUD_Y, z0},
				{x0, CLOUD_Y, z1},
				{x1, CLOUD_Y, z1},
			};
			const int order[6] = {0, 2, 1, 1, 2, 3};

			for (int i = 0 ; i < 6 ; ++i) {
				const float *c = corners[order[i]];

				vertices.emplace_back();
				Vertex &v = vertices.back();
				v.coord3d[0] = c[0];
				v.coord3d[1] = c[1];
				v.coord3d[2] = c[2];
				v.coord3d[3] = -1; // 避免 basic shader 里的面着色分支
				v.texCoord[0] = 0.f; // 指向 1x1 白纹理
				v.texCoord[1] = 0.f;
				v.color[0] = color[0];
				v.color[1] = color[1];
				v.color[2] = color[2];
				v.color[3] = color[3];
			}
		}
	}

	m_vertexCount = u32(vertices.size());

	if (m_vertexCount)
		m_vbo.update(vertices.data(), m_vertexCount * sizeof(Vertex));
}
