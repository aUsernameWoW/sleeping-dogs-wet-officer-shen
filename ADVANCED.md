# SDWet — advanced users and developers

> [!IMPORTANT]
> **关于这个 mod**：它完全是用 Claude Code 里的 Claude Fable 和 Opus vibe coding 写出来的，几乎没有经过审查，请当作实验性质的 mod 使用，发现异常请反馈。作为一个长期缺少 mod 而自己下场做 mod 的普通玩家，我在 vibe 的过程中收获了许多快乐，并建议所有人都可以试着 vibe 一下去实现自己的灵感。由于这些代码都是 vibe 出来的，所以我不会以我的 mod 盈利，也不接受捐助。如果你喜欢我的作品，请考虑向[致谢](#致谢)中提到的组织和个人捐赠，祝你游玩愉快！
>
> **About this mod**: it was fully vibe-coded with Claude Fable and Opus in Claude Code, with little review, so treat
> it as experimental and please report anything unusual. I'm just an ordinary player who went a long time without
> mods for this game and finally started making them myself. Vibe coding them has been a lot of fun, and I'd encourage
> everyone to give it a try and bring their own ideas to life. Since all this code is vibe-coded, I won't make money
> from my mods and don't accept donations. If you like my work, please consider donating to the organizations and
> people listed in the [Credits](#credits) instead. Have fun!

[中文](#中文) | [English](#english)

新手安装说明见 [README.md](README.md)。 · Step-by-step install for players: [README.md](README.md).

## 中文

### 做了什么

终极版里的湿身问题（PCGamingWiki 上的 “Wetness Bug on default and many other outfits”）其实是两个互不相关的
bug，本 mod 分别修复：

1. **湿了也看不出来**（淋雨和游泳都一样）。角色淋雨时，游戏会把湿度累积到 1，渲染时换用带湿身效果的着色器
   变体（`HK_CHARACTER*_WS`）。这个着色器用高光贴图 `texSpecular` 的 x 通道（乘 10）和 z 通道决定湿了以后亮多少，
   再把颜色压暗最多 35%。但终极版里几乎所有衣服和皮肤的高光贴图都只有 y 通道（普通光泽），x 和 z 全是 0：
   上衣 62 件里只有 1 件、裤子 46 条里只有西裤有数据，头发则是 87 张里有 78 张。所以湿度拉满也几乎看不出来，
   只有头发和少数衣服会湿。

   本 mod 在游戏加载着色器时，给这 28 个湿身变体（20 份不同的字节码）插入三条指令：贴图的 x 和 z 都为 0 时，
   改用 `SDWet.ini` 里的 `Shine`/`Gloss`。这两个通道只在角色湿了（或出汗）时参与计算，所以干的时候外观完全
   不变；自带湿身遮罩的衣服和头发也保持原样。

2. **游泳上岸完全不湿**。游泳的动作树节点上挂着 `ApplyWetnessOrSweatTrack`，会把湿度设为 1。它按
   `CharacterLookComponent` 的类型（`0xCC000005`）在角色的组件表里查找外观组件，但玩家的外观组件登记成了父类
   `CompositeLookComponent` 的类型（`0xCC000001`），所以永远查不到，什么也没写。本 mod 把这些动作的数值按角色
   记下来，由外观组件在下一帧自己更新时写入，不经过查找。一些过场动画也用同一种动作设置湿度，路人和混混在雨里
   撑伞时也有一个（`DryMeOff!`，从名字看是让撑伞的人保持干燥）；玩家本身没有撑伞的动作。

雨中湿度的累积和雨停后的变干（每秒 1/60）都是游戏原有的逻辑，没有改动。

另外**新增了上岸后的湿脚印**，这不是修 bug，原版本来就没有。游戏的脚印来自“物理体积”：角色站在血泊、泥地、
水坑或关卡里放置的某些区域上时，每一步都按该体积的设置踩出特效和左右脚贴花，离开后还会接着印 5 秒。湿脚印贴花
（`HK_WetFootPrintLeft/Right_Effect`，体积 `PhysVol_WetFootPrints`）只有水坑和少数手工放置的 2 m² 区域在用；
海水（`Water`）只有溅水特效，所以游泳上岸不会留脚印。本 mod 在角色被游泳的动作弄湿之后，把这对贴花临时放进
脚步特效的覆盖槽（任务脚本 `set_footstep_override_effect` 用的同一个位置）：只在每一步的处理期间放进去，
之后清空。持续 `WetFootprints` 秒（按游戏时间算），或者到身体干到对应程度为止；站在浅水里的
脚步不留；任务脚本已经设置了自己的脚印时保持原样。

**撑伞（新增，默认开启，`[Umbrella] Enabled = 0` 关闭）**：Wei 只能把伞当近战武器拿在手里（路人在雨里撑伞，
受惊时会把伞扔掉），原版没有撑开的动作。拿着伞**长按 E**（游戏的“动作”键，手柄 Y，改过键位就跟着改）半秒，Wei
播放路人撑伞时的上半身动作（这些动作在全局动作库里），伞也随之打开；再长按一次收起，收伞动作会顺手甩掉伞上
的水。E 在游戏里本来还管反击、搭话、上车和叫出租车：按下的这个 E 如果游戏要用（出现了搭话提示，或者 Wei 因此
开始反击、走向车门、叫车，或者他根本不在走路/站着），伞就不动。伞撑开时 E 不会直接传给游戏（收起后要松开 E
才恢复）：在车门边或搭话对象面前按下 E（也就是游戏正等着 E 的上车/搭话提示），Wei 马上收伞，伞收好后
（约 2 秒）再把这次按键原样交给游戏；收伞期间离开了、提示没了，这次按键就作废。出租车不等人（收伞动画还没
放完车就可能开走），所以在出租车旁按 E 伞会瞬间合上，按键立刻交给游戏：按住超过 1/3 秒是打车，短按是抢
司机的座位，和原版一样。其他地方按住 E 半秒是收伞。坐出租车时 Wei 是直接“贴”进后座的，这是原版的做法
（带上车动画的那个节点被开发者关掉了），手里的武器也会带着。下水（开始游泳）时伞瞬间合上，限制解除，可以
冲刺游泳。和停车场管理员说话时游戏会把伞收走，收不收伞都一样，这是游戏自己的处理。
F7 也能撑开/收起（调试用）。撑开期间 Wei 只能走路，按住 Shift 是快走；不能冲刺、攻击、抓人、捡东西、换武器、
跑酷或进掩体。这些限制用的都是游戏脚本本来就有的开关（`allow_jog`/`allow_sprint` 和按名字禁用的操作请求），
收伞或伞离手时恢复；撑伞期间游戏自己改过的冲刺/慢跑开关（比如出入室内时的强制慢走）以游戏为准。伞本身的
动作树下雨时会自动撑开（给路人用的），所以 Wei 手里那把伞的“是否在下雨”判定改为“是否撑着”，路人的伞不受
影响。还没做：过场时自动收伞、伞下不被淋湿、手柄快走。

### 原理

安装版 Steam exe 不带符号，函数靠字节特征码定位；特征码取自旧版 v1.0 exe 及其 PDB（来自 SDmodding 项目），
在两个版本里都唯一匹配。找不到时对应功能不启用，日志里写 `MISSING`。hook 点：

- `Illusion::StageShader::LoadShader`：游戏把 `data\shaders\shaders.temp.bin` 里的字节码交给 D3D11 创建着色器、
  再用 `D3DReflect` 读取反射信息的地方。在这里换成补丁版，所以不需要 hook D3D11，也不会和 ReShade 冲突。
  补丁按结构识别湿身着色器（绑定了 `texSpecular` 和 `cbSceneryInstance`，并读取其中的汗水和湿度），重新计算
  DXBC 校验和，D3D 会校验它。
- `ApplyWetnessOrSweatTask::Begin/Update/End` 和 `CharacterLookComponent::Update`：见上面第 2 条。
- `CharacterEffectsComponent::HandleFootstep`：每一步的脚步特效，湿脚印在这里加上。
- `ActionTreeComponent::update`：撑伞在玩家自己的动作树更新之后，更新它自己的上半身动作控制器（仿照游戏
  生成子控制器的 `SpawnTask`）。`IsRainingCondition::Match`：伞的“是否在下雨”判定。
  `TSCharacter::Mthd_allow_jog/allow_sprint`：脚本开关慢跑和冲刺的方法，撑伞期间记下游戏的改动。
  `ReadControllerInputTask::Update`：PlayerAI 树里把手柄/键盘输入变成操作请求的任务。撑伞时在它读输入的那一刻
  把 E 改成“没按”（交还按键时改成“刚按下”），游戏的两棵动作树都看不到真实的 E。只屏蔽请求不够：PlayerAI 树
  在屏蔽生效之前就读到了请求，撑着伞照样能上车。E 的真实状态直接读游戏的输入数据；“这个 E 游戏要不要用”用
  游戏自己的 `ActionController::IsPlaying`（动作树条件 `IsPlaying` 调用的函数）查 Wei 的动作树和处理按键的
  PlayerAI 树，搭话提示看社交目标是否锁定（`TargetIsLocked` 条件读的同一处）；撑伞时游戏在等哪种 E，看 PlayerAI
  树停在哪个提示节点（`InteractHandler\Prompts\...\Available`）。

所有细节（结构偏移、着色器公式、调查过程）见 [CLAUDE.md](CLAUDE.md)（英文）；调查用的脚本（着色器解包、
材质解析、RenderDoc 抓帧分析与离线预览）在 [research/](research/README.md)。

### 设置（`plugins\SDWet.ini`）

| 项 | 默认 | 说明 |
|---|---|---|
| `[Wet] WetLook` | 1 | 着色器补丁（让湿身看得出来）。 |
| `[Wet] Shine` | 0.05 | 贴图没有湿身遮罩时的 x 通道默认值：湿透时光泽度增加 `Shine × 10`。0 = 游戏原样，0.1 以上接近塑料感。 |
| `[Wet] Gloss` | 0.10 | 同上，z 通道：额外的湿润光泽。 |
| `[Wet] ActionWetness` | 1 | 让游泳等动作设置的湿度生效。 |
| `[Wet] WetFootprints` | 25 | 上岸后留湿脚印的秒数，0 = 关闭。需要 `ActionWetness = 1`。 |
| `[Umbrella] Enabled` | 1 | 撑伞：拿着伞时长按 E 撑开/收起（F7 也行），撑着伞在上车/打车/搭话提示处按 E 先收伞再交给游戏，F9 把状态写进日志。旧版写进 ini 的 `Prototype = 0` 已经不再读取。 |
| `[Debug] Logging` | 1 | 写 `SDWet.log`；崩溃时另写 `SDWet-crash-<n>.dmp`。 |
| `[Debug] LogWetnessTracks` | 0 | 记录每个湿度动作，以及之后玩家湿度的变化曲线。 |

改完重启游戏生效。

### 日志

- `shaders: wet/sweat pixel shader #N patched`：游戏启动时应有 20 行。
- `wetness: applied a track to look ... registered as type 0xCC000001; the game looks for 0xCC000005`：第一次
  游泳时出现，说明第 2 条修复在工作。
- `footprints: sim object ... soaked`、`first wet footprint`，以及上岸 `WetFootprints` 秒后的
  `... N wet footprints, M steps in water ...`：湿脚印从开始到结束的记录。
- `umbrella:` 开头的行：撑伞的每一步（`E held ... s: open/close` 长按触发、`E held, but it's the game's: ...` 这次
  E 归游戏的原因、`E kept from the game` / `E back to the game` / `E pressed for the game`、
  `E pressed at the ... prompt with the umbrella open` 撑伞时在提示处按 E、`E goes to the game for the ... prompt`
  收好伞后交还按键（或 `... the press is dropped` 作废及原因）、`Wei is in the water` 下水收伞、动作、伞的状态、
  慢走/快走切换、
  `player can [not] jog/sprint` 冲刺开关的每次变化及来源、`rain check for the umbrella in hand` 下雨判定）。
- `crash:` 开头的行：崩溃时的位置和调用栈。游戏每次退出都会崩一次（原版问题，地址以 `488C` 结尾），这一条可以
  忽略。
- `MISSING` 或 `not hooked`：没找到对应的游戏函数，该功能关闭。

### 需求与兼容性

- 《热血无赖：终极版》的两个发行版本：当前 Steam 版（在游戏里验证过）和旧版 v1.0（特征码在它上面同样唯一
  匹配），Windows 10/11 x64。不依赖 Windows 专有服务，应当能在 Wine/Proton/CrossOver 下运行（未测试）。
- 任意 ASI 加载器，例如 [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader)（`SDWet.zip`
  里自带一份，作为 `dinput8.dll`）。
- 与 ReShade 兼容（在装有 ReShade 6.8.0 的环境里测试）。不修改任何游戏文件，也不影响存档。

### 下载

[Releases](https://github.com/aUsernameWoW/sleeping-dogs-wet-officer-shen/releases) 里每个版本都有：

| 文件 | 内容 |
| --- | --- |
| `SDWet.zip` | 解压到游戏目录：`dinput8.dll`（Ultimate ASI Loader）+ `plugins\SDWet.asi` + 许可声明 |
| `SDWet.asi` | 只有 mod 本体，放进已有加载器的 `plugins\` |
| `SDWet.pdb` | 调试符号，只在分析崩溃转储时需要 |
| `THIRD-PARTY-NOTICES.md` | 第三方代码的许可证 |

`main` 上每次提交都会自动编译、测试并发布为预发布版 `build-<N>`（没有在游戏里测过）。在游戏里验证过的构建会被
转为正式版；README 里的下载链接指向最新的正式版。

已经有 ASI 加载器（不论叫 `dinput8.dll`、`winmm.dll` 还是别的名字）时，只需要把 `SDWet.asi` 放进它加载插件
的目录（通常是 `plugins\`）。`SDWet.ini` 和 `SDWet.log` 写在 `.asi` 旁边。

### 编译与测试

Visual Studio 2022（v143），Windows SDK 10.0.26100。项目需要放在工作区的 `mods\SDWet`，工作区里还要有
`reference\minhook`（[MinHook](https://github.com/TsudaKageyu/minhook) v1.3.4 源码，随项目一起编译）。
在工作区根目录运行 `.\tools\build.ps1 -Mod SDWet -Test`，会编译并运行 `tests\` 里的测试：

- `dxbc_test`：用 D3DCompile 编一个结构相同的着色器，检查校验和、识别逻辑、对损坏输入的处理，并在 WARP 上实际
  渲染，验证有无湿身遮罩、干湿两种状态下的输出。
- `game_shaders_test`：用已安装游戏的全部 2237 个着色器验证：校验和全对、恰好补丁 28 个湿身变体、D3D11 全部
  接受（没装游戏时跳过）。
- `load_test`：在游戏之外加载 .asi，不能崩溃，写出默认 ini，并报告找不到游戏函数。
- `umbrella_load_test`：同上，但用旧版写的 ini（`[Umbrella] Prototype = 0`）：撑伞仍然开启。
- `umbrella_off_test`：`[Umbrella] Enabled = 0` 时撑伞关闭，不查找它的游戏函数。

GitHub Actions 会对推送和 PR 按同样的布局编译（`-warnAsError`）并运行自动测试，依赖的确切版本见
`.github/reference.env`；然后打包 `SDWet.zip`，其中 Ultimate ASI Loader 的版本和 SHA-256 固定在
`.github/asi-loader.env`。推送到 `main` 且测试通过的构建会发布为预发布版 `build-<N>`。
`asi-loader.yml` 每月检查一次 Ultimate ASI Loader 的新版本，有新版时开 PR 更新 `asi-loader.env`；
`reference.yml` 对编译所用的依赖做同样的检查，开 PR 更新 `reference.env`；Dependabot 每月更新 Actions 的版本。
Nexus Mods 上传（`build.yml` 的 `nexus` 任务和 `nexus-release.yml`）要等有了 Nexus 页面、设置好仓库变量才会运行。

### 致谢

这个 mod 用到或参考了下面这些人和项目的成果，在此致谢。

**研究资料**

- [PCGamingWiki](https://www.pcgamingwiki.com/wiki/Sleeping_Dogs:_Definitive_Edition)：上面记录了终极版的湿身问题
  （“Wetness Bug on default and many other outfits”）。
- [SDmodding](https://github.com/SDmodding)，几乎全部出自 [sneakyevil](https://github.com/sneakyevil) 一人之手。这个 mod 用到了：
  - SDmodding 随 [SDK](https://github.com/SDmodding/SDK) 发布的 [Visual Studio 2022 项目模板](https://github.com/SDmodding/SDK/releases/tag/vs2022)：这个 mod 的 Visual Studio 工程源自这个模板，编译设置和以 `dllmain.cc` 为起点的源文件结构都来自它；
  - SDmodding 分享的游戏 v1.0 版 exe 和调试符号（PDB，Steam 首发版自带）：函数特征码和游戏的数据结构都是从这里查到的；
  - [SDK](https://github.com/SDmodding/SDK) 和 [TheoryEngine](https://github.com/SDmodding/TheoryEngine)：材质与着色器资源的结构，以及游戏的字符串哈希；SDK 里的游戏输入（E 键对应的 `ActionDef_Action` 及其数据结构）和目标类型列表：长按 E 撑伞；
  - [Files](https://github.com/SDmodding/Files) 里导出的动作树（ActionNodes）：撑伞；
  - [BigFileSystem](https://github.com/SDmodding/BigFileSystem)、[TheoryEngine](https://github.com/SDmodding/TheoryEngine)，以及 sneakyevil 的 [SD-BigFileExplorer](https://github.com/sneakyevil/SD-BigFileExplorer) 和 [Ekey](https://github.com/Ekey) 的 SDDEUnpacker 里的文件名列表：
    读取游戏资源包（`.big`）的工具是照着它们写的，研究脚本（`research/`）用它读取游戏的材质、着色器和贴图。

**mod 里包含的代码**（许可证全文见 [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)）

- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader)（ThirteenAG）：压缩包里的 `dinput8.dll`，让游戏加载 mod。它本身还包含 MinHook、
  [miniz](https://github.com/richgel999/miniz)（Rich Geldreich 等）和 [praydog](https://github.com/praydog) 的 FunctionHookMinHook。
- [MinHook](https://github.com/TsudaKageyu/minhook)（Tsuda Kageyu，内含 Vyacheslav Patkov 的 Hacker Disassembler Engine）：mod 靠它接入游戏。

**工具**

- [RenderDoc](https://renderdoc.org)（Baldur Karlsson 等）：抓帧分析和离线预览。
- Microsoft 的 HLSL 编译器（fxc、D3DCompiler）和 WARP：编译对照用的着色器、运行测试。
- [Python](https://www.python.org) 和 [Pillow](https://python-pillow.org)：研究脚本。
- [IDA Pro](https://hex-rays.com/ida-pro)（Hex-Rays）和 [ida-pro-mcp](https://github.com/mrexodia/ida-pro-mcp)（mrexodia）：分析游戏程序。
- [Claude Code](https://claude.com/claude-code)（Anthropic）：这个 mod 完全是用 Claude Fable 和 Opus vibe coding 写出来的，代码、文档和逆向分析都出自 Claude，几乎没有经过人工审查。

**游戏与商标**

《热血无赖：终极版》（Sleeping Dogs: Definitive Edition）由 United Front Games 开发、Square Enix 发行，
游戏及其内容的版权归 Square Enix 所有。

与 Square Enix、United Front Games 均无关联。

## English

### What it does

The Definitive Edition's wetness problem (PCGamingWiki's "Wetness Bug on default and many other outfits") is
two unrelated bugs; the mod fixes both:

1. **Wetness that doesn't show** (rain and swimming alike). In the rain the game raises a character's wetness to
   1 and draws them with the wet/sweat permutation of their shader (`HK_CHARACTER*_WS`). That shader takes the
   specular map's x channel (times 10) and z channel as the wet mask for how much glossier the surface gets, and
   darkens the colour by up to 35%. In the Definitive Edition nearly every clothing and skin specular map has
   only y (plain glossiness); x and z are zero: 1 of 62 tops, only the slacks among 46 trousers, against 78 of
   87 hair maps. So even fully wet, clothes barely change; only hair and a few items look wet.

   As the game loads its shaders, the mod inserts three instructions into those 28 permutations (20 distinct
   blobs): where the map's x and z are both zero, use `Shine`/`Gloss` from `SDWet.ini` instead. The two channels
   only matter while a character is wet (or sweaty), so nothing changes when dry, and items with a wet mask of
   their own (hair, slacks) keep it.

2. **Swimming never makes anyone wet**. The swimming action-tree nodes carry `ApplyWetnessOrSweatTrack`, which
   sets wetness to 1. It looks the look component up by `CharacterLookComponent`'s type (`0xCC000005`), but the
   player's look component is registered as its base `CompositeLookComponent` (`0xCC000001`), so the lookup
   always fails and nothing is written. The mod queues these actions' values per character and lets the look
   component apply them in its own next update, without a lookup. Some cutscenes set wetness with the same
   action, and so do pedestrians and thugs carrying an open umbrella in the rain (`DryMeOff!`, by its name
   keeping them dry); the player has no umbrella action.

How wetness builds up in the rain and dries afterwards (1/60 per second) is the game's own logic, unchanged.

The mod also **adds wet footprints after a swim**, which the original never had (not a bug fix). The game's
footprints come from "physics volumes": standing in a blood pool, mud, a puddle or certain placed areas, each
step places that volume's effect and left/right decal, and the decals continue for 5 s after leaving it. The wet
footprint decals (`HK_WetFootPrintLeft/Right_Effect`, volume `PhysVol_WetFootPrints`) are only used by puddles
and a few hand-placed 2 m patches; sea water (`Water`) only splashes, so climbing out leaves no prints. Once the
swimming actions have soaked a character, the mod puts those decals in the footstep override slot (the one the
scripts' `set_footstep_override_effect` uses), only for the duration of each step's handling. It lasts
`WetFootprints` seconds (game time) or until the character has dried that far; steps in shallow water leave
none, and a script's own footprints are left alone.

**Umbrella (added, on by default, `[Umbrella] Enabled = 0` turns it off)**: Wei can only hold an umbrella as a
melee weapon (pedestrians carry them in the rain and drop them when scared); the original has no way to open it.
With one in hand, **holding E** (the game's Action button, gamepad Y, following any remapping) for half a second
plays the upper body animation pedestrians open theirs with (it lives in the shared action library) and the
umbrella opens; holding it again closes it, shaking the water off. E also counters, talks, gets into vehicles and
hires taxis: when the game has a use for that press (the talk prompt is up, or the press made Wei counter, walk to
a car door or hail a taxi, or he isn't walking or standing at all), the umbrella stays as it is. While it's open
E doesn't go straight to the game (again only once it's let go after closing): pressed at a car door, a taxi or
someone to talk to (where the game waits for E with its get-in or talk prompt), Wei closes the umbrella at once,
and when it's folded (about 2 s) the game gets that press as it was; if he walked away or the prompt is gone by
then, the press is dropped. A taxi doesn't wait (it may drive off during the animation), so at a taxi the
umbrella snaps shut and the press goes to the game right away: holding it over 1/3 s hires the taxi, a tap takes
the driver's seat, as in the original. Anywhere else, holding E half a second closes it. Taking a taxi, Wei is
put straight into the back seat: the game's own way (the node with a get-in animation is switched off), and he
keeps whatever he holds. Going into the water (swimming), the umbrella snaps shut and the rules end, so he can
swim fast. Talking to a car park valet, the game takes the umbrella away, closed or not: the game's own doing. F7 opens and closes it too (for testing). While it's open Wei only walks, a brisk walk while Shift is held; no
sprinting, attacks, grabs, pickups, weapon changes, parkour or cover. All through switches the game's scripts
already have (`allow_jog`/`allow_sprint`, action requests disabled by name), restored when it closes or leaves his
hand; changes the game makes meanwhile (such as an interior's forced walk) win. The umbrella's own tree opens it
whenever it rains (for pedestrians), so for the umbrella in Wei's hand "is it raining" answers "is he holding it
open"; pedestrians' umbrellas are untouched. Not done: closing it for cutscenes, staying dry under it,
a brisk walk on gamepads.

### How it works

The installed Steam exe has no symbols. Functions are found by byte signatures made from the legacy v1.0 exe
and its PDB (from the SDmodding project); they match uniquely in both builds. When one isn't found, that
feature stays off and the log says `MISSING`. Hook points:

- `Illusion::StageShader::LoadShader`, where the game hands bytecode from `data\shaders\shaders.temp.bin` to
  D3D11 and then reads it back with `D3DReflect`. Swapping in the patched copy there means no D3D11 hooks and
  nothing to conflict with ReShade. The patch recognizes the wet shaders by structure (they bind `texSpecular`
  and `cbSceneryInstance` and read its sweat and wetness) and recomputes the DXBC checksum, which D3D checks.
- `ApplyWetnessOrSweatTask::Begin/Update/End` and `CharacterLookComponent::Update`: point 2 above.
- `CharacterEffectsComponent::HandleFootstep`: each step's footstep effects; the wet footprints go in here.
- `ActionTreeComponent::update`: right after the player's own action tree, the umbrella updates an upper body
  action controller of its own (made the way the game's `SpawnTask` makes sub-controllers).
  `IsRainingCondition::Match`: the umbrella's rain check. `TSCharacter::Mthd_allow_jog/allow_sprint`: the scripts'
  jog and sprint switches, noted while the umbrella is open. `ReadControllerInputTask::Update`: the PlayerAI tree's
  task that turns the pad and keyboard into action requests. While the umbrella is open it reads E as released (as
  just pressed when the press is handed over), so neither of the game's action trees sees the real E. Masking
  the request wasn't enough: the PlayerAI tree reads requests before the mask applies, and Wei got into cars with
  the umbrella open. Whether E is really held comes straight from the game's input data; whether the game has a use
  for it from the game's own `ActionController::IsPlaying` (what action tree `IsPlaying` conditions call) on Wei's
  tree and on the PlayerAI tree, plus whether a social target is locked (read where the `TargetIsLocked` condition
  reads it), which is the talk prompt; which E the game waits for while the umbrella is open from the prompt node
  the PlayerAI tree is at (`InteractHandler\Prompts\...\Available`).

All the details (offsets, the shader's formula, how it was found) are in [CLAUDE.md](CLAUDE.md); the scripts
used to find them (shader unpacking, material parsing, RenderDoc capture analysis and offline previews) are in
[research/](research/README.md).

### Settings (`plugins\SDWet.ini`)

| Key | Default | Meaning |
|---|---|---|
| `[Wet] WetLook` | 1 | The shader patch (wetness you can see). |
| `[Wet] Shine` | 0.05 | x channel used where the specular map has no wet mask: soaked, glossiness rises by `Shine × 10`. 0 = the game as is; 0.1+ looks like plastic. |
| `[Wet] Gloss` | 0.10 | The same for z: extra wet gloss. |
| `[Wet] ActionWetness` | 1 | Let swimming and other actions set wetness. |
| `[Wet] WetFootprints` | 25 | Seconds of wet footprints after climbing out of the water, 0 = off. Needs `ActionWetness = 1`. |
| `[Umbrella] Enabled` | 1 | The umbrella: with one in hand, holding E opens/closes it (F7 too); with it open, E at a get-in, hire or talk prompt closes it first and then goes to the game; F9 logs the state. The `Prototype = 0` older versions wrote into the ini is no longer read. |
| `[Debug] Logging` | 1 | Write `SDWet.log`, and `SDWet-crash-<n>.dmp` on a crash. |
| `[Debug] LogWetnessTracks` | 0 | Log every wetness action and the player's wetness afterwards. |

Restart the game after changing them.

### The log

- `shaders: wet/sweat pixel shader #N patched`: 20 lines at start-up.
- `wetness: applied a track to look ... registered as type 0xCC000001; the game looks for 0xCC000005`: on the
  first swim; fix 2 at work.
- `footprints: sim object ... soaked`, `first wet footprint`, and `WetFootprints` seconds out of the water
  `... N wet footprints, M steps in water ...`: the wet footprints from start to end.
- `umbrella:` lines: each step of the umbrella (`E held ... s: open/close` for the hold, `E held, but it's the
  game's: ...` with why that E was left to the game, `E kept from the game` / `E back to the game` / `E pressed for
  the game`, `E pressed at the ... prompt with the umbrella open`, `E goes to the game for the ... prompt` once it's
  closed (or `... the press is dropped` and why), `Wei is in the water` when it closes for swimming, animations, the
  umbrella's state, slow/brisk walk, every change to the jog/sprint switches and who made it as
  `player can [not] jog, ...`, and the `rain check for the umbrella in hand` answers).
- `crash:` lines: where a crash happened, with the stack. The game crashes on every exit (an old problem of its
  own, at an address ending in `488C`); ignore that one.
- `MISSING` or `not hooked`: a game function wasn't found and that feature is off.

### Requirements and compatibility

- Both released builds of Sleeping Dogs: Definitive Edition: the current Steam one (verified in game) and the
  legacy v1.0 (the signatures match it uniquely too), Windows 10/11 x64. No Windows-only services, so it should
  run under Wine/Proton/CrossOver (untested).
- Any ASI loader, e.g. [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) (`SDWet.zip`
  ships one as `dinput8.dll`).
- Works with ReShade (tested with ReShade 6.8.0 installed). Changes no game files and doesn't touch saves.

### Downloads

Every version on [Releases](https://github.com/aUsernameWoW/sleeping-dogs-wet-officer-shen/releases) has:

| File | Contents |
| --- | --- |
| `SDWet.zip` | Unpacks into the game folder: `dinput8.dll` (Ultimate ASI Loader) + `plugins\SDWet.asi` + notices |
| `SDWet.asi` | The mod alone, for the `plugins\` folder of an existing loader |
| `SDWet.pdb` | Debug symbols, only needed to read crash dumps |
| `THIRD-PARTY-NOTICES.md` | Licenses of the third-party code |

Every commit on `main` is built, tested and published as a prerelease `build-<N>` (not tested in game).
Builds verified in game are promoted to full releases; the README's download link points to the newest one.

If you already have an ASI loader (whether it's called `dinput8.dll`, `winmm.dll` or something else), just put
`SDWet.asi` where it loads plugins from (usually `plugins\`). `SDWet.ini` and `SDWet.log` are written next to
the `.asi`.

### Building and tests

Visual Studio 2022 (v143), Windows SDK 10.0.26100. The project has to sit in the modding workspace as
`mods\SDWet`, next to `reference\minhook` ([MinHook](https://github.com/TsudaKageyu/minhook) v1.3.4 source,
compiled in). From the workspace root, `.\tools\build.ps1 -Mod SDWet -Test` builds and runs `tests\`:

- `dxbc_test`: a shader with the same structure compiled with D3DCompile: checksum, recognition, corrupted input,
  and actual WARP renders with and without a wet mask, wet and dry.
- `game_shaders_test`: all 2237 shaders of the installed game: checksums verify, exactly the 28 wet
  permutations are patched, D3D11 accepts each (skipped without the game).
- `load_test`: loads the .asi outside the game: no crash, default ini written, missing game functions reported.
- `umbrella_load_test`: the same with an ini older versions wrote (`[Umbrella] Prototype = 0`): the umbrella stays
  on.
- `umbrella_off_test`: with `[Umbrella] Enabled = 0` the umbrella is off and its game functions aren't looked for.

GitHub Actions builds pushes and PRs in the same layout (`-warnAsError`) and runs the automated tests, against
the exact dependency versions in `.github/reference.env`; then it packages `SDWet.zip`, with the Ultimate ASI
Loader version and SHA-256 pinned in `.github/asi-loader.env`. Builds of `main` that pass are published as
prereleases `build-<N>`. `asi-loader.yml` checks monthly for a newer Ultimate ASI Loader and opens a PR that
moves `asi-loader.env`; `reference.yml` does the same for the build's dependencies and `reference.env`;
Dependabot updates the Actions monthly. The Nexus Mods uploads (`build.yml`'s `nexus` job and
`nexus-release.yml`) stay off until there is a Nexus page and the repository variables are set.

### Credits

This mod uses or builds on the work of these people and projects. Thank you.

**Research**

- [PCGamingWiki](https://www.pcgamingwiki.com/wiki/Sleeping_Dogs:_Definitive_Edition): its write-up of the Definitive
  Edition's wetness problem ("Wetness Bug on default and many other outfits").
- [SDmodding](https://github.com/SDmodding), almost all of it the work of one person, [sneakyevil](https://github.com/sneakyevil). This mod used:
  - the [Visual Studio 2022 project template](https://github.com/SDmodding/SDK/releases/tag/vs2022) released with SDmodding's [SDK](https://github.com/SDmodding/SDK): the mod's Visual Studio project derives from it, including its build settings and the source layout that starts at `dllmain.cc`;
  - the game's v1.0 exe and its debug symbols (PDB, shipped with the original Steam release), shared by
    SDmodding: the function signatures and the game's data structures come from them;
  - the [SDK](https://github.com/SDmodding/SDK) and [TheoryEngine](https://github.com/SDmodding/TheoryEngine): the material and shader resource layouts, and the game's string hash; the SDK's game input (`ActionDef_Action`, the E key, and its data layout) and list of target types: holding E for the umbrella;
  - the action trees (ActionNodes) exported in [Files](https://github.com/SDmodding/Files): the umbrella;
  - [BigFileSystem](https://github.com/SDmodding/BigFileSystem), [TheoryEngine](https://github.com/SDmodding/TheoryEngine), and the file name lists in sneakyevil's [SD-BigFileExplorer](https://github.com/sneakyevil/SD-BigFileExplorer) and in [Ekey](https://github.com/Ekey)'s
    SDDEUnpacker: the tool that reads the game's `.big` archives follows them; the research scripts (`research/`) read the game's materials, shaders and textures with it.

**Code in the mod** (full license texts in [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md))

- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) (ThirteenAG): the `dinput8.dll` in the zip, which makes the game load mods.
  It contains MinHook, [miniz](https://github.com/richgel999/miniz) (Rich Geldreich and others) and [praydog](https://github.com/praydog)'s FunctionHookMinHook.
- [MinHook](https://github.com/TsudaKageyu/minhook) (Tsuda Kageyu, with Vyacheslav Patkov's Hacker Disassembler Engine): how the mod hooks into the game.

**Tools**

- [RenderDoc](https://renderdoc.org) (Baldur Karlsson and others): frame captures and offline previews.
- Microsoft's HLSL compiler (fxc, D3DCompiler) and WARP: reference shaders and the tests.
- [Python](https://www.python.org) and [Pillow](https://python-pillow.org): the research scripts.
- [IDA Pro](https://hex-rays.com/ida-pro) (Hex-Rays) and [ida-pro-mcp](https://github.com/mrexodia/ida-pro-mcp) (mrexodia): analyzing the game's code.
- [Claude Code](https://claude.com/claude-code) (Anthropic): this mod was fully vibe-coded with Claude Fable and Opus; its code,
  documentation and reverse engineering are all Claude's, with little human review.

**The game and trademarks**

Sleeping Dogs: Definitive Edition was developed by United Front Games and published by Square Enix; the game
and its content are © Square Enix.

Not affiliated with Square Enix or United Front Games.
