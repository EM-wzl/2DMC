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
#ifndef LANPUBLISHSTATE_HPP_
#define LANPUBLISHSTATE_HPP_

#include <string>
#include <vector>

#include "client/gui/MenuWidget.hpp"
#include "client/gui/Text.hpp"
#include "client/gui/TextInput.hpp"
#include "client/states/InterfaceState.hpp"

class Client;
class ClientCommandHandler;

// 「开放局域网」界面（从 ESC 暂停菜单进入）。
//
// 功能：
//   - 显示当前服务端监听的端口（可编辑，作为「希望使用的端口」记录）
//   - 点击「开放局域网」后向服务端请求局域网信息（实际端口 + 本机所有 IPv4）
//   - 把 "IP:端口" 列表显示出来，玩家可以直接告诉朋友
//
// 说明：服务端在单机模式下已监听 0.0.0.0（SFML TcpListener 默认绑定 Any），
// 所以「开放」本质上是把端口/IP 告知玩家，并确认服务端已就绪。
// 运行中改端口需要重新监听（会断开现有连接），因此这里只做「记录 + 展示」，
// 真正生效的端口在下次进入世界时使用。
class LanPublishState : public InterfaceState {
	public:
		LanPublishState(Client &client, ClientCommandHandler &clientCommandHandler, DrawableState *parent = nullptr);

		void init() override;
		void onEvent(const SDL_Event &event) override;

	protected:
		void update() override;

	private:
		void onGuiScaleChanged(const GuiScaleChangedEvent &event);
		void updateWidgetPosition() override;
		void draw(RenderTarget &target, RenderStates states) const override;

		void requestLanInfo();
		void applyLanInfo();
		void openLan();
		void closeLan();

		Client &m_client;
		ClientCommandHandler &m_clientCommandHandler;

		MenuWidget m_menuWidget{1, 2};

		Text m_title;
		Text m_portLabel;
		TextInput m_portInput;
		Text m_infoText;
		Text m_statusText;

		bool m_hasLanInfo = false;
		bool m_lanOpen = false;
		u16 m_lanPort = 0;
		std::vector<std::string> m_lanAddresses;

		int m_requestCooldown = 0;
};

#endif // LANPUBLISHSTATE_HPP_
