# 2DMC Web

浏览器里可玩的 2D Minecraft。纯静态、零依赖、零构建：Canvas 2D 渲染 + 原生 JavaScript，存档保存在 `localStorage`。

这是 `beta/beta2.2.cpp`（控制台 C++ 版）的网页实现，重点把**末地维度**和**链式末影龙 Boss**搬到了浏览器里；另外补齐了原版贴图/皮肤、**程序化音效**、**饥饿与食物**、**天气**和**小地图**等完整生存体验。

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
| T | 原版贴图 / 程序化纹理切换 | 回车 或 / | 命令 |
| M | 音效开 / 关（记住设置） | V | 小地图开 / 关 |
| H / Esc | 帮助 / 关闭面板 | 左键按住 | 持续挖掘 |

按 `/help` 查看全部命令。常用的：`/end`（前往末地）、`/nether`、`/give 钻石 8`、`/tp 100 80`、`/gamemode creative`、`/time 13000`、`/weather rain|snow|thunder|clear`、`/hunger`、`/sound on|off`、`/skin alex`、`/lan host`、`/lan join`、`/say 消息`、`/selftest`、`/save`、`/load`。

## 内容

**世界**：1024 × 256 的三维度平面（主世界 / 下界 / 末地），值噪声高度图、洞穴、湖泊、矿脉（煤/铁/铜/金/钻石/青金石/红石/绿宝石）、深层深板岩变体矿、紫水晶晶洞、幽匿块、八种群系（含樱花林、红树林、苍白之园）、树木与植被、要塞与末地传送门框架。

**物品与合成**：约 180 种方块/物品，木/石/铁/钻石/下界合金工具线，2×2 随身合成与 3×3 工作台，**原版有序配方（3×3 图案 + 网格内平移）与无序配方（材料集合）**，原版光标拿取（左键整堆 / 合并 / 交换，右键一半 / 一个），工具等级决定掉落，熔炉 / 附魔台 / 酿造台 / 箱子通用面板。合成面板右侧是**配方书**：按当前背包材料列出「可做 / 缺材料」并支持搜索，点「可做」一键把材料摆进合成格。

**26.x 内容**：26.1 金蒲公英 / 命名牌 / 收纳袋；26.2 硫磺洞穴（朱砂块、强效硫磺、硫磺池、间歇泉、尖刺）、硫磺方块怪、剪刀；26.3 荒野植被与装饰方块。

**维度**：下界（岩浆海、菌柄、萤石、远古残骸、猪灵）、末地（浮空中央岛、外围岛链、四根黑曜石柱 + 末影水晶、退出传送门 + 龙蛋、紫颂植物、虚空伤害、无昼夜），切换是 O(1) 的平面指针切换。

**末影龙**：见下节。

**局域网**：`BroadcastChannel`，主机权威，同步完整世界快照（RLE 压缩）、方块变更、玩家位置、维度、末影龙状态与聊天。同浏览器开两个标签即可联机。

**生存系统**：20 点生命 + 20 点饱食度 + 隐藏饱和度/消耗度三段式。走动、跳跃、挖掘、受伤会消耗；饱食 ≥18 每 80 帧自然回血，饿空每 100 帧掉 1 血（最低留 1 点）。选中食物**右键食用**（面包 +5、苹果 +4、金苹果 +8、浆果 / 紫颂果 +2/+4），初始背包带面包、苹果、金苹果。HUD 左边红心后面是橙色饱食度条，创造模式不消耗。

**天气**：主世界随机切换晴 / 雨 / 雪 / 雷暴，强度平滑渐变；雪原下雨自动变降雪、沙漠干旱、下界与末地永远晴朗。阴天压暗天空并削弱光照，雨丝 / 雪花 / 落地溅水 / 雷暴闪电白闪都在 Canvas 前景绘制，地下自动隐藏。天气状态随存档往返，可用 `/weather` 强制切换。

**音效**：`js/audio.js` 用 Web Audio 实时合成全部声音（无音频文件）——脚步、挖掘、破坏、放置、跳跃、落地、受伤、死亡、进食、拾取、合成、爆炸、龙息、龙吼、传送、雷声、升级提示，以及雨 / 洞穴 / 下界 / 末地四种低音量环境声。浏览器策略要求首次点击或按键解锁音频，解锁后按 **M** 静音（写入 `localStorage`）。Node / 无音频设备环境自动降级为 no-op。

**小地图**：右上角 172×116 的地形切片（每像素 2 格），标出玩家（白色菱形 + 朝向）、生物（红）、末影水晶与龙（紫）、传送门与远程玩家（青），按 **V** 开关。

**存档**：三个维度各自 RLE 压缩后存进 `localStorage`（`2dmc-web-save-v1`），含玩家（位置 / 生命 / 创造 / 饱食度 / 饱和度 / 消耗度）、背包、合成网格、通用面板、末影龙状态、**天气状态**、时间。自动保存 + `/save` 手动保存。

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
- **龙息滞留云**：龙息弹丸命中玩家 / 撞到方块 / 消散后留下原版式 area effect cloud，半径先涨后缩、150 帧消散，范围内每 20 帧造成 2 点伤害。
- **渲染**：Canvas 画蛇形渐变身体、扇动翅膀、头角与发光眼、紫色龙息弹丸与滞留云，顶部有 Boss 血条。
- **存档/联机**：龙状态随存档保存；主机每 4 帧广播龙的状态给客户端。

## 贴图（用项目文件夹里的资源）

网页版默认使用**项目内已有的贴图**，不是程序化色块：

| 来源 | 内容 | 数量 |
|------|------|------|
| `Default-Java-26.2/assets/minecraft/textures/block` | MC Java 26.2 原版方块贴图（16×16） | 103 |
| `Default-Java-26.2/assets/minecraft/textures/item` | MC Java 26.2 原版物品/工具图标 | 74 |
| `beta1.8/assets/tiles` | 2DMC 自有贴图补充 | 4 |

`node test/build-textures.cjs` 会读取 `js/items.js` 的方块/物品表，按名字自动匹配贴图，
复制到 `assets/tiles/<name>.png`，并生成 `js/textures.js`（id → 贴图名 + 异步加载器）。
**181 / 183 项命中**，打包后只有约 55 KB。

细节：

- 动画图集（`water_still` 16×512、`lava_still`、`nether_portal` 等）在浏览器里只取最上面一帧 16×16，再最近邻放大到 32×32 缓存，与程序化 tile 尺寸一致。
- 命名不一致的做了映射：草方块→`grass_block_side`、工作台→`crafting_table_front`、紫珀柱→`purpur_pillar_side`、附魔台→`enchanting_table_side`、指南针→`compass_00` 等；硫磺/朱砂等 26.2 自定义方块用最接近的原版方块贴图。
- 箱子、末地传送门保留程序化绘制（原版箱子没有单张方块贴图，紫色漩涡传送门比静态图更好看）。
- 方块、快捷栏、背包槽位、鼠标上拿着的物品、光标提示全部换成真实图标；按 **T** 可一键切回程序化纹理对比。

重新生成贴图包：

```bash
cd web
node test/build-textures.cjs
```

## 玩家皮肤与生物 sprite

除了方块贴图，项目内的皮肤也接了进来：

| 来源 | 用途 |
|------|------|
| `beta1.8/assets/skins/*.png` | 6 套玩家皮肤（steve / alex / ari / kai / sunny / 自定义），64×64 |
| `tools/mob_textures_src/*.png` | 僵尸 / 骷髅 / 苦力怕 / 羊 / 僵尸猪灵的 64×32 skin |

`js/sprites.js` 按经典 UV 把皮肤裁成 2D 正面 sprite（20×32）：头 `(8,8,8,8)`、身 `(20,20,8,12)`、腿 `(4,20,4,12)`、臂 `(44,20,4,12)`，64×64 皮肤另有左腿 `(20,52)` / 左臂 `(36,52)`；苦力怕与羊是另一套裁剪（用 `test/skinscan.html` 扫过非透明区域定位）。掉落物也换成真实物品贴图。

- `/skin` 查看可选皮肤，`/skin alex` 切换，选择记在 `localStorage`。
- 蜘蛛、末影人、硫磺方块怪仍用程序化绘制（保持各自的形状特征）。
- `test/spriteview.html` 可预览所有拼装结果，`test/skinscan.html` 可查看 skin 的 UV 内容分布。

## 文件结构

```
web/
  index.html              页面骨架（HUD / 面板 / 帮助 / 启动页）
  styles.css              深色 UI
  assets/tiles/*.png      项目内贴图精选（181 张，构建脚本生成）
  assets/skins/*.png      玩家皮肤（6 套）
  assets/mobs/*.png       生物 skin（8 张）
  js/textures.js          id → 贴图映射 + 异步加载器（构建脚本生成）
  js/sprites.js           皮肤 UV 裁剪 → 2D 正面 sprite
  js/core.js              常量、方块/物品 id、xorshift RNG、噪声、RLE
  js/audio.js             Web Audio 程序化音效 + 环境声（无外部音频文件）
  js/weather.js           天气状态机（纯函数，雨/雪/雷暴 + 存档）
  js/items.js             方块属性表、配方表、合成匹配（纯函数）
  js/inventory.js         背包、原版光标拿取、合成网格、通用面板（纯函数）
  js/world.js             三维度生成、光照、RLE 存档（纯函数）
  js/entities.js          玩家物理、生物、投射物、末影龙链式 AI（纯函数）
  js/render.js            Canvas 贴图/纹理、天空、实体/龙渲染、天气、小地图、粒子
  js/ui.js                HUD、背包/合成面板、命令、提示
  js/lan.js               BroadcastChannel 局域网
  js/selftest.js          浏览器 / Node 共用断言
  js/game.js              主循环、输入、交互、命令、存档
  test/selftest.cjs       Node 自检（vm 加载纯逻辑模块）
  test/build-textures.cjs 从项目内贴图资源生成 assets/tiles + textures.js
  test/serve.cjs          本地静态服务器
  test/browser-check.html headless 浏览器加载自检
  test/playtest.html      自动试玩：真实键盘事件逐帧跑主循环
  test/playtest-shot.html 跳跃 / 生物站位的视觉验证页
  test/screenshot*.html   主世界 / 末地 / 面板 / 天气小地图截图验证页
  test/spriteview.html    玩家与生物 sprite 拼装预览
  test/skinscan.html      skin UV 区域扫描（定位裁剪坐标）
```

`core / audio / weather / items / inventory / world / entities / selftest` 是纯逻辑模块（audio 在无音频设备时自动降级），不依赖 DOM，因此可以同时被浏览器和 Node 加载。

## 自检

```bash
node test/selftest.cjs              # 98 项
node test/selftest.cjs --verbose    # 打印明细
```

98 项覆盖：RLE 往返、有序/无序配方与 2×2 限制、配方平移、末影水晶/末地传送门框架/紫珀柱配方、原版光标拿取六种情况、背包堆叠、三维度世界生成（地表/矿脉/基岩/末地柱/水晶/传送门/龙蛋/虚空）、三维度 RLE 存档逐格往返、龙链长度与跟随收敛、体节间距、飞行限速、三态循环、龙息、被打降落、水晶回血与打掉后停止回血、死亡回调、**玩家重力/落地/单次跳跃高度与滞空/行走速度**、**生物生成点校验、跑动 400 帧不卡方块、不掉出世界、会走动、被埋能自动脱困**、**龙息云持续伤害/范围判定/到期消散**、**配方书材料统计、尺寸过滤、一键摆料后网格可匹配**、**饥饿三段式（食用/消耗/回血/饿空下限/重生/创造模式不消耗）**、**天气状态机（雨雪判定、沙漠与雪原群系、雷暴闪电、下界停雨、光照削弱、存档往返）**、**音效模块无设备降级与静音开关**。

游戏内输入 `/selftest` 会跑同一套断言并把结果显示在聊天区。

## 自动试玩

`test/playtest.html` 会在真实浏览器里加载游戏、派发**真实键盘 / 鼠标事件**（空格跳跃、D 行走、右键吃面包、M 静音、V 小地图），
显式驱动真实主循环逐帧运行，并生成 12 只生物跑 400 帧：

```bash
node test/serve.cjs 8765
# Chrome headless:
chrome --headless=new --virtual-time-budget=15000 --dump-dom \
  http://127.0.0.1:8765/test/playtest.html
```

实测输出（修复后）：

```
jump_rise=1.60格 | tap_air_frames=24 | tap_rise=1.55格 | landed_back=true
walk_dx=4.48格 | eat_hunger=15 left=2 | mute=true unmute=true
minimap_toggle=true | weather=rain/1.00
mob_stuck=0/6269 | mob_oob=0 | mobs_moved=12
dragon_segs=14 | RESULT=PASS
```

> 修复前跳跃初速是 `-8.1 格/帧`、重力 `0.44 格/帧²`，一次跳跃能升约 **74 格**（看起来像飞）；
> 生物每帧下坠 `0.4 格` 且只检测单点碰撞，会穿进方块。现在所有速度统一为「格/帧」，
> 玩家与生物共用同一套 AABB 分轴碰撞，生成点会校验头顶空间与脚下支撑，
> 已经嵌进方块的生物每帧会尝试脱困。

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
- 音效是程序化合成而非原版音轨；天气只影响画面与光照，不改变作物生长、怪物刷新或方块行为。
- 玩家皮肤用的是经典 64×64 布局，不含第二层（帽子 / 外套）叠加；末影龙本体仍是链式蛇身自绘。
- 存档在浏览器本地，不同设备不共享。
