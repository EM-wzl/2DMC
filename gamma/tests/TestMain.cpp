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
//------------------------------------------------------------------------------
// TestMain.cpp
//
// 测试可执行文件的入口。
//
// 问题：openminer_common 传递链接了 SDL2main（Windows 上 SDL2 的标准做法），
//       而 SDL2main 要求提供 `SDL_main` 符号，否则链接失败：
//         undefined reference to `SDL_main'
// 解决：这里显式提供 SDL_main，转调 Catch2 的 Session。
//       这样既满足 SDL2main，又能用 Catch2 的断言/命令行参数。
//
// 注意：只在 Windows/MinGW 上需要；其它平台 SDL2main 不参与链接，
//       但定义 SDL_main 也无害，所以不加平台条件。
//------------------------------------------------------------------------------
#define CATCH_CONFIG_RUNNER
#include <catch2/catch_session.hpp>

#if defined(SDL_MAIN_NEEDED) || defined(SDL_MAIN_AVAILABLE)
#	include <SDL.h>
#endif

int main(int argc, char *argv[]) {
	return Catch::Session().run(argc, argv);
}

// SDL2main 在 Windows 上会调用 SDL_main；把参数转给上面的 main。
extern "C" int SDL_main(int argc, char *argv[]) {
	return main(argc, argv);
}
