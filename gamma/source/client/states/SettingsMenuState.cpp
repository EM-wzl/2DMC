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
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>

#include "common/core/ApplicationStateStack.hpp"
#include "common/core/Debug.hpp"
#include "common/core/EventHandler.hpp"
#include "common/core/Registry.hpp"
#include "common/event/Events.hpp"
#include "common/world/World.hpp"

#include "client/core/Config.hpp"
#include "client/core/KeyboardHandler.hpp"
#include "client/core/input/GamePad.hpp"
#include "client/states/SettingsMenuState.hpp"
#include "client/states/TexturePackSelectionState.hpp"

SettingsMenuState::SettingsMenuState(DrawableState *parent) : InterfaceState(parent) {
	m_background.setScale(Config::guiScale * 2.f, Config::guiScale * 2.f);

	m_filter1.setFillColor(Color::fromRGBA32(0, 0, 0, 192));
	m_filter2.setFillColor(Color::fromRGBA32(0, 0, 0, 120));

	m_title.setScale(Config::guiScale, Config::guiScale);
	m_title.setString("选项");
	m_title.updateVertexBuffer();

	m_menuWidget.setScale(Config::guiScale, Config::guiScale);

	m_doneButton.setScale(Config::guiScale, Config::guiScale);
	m_doneButton.setText("Done");
	m_doneButton.setCallback([this] (TextButton &) {
		doneButtonAction();
	});

	updateWidgetPosition();

	addMainButtons();
}

void SettingsMenuState::init() {
	m_eventHandler->addListener<GuiScaleChangedEvent>(&SettingsMenuState::onGuiScaleChanged, this);
}

void SettingsMenuState::onEvent(const SDL_Event &event) {
	InterfaceState::onEvent(event);

	if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
		if (&m_stateStack->top() != this)
			m_menuWidget.onEvent(event);
		else if (m_state == MenuState::Graphics && m_resolutionButton)
			m_resolutionButton->setText("分辨率：" + std::to_string(Config::screenWidth) + "x" + std::to_string(Config::screenHeight));
	}

	if (!m_stateStack->empty() && &m_stateStack->top() == this) {
		m_menuWidget.onEvent(event);
		m_doneButton.onEvent(event);

		if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
			doneButtonAction();
		}
		else if (m_currentKeyButton && event.type == SDL_KEYDOWN) {
			KeyboardHandler *keyboardHandler = dynamic_cast<KeyboardHandler *>(GamePad::getInputHandler());
			keyboardHandler->setKeycode(m_currentKey, event.key.keysym.sym);

			m_key->setKeycode(event.key.keysym.sym);
			m_key = nullptr;

			m_currentKeyButton->setText(m_currentKeyButton->text() + keyboardHandler->getKeyName(m_currentKey));
			m_currentKeyButton = nullptr;
		}
	}
}

void SettingsMenuState::onGuiScaleChanged(const GuiScaleChangedEvent &event) {
	m_background.setScale(event.guiScale * 2.f, event.guiScale * 2.f);
	m_title.setScale(event.guiScale, event.guiScale);

	m_menuWidget.setScale(event.guiScale, event.guiScale);
	m_doneButton.setScale(event.guiScale, event.guiScale);

	updateWidgetPosition();

	m_menuWidget.onGuiScaleChanged(event);
}

void SettingsMenuState::updateWidgetPosition() {
	m_background.setPosRect(0, 0,
		u16(Config::screenWidth / m_background.getScale().x),
		u16(Config::screenHeight / m_background.getScale().y)
	);

	m_background.setClipRect(0, 0,
		u16(Config::screenWidth / m_background.getScale().x),
		u16(Config::screenHeight / m_background.getScale().y)
	);

	m_filter1.setSize(Config::screenWidth, Config::screenHeight);

	const int topBorderSize = 25 * Config::guiScale;
	const int bottomBorderSize = 25 * Config::guiScale;
	m_filter2.setSize(Config::screenWidth, float(Config::screenHeight - topBorderSize - bottomBorderSize));
	m_filter2.setPosition(0, (float)topBorderSize);

	m_title.setPosition(
		roundf(Config::screenWidth / 2.0f - float(m_title.getSize().x * Config::guiScale) / 2.0f),
		roundf((float)topBorderSize / 2.0f - float(m_title.getSize().y * Config::guiScale) / 2.0f)
	);

	m_doneButton.setPosition(
		roundf(Config::screenWidth / 2.0f - m_doneButton.getGlobalBounds().sizeX / 2.0f),
		roundf(Config::screenHeight - (float)bottomBorderSize / 2.0f - m_doneButton.getGlobalBounds().sizeY / 2.0f)
	);

	m_menuWidget.setPosition(
		roundf(Config::screenWidth / 2.0f - m_menuWidget.getGlobalBounds().sizeX / 2.0f),
		roundf((float)topBorderSize + 5.0f * Config::guiScale)
	);
}

void SettingsMenuState::doneButtonAction() {
	if (m_state != MenuState::Main) {
		m_resolutionButton = nullptr;
		m_state = MenuState::Main;
		addMainButtons();
	} else {
		m_stateStack->pop();
	}
}

void SettingsMenuState::addMainButtons() {
	m_menuWidget.reset(1, 8);

	m_menuWidget.addButton("玩法…", [this] (TextButton &) {
		m_state = MenuState::Gameplay;
		addGameplayButtons();
	});

	m_menuWidget.addButton("界面…", [this] (TextButton &) {
		m_state = MenuState::Gameplay;
		addInterfaceButtons();
	});

	m_menuWidget.addButton("图形…", [this] (TextButton &) {
		m_state = MenuState::Graphics;
		addGraphicsButtons();
	});

	m_menuWidget.addButton("控制…", [this] (TextButton &) {
		m_state = MenuState::Input;
		addInputButtons();
	});

	m_menuWidget.addButton("调试…", [this] (TextButton &) {
		m_state = MenuState::Debug;
		addDebugButtons();
	});

	m_menuWidget.addButton("材质包…", [this] (TextButton &) {
		m_stateStack->push<TexturePackSelectionState>(this);
	});

	updateWidgetPosition();
}

void SettingsMenuState::addGameplayButtons() {
	m_menuWidget.reset(1, 8);

	addToggleButton("飞行模式", Config::isFlyModeEnabled, false);
	addToggleButton("穿墙模式", Config::isNoClipEnabled, false);

	m_menuWidget.addSlider("最大触及距离：" + std::to_string(Config::maxBlockReach), [] (SliderWidget &slider, u32) {
		Config::maxBlockReach = (u16)slider.getCurrentValue();
		slider.setText("最大触及距离：" + std::to_string(Config::maxBlockReach));
	}, 1, 15, Config::maxBlockReach);

	updateWidgetPosition();
}

void SettingsMenuState::addInterfaceButtons() {
	m_menuWidget.reset(1, 8);

	addToggleButton("显示方块信息", Config::isBlockInfoWidgetEnabled, false);
	addToggleButton("显示帧率", Config::isFpsCounterEnabled, false);
	addToggleButton("显示快捷栏", Config::isHotbarVisible, false);
	addToggleButton("显示准星", Config::isCrosshairVisible, false);

	updateWidgetPosition();
}

void SettingsMenuState::addGraphicsButtons() {
	m_menuWidget.reset(2, 8);

	m_menuWidget.addSlider("渲染距离：" + std::to_string(Config::renderDistance), [] (SliderWidget &slider, u32) {
		Config::renderDistance = (u16)slider.getCurrentValue();
		slider.setText("渲染距离：" + std::to_string(Config::renderDistance));
		World::isReloadRequested = true;
	}, 4, 16, Config::renderDistance);

	m_menuWidget.addButton(std::string("平滑光照：") + (Config::isSmoothLightingEnabled ? "开" : "关"), [&] (TextButton &button) {
		Config::isSmoothLightingEnabled = !Config::isSmoothLightingEnabled;
		button.setText(std::string("平滑光照：") + (Config::isSmoothLightingEnabled ? "开" : "关"));

		// FIXME: If AO is set to "lighting based" and smooth lighting is set to OFF
		//        it needs to set AO to Basic and update the button

		World::isReloadRequested = true;
	});

	const std::string aoValueNames[3] = {
		"关",
		"基础",
		"基于光照"
	};

	m_menuWidget.addButton(std::string("环境光遮蔽：") + aoValueNames[Config::ambientOcclusion], [&, aoValueNames] (TextButton &button) {
		Config::ambientOcclusion = u8((Config::ambientOcclusion + 1) % (Config::isSmoothLightingEnabled ? 3 : 2));
		button.setText(std::string("环境光遮蔽：") + aoValueNames[Config::ambientOcclusion]);

		World::isReloadRequested = true;
	});

	m_menuWidget.addSlider("界面缩放：" + std::to_string(Config::guiScale), [this] (SliderWidget &slider, u32 eventType) {
		slider.setText("界面缩放：" + std::to_string(slider.getCurrentValue()));
		if (eventType == SDL_MOUSEBUTTONUP) {
			Config::guiScale = (u8)slider.getCurrentValue();
			m_eventHandler->emplaceEvent<GuiScaleChangedEvent>(Config::guiScale);
		}
	}, 1, 3, Config::guiScale);

	m_resolutionButton = &m_menuWidget.addButton("分辨率：" + std::to_string(Config::screenWidth) + "x" + std::to_string(Config::screenHeight), [] (TextButton &button) {
		if (Config::isFullscreenModeEnabled) return;

		// FIXME: Find a better way to do this
		if (Config::screenWidth == 1600 && Config::screenHeight == 1050) {
			Config::screenWidth = 1280;
			Config::screenHeight = 720;
		}
		else if (Config::screenWidth == 1280 && Config::screenHeight == 720) {
			Config::screenWidth = 1920;
			Config::screenHeight = 1080;
		}
		else {
			Config::screenWidth = 1600;
			Config::screenHeight = 1050;
		}

		button.setText("分辨率：" + std::to_string(Config::screenWidth) + "x" + std::to_string(Config::screenHeight));
	});

	m_resolutionButton->setEnabled(!Config::isFullscreenModeEnabled);

	m_menuWidget.addButton(std::string("全屏：") + (Config::isFullscreenModeEnabled ? "开" : "关"), [&, aoValueNames] (TextButton &button) {
		Config::isFullscreenModeEnabled = !Config::isFullscreenModeEnabled;
		button.setText(std::string("全屏：") + (Config::isFullscreenModeEnabled ? "开" : "关"));

		m_resolutionButton->setEnabled(!Config::isFullscreenModeEnabled);
	});

	addToggleButton("垂直同步", Config::isVerticalSyncEnabled, false);

	m_menuWidget.addSlider("多级纹理：" + std::to_string(Config::mipmapLevels), [] (SliderWidget &slider, u32) {
		Config::mipmapLevels = (u8)slider.getCurrentValue();
		slider.setText("多级纹理：" + std::to_string(Config::mipmapLevels));
	}, 0, 4, Config::mipmapLevels);

	m_menuWidget.addSlider("视野：" + std::to_string((int)Config::cameraFOV), [] (SliderWidget &slider, u32) {
		Config::cameraFOV = (float)slider.getCurrentValue();
		slider.setText("视野：" + std::to_string((int)Config::cameraFOV));
	}, 45, 135, (int)Config::cameraFOV);

	addToggleButton("星空渲染", Config::isStarRenderingEnabled, false);

	addToggleButton("视角摆动", Config::isViewBobbingEnabled, false);

	updateWidgetPosition();
}

void SettingsMenuState::addInputButtons() {
	KeyboardHandler *keyboardHandler = dynamic_cast<KeyboardHandler *>(GamePad::getInputHandler());
	m_menuWidget.reset(2, u16((float)keyboardHandler->keyCount() / 2.f + 1.5f));

	for (auto &it : keyboardHandler->keys()) {
		m_menuWidget.addButton(it.second.name() + ": " + keyboardHandler->getKeyName(it.first), [this, it] (TextButton &button) {
			button.setText(it.second.name() + ": ");
			m_currentKey = (u16)it.first;
			m_currentKeyButton = &button;
			m_key = const_cast<Key *>(&it.second);
		});
	}

	m_menuWidget.addSlider("鼠标灵敏度：" + std::to_string(Config::mouseSensitivity), [] (SliderWidget &slider, u32) {
		Config::mouseSensitivity = (u8)slider.getCurrentValue();
		slider.setText("鼠标灵敏度：" + std::to_string(Config::mouseSensitivity));
	}, 4, 32, Config::mouseSensitivity);

	updateWidgetPosition();
}

void SettingsMenuState::addDebugButtons() {
	m_menuWidget.reset(1, 8);

	addToggleButton("显示区块小地图", Config::isChunkMinimapEnabled, false);
	addToggleButton("显示光照图查看器", Config::isLightmapViewerEnabled, false);
	addToggleButton("显示性能分析窗口", Config::isProfilerWindowEnabled, false);

#ifdef OM_DEBUG
	m_menuWidget.addButton(std::string("线框模式：") + (Config::isWireframeModeEnabled ? "开" : "关"), [&] (TextButton &button) {
		Config::isWireframeModeEnabled = !Config::isWireframeModeEnabled;
		button.setText(std::string("线框模式：") + (Config::isWireframeModeEnabled ? "开" : "关"));

		if (Config::isWireframeModeEnabled)
			bgfx::setDebug(BGFX_DEBUG_WIREFRAME);
		else
			bgfx::setDebug(0);
	});
#endif // OM_DEBUG

	updateWidgetPosition();
}

TextButton &SettingsMenuState::addToggleButton(const std::string &text, bool &configOption, bool worldReloadRequested) {
	return m_menuWidget.addButton(text + "：" + (configOption ? "开" : "关"), [=, &configOption] (TextButton &button) {
		configOption = !configOption;
		button.setText(text + "：" + (configOption ? "开" : "关"));

		if (worldReloadRequested)
			World::isReloadRequested = true;
	});
}

void SettingsMenuState::draw(RenderTarget &target, RenderStates states) const {
	prepareDraw(target, states);

	target.draw(m_background, states);
	target.draw(m_filter1, states);
	target.draw(m_filter2, states);

	target.draw(m_title, states);

	target.draw(m_menuWidget, states);
	target.draw(m_doneButton, states);

	target.disableView();
}
