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
#include <cstdlib>

#include "common/core/ApplicationStateStack.hpp"
#include "common/core/EventHandler.hpp"
#include "common/event/Events.hpp"

#include "client/core/Config.hpp"
#include "client/network/Client.hpp"
#include "client/network/ClientCommandHandler.hpp"
#include "client/states/LanPublishState.hpp"

LanPublishState::LanPublishState(Client &client, ClientCommandHandler &clientCommandHandler, DrawableState *parent)
	: InterfaceState(parent), m_client(client), m_clientCommandHandler(clientCommandHandler)
{
	m_menuWidget.setScale(Config::guiScale, Config::guiScale, 1);

	m_title.setScale(Config::guiScale * 2.f, Config::guiScale * 2.f);
	m_title.setString("开放局域网");
	m_title.updateVertexBuffer();

	m_portLabel.setScale(Config::guiScale, Config::guiScale);
	m_portLabel.setString("端口：");
	m_portLabel.updateVertexBuffer();

	// 端口输入框：预填服务端默认端口（4242），玩家可改成想用的端口
	m_portInput.setString("4242");
	m_portInput.setCharacterLimit(5);
	m_portInput.setBackgroundSize(100, 20);
	m_portInput.setBackgroundOutline(1, Color::White);
	m_portInput.setPadding(5, 6);
	m_portInput.setScale(Config::guiScale, Config::guiScale);
	m_portInput.setPlaceholder("端口");
	m_portInput.setFocus(false);

	m_infoText.setScale(Config::guiScale, Config::guiScale);
	m_infoText.setColor(Color::White);

	m_statusText.setScale(Config::guiScale, Config::guiScale);
	m_statusText.setColor(Color::Yellow);

	m_menuWidget.addButton("开放局域网", [this] (TextButton &) {
		openLan();
	});

	m_menuWidget.addButton("返回", [this] (TextButton &) {
		m_stateStack->pop();
	});

	// 进入界面就请求一次局域网信息。
	// 先清空客户端缓存：否则第二次打开本界面时 lanPort() 仍是上次的旧值，
	// update() 会立刻用旧数据填界面、再也不发请求。
	m_clientCommandHandler.clearLanInfo();
	requestLanInfo();

	updateWidgetPosition();
}

void LanPublishState::init() {
	m_eventHandler->addListener<GuiScaleChangedEvent>(&LanPublishState::onGuiScaleChanged, this);
}

void LanPublishState::onEvent(const SDL_Event &event) {
	InterfaceState::onEvent(event);

	if (!m_stateStack->empty() && &m_stateStack->top() == this) {
		m_portInput.onEvent(event);
		m_menuWidget.onEvent(event);

		if (event.type == SDL_MOUSEBUTTONDOWN
		&& !m_portInput.isPointInWidget(event.button.x, event.button.y))
			m_portInput.setFocus(false);

		if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
			m_stateStack->pop();
		}
	}
}

void LanPublishState::update() {
	InterfaceState::update();

	// 每秒最多请求一次，避免客户端还没连上时刷屏
	if (m_requestCooldown > 0)
		--m_requestCooldown;
	else if (!m_hasLanInfo) {
		requestLanInfo();
		m_requestCooldown = 60; // 约 1 秒（20tps）
	}

	// 服务端回了 LanInfo 就刷新显示
	if (!m_hasLanInfo && m_clientCommandHandler.lanPort() != 0) {
		applyLanInfo();
	}
}

void LanPublishState::requestLanInfo() {
	m_clientCommandHandler.requestLanInfo();
}

void LanPublishState::applyLanInfo() {
	m_lanPort = m_clientCommandHandler.lanPort();
	m_lanAddresses = m_clientCommandHandler.lanAddresses();
	m_hasLanInfo = true;

	// 用实际端口覆盖输入框，让玩家看到真实值
	m_portInput.setString(std::to_string(m_lanPort));

	std::string info;
	for (const std::string &addr : m_lanAddresses)
		info += addr + ":" + std::to_string(m_lanPort) + "\n";
	if (info.empty())
		info = "（未检测到可用网卡地址）";
	m_infoText.setString(info);

	updateWidgetPosition();
}

void LanPublishState::openLan() {
	// 校验端口输入
	int port = 0;
	try {
		port = std::stoi(m_portInput.string());
	}
	catch (...) {
		m_statusText.setString("错误：端口必须是数字");
		m_statusText.updateVertexBuffer();
		updateWidgetPosition();
		return;
	}

	if (port <= 0 || port > 65535) {
		m_statusText.setString("错误：端口范围 1-65535");
		m_statusText.updateVertexBuffer();
		updateWidgetPosition();
		return;
	}

	// 服务端已经在监听（单机内嵌服务端），这里把实际端口与请求端口对比提示
	if (m_hasLanInfo && m_lanPort != 0) {
		if ((int)m_lanPort == port) {
			m_lanOpen = true;
			m_statusText.setString("已开放到局域网（端口 " + std::to_string(m_lanPort) + "）");
		}
		else {
			// 运行中改端口需要重启世界，这里只记录，提示玩家
			m_statusText.setString(
				"当前世界监听端口为 " + std::to_string(m_lanPort)
				+ "，新端口 " + std::to_string(port)
				+ " 将在下次进入世界时生效");
			Config::defaultServerAddress = "localhost:" + std::to_string(port);
		}
	}
	else {
		m_statusText.setString("等待服务端响应…");
	}

	// 同时在聊天栏提示（让其他玩家也能看到）
	m_clientCommandHandler.sendChatMessage("/publish");

	m_statusText.updateVertexBuffer();
	updateWidgetPosition();
}

void LanPublishState::closeLan() {
	m_lanOpen = false;
	m_statusText.setString("已关闭局域网开放");
	m_statusText.updateVertexBuffer();
	updateWidgetPosition();
}

void LanPublishState::onGuiScaleChanged(const GuiScaleChangedEvent &event) {
	m_menuWidget.setScale(event.guiScale, event.guiScale);
	m_menuWidget.onGuiScaleChanged(event);

	m_title.setScale(event.guiScale * 2.f, event.guiScale * 2.f);
	m_portLabel.setScale(event.guiScale, event.guiScale);
	m_portInput.setScale(event.guiScale, event.guiScale);
	m_infoText.setScale(event.guiScale, event.guiScale);
	m_statusText.setScale(event.guiScale, event.guiScale);

	updateWidgetPosition();
}

void LanPublishState::updateWidgetPosition() {
	// 与 PauseMenuState 一致：直接用屏幕像素坐标（HUD/界面坐标系）
	const float centerX = Config::screenWidth / 2.f;

	m_title.setPosition(
		roundf(centerX - (float)m_title.getSize().x * m_title.getScale().x / 2.f),
		roundf(Config::screenHeight * 0.12f)
	);

	float y = Config::screenHeight * 0.26f;

	m_portLabel.setPosition(roundf(centerX - 110.f), roundf(y));
	m_portInput.setPosition(roundf(centerX - 40.f), roundf(y - 4.f));

	y += 44.f;
	m_infoText.setPosition(roundf(centerX - 160.f), roundf(y));

	y += 70.f;
	m_menuWidget.setPosition(
		roundf(centerX - m_menuWidget.getGlobalBounds().sizeX / 2.f),
		roundf(y)
	);

	y += m_menuWidget.getGlobalBounds().sizeY + 16.f;
	m_statusText.setPosition(roundf(centerX - 160.f), roundf(y));
}

void LanPublishState::draw(RenderTarget &target, RenderStates states) const {
	prepareDraw(target, states);

	target.draw(m_title, states);
	target.draw(m_portLabel, states);
	target.draw(m_portInput, states);
	target.draw(m_infoText, states);
	target.draw(m_menuWidget, states);
	target.draw(m_statusText, states);
}
