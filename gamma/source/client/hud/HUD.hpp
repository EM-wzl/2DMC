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
#ifndef HUD_HPP_
#define HUD_HPP_

#include <memory>
#include <vector>

#include <entt/entt.hpp>

#include "client/hud/BlockCursor.hpp"
#include "client/hud/BlockInfoWidget.hpp"
#include "client/hud/Chat.hpp"
#include "client/hud/Crosshair.hpp"
#include "client/hud/DebugLightmapViewer.hpp"
#include "client/hud/DebugOverlay.hpp"
#include "client/hud/DebugProfilerWindow.hpp"
#include "client/hud/Hotbar.hpp"
#include "client/hud/Minimap.hpp"
#include "client/graphics/Shader.hpp"
#include "client/graphics/View.hpp"

struct GuiScaleChangedEvent;

class ClientCommandHandler;

class HUD : public Transformable, public Drawable {
	public:
		HUD(ClientPlayer &player, ClientWorld &world, ClientCommandHandler &client);

		void setup();

		void onEvent(const SDL_Event &event);
		void onGuiScaleChanged(const GuiScaleChangedEvent &event);

		void update();

		void pause() { m_blockCursor.reset(); }

		const BlockCursor &blockCursor() const { return m_blockCursor; }

		Chat &chat() { return m_chat; }

		Minimap &minimap() { return m_minimap; }

	private:
		void draw(RenderTarget &target, RenderStates states) const override;

		void updateMobHealthText();

		// 其他玩家头顶名字：世界坐标 → 屏幕投影
		void updatePlayerNameLabels();

		ClientPlayer &m_player;
		ClientWorld &m_world;
		ClientCommandHandler &m_clientCommandHandler;

		Shader m_shader;
		View m_view;

		Hotbar m_hotbar;

		BlockCursor m_blockCursor;
		Crosshair m_crosshair;

		DebugOverlay m_debugOverlay;
		bool m_isDebugOverlayVisible = false;
		bool m_printRendererInfo = false;

		BlockInfoWidget m_blockInfoWidget;

		Text m_fpsText;
		Text m_healthText;      // 玩家自己的 HP
		Text m_mobHealthText;   // 准星指向的生物 HP
		entt::entity m_mobHealthTarget = entt::null; // 当前指向的生物（左键攻击用）

		// 其他玩家头顶的名字（多人游戏时显示）。
		// 用 clientID 作为稳定 key，Text 缓存复用，避免每帧重建。
		// 注意：Text 不可移动/拷贝，所以用 unique_ptr 存，容器才支持 erase。
		struct PlayerNameLabel {
			u16 clientID = 0;
			std::unique_ptr<Text> text;
		};
		std::vector<std::unique_ptr<PlayerNameLabel>> m_playerNameLabels;

		Chat m_chat;

		Minimap m_minimap;

		DebugLightmapViewer m_debugLightmapViewer;

		DebugProfilerWindow m_debugProfilerWindow;
};

#endif // HUD_HPP_
