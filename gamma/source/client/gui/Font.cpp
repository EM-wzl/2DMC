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
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "common/resource/ResourceHandler.hpp"

#include "client/graphics/Texture.hpp"
#include "client/gui/Font.hpp"

namespace {
	// 逗号分隔切分，用于 glyph.<cp>=x,y,w,h,advance
	std::vector<std::string> splitCommas(const std::string &s) {
		std::vector<std::string> out;
		std::string cur;
		for (char c : s) {
			if (c == ',') { out.push_back(cur); cur.clear(); }
			else cur += c;
		}
		out.push_back(cur);
		return out;
	}
}

Font::Font(const std::string &textureName, const std::string &configPath)
	: m_texture(ResourceHandler::getInstance().get<Texture>(textureName))
{
	parseConfig(configPath);
	buildAsciiGlyphs();
}

void Font::parseConfig(const std::string &configPath) {
	std::ifstream file(configPath);
	if (!file.is_open())
		return;

	const float texW = (float)m_texture.getSize().x;
	const float texH = (float)m_texture.getSize().y;

	std::string line;
	while (std::getline(file, line, '\n')) {
		if (line.empty()) continue;
		// 去掉行尾的 \r（Windows 换行）
		if (line.back() == '\r') line.pop_back();
		if (line.empty()) continue;

		size_t equal = line.find_first_of('=');
		if (equal == std::string::npos) continue;
		std::string key = line.substr(0, equal);
		std::string value = line.substr(equal + 1);

		if (key == "columns") {
			m_asciiColumns = (u8)std::stoi(value);
			continue;
		}
		if (key == "cellheight") {
			m_asciiHeight = (u8)std::stoi(value);
			continue;
		}
		// 中文等：glyph.<码点>=x,y,w,h,advance
		if (key.compare(0, 6, "glyph.") == 0) {
			u32 cp = (u32)std::stoul(key.substr(6));
			std::vector<std::string> f = splitCommas(value);
			if (f.size() >= 5) {
				Glyph gl;
				float gx = (float)std::stoi(f[0]);
				float gy = (float)std::stoi(f[1]);
				gl.width = (u8)std::stoi(f[2]);
				gl.height = (u8)std::stoi(f[3]);
				gl.advance = (u8)std::stoi(f[4]);
				gl.u0 = gx / texW;
				gl.v0 = gy / texH;
				gl.u1 = (gx + gl.width) / texW;
				gl.v1 = (gy + gl.height) / texH;
				m_glyphs[cp] = gl;
			}
			continue;
		}
		// 旧格式：width.<byte>=<步进>
		if (key.compare(0, 6, "width.") == 0) {
			int k = std::stoi(key.substr(6));
			if (k >= 0 && k < 256)
				m_asciiAdvance[k] = (u8)std::stoi(value);
			continue;
		}
		// 其它未知属性忽略（保持向后兼容）
	}

	// 行高：至少容纳最高的字形
	m_lineHeight = m_asciiHeight;
	for (auto &kv : m_glyphs)
		if (kv.second.height > m_lineHeight)
			m_lineHeight = kv.second.height;
}

void Font::buildAsciiGlyphs() {
	const float texW = (float)m_texture.getSize().x;
	const float texH = (float)m_texture.getSize().y;

	for (u32 cp = 0; cp < 256; ++cp) {
		u32 col = cp % m_asciiColumns;
		u32 row = cp / m_asciiColumns;
		float x = (float)(col * m_asciiWidth);
		float y = (float)(row * m_asciiHeight);

		Glyph gl;
		gl.width = m_asciiWidth;
		gl.height = m_asciiHeight;
		gl.advance = m_asciiAdvance[cp] ? m_asciiAdvance[cp] : m_asciiWidth;
		// 基线对齐：ASCII 字形墨迹占满自己的 8px 格（墨迹 0..7，基线在 7），
		// 中文字形墨迹在 cell 里占 DRAW_Y..(DRAW_Y+FONT_PX-1)（基线≈格底-1）。
		// 让两者底边落在同一行：offsetY = lineHeight - asciiHeight - 1。
		gl.offsetY = (m_lineHeight > m_asciiHeight)
		             ? (signed char)(m_lineHeight - gl.height - 1)
		             : (signed char)0;
		gl.u0 = x / texW;
		gl.v0 = y / texH;
		gl.u1 = (x + gl.width) / texW;
		gl.v1 = (y + gl.height) / texH;
		m_glyphs[cp] = gl;
	}

	// 回退字形：ASCII '?' （缺字时可见，便于发现问题）；步进用空格宽度
	m_fallback = m_glyphs[0x3F];
	m_fallback.advance = m_asciiAdvance[0x20] ? m_asciiAdvance[0x20] : m_asciiWidth;
}

const Font::Glyph &Font::glyph(u32 codepoint) const {
	auto it = m_glyphs.find(codepoint);
	if (it != m_glyphs.end())
		return it->second;
	return m_fallback;
}
