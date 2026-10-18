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
#ifndef FONT_HPP_
#define FONT_HPP_

#include <string>
#include <unordered_map>

#include "common/core/Vector2.hpp"

class Texture;

//==============================================================================
// 字体（本地改动：支持中文 / 任意 Unicode 码点）
//
// 原版只支持单字节(0..255)，字形是 8x8 网格里按 byte 值索引的固定格子。
// 现在改成「按码点查表」：
//   * 0..255 仍然走原来的 8x8 ASCII 网格（英文保持像素风），列数从
//     properties 的 columns= 读取，不再从图集宽度推算（图集被加宽了）；
//   * 其它码点（中文等）由 tools/gen_font.ps1 预烘进图集下方，properties
//     里用 glyph.<码点>=x,y,w,h,advance 描述矩形与步进。
// 每个字形自带尺寸，所以中文可以是 16x18 而英文仍是 8x8。
//==============================================================================
class Font {
	public:
		struct Glyph {
			float u0 = 0.f, v0 = 0.f, u1 = 0.f, v1 = 0.f;   // 归一化图集坐标
			u8 width = 8;                                    // 像素宽
			u8 height = 8;                                   // 像素高
			u8 advance = 8;                                  // 水平步进
			// 基线对齐用的垂直偏移：ASCII 只有 8px 高，中文 20px，
			// 若都以行顶对齐，中文会比英文低约 7px（混排时明显错位）。
			// 这里给矮字形一个下移量，让两者的基线落在同一位置。
			signed char offsetY = 0;
		};

		Font(const std::string &textureName, const std::string &configPath);

		// 取一个码点对应的字形；缺字形时回退（见 m_fallback）
		const Glyph &glyph(u32 codepoint) const;

		// 行高（能容纳最高的字形，中文=18）
		u32 lineHeight() const { return m_lineHeight; }

		// 保留旧接口：ASCII 单元格尺寸
		Vector2i getTileSize() const { return {m_asciiWidth, m_asciiHeight}; }

		const Texture &texture() const { return m_texture; }

	private:
		void parseConfig(const std::string &configPath);
		void buildAsciiGlyphs();

		Texture &m_texture;

		std::unordered_map<u32, Glyph> m_glyphs;
		Glyph m_fallback;

		u8 m_asciiWidth = 8;
		u8 m_asciiHeight = 8;
		u8 m_asciiColumns = 16;
		u8 m_asciiAdvance[256] = {};
		u32 m_lineHeight = 9;
};

#endif // FONT_HPP_
