# gamma 依赖治理与 git fork 迁移计划（提示词 2）

> 状态：**方案已落地，vcpkg 实际接入待执行**（本机无 vcpkg 环境）。
> 当前 `OM_USE_VCPKG=OFF`（默认），构建仍走 vendored `external/`，`build.bat` 未受影响。

## 一、当前依赖清单（已核对 `gamma/external/` 实际目录）

| 依赖 | vendored 路径 | 用途 | vcpkg port |
|------|---------------|------|------------|
| bgfx / bx / bimg / glslang / spirv-* | `external/bgfx.cmake` | 渲染后端 + 着色器工具链 | `bgfx`（overlay） |
| entt | `external/entt` | ECS | `entt` |
| FastNoiseLite | `external/FastNoiseLite` | 地形噪声 | 无官方 port（header-only，建议 vendored 或自制 port） |
| glm | `external/glm` | 数学库 | `glm` |
| lua / luajit | `external/lua` | Lua 运行时 | `lua` 或 `luajit` |
| SDL2 | `external/SDL` | 窗口/输入 | `sdl2` |
| SDL2_image | `external/SDL_image` | 贴图加载 | `sdl2-image` |
| SFML | `external/SFML` | 网络层（仅 Network 组件） | `sfml`（features: network） |
| sol2 | `external/sol2` | C++ ↔ Lua 绑定 | `sol2`（overlay） |
| thread | `external/thread` | 线程池 | 无官方 port（header-only） |
| tinyxml2 | `external/tinyxml2` | XML 解析 | `tinyxml2` |
| zlib | `external/zlib` | 压缩（存档/网络） | `zlib` |

## 二、已交付的 vcpkg 资产

### `gamma/vcpkg.json`（manifest）
列出全部可映射依赖；`FastNoiseLite` 与 `thread` 为 header-only 小库，暂留 vendored。

### overlay ports
```
gamma/vcpkg-overlays/ports/
├── bgfx/
│   ├── vcpkg.json
│   ├── portfile.cmake
│   └── patch/0001-force-include-stdint-for-gcc16.patch
└── sol2/
    ├── vcpkg.json
    ├── portfile.cmake
    └── patch/0001-guard-mingw-only-emplace-patch.patch
```

两个 overlay 保留的补丁语义：

1. **bgfx：`-include stdint.h`（仅 GCC/Clang）**
   - 原因：bgfx 内置 2019 年版 3rdparty（glslang / spirv-tools / glsl-optimizer）使用 `uint32_t` 等类型却没 include `<stdint.h>`。老版 libstdc++ 间接引入，g++ 16 不会。
   - 现状：已就地修复为 `if(NOT MSVC) add_compile_options(-include stdint.h) endif()`。

2. **sol2：`optional<T&>::emplace` 的 MinGW-only 补丁**
   - 原因：引用特化里没有 `construct()`，上游 `this->construct(...)` 在 g++ 16 下无法编译，本地改成 `emplace(U&&)`；但该替换在 MSVC 下触发 `C2061`（`detail::enable_if_t` 默认模板实参解析失败）并连带 7 个错误。
   - 现状：已就地修复为 `#if defined(__MINGW32__) <本地补丁> #else <上游原版 emplace(Args&&...)> #endif`。

3. **MSVC `/utf-8`（不在 overlay，属工具链配置）**
   - 原因：源码是无 BOM 的 UTF-8（含中文注释），MSVC 默认按系统 ANSI 代码页（936/GBK）读取，多字节序列吞掉换行导致后续声明语法错误（实测 14 个 error）。
   - 现状：已在 `gamma/cmake/compiler_flags.cmake` 的 `elseif(MSVC)` 分支加 `/utf-8`。
   - 建议：接入 vcpkg 后写入 triplet 的 `VCPKG_CXX_FLAGS`，或保留在根 CMake。

## 三、CMake 开关方案（已落地）

`gamma/CMakeLists.txt` 新增：

```cmake
option(OM_USE_VCPKG "Use vcpkg-provided dependencies instead of vendored external/" OFF)

if(OM_USE_VCPKG)
    message(STATUS "OM_USE_VCPKG=ON: using vcpkg-provided dependencies")
    find_package(bgfx CONFIG REQUIRED)
    find_package(bx CONFIG REQUIRED)
    find_package(bimg CONFIG REQUIRED)
    find_package(glm CONFIG REQUIRED)
    find_package(EnTT CONFIG REQUIRED)
    find_package(sol2 CONFIG REQUIRED)
    find_package(Lua REQUIRED)
    find_package(tinyxml2 CONFIG REQUIRED)
    find_package(ZLIB REQUIRED)
    find_package(SDL2 CONFIG REQUIRED)
    find_package(SDL2_image CONFIG REQUIRED)
    find_package(SFML CONFIG REQUIRED COMPONENTS network)

    add_library(om_external_deps INTERFACE)
    target_link_libraries(om_external_deps INTERFACE ...)
else()
    message(STATUS "OM_USE_VCPKG=OFF: using vendored dependencies from external/")
    add_subdirectory(external)
endif()
```

**默认 OFF**：`build.bat` 与 `cmake --preset win-mingw` 行为完全不变（已实测 EXIT=0）。

## 四、迁移执行顺序（待 vcpkg 环境就绪后）

```bash
# 1) 安装 vcpkg（一次性）
git clone https://github.com/microsoft/vcpkg.git D:/vcpkg
D:/vcpkg/bootstrap-vcpkg.bat

# 2) 用 overlay + manifest 安装依赖
cd D:/cpp/2DMC/gamma
D:/vcpkg/vcpkg.exe install \
  --triplet x64-mingw-static \
  --overlay-ports=./vcpkg-overlays/ports \
  --x-manifest-root=. \
  --x-install-root=./vcpkg_installed

# 3) 用 vcpkg toolchain configure + build（先并存，不删 external/）
cmake --preset win-mingw \
  -DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DOM_USE_VCPKG=ON \
  -B build/vcpkg-mingw
cmake --build build/vcpkg-mingw

# 4) 三平台都通过后，再删除 gamma/external/
#    （此时 build.bat 需同步切到 vcpkg toolchain）

# 5) 最后把 build.bat 的 cmake 调用改为带 toolchain + -DOM_USE_VCPKG=ON
```

### 回滚
- 任何一步失败：`OM_USE_VCPKG=OFF`（默认）即可回到 vendored 路径。
- 删了 `external/` 之后要回滚：`git checkout -- gamma/external`（若已提交）或从备份恢复。
- `build.bat` 改动前先备份。

## 五、git fork / submodule 迁移计划（**未执行**）

### 现状
- `gamma/` 不是独立 git 仓库，`.gitmodules` 是遗留死文件。
- `gamma/external/*` 是"打过补丁的第三方源码快照"，直接 commit 进 2DMC 根仓库 → 无法跟上游更新、无法审计改动。

### 目标结构
```
D:/cpp/2DMC/                 <- 根仓库（2DMC 玩法/资源/文档）
├── .gitmodules              <- gamma 作为 submodule
├── gamma/                   <- 独立 git 仓库（OpenMiner fork）
│   ├── .git/                <- 自己的历史
│   └── external/            <- 迁移完成后删除（或作为 submodule 的 submodule）
├── classic/ Alpha/ beta/ ...  <- 历史目录，不动
└── ...
```

### 执行步骤（需人工确认后执行）

```bash
# 1) 在 gamma/ 初始化独立仓库
cd D:/cpp/2DMC/gamma
git init
git add .
git commit -m "chore: snapshot 2DMC gamma before upstream rebase"

# 2) 加 upstream remote（上游真实基线）
git remote add upstream https://github.com/Unarelith/OpenMiner
git fetch upstream

# 3) 以上游 master 最新稳定 commit 为基线
#    实测（2026-10-30 查询 GitHub API）：
#      upstream/master HEAD = 19a63c4691bb0bc235f60508861a72119094c72d
#      commit message: "Fixed compilation on Windows."
#      date: 2023-11-20T17:17:28Z
#    注意：上游最后更新是 2023-11，之后无新 commit，基线稳定。
git checkout -b 2dmc 19a63c4691bb0bc235f60508861a72119094c72d

# 4) 把 2DMC 的玩法补丁 rebase 到 2dmc 分支上
#    （流体/生物/命令/下界/CJK/Lua 修复等，逐组 cherry-pick 或重新提交）
#    目标：`git diff upstream/master..2dmc` 就是一份可审计的 2DMC 改动集

# 5) 推送到自己的 fork（需要先建远程仓库）
git remote add origin <your-fork-url>
git push -u origin 2dmc

# 6) 在 2DMC 根仓库接入 submodule
cd D:/cpp/2DMC
git rm -r --cached gamma
git submodule add -b 2dmc <your-fork-url> gamma
git commit -m "build: add gamma as submodule (OpenMiner fork, 2dmc branch)"
```

### 风险与缓解
| 风险 | 缓解 |
|------|------|
| `git rm -r --cached gamma` 后误删文件 | 先 `git stash`/备份整个 `gamma/` |
| 嵌套 `.git` 与根仓库冲突 | 根仓库加 submodule 前先确认 `gamma/.git` 已被识别为 submodule |
| `external/` 体积大，fork 后仓库膨胀 | 迁移 vcpkg 后删除 `external/`，历史用 `git filter-repo` 清理（可选） |
| 基线选错导致 patch 冲突 | 上游 2023-11 后无更新，基线稳定；冲突时按文件手工合并 |

### 为什么不现在执行
- 会创建嵌套 `.git`，影响当前工作区与其他 agent 的并行改动。
- `git rm -r --cached gamma` 属破坏性操作，需在干净工作区 + 备份后执行。
- 建议在 vcpkg 迁移完成、`external/` 删除之后再一并做。

## 六、验证记录

| 项 | 命令 | 结果 |
|----|------|------|
| build.bat（默认 OFF） | `gamma\build.bat Release` | **EXIT=0** |
| CMake 开关生效 | 构建日志 | 输出 `-- OM_USE_VCPKG=OFF: using vendored dependencies from external/` |
| win-mingw preset | `cmake --preset win-mingw && cmake --build --preset win-mingw` | **EXIT=0**（见提示词 1 报告） |
| `OM_USE_VCPKG=ON` | 本机无 vcpkg | **预期失败**（`find_package(bgfx CONFIG)` 找不到），默认 OFF 不受影响 |

## 七、遗留项

1. `vcpkg.json` 的 `builtin-baseline` 需按实际 vcpkg 版本调整。
2. overlay port 的 `SHA512 0` 是占位符，首次 `vcpkg install` 会报出真实哈希，替换即可。
3. `FastNoiseLite` 与 `thread` 无官方 port，建议保留 vendored 或自制 overlay port。
4. `find_package(bgfx)` 导出的 target 名称随 port 版本变化，接入时需按实际调整。
5. git fork/submodule 未执行（见第五节）。
