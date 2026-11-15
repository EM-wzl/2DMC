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
#ifndef TITLESCREENSTATE_HPP_
#define TITLESCREENSTATE_HPP_

#include <thread>

#include "client/gui/MenuWidget.hpp"
#include "client/gui/TextInput.hpp"
#include "client/states/InterfaceState.hpp"

struct GuiScaleChangedEvent;
struct ServerOnlineEvent;

class TitleScreenState : public InterfaceState {
	public:
		TitleScreenState(u16 port = 4242);
		~TitleScreenState();

		void init() override;

		void onEvent(const SDL_Event &event) override;

		void startSingleplayer(bool showLoadingState, const std::string &worldName = "", s32 seed = 1337);
		void startMultiplayer(const std::string &host);

		// 读取并校验输入框里的玩家名（空/非法回退 "Player"），同时写回输入框
		std::string sanitizedUsername();

	private:
		// 按当前 guiScale 重设输入框的缩放/背景尺寸/内边距。
		// 构造函数与 onGuiScaleChanged() 都要调用：玩家在设置里改 GUI 缩放后
		// 回到标题页，若只重设菜单与标题而不重设输入框，输入框会保持旧的
		// 缩放倍率（且背景尺寸仍是旧倍率算出的值），布局会错乱。
		void applyInputScale(float guiScale);

		// 玩家名输入框的**屏幕像素**尺寸（与 guiScale 无关）。
		//
		// TextInput 的背景框/内边距是在 Text 的局部坐标系里给的，绘制时会被
		// TextInput 的缩放再乘一次，所以实际传入的值要除以 guiScale，
		// 这样最终渲染正好是这里的像素值。固定像素尺寸的好处是：
		// 换分辨率/换 GUI 缩放时输入框大小不变，也不会把标题或菜单按钮挡住。
		//
		// 尺寸必须**容得下占位文字**「玩家名称」：
		//   宽度：4 个中文字形 ≈ 16px/字 = 64px（局部） + padding
		//   高度：一行中文字形 ≈ 18px（局部） + padding
		// 注意 Text::updateVertexBuffer 里背景高 = max(设定高, 文字高+padding)
		// 这个 max **只在有文字内容时生效**；空输入框（只有 placeholder）
		// 走 m_text（无顶点）那条路，背景保持设定高度 —— 设小了文字就会溢出框。
		static constexpr float INPUT_WIDTH = 216.f;
		static constexpr float INPUT_HEIGHT = 64.f;
		static constexpr float INPUT_PADDING_X = 6.f;
		static constexpr float INPUT_PADDING_Y = 6.f;

		void onGuiScaleChanged(const GuiScaleChangedEvent &event);

		void updateWidgetPosition() override;

		void draw(RenderTarget &target, RenderStates states) const override;

		MenuWidget m_menuWidget{1, 3};

		// 玩家登录名称输入框（单机与多人共用；写入 Config::defaultUsername）
		TextInput m_usernameInput;

		Image m_background{"texture-title_screen"};

		std::thread m_thread;

		u16 m_port = 4242;

		Text m_titleText;
		Text m_versionText;
		Text m_copyrightText;
		Text m_licenseText;
};

#endif // TITLESCREENSTATE_HPP_
