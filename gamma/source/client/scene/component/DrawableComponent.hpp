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
#ifndef DRAWABLECOMPONENT_HPP_
#define DRAWABLECOMPONENT_HPP_

#include <memory>
#include <vector>

#include "client/graphics/Drawable.hpp"

// 一个实体可以有多块（身体 + 头 + 腿…）。原来的单指针实现导致生物
// 只能显示一个方块（"动物没有身子"），这里改成列表，按顺序绘制。
class DrawableComponent {
	public:
		template<typename T, typename... Args>
		auto addDrawable(Args &&...args) -> typename std::enable_if<std::is_base_of<Drawable, T>::value, T &>::type {
			auto drawable = std::make_shared<T>(std::forward<Args>(args)...);
			m_drawables.emplace_back(drawable);
			return *std::static_pointer_cast<T>(drawable);
		}

		// 兼容旧调用：清空后只放一个
		template<typename T, typename... Args>
		auto setDrawable(Args &&...args) -> typename std::enable_if<std::is_base_of<Drawable, T>::value, T &>::type {
			m_drawables.clear();
			return addDrawable<T>(std::forward<Args>(args)...);
		}

		void clear() { m_drawables.clear(); }

		size_t count() const { return m_drawables.size(); }

		void draw(RenderTarget &target, RenderStates states) {
			for (auto &drawable : m_drawables) {
				if (drawable)
					target.draw(*drawable, states);
			}
		}

	private:
		std::vector<std::shared_ptr<Drawable>> m_drawables;
};

#endif // DRAWABLECOMPONENT_HPP_
