# 2DMC Web

浏览器里可玩的 2D Minecraft。纯静态、零依赖、零构建：Canvas 2D 渲染 + 原生 JavaScript，存档保存在 `localStorage`。

这是 `beta/beta2.2.cpp`（控制台 C++ 版）的网页实现，重点把**末地维度**和**链式末影龙 Boss**搬到了浏览器里。

## 在线游玩

GitHub Pages 部署在仓库的 `gh-pages` 分支：

```
https://em-wzl.github.io/2DMC/
```

本地运行（推荐，联机功能需要 http 源）：

```bash
cd web
node test/serve.cjs 8765
# 打开 http://127.0.0.1:8765/
```

也可以直接双击 `index.html`（`file://`）单人游玩；`BroadcastChannel` 在 `file://` 下不保证跨标签通信，所以联机请用 http。

首次进入会显示启动页，输入种子点「开始新世界」，或点「读取存档」继续。

## 操作

| 按键 | 功能 | 按键 | 功能 |
|------|------|------|------|
| A / D、← → | 移动 | 空格 / W / ↑ | 跳跃 |
| 左键按住 | 挖掘 / 攻击 | 右键 | 放置 / 交互 |
| 1–9 / 滚轮 | 快捷栏 | E | 背包（2×2 合成） |
| F | 生存 / 创造切换 | 空格 / Shift | 创造模式上升 / 下降 |
| 回车 或 / | 命令 | H / Esc | 帮助 / 关闭面板 |

按 `/help` 查看全部命令。常用的：`/end`（前往末地）、`/nether`、`/give 钻石 8`、`/tp 100 80`、`/gamemode creative`、`/time 13000`、`/lan host`、`/lan join`、`/say 消息`、`/selftest`、`/save`、`/load`。

## 内容

**世界**：1024 × 256 的三维度平面（主世界 / 下界 / 末地），值噪声高度图、洞穴、湖泊、矿脉（煤/铁/铜/金/钻石/青金石/红石/绿宝石）、深层深板岩变体矿、紫水晶晶洞、幽匿块、八种群系（含樱花林、红树林、苍白之园）、树木与植被、要塞与末地传送门框架。

**物品与合成**：约 180 种方块/物品，木/石/铁/钻石/下界合金工具线，2×2 随身合成与 3×3 工作台，**原版有序配方（3×3 图案 + 网格内平移）与无序配方（材料集合）**，原版光标拿取（左键整堆 / 合并 / 交换，右键一半 / 一个），工具等级决定掉落，熔炉 / 附魔台 / 酿造台 / 箱子通用面板。

**26.x 内容**：26.1 金蒲公英 / 命名牌 / 收纳袋；26.2 硫磺洞穴（朱砂块、强效硫磺、硫磺池、间歇泉、尖刺）、硫磺方块怪、剪刀；26.3 荒野植被与装饰方块。

**维度**：下界（岩浆海、菌柄、萤石、远古残骸、猪灵）、末地（浮空中央岛、外围岛链、四根黑曜石柱 + 末影水晶、退出传送门 + 龙蛋、紫颂植物、虚空伤害、无昼夜），切换是 O(1) 的平面指针切换。

**末影龙**：见下节。

**局域网**：`BroadcastChannel`，主机权威，同步完整世界快照（RLE 压缩）、方块变更、玩家位置、维度、末影龙状态与聊天。同浏览器开两个标签即可联机。

**存档**：三个维度各自 RLE 压缩后存进 `localStorage`（`2dmc-web-save-v1`），含玩家、背包、合成网格、通用面板、末影龙状态、时间。自动保存 + `/save` 手动保存。

## 末影龙：链式 Boss

骨架**直接复用 C++ 版「世界吞噬者」的多节链式逻辑**：

```
头部 ──▶ 体节 ×12 ──▶ 尾部
  ▲                      │
  └──── conl 回指上一节 ──┘
```

- 头部自主飞行并持有全部血量（160），体节/尾部沿链跟随：`p[i] += (p[i-1] - p[i]) / 6`，与 `ai_boss3` 里 `gx += (shiti[conl].gx - gx) / 7` 同一套写法。
- **状态机**：`CRUISE` 绕岛盘旋并间歇喷龙息 → `DIVE` 从玩家身后 34 格设为目标点、穿身掠过（接触伤害 6）→ `PERCH` 降落到玩家所在列地面停留 400 帧（玩家主要输出窗口）。
- **被打即降落**：鼠标点击龙首近战命中会立刻把它打落栖息，可反复近战输出（原版机制）。
- **末影水晶回血**：柱顶四枚水晶只要还有存货，栖息期间每 30 帧回 1 血；挖掉水晶就不再回血。
- **死亡**：清空整条链 → 爆炸粒子 → 龙蛋进背包 → 退出传送门激活 → HUD 提示与成就日志。
- **渲染**：Canvas 画蛇形渐变身体、扇动翅膀、头角与发光眼、紫色龙息弹丸，顶部有 Boss 血条。
- **存档/联机**：龙状态随存档保存；主机每 4 帧广播龙的状态给客户端。

## 文件结构

```
web/
  index.html              页面骨架（HUD / 面板 / 帮助 / 启动页）
  styles.css              深色 UI
  js/core.js              常量、方块/物品 id、xorshift RNG、噪声、RLE
  js/items.js             方块属性表、配方表、合成匹配（纯函数）
  js/inventory.js         背包、原版光标拿取、合成网格、通用面板（纯函数）
  js/world.js             三维度生成、光照、RLE 存档（纯函数）
  js/entities.js          玩家物理、生物、投射物、末影龙链式 AI（纯函数）
  js/render.js            Canvas 程序化纹理、天空、实体/龙渲染、粒子
  js/ui.js                HUD、背包/合成面板、命令、提示
  js/lan.js               BroadcastChannel 局域网
  js/selftest.js          浏览器 / Node 共用断言
  js/game.js              主循环、输入、交互、命令、存档
  test/selftest.cjs       Node 自检（vm 加载纯逻辑模块）
  test/serve.cjs          本地静态服务器
  test/browser-check.html headless 浏览器加载自检
  test/screenshot*.html   截图验证页
```

`core / items / inventory / world / entities / selftest` 是纯逻辑模块，不碰 DOM，因此可以同时被浏览器和 Node 加载。

## 自检

```bash
node test/selftest.cjs              # 52 项
node test/selftest.cjs --verbose    # 打印明细
```

52 项覆盖：RLE 往返、有序/无序配方与 2×2 限制、配方平移、末影水晶/末地传送门框架/紫珀柱配方、原版光标拿取六种情况、背包堆叠、三维度世界生成（地表/矿脉/基岩/末地柱/水晶/传送门/龙蛋/虚空）、三维度 RLE 存档逐格往返、龙链长度与跟随收敛、体节间距、飞行限速、三态循环、龙息、被打降落、水晶回血与打掉后停止回血、死亡回调、玩家重力与落地。

游戏内输入 `/selftest` 会跑同一套断言并把结果显示在聊天区。

## 与 C++ 主线的对照

| C++ 版 | 网页版 |
|--------|--------|
| 控制台字符画 `printbuf` | Canvas 2D 程序化纹理 |
| `dim_terrain[3]` + `s` 指针 | `World.planes[3]` + `World.dim` |
| `make_eow` / `ai_boss3` 链式 Boss | `EnderDragon` + `followChain` |
| `make_st` / `conl` / `conr` | `segs[]` / 前一节索引 |
| `TAG_ENDDATA` RLE + `TAG_SHITI3` | `localStorage` + `rleEncode` |
| Winsock2 TCP `/lan host|join` | BroadcastChannel `/lan host|join` |
| 原版合成 `craft_match` | `Items.craftMatch(grid,size)` |
| 光标拿取 `craft_cursor_left/right` | `Inv.cursorLeft/Right` |
| `--selftest-end` 等 | `/selftest` + `node test/selftest.cjs` |

## 已知限制

- 联机需要同源 http(s)；`file://` 下 BroadcastChannel 跨标签不保证可用。
- 生物 AI 是简化版（追逐 / 接触伤害 / 苦力怕引信 / 硫磺方块怪吞噬），没有完整的村民交易、红石、试炼密室。
- 龙息是高速弹丸，没有原版的持续滞留云团。
- 存档在浏览器本地，不同设备不共享。
