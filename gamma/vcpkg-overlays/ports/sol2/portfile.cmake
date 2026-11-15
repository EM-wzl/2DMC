# 2DMC overlay port: sol2
#
# 目的：保留 2DMC 对 sol2 的 MinGW-only 补丁，同时让 MSVC 走上游原版。
#
# 原始补丁位置（vendored 时代）：
#   gamma/external/sol2/include/sol/optional_implementation.hpp:2186
#
#   本地补丁（MinGW/g++ 16）：引用特化里没有 construct()，原实现
#   this->construct(...) 无法编译，于是把上游的
#       template <class... Args> T& emplace(Args&&...)
#   换成了
#       template <class U = T, detail::enable_if_t<...>* = nullptr> T& emplace(U&&)
#
#   这个替换在 GCC 下 OK，但 MSVC 解析 optional<T&> 的
#   detail::enable_if_t<...> 默认模板实参时报 C2061，并连带触发引用联合体等
#   7 个错误。因此必须加平台守卫：
#       #if defined(__MINGW32__)  <本地补丁>  #else  <上游原版>  #endif
#
# 注意：当前 vendored 版本已经手工加好了这个守卫（见
# gamma/external/sol2/include/sol/optional_implementation.hpp:2186）。
# 本 overlay port 的作用是把这个补丁固化下来，避免接入 vcpkg 后丢失。

set(VCPKG_POLICY_EMPTY_INCLUDE_FOLDER enabled)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO ThePhD/sol2
    REF "v3.2.3"
    SHA512 0  # 占位：接入 vcpkg 时由 `vcpkg install` 报出的实际哈希替换
    HEAD_REF develop
    PATCHES
        "0001-guard-mingw-only-emplace-patch.patch"
)

set(SOL2_BUILD_LUA OFF CACHE BOOL "")

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DSOL2_BUILD_LUA=OFF
        -DSOL2_TESTS=OFF
        -DSOL2_EXAMPLES=OFF
)

vcpkg_cmake_install()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(INSTALL "${SOURCE_PATH}/LICENSE.txt" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
