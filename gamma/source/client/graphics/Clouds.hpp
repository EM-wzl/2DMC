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
#ifndef CLOUDS_HPP_
#define CLOUDS_HPP_

#include "client/graphics/Drawable.hpp"
#include "client/graphics/Shader.hpp"
#include "client/graphics/Texture.hpp"
#include "client/graphics/VertexBuffer.hpp"

class Camera;
struct RenderStates;
class RenderTarget;

//==============================================================================
// 云层（Minecraft 风格的方块状云）
//
// README 里列为缺失特性(#156)。实现方式：在 y=CLOUD_Y 的高度铺一层 12x12 的
// 方块状云片，形状由一个确定性的二维值噪声决定，因此云可以「无限」延伸：
// 云的存在只取决于整数格子坐标，玩家移动到哪就生成到哪。
//
// 网格按格子索引对齐，并用平移（而非每帧重建）来处理玩家移动与云的漂移，
// 所以顶点缓冲只有在跨过格子边界时才需要重建。
//==============================================================================
class Clouds : public Drawable {
	public:
		explicit Clouds(Camera &camera);

	private:
		void draw(RenderTarget &target, RenderStates states) const override;

		void rebuild(int baseCi, int baseCj) const;

		Camera &m_camera;

		Shader m_shader;

		// 云片是纯色的，但 basic shader 里声明了 SAMPLER2D，
		// 所以绑一张 1x1 白纹理保证采样器有效（结果会被顶点色相乘）。
		Texture m_whiteTexture;

		mutable VertexBuffer m_vbo;

		mutable int m_baseCi = 0x7fffffff;
		mutable int m_baseCj = 0x7fffffff;
		mutable u32 m_vertexCount = 0;
};

#endif // CLOUDS_HPP_
