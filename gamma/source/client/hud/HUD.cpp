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

#include <cmath>
#include "common/core/GameClock.hpp"
#include "common/event/Events.hpp"
#include "common/scene/component/HealthComponent.hpp"
#include "common/scene/component/PositionComponent.hpp"

#include "client/core/Config.hpp"
#include "client/core/ClientProfiler.hpp"
#include "client/graphics/Camera.hpp"
#include "client/hud/HUD.hpp"
#include "client/network/Client.hpp"
#include "client/network/ClientCommandHandler.hpp"
#include "client/world/ClientPlayer.hpp"

HUD::HUD(ClientPlayer &player, ClientWorld &world, ClientCommandHandler &client)
	: m_player(player), m_world(world), m_clientCommandHandler(client),
	m_hotbar(player, client),
	m_blockCursor(player, world, client),
	m_debugOverlay(player, world),
	m_chat(client.client()),
	m_debugLightmapViewer(player)
{
	setScale(Config::guiScale, Config::guiScale, 1);

	m_shader.loadFromFile("basic");

	setup();
}

void HUD::setup() {
	m_view.setSize((float)Config::screenWidth, (float)Config::screenHeight);
	m_view.setCenter((float)Config::screenWidth / 2.f, (float)Config::screenHeight / 2.f);

	// GUI 缩放可能变了：名字标签的 Text 是用旧 guiScale 创建并 setScale 的，
	// 必须清空让 updatePlayerNameLabels() 用新缩重重建（否则位置/字号错乱）。
	m_playerNameLabels.clear();

	m_hotbar.setPosition(
		Config::screenWidth / getScale().x / 2.f - (float)m_hotbar.width() / 2.f,
		Config::screenHeight / getScale().y - (float)m_hotbar.height()
	);

	m_blockInfoWidget.setPosition(Config::screenWidth / getScale().x / 2.f - (float)m_blockInfoWidget.width() / 2.f, 2.f);

	m_fpsText.setPosition(Config::screenWidth / getScale().x - 36.f, 2.f);

	m_healthText.setPosition(2.f, 12.f);
	m_healthText.setColor(Color::White);

	m_mobHealthText.setPosition(Config::screenWidth / getScale().x / 2.f - 40.f, 28.f);
	m_mobHealthText.setColor(Color::Yellow);

	m_crosshair.setup();

	m_chat.setPosition(2.f, Config::screenHeight / Config::guiScale - 50.f);

	m_minimap.setPosition(float(Config::screenWidth / Config::guiScale - Minimap::minimapSize) - 15.f, 15.f);

	m_debugLightmapViewer.setPosition(0.f, float(Config::screenHeight / Config::guiScale - DebugLightmapViewer::totalSize));

	m_debugProfilerWindow.setPosition(2.f, 2.f);
}

void HUD::onEvent(const SDL_Event &event) {
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F3) {
		m_isDebugOverlayVisible ^= 1;
		m_printRendererInfo = event.key.keysym.mod & KMOD_SHIFT;
	}

	if (Config::isHotbarVisible)
		m_hotbar.onEvent(event);

	// 左键攻击：准星指向生物时打它（BlockCursor 只处理方块，这里单独处理实体）
	// 注意：m_mobHealthTarget 是客户端本地 entt 句柄，可能已被 EntityDespawn
	// 销毁（HUD::update 每帧才刷新一次，事件可能在两帧之间到达）。这里先校验
	// 句柄仍然有效，避免把失效句柄当网络 ID 发出去。
	if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
	&& m_mobHealthTarget != entt::null
	&& m_world.scene().registry().valid(m_mobHealthTarget))
		m_clientCommandHandler.sendPlayerAttackEntity(m_mobHealthTarget);

	m_blockCursor.onEvent(event, m_hotbar);

	if (Config::isChunkMinimapEnabled)
		m_minimap.onEvent(event);

	if (Config::isProfilerWindowEnabled)
		m_debugProfilerWindow.onEvent(event);
}

void HUD::onGuiScaleChanged(const GuiScaleChangedEvent &event) {	setScale(event.guiScale, event.guiScale, 1);

	setup();
}

void HUD::update() {
	// FIXME: Shouldn't be called every tick
	if (Config::isHotbarVisible)
		m_hotbar.update();

	if (Config::isFpsCounterEnabled)
		m_fpsText.setString(std::to_string(GameClock::getInstance().getFpsAverage()) + " FPS");

	m_healthText.setString("HP " + std::to_string(m_player.health()) + "/" + std::to_string(m_player.maxHealth()));
	m_healthText.setColor(m_player.health() <= 6 ? Color::Red : Color::White);

	// 准星指向的生物血量（最近且在视线夹角内、6 格以内；无目标时隐藏）
	updateMobHealthText();

	// 其他玩家头顶名字（多人游戏）
	updatePlayerNameLabels();

	m_blockCursor.update(m_hotbar);

	if (m_isDebugOverlayVisible)
		m_debugOverlay.update(m_printRendererInfo);
	else if (Config::isProfilerWindowEnabled)
		m_debugProfilerWindow.update();

	if (Config::isBlockInfoWidgetEnabled) {
		m_blockInfoWidget.update();

		if (m_blockCursor.currentBlock() != m_blockInfoWidget.currentBlock())
			m_blockInfoWidget.setCurrentBlock(m_blockCursor.currentBlock());
	}

	if (Config::isChunkMinimapEnabled)
		m_minimap.update(m_player, m_world);

	if (Config::isLightmapViewerEnabled)
		m_debugLightmapViewer.update(m_world);
}

// 找到准星指向的生物：视线方向夹角足够小、距离 6 格内、且有 HealthComponent。
// 找到后更新 m_mobHealthText 并记录目标（供左键攻击用）。
void HUD::updateMobHealthText() {
	auto &registry = m_world.scene().registry();

	// 视线方向（ClientPlayer::updateCamera 已经算好 m_forwardDir）
	float fx = m_player.dirTargetedX();
	float fy = m_player.dirTargetedY();
	float fz = m_player.dirTargetedZ();

	entt::entity best = entt::null;
	double bestDistance = 36.0; // 6 格平方

	registry.view<PositionComponent, HealthComponent>().each([&](auto entity, auto &position, auto &health) {
		// 血量归零 = 生物已死（服务端紧接着会发 EntityDespawn）。
		// 这里必须排除：否则在 EntityHealth(0) 与 EntityDespawn 之间会渲染出
		// "Mob HP 0/20" 的一帧，且 m_mobHealthTarget 仍指向即将销毁的实体，
		// 用户看到的就是"血量停在 0/1 不动"的残留表现。
		if (health.maxHealth == 0 || health.health == 0) return;

		double dx = position.x - m_player.x();
		double dy = position.y - m_player.y();
		double dz = position.z - m_player.z();
		double distance = dx * dx + dy * dy + dz * dz;
		if (distance > bestDistance || distance < 0.0001) return;

		// 归一化后与视线点积 > 0.9（约 25° 锥角），足够选中准星附近的目标
		double len = std::sqrt(distance);
		double dot = (dx * fx + dy * fy + dz * fz) / len;
		if (dot < 0.9) return;

		bestDistance = distance;
		best = entity;
	});

	if (best != entt::null) {
		auto &health = registry.get<HealthComponent>(best);
		m_mobHealthText.setString("Mob HP " + std::to_string(health.health) + "/" + std::to_string(health.maxHealth));
		m_mobHealthText.setColor(health.hurtTimer > 0 ? Color::Red : Color::Yellow);
		m_mobHealthTarget = best;
	}
	else {
		m_mobHealthText.setString("");
		m_mobHealthTarget = entt::null;
	}
}

// 把其他玩家头顶的世界坐标投影到屏幕，生成/更新名字标签。
//
// 投影用相机参数手工构造：相机看向 -Z（OpenGL 约定），
//   clip = projection * view * worldPos
// 再除以 w 得到 NDC，最后映射到屏幕像素。
// 只显示 32 格内、且在相机前方的玩家。
void HUD::updatePlayerNameLabels() {
	const auto &players = m_clientCommandHandler.client().players();

	Camera &camera = m_player.camera();
	const Vector3d &camPos = camera.getDPosition();

	const float fov = camera.getFieldOfView();
	const float aspect = camera.getAspectRatio();
	const float nearP = camera.getNearClippingPlane();
	const float farP = camera.getFarClippingPlane();

	// 相机朝向（单位向量）
	Vector3f forward = camera.getDirection();
	Vector3f up = camera.getUpVector();
	// right = forward × up
	Vector3f right{
		forward.y * up.z - forward.z * up.y,
		forward.z * up.x - forward.x * up.z,
		forward.x * up.y - forward.y * up.x
	};

	const float tanHalfFov = std::tan(fov * 0.5f * 3.14159265358979f / 180.f);

	// 需要显示的 clientID 集合（用于清理已离开的玩家）
	std::vector<u16> visible;

	for (const auto &it : players) {
		u16 clientID = it.first;
		const Player &p = it.second;

		// 跳过自己：GameState 会把本地玩家也 addPlayer 进来，
		// 不排除的话会在自己头顶（屏幕中心）画一条名字，遮挡视野。
		if (clientID == m_clientCommandHandler.client().id())
			continue;

		// 相对相机的坐标
		float rx = (float)(p.x() - camPos.x);
		float ry = (float)(p.y() - camPos.y);
		float rz = (float)(p.z() - camPos.z);

		float distance = std::sqrt(rx * rx + ry * ry + rz * rz);
		if (distance > 32.f || distance < 0.001f)
			continue;

		// 相机空间
		float camX = rx * right.x + ry * right.y + rz * right.z;
		float camY = rx * up.x + ry * up.y + rz * up.z;
		float camZ = rx * forward.x + ry * forward.y + rz * forward.z; // 前方为正

		if (camZ <= nearP || camZ >= farP)
			continue;

		// 透视投影到 NDC
		float ndcX = camX / (camZ * tanHalfFov * aspect);
		float ndcY = camY / (camZ * tanHalfFov);

		if (ndcX < -1.f || ndcX > 1.f || ndcY < -1.f || ndcY > 1.f)
			continue;

		// 头顶：玩家脚底位置 + 约 1.9 格
		float headOffset = 1.9f;
		// 注意：HUD 的 m_view 尺寸就是 Config::screenWidth/Height（像素空间），
		// 而 HUD 自身又 setScale(guiScale)。所以这里的屏幕坐标必须用**像素**，
		// 不能再除以 guiScale（否则会双重缩放，名字跑到屏幕外）。
		float screenX = (ndcX * 0.5f + 0.5f) * (float)Config::screenWidth;
		float screenY = (1.f - (ndcY * 0.5f + 0.5f)) * (float)Config::screenHeight;

		// 头顶偏移（世界 +1.9 格 → 屏幕约上移，按距离衰减）
		screenY -= headOffset * (float)Config::screenHeight / (camZ * tanHalfFov * 2.f);

		visible.push_back(clientID);

		// 找到或创建标签
		PlayerNameLabel *label = nullptr;
		for (auto &l : m_playerNameLabels) {
			if (l->clientID == clientID) { label = l.get(); break; }
		}
		if (!label) {
			auto newLabel = std::make_unique<PlayerNameLabel>();
			newLabel->clientID = clientID;
			newLabel->text = std::make_unique<Text>();
			newLabel->text->setScale(Config::guiScale, Config::guiScale);
			newLabel->text->setColor(Color::White);
			label = newLabel.get();
			m_playerNameLabels.emplace_back(std::move(newLabel));
		}

		std::string name = p.name().empty() ? ("Player" + std::to_string(clientID)) : p.name();
		if (label->text->string() != name)
			label->text->setString(name);

		// 水平居中：粗略按字符数估宽（字体等宽，约 6px/字符 * guiScale）
		float textWidth = (float)name.size() * 6.f * (float)Config::guiScale;
		label->text->setPosition(
			std::round(screenX - textWidth * 0.5f),
			std::round(screenY)
		);

		// 距离越远越透明（32 格内线性淡出）
		// 注意：Color 是 float 0-1 语义，必须用 fromRGBA32 构造，
		// 直接写 Color{255,255,255,alpha} 会让 RGB=255.0（严重过曝）。
		u8 alpha = (u8)std::max(0.f, std::min(255.f, 255.f * (1.f - (distance - 16.f) / 16.f)));
		label->text->setColor(Color::fromRGBA32(255, 255, 255, alpha));
	}

	// 清理已不可见/已离开的标签
	for (auto it = m_playerNameLabels.begin(); it != m_playerNameLabels.end(); ) {
		bool still = false;
		for (u16 id : visible) {
			if (id == (*it)->clientID) { still = true; break; }
		}
		if (!still)
			it = m_playerNameLabels.erase(it);
		else
			++it;
	}
}

void HUD::draw(RenderTarget &target, RenderStates states) const {	OM_PROFILE_START("HUD::draw");

	target.setView(m_view);

	states.shader = &m_shader;

	states.transform *= getTransform();

	if (m_isDebugOverlayVisible)
		target.draw(m_debugOverlay, states);
	else if (Config::isProfilerWindowEnabled)
		target.draw(m_debugProfilerWindow, states);

	if (Config::isBlockInfoWidgetEnabled)
		target.draw(m_blockInfoWidget, states);

	if (Config::isHotbarVisible)
		target.draw(m_hotbar, states);

	if (Config::isFpsCounterEnabled)
		target.draw(m_fpsText, states);

	target.draw(m_healthText, states);
	target.draw(m_mobHealthText, states);

	// 其他玩家头顶名字（最后画，保证在最上层）
	for (const auto &label : m_playerNameLabels) {
		if (label && label->text)
			target.draw(*label->text, states);
	}

	if (Config::isChunkMinimapEnabled)
		target.draw(m_minimap, states);

	if (Config::isLightmapViewerEnabled)
		target.draw(m_debugLightmapViewer, states);

	target.draw(m_chat, states);

	states.transform = Transform::Identity;

	if (Config::isCrosshairVisible)
		target.draw(m_crosshair, states);

	OM_PROFILE_END("HUD::draw");
}
