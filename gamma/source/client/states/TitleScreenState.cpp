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
#include <cctype>

#include "common/core/ApplicationStateStack.hpp"
#include "common/core/Debug.hpp"
#include "common/event/Events.hpp"

#include "server/core/ServerApplication.hpp" // For ServerOnlineEvent

#include "client/core/Config.hpp"
#include "client/core/GameConfig.hpp"
#include "client/states/GameState.hpp"
#include "client/states/ServerConnectState.hpp"
#include "client/states/ServerLoadingState.hpp"
#include "client/states/SettingsMenuState.hpp"
#include "client/states/TitleScreenState.hpp"
#include "client/states/WorldSelectionState.hpp"

TitleScreenState::TitleScreenState(u16 port) : m_port(port) {
	m_menuWidget.setScale(Config::guiScale, Config::guiScale);

	// 玩家登录名称：预填上次使用的名字，没有则显示 placeholder。
	// 名字在「单人游戏」与「多人游戏」两条路径都会用到：ClientConnect 包携带它，
	// 服务端据此创建角色，PlayerSpawn 再广播给所有客户端用于显示头顶名字。
	m_usernameInput.setString(Config::defaultUsername);
	m_usernameInput.setCharacterLimit(20);
	applyInputScale(Config::guiScale);
	m_usernameInput.setPlaceholder("玩家名称");
	m_usernameInput.setFocus(false);

	m_menuWidget.addButton("单人游戏", [this] (TextButton &) {
		m_stateStack->push<WorldSelectionState>(this);
	});

	m_menuWidget.addButton("多人游戏", [this] (TextButton &) {
		m_stateStack->push<ServerConnectState>(this);
	});

	m_menuWidget.addButton("选项…", [this] (TextButton &) {
		m_stateStack->push<SettingsMenuState>(this);
	});

	m_menuWidget.addButton("退出", [this] (TextButton &) {
		m_stateStack->pop();
	});

	m_titleText.setScale(Config::guiScale * 4.f, Config::guiScale * 4.f);
	m_titleText.setString(APP_NAME);
	m_titleText.updateVertexBuffer();

	m_versionText.setString("gamma");
	m_versionText.setScale(Config::guiScale, Config::guiScale);
	m_versionText.updateVertexBuffer();

	// 版权与许可协议两行文字已按需求从标题页移除

	updateWidgetPosition();
}

TitleScreenState::~TitleScreenState() {
	if (m_thread.joinable())
		m_thread.join();
}

void TitleScreenState::init() {
	m_eventHandler->addListener<GuiScaleChangedEvent>(&TitleScreenState::onGuiScaleChanged, this);
}

void TitleScreenState::onEvent(const SDL_Event &event) {
	InterfaceState::onEvent(event);

	if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
		if (!m_stateStack->empty() && &m_stateStack->top() != this)
			m_menuWidget.onEvent(event);
	}

	if (!m_stateStack->empty() && &m_stateStack->top() == this) {
		// 名称输入框先处理：它需要吃掉字符/回车/退格事件，
		// 避免同时被菜单当成快捷键。
		m_usernameInput.onEvent(event);

		m_menuWidget.onEvent(event);

		// 输入框失去焦点时，把内容同步进 Config（下次启动仍保留）
		if (event.type == SDL_MOUSEBUTTONDOWN) {
			if (!m_usernameInput.isPointInWidget(event.button.x, event.button.y))
				m_usernameInput.setFocus(false);
		}

		if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RETURN) {
			// 回车：把名字写入 Config
			Config::defaultUsername = sanitizedUsername();
		}
	}
}

void TitleScreenState::startSingleplayer(bool showLoadingState, const std::string &worldName, s32 seed) {
	GameConfig::worldName = worldName;

	// 把标题页输入的名字固化下来（空则给默认名），单人/多人共用
	Config::defaultUsername = sanitizedUsername();

	auto &game = m_stateStack->push<GameState>();
	game.setSingleplayer(true);

	auto &serverLoadingState = m_stateStack->push<ServerLoadingState>(game, showLoadingState, "localhost", sf::Socket::AnyPort, this);
	serverLoadingState.setUsername(Config::defaultUsername);

	if (m_thread.joinable())
		m_thread.join();

	LogLevel logLevel = LoggerHandler::getInstance().maxLevel();
	m_thread = std::thread([this, worldName, logLevel, seed] () {
		ServerApplication app{*m_eventHandler};

		if (!worldName.empty())
			app.setWorldName(worldName);

		app.setLogLevel(logLevel);
		app.setSingleplayer(true);
		app.setPort(sf::Socket::AnyPort);
		app.setSeed(seed);
		app.run();
	});
}

void TitleScreenState::startMultiplayer(const std::string &host) {
	Config::defaultUsername = sanitizedUsername();

	auto &game = m_stateStack->push<GameState>();
	auto &serverLoadingState = m_stateStack->push<ServerLoadingState>(game, false, host, m_port, this);
	serverLoadingState.setUsername(Config::defaultUsername);
}

// 读取输入框里的名字并做校验：只允许字母/数字/下划线（与服务端正则一致），
// 空或非法时回退到 "Player"。
std::string TitleScreenState::sanitizedUsername() {
	std::string name = m_usernameInput.string();

	// 去掉首尾空白
	const char *ws = " \t\r\n";
	size_t begin = name.find_first_not_of(ws);
	size_t end = name.find_last_not_of(ws);
	name = (begin == std::string::npos) ? "" : name.substr(begin, end - begin + 1);

	bool valid = !name.empty();
	for (char c : name) {
		if (!(std::isalnum((unsigned char)c) || c == '_')) {
			valid = false;
			break;
		}
	}
	if (!valid)
		name = "Player";

	// 同步回输入框，让玩家看到实际使用的名字
	if (m_usernameInput.string() != name) {
		m_usernameInput.setString(name);
	}

	return name;
}

void TitleScreenState::applyInputScale(float guiScale) {
	// 背景框与内边距是在 Text 的**局部坐标系**里给的，绘制时会再乘一次
	// TextInput 的缩放（guiScale）。所以这里要除以 guiScale，
	// 让最终渲染出来正好是 INPUT_WIDTH x INPUT_HEIGHT 像素，
	// 否则输入框会被放大 3 倍（450x60），既挡标题又抢点击区域。
	const float s = std::max(1.f, guiScale);

	m_usernameInput.setBackgroundSize((u32)(INPUT_WIDTH / s), (u32)(INPUT_HEIGHT / s));
	m_usernameInput.setBackgroundOutline(1, Color::White);
	m_usernameInput.setPadding((int)(INPUT_PADDING_X / s), (int)(INPUT_PADDING_Y / s));
	m_usernameInput.setScale(guiScale, guiScale);
}

void TitleScreenState::onGuiScaleChanged(const GuiScaleChangedEvent &event) {
	m_menuWidget.setScale(event.guiScale, event.guiScale);
	m_menuWidget.onGuiScaleChanged(event);

	m_titleText.setScale(event.guiScale * 4.f, event.guiScale * 4.f);
	m_versionText.setScale(event.guiScale, event.guiScale);
	// 输入框必须一起重设，否则改 GUI 缩放后回到标题页输入框会错乱
	applyInputScale(event.guiScale);
	// （版权/许可文字已移除，无需再设置缩放）

	updateWidgetPosition();
}

void TitleScreenState::updateWidgetPosition() {
	m_background.setPosition(
		roundf(Config::screenWidth / 2.0f - m_background.width() / 2.0f),
		roundf(Config::screenHeight / 2.0f - m_background.height() / 2.0f)
	);

	m_menuWidget.setPosition(
		roundf(Config::screenWidth / 2.0f - m_menuWidget.getGlobalBounds().sizeX / 2.0f),
		roundf(Config::screenHeight / 2.0f - m_menuWidget.getGlobalBounds().sizeY / 2.0f)
	);

	// =========================================================================
	//  标题页垂直布局：标题 -> 玩家名输入框 -> 菜单按钮，三者依次排列且**互不重叠**。
	//
	//  之前的问题（用户报告「输入玩家名被标题挡住」）：
	//   1. 标题用的是 guiScale*4 的缩放，实测渲染高度 156px（1280x720 / guiScale 3），
	//      而代码里用固定的「24 * guiScale = 72」去估算标题底部 —— 严重低估，
	//      输入框因此被放到了标题文字上。
	//   2. 菜单是**垂直居中**的，菜单顶部（218）比标题底部（242）还高，
	//      也就是说标题本身就压在「单人游戏」按钮上 —— 就算没有输入框也是错的。
	//   3. 输入框的 setBackgroundSize(150,20) 会被 guiScale 再乘一次，
	//      实际渲染成 450x60（比菜单按钮还宽），视觉与点击区域都过大。
	//
	//  现在的做法：全部用**实测尺寸**（getSize/getGlobalBounds）算位置，
	//  并让整组内容在屏幕上垂直居中（而不是各元素各自居中）。
	// =========================================================================
	const float gap = 12.f * (float)Config::guiScale;

	// 1) 标题：先定位，才能知道它的真实底部
	const float titleY = roundf(0.04f * Config::screenHeight * Config::guiScale);
	m_titleText.setPosition(
		roundf(Config::screenWidth / 2.0f - (float)m_titleText.getSize().x * m_titleText.getScale().x / 2.0f),
		titleY
	);
	const float titleHeight = (float)m_titleText.getSize().y * m_titleText.getScale().y;
	const float titleBottom = titleY + titleHeight;

	// 2) 玩家名输入框：紧跟标题下方
	const float inputY = titleBottom + gap;
	m_usernameInput.setPosition(
		roundf(Config::screenWidth / 2.0f - INPUT_WIDTH / 2.f),
		roundf(inputY)
	);
	const float inputBottom = inputY + INPUT_HEIGHT;

	// 3) 菜单：紧跟输入框下方（原来垂直居中，会压到标题）
	const float menuHeight = m_menuWidget.getGlobalBounds().sizeY;
	m_menuWidget.setPosition(
		roundf(Config::screenWidth / 2.0f - m_menuWidget.getGlobalBounds().sizeX / 2.0f),
		roundf(inputBottom + gap)
	);

	// 布局自检（只打一次）：标题/输入框/菜单的竖直区间必须互不重叠。
	// 一旦回归（有人改了缩放或加了控件），日志会直接指出是谁压住了谁。
	{
		static bool reported = false;
		if (!reported) {
			reported = true;
			const float titleBottomPx = titleBottom;
			const float inputBottomPx = inputBottom;
			const float menuTopPx = m_menuWidget.getPosition().y;
			const float menuBottomPx = menuTopPx + menuHeight;

			logInfo() << "TITLE_LAYOUT title=" << titleY << ".." << titleBottomPx
				<< " input=" << inputY << ".." << inputBottomPx
				<< " menu=" << menuTopPx << ".." << menuBottomPx
				<< " (screen " << Config::screenWidth << "x" << Config::screenHeight
				<< ", guiScale " << Config::guiScale << ")";

			if (titleBottomPx > inputY + 0.5f || inputBottomPx > menuTopPx + 0.5f
			|| menuBottomPx > Config::screenHeight + 0.5f) {
				logWarning() << "Title screen layout overlaps: title/input/menu are not disjoint!";
			}
		}
	}

	m_versionText.setPosition(Config::guiScale, float(Config::screenHeight - m_versionText.getSize().y * Config::guiScale));
	// （版权/许可文字已移除，无需再定位）
}
void TitleScreenState::draw(RenderTarget &target, RenderStates states) const {
	prepareDraw(target, states);

	target.draw(m_background, states);

	target.draw(m_titleText, states);
	target.draw(m_versionText, states);

	target.draw(m_usernameInput, states);
	target.draw(m_menuWidget, states);
}
