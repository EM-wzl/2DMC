# 2DMC overlay port: bgfx
#
# 目的：保留 2DMC 对 bgfx 系（bgfx / bx / bimg / glslang / spirv-tools）的
#       MinGW + g++ 16 补丁语义，同时让 MSVC / Clang 走正常路径。
#
# 原始补丁位置（vendored 时代）：
#   gamma/external/CMakeLists.txt 第 12-19 行
#     if(NOT MSVC)
#         add_compile_options(-include stdint.h)
#     endif()
#   原因：bgfx 内置的 2019 年版 3rdparty（glslang / spirv-tools /
#         glsl-optimizer）使用 uint32_t 等类型却没 include <stdint.h>。
#         老版 libstdc++ 会间接引入，g++ 16 不会 → 编译失败。
#
# vcpkg 化之后，这个补丁不再需要打进 upstream 源码：
#   - vcpkg 的 bgfx port 已经按其自身的构建脚本处理 3rdparty；
#   - 若仍需要，下面的 patch 会在 CMake 层注入同样的 flag（仅 GCC/Clang）。
#
# 注意：本 port 使用 vcpkg 官方 bgfx 作为基底，仅叠加 2DMC 补丁。

set(VCPKG_POLICY_EMPTY_INCLUDE_FOLDER enabled)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO bkaradzic/bgfx
    REF "v1.129.8945"
    SHA512 0  # 占位：接入 vcpkg 时由 `vcpkg install` 报出的实际哈希替换
    HEAD_REF master
)

# 2DMC 补丁：g++ 16 需要强制 include <stdint.h>（仅 GCC/Clang）
if(NOT VCPKG_TARGET_IS_WINDOWS OR VCPKG_TARGET_IS_MINGW)
    # 交由 2dmc-toolchain-fixes patch 处理，见 patch/ 目录
    list(APPEND PATCHES
        "0001-force-include-stdint-for-gcc16.patch"
    )
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBGFX_BUILD_EXAMPLES=OFF
        -DBGFX_BUILD_TOOLS=ON
        -DBGFX_INSTALL=ON
)

vcpkg_cmake_install()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
