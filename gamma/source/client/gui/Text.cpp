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
#include "common/core/Color.hpp"
#include "common/resource/ResourceHandler.hpp"

#include "client/graphics/Vertex.hpp"
#include "client/gui/Font.hpp"
#include "client/gui/Text.hpp"

Text::Text() : m_font(ResourceHandler::getInstance().get<Font>("font-ascii")) {
	m_vbo.setupDefaultLayout();

	m_background.setFillColor(Color::Transparent);
}

void Text::setString(const std::string &string) {
	if (m_string != string) {
		m_string = string;
		m_isUpdateNeeded = true;
	}
}

void Text::setColor(const Color &color) {
	if (m_color != color) {
		m_color = color;
		m_isUpdateNeeded = true;
	}
}

void Text::setPadding(int x, int y) {
	if (m_padding.x != x || m_padding.y != y) {
		m_padding.x = x;
		m_padding.y = y;
		m_isUpdateNeeded = true;
	}
}

void Text::setMaxLineLength(unsigned int maxLineLength) {
	if (m_maxLineLength != maxLineLength) {
		m_maxLineLength = maxLineLength;
		m_isUpdateNeeded = true;
	}
}

void Text::draw(RenderTarget &target, RenderStates states) const {
	if (m_isUpdateNeeded) {
		updateVertexBuffer();
		m_isUpdateNeeded = false;
	}

	states.transform *= getTransform();

	if (m_background.color() != Color::Transparent || m_background.outlineThickness() > 0)
		target.draw(m_background, states);

	if (m_verticesCount == 0) return;

	states.transform.translate((float)m_padding.x, (float)m_padding.y);
	states.texture = &m_font.texture();

	states.isCullFaceEnabled = false;
	states.isDepthTestEnabled = false;

	target.draw(m_vbo, 0, m_verticesCount, states);
}

namespace {
	// UTF-8 解码：返回码点，并把 i 前进到下一个字符
	u32 decodeUtf8(const std::string &s, size_t &i) {
		unsigned char c0 = (unsigned char)s[i];
		if (c0 < 0x80) { i += 1; return c0; }

		u32 cp = 0;
		size_t n = 0;
		if ((c0 & 0xE0) == 0xC0) { cp = c0 & 0x1F; n = 1; }
		else if ((c0 & 0xF0) == 0xE0) { cp = c0 & 0x0F; n = 2; }
		else if ((c0 & 0xF8) == 0xF0) { cp = c0 & 0x07; n = 3; }
		else { i += 1; return 0xFFFD; }

		if (i + n >= s.size()) { i = s.size(); return 0xFFFD; }
		for (size_t k = 1; k <= n; ++k) {
			unsigned char cc = (unsigned char)s[i + k];
			if ((cc & 0xC0) != 0x80) { i += 1; return 0xFFFD; }
			cp = (cp << 6) | (cc & 0x3F);
		}
		i += n + 1;
		return cp;
	}
}

void Text::updateVertexBuffer() const {
	if (!m_isUpdateNeeded) return;

	std::vector<Vertex> vertices;

	u32 x = 0;
	u32 y = 0;
	u32 maxX = 0;
	const u32 lineH = m_font.lineHeight() + 1;

	if (m_isShadowEnabled) {
		Color color = m_shadowColor;
		size_t i = 0;
		while (i < m_string.size()) {
			if (m_string[i] == '\n') { i++; y += lineH; x = 0; continue; }

			u32 cp = decodeUtf8(m_string, i);
			const Font::Glyph &gl = m_font.glyph(cp);
			if (m_maxLineLength && x + gl.advance >= m_maxLineLength) { y += lineH; x = 0; continue; }

			addCharacter(x + 1, y + 1, color, gl, vertices);

			x += gl.advance;
		}
	}

	x = 0;
	y = 0;
	Color color = m_color;
	size_t i = 0;
	while (i < m_string.size()) {
		if (m_string[i] == '\n') { i++; maxX = std::max(x, maxX); y += lineH; x = 0; continue; }

		u32 cp = decodeUtf8(m_string, i);
		const Font::Glyph &gl = m_font.glyph(cp);
		if (m_maxLineLength && x + gl.advance >= m_maxLineLength) { maxX = std::max(x, maxX); y += lineH; x = 0; continue; }

		if (cp == '[')
			color = Color::TextBlue;

		addCharacter(x, y, color, gl, vertices);

		if (cp == ']')
			color = m_color;

		x += gl.advance;
	}
	maxX = std::max(x, maxX);

	m_verticesCount = (u32)vertices.size();

	if (m_verticesCount) {
		m_vbo.init(vertices.data(), sizeof(Vertex) * m_verticesCount, true);

		m_size.x = std::max(x, maxX);
		m_size.y = y + lineH;

		float backgroundX = std::max(m_background.getSize().x, float(m_size.x + m_padding.x));
		float backgroundY = std::max(m_background.getSize().y, float(m_size.y + m_padding.y));

		m_background.setSize(backgroundX, backgroundY);
	}
}

void Text::addCharacter(u32 x, u32 y, const Color &color, const Font::Glyph &glyph, std::vector<Vertex> &vertices) const {
	static const u8 coords[6][2] = {
		{1, 0},
		{0, 0},
		{1, 1},

		{1, 1},
		{0, 0},
		{0, 1},
	};

	// 每个字形自带尺寸：英文 8x8、中文 16x18
	for (int i = 0 ; i < 6 ; ++i) {
		vertices.emplace_back();
		Vertex &vertex = vertices.back();

		vertex.coord3d[0] = float(x + coords[i][0] * glyph.width);
		vertex.coord3d[1] = float(y + glyph.offsetY + coords[i][1] * glyph.height);
		vertex.coord3d[2] = 0;
		vertex.coord3d[3] = -1;

		vertex.color[0] = color.r;
		vertex.color[1] = color.g;
		vertex.color[2] = color.b;
		vertex.color[3] = color.a;

		// 3D 的 y 与图集的 v 都向下增长，所以 coords 的 y 直接映射到 v
		vertex.texCoord[0] = (coords[i][0] ? glyph.u1 : glyph.u0);
		vertex.texCoord[1] = (coords[i][1] ? glyph.v1 : glyph.v0);
	}
}
