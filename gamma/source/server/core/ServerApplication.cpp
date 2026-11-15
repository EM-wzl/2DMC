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
#include <filesystem.hpp>

#include "common/core/BlockGeometry.hpp"
#include "common/core/GameTime.hpp"
#include "common/event/Events.hpp"

#include "server/core/ServerApplication.hpp"
#include "server/core/ServerConfig.hpp"
#include "server/world/ServerBlock.hpp"

namespace fs = ghc::filesystem;

static bool hasBeenInterrupted = false;

#ifdef SFML_SYSTEM_LINUX

#include <stdio.h>
#include <signal.h>
#include <cstdlib>

static void sigintHandler(int) {
	signal(SIGINT, sigintHandler);
	hasBeenInterrupted = true;
}

#endif // SFML_SYSTEM_LINUX

ServerApplication::ServerApplication(int argc, char **argv) : m_argumentParser(argc, argv) {
#ifdef SFML_SYSTEM_LINUX
	signal(SIGINT, sigintHandler);
#endif

	m_worldName = "_server";
}

ServerApplication::ServerApplication(EventHandler &eventHandler) {
	m_eventHandler = &eventHandler;
}

bool ServerApplication::init() {
	std::srand((unsigned int)std::time(nullptr));

	LoggerHandler::setInstance(m_loggerHandler);
	m_loggerHandler.setName("server");

	fs::create_directory("logs");
	m_loggerHandler.openLogFile("logs/openminer_server.log");

	BlockGeometry::initOrientation();

	m_argumentParser.addArgument("port", {"-p", "--port", "Select the port to use.", "port"});
	m_argumentParser.addArgument("world", {"-w", "--world", "Select a world to use.", "world"});
	m_argumentParser.addArgument("working-dir", {"-d", "--working-dir", "Change the working directory to <dir>.", "dir"});
	m_argumentParser.addArgument("log-level", {"-l", "--log-level", "Choose the log level (debug, info, warning, error)", "level"});

	m_argumentParser.parse();

	if (m_argumentParser.getArgument("help").isFound)
		return false;

	if (m_argumentParser.getArgument("working-dir").isFound) {
		try {
			fs::current_path(m_argumentParser.getArgument("working-dir").parameter);
		}
		catch (fs::filesystem_error &e){
			logError() << "Failed to change working directory:" << e.what();
			return false;
		}
	}

	if (m_argumentParser.getArgument("port").isFound)
		m_port = (u16)std::stoi(m_argumentParser.getArgument("port").parameter);

	if (m_argumentParser.getArgument("world").isFound)
		m_worldName = m_argumentParser.getArgument("world").parameter;

	if (m_argumentParser.getArgument("log-level").isFound) {
		std::unordered_map<std::string, LogLevel> levels = {
			{"debug",   LogLevel::Debug},
			{"info",    LogLevel::Info},
			{"warning", LogLevel::Warning},
			{"error",   LogLevel::Error},
		};

		auto it = levels.find(m_argumentParser.getArgument("log-level").parameter);
		if (it != levels.end())
			m_loggerHandler.setMaxLevel(it->second);
		else
			logWarning() << ("Failed to set log level to '" + m_argumentParser.getArgument("log-level").parameter + "': Invalid value").c_str();
	}

	ServerConfig::loadConfigFromFile("config/server.lua");

	m_server.init(m_port);
	m_server.setRunning(true);

	Registry::setInstance(m_registry);

	BlockState &blockState = m_registry.registerBlock<ServerBlock>(BLOCK_AIR).getState(0);
	blockState.label("Air");
	blockState.isOpaque(false);
	blockState.isCollidable(false);
	m_registry.registerItem<Item>({}, BLOCK_AIR, "Air").setIsBlock(true);

	m_modLoader.loadMods();

	m_serverCommandHandler.setupCallbacks();

	m_worldController.setServer(m_serverCommandHandler);
	m_worldController.init(m_players, m_seed);

	m_scriptEngine.luaCore().setRegistry(&m_registry);

	logInfo() << ("Server is running on localhost:" + std::to_string(m_server.port())).c_str();

	// 监听地址是 0.0.0.0（SFML TcpListener::listen 默认 IpAddress::Any），
	// 所以局域网内其它机器可以直接连。把可用地址打印到日志，方便玩家告知朋友；
	// 游戏内也可以随时用 /publish（别名 /openlan、/lan）重新查询。
	{
		std::vector<std::string> addresses = Server::localIPv4Addresses();
		for (const std::string &address : addresses)
			logInfo() << ("LAN: " + address + ":" + std::to_string(m_server.port())).c_str();
	}

	// 加载存档。损坏/被截断的存档不能让整个服务端崩溃退出：
	// 记录错误后以「空世界」继续运行（玩家仍可进入并重新生成地形）。
	if (!m_worldName.empty()) {
		try {
			m_worldController.load(m_worldName);
		}
		catch (const std::exception &e) {
			logError() << "Failed to load world '" << m_worldName << "': " << e.what()
				<< " (starting with an empty world instead)";
		}
		catch (...) {
			logError() << "Failed to load world '" << m_worldName
				<< "': unknown exception (starting with an empty world instead)";
		}
	}

	// TEMP: 流体验证自动测试钩子（2DMC_AUTOTEST=1 时启用，验证后移除）
	if (std::getenv("2DMC_AUTOTEST"))
		m_autotestCountdown = 500;

	if (m_eventHandler)
		m_eventHandler->emplaceEvent<ServerOnlineEvent>(true, m_server.port());

	return true;
}

int ServerApplication::run(bool isProtected) {
	bool isInitSuccessful = false;
	if (isProtected) {
		try {
			if ((isInitSuccessful = init()))
				mainLoop();
		}
		catch(const Exception &e) {
			if (m_eventHandler)
				m_eventHandler->emplaceEvent<ServerOnlineEvent>(false, (u16)0);

			logError() << "Fatal error" << e.what();

			ServerConfig::saveConfigToFile("config/server.lua");
			ServerConfig::options.clear();
			ServerConfig::userOptions.clear();

			m_serverCommandHandler.sendServerClosed(std::string("Server error ") + e.what());

			if (!m_server.isSingleplayer()) // Needed to prevent a crash when using '/stop'
				m_registry.clear();

			m_worldController.clearEntities();

			return 1;
		}
	}
	else {
		if ((isInitSuccessful = init()))
			mainLoop();
	}

	if (isInitSuccessful) {
		logInfo() << "Stopping server...";

		if (!m_worldName.empty()) {
			m_worldController.save(m_worldName);
		}

		ServerConfig::saveConfigToFile("config/server.lua");
		ServerConfig::options.clear();
			ServerConfig::userOptions.clear();

		m_serverCommandHandler.sendServerClosed("Server closed.");

		m_server.disconnectAllClients();

		m_registry.clear();
		m_worldController.clearEntities();

		return 0;
	}
	else
		return 1;
}

void ServerApplication::update() {
	static u64 lastTick = m_clock.getTicks() / 50;
	bool doTick = false;
	if (lastTick < m_clock.getTicks() / 50) {
		lastTick = m_clock.getTicks() / 50;
		doTick = true;

		GameTime::incrementTicks();

		m_serverCommandHandler.sendServerTick();
	}

	m_worldController.update(doTick);

	// 服务端 tick 事件：Lua 侧生物 AI/自然生成的主驱动（20tps）。
	// 放在世界更新之后，保证本 tick 的区块与实体状态已经就绪。
	if (doTick) {
		try {
			m_scriptEngine.luaCore().onEvent(LuaEventType::ServerTick, m_serverCommandHandler);
		}
		catch (const std::exception &e) {
			logError() << "[ServerTick] " << e.what();
		}
	}

	// TEMP: 流体验证自动测试触发（到点后调用 Lua 全局，验证后移除）
	if (m_autotestCountdown > 0 && !m_autotestDone) {
		--m_autotestCountdown;
		if (m_autotestCountdown == 0) {
			m_autotestDone = true;
			try {
				auto &world = m_worldController.getWorld(0);
				// 测试区在区块 (0,0,0)（x 0-15 / y 0-15 / z 0-31），先强制生成
				for (int cx = -1 ; cx <= 1 ; ++cx)
					for (int cy = -1 ; cy <= 0 ; ++cy)
						for (int cz = -1 ; cz <= 1 ; ++cz) {
							auto &chunk = world.getOrCreateChunk(cx, cy, cz);
							world.generateChunk(chunk);
						}
				logInfo() << "[AUTOTEST] chunks ready, calling autotest_server_run";
				m_scriptEngine.lua().script_file("mods/default/autotest.lua");
				sol::object r = m_scriptEngine.lua()["autotest_server_run"](&world);
				if (r.valid())
					logInfo() << "[FLUIDTEST] " << r.as<std::string>();
				std::fflush(nullptr);
			}
			catch (const std::exception &e) {
				logError() << e.what();
			}
		}
	}
	else if (m_autotestDone && std::getenv("2DMC_AUTOTEST")) {
		// TEMP: 每 20 帧采样一次测试区状态，结果写入 autotest_result.txt（验证后移除）
		static u64 fluidTestFrame = 0;
		if (++fluidTestFrame % 20 == 0) {
			try {
				sol::function fn = m_scriptEngine.lua()["autotest_server_tick"];
				if (fn.valid()) {
					auto &world = m_worldController.getWorld(0);
					sol::object r = fn(&world);
					if (r.valid())
						logInfo() << "[FLUIDTEST] " << r.as<std::string>();
				}
				std::fflush(nullptr);
			}
			catch (const std::exception &e) {
				logError() << "[AUTOTEST-TICK] " << e.what();
			}
		}
	}

	if (m_clock.getTicks() % 100 < 10) {
		for (auto &it : m_players) {
			if (it.second.isOnline()) {
				m_serverCommandHandler.sendPlayerPosUpdate(it.second.clientID());
				m_serverCommandHandler.sendPlayerRotUpdate(it.second.clientID());
			}
		}
	}
}

void ServerApplication::mainLoop() {
	while (!hasBeenInterrupted && m_server.isRunning()) {
		m_server.handleGameEvents();

		m_clock.updateGame([this] {
			update();
		});

		m_clock.waitForNextFrame();
	}
}
