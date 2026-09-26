# SDWet — advanced users and developers

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
   记下来，由外观组件在下一帧自己更新时写入，不经过查找。同一套动作还负责打伞时变干等效果。

雨中湿度的累积和雨停后的变干（每秒 1/60）都是游戏原有的逻辑，没有改动。

### 原理

安装版 Steam exe 不带符号，函数靠字节特征码定位；特征码取自旧版 v1.0 exe 及其 PDB（来自 SDmodding 项目），
在两个版本里都唯一匹配。找不到时对应功能不启用，日志里写 `MISSING`。hook 点：

- `Illusion::StageShader::LoadShader`：游戏把 `data\shaders\shaders.temp.bin` 里的字节码交给 D3D11 创建着色器、
  再用 `D3DReflect` 读取反射信息的地方。在这里换成补丁版，所以不需要 hook D3D11，也不会和 ReShade 冲突。
  补丁按结构识别湿身着色器（绑定了 `texSpecular` 和 `cbSceneryInstance`，并读取其中的汗水和湿度），重新计算
  DXBC 校验和，D3D 会校验它。
- `ApplyWetnessOrSweatTask::Begin/Update/End` 和 `CharacterLookComponent::Update`：见上面第 2 条。

所有细节（结构偏移、着色器公式、调查过程）见 [CLAUDE.md](CLAUDE.md)（英文）；调查用的脚本（着色器解包、
材质解析、RenderDoc 抓帧分析与离线预览）在 [research/](research/README.md)。

### 设置（`plugins\SDWet.ini`）

| 项 | 默认 | 说明 |
|---|---|---|
| `[Wet] WetLook` | 1 | 着色器补丁（让湿身看得出来）。 |
| `[Wet] Shine` | 0.05 | 贴图没有湿身遮罩时的 x 通道默认值：湿透时光泽度增加 `Shine × 10`。0 = 游戏原样，0.1 以上接近塑料感。 |
| `[Wet] Gloss` | 0.10 | 同上，z 通道：额外的湿润光泽。 |
| `[Wet] ActionWetness` | 1 | 让游泳、打伞等动作设置的湿度生效。 |
| `[Debug] Logging` | 1 | 写 `SDWet.log`。 |
| `[Debug] LogWetnessTracks` | 0 | 记录每个湿度动作，以及之后玩家湿度的变化曲线。 |

改完重启游戏生效。

### 日志

- `shaders: wet/sweat pixel shader #N patched`：游戏启动时应有 20 行。
- `wetness: applied a track to look ... registered as type 0xCC000001; the game looks for 0xCC000005`：第一次
  游泳时出现，说明第 2 条修复在工作。
- `MISSING` 或 `not hooked`：没找到对应的游戏函数，该功能关闭。

### 需求与兼容性

- 《热血无赖：终极版》的两个发行版本：当前 Steam 版（在游戏里验证过）和旧版 v1.0（特征码在它上面同样唯一
  匹配），Windows 10/11 x64。不依赖 Windows 专有服务，应当能在 Wine/Proton/CrossOver 下运行（未测试）。
- 任意 ASI 加载器，例如 [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader)（`SDWet.zip`
  里自带一份，作为 `dinput8.dll`）。
- 与 ReShade 兼容（在装有 ReShade 6.8.0 的环境里测试）。不修改任何游戏文件，也不影响存档。

### 编译与测试

Visual Studio 2022（v143），Windows SDK 10.0.26100。项目需要放在工作区的 `mods\SDWet`，工作区里还要有
`reference\minhook`（[MinHook](https://github.com/TsudaKageyu/minhook) v1.3.4 源码，随项目一起编译）。
在工作区根目录运行 `.\tools\build.ps1 -Mod SDWet -Test`，会编译并运行 `tests\` 里的测试：

- `dxbc_test`：用 D3DCompile 编一个结构相同的着色器，检查校验和、识别逻辑、对损坏输入的处理，并在 WARP 上实际
  渲染，验证有无湿身遮罩、干湿两种状态下的输出。
- `game_shaders_test`：用已安装游戏的全部 2237 个着色器验证：校验和全对、恰好补丁 28 个湿身变体、D3D11 全部
  接受（没装游戏时跳过）。
- `load_test`：在游戏之外加载 .asi，不能崩溃，并报告找不到游戏函数。

### 致谢

- [SDmodding](https://github.com/SDmodding)：旧版 PDB、SDK 和 TheoryEngine 头文件（材质与着色器资源的结构）。
- [MinHook](https://github.com/TsudaKageyu/minhook)。
- [RenderDoc](https://renderdoc.org)：调查时的抓帧与离线预览。
- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader)。
- [PCGamingWiki](https://www.pcgamingwiki.com/wiki/Sleeping_Dogs:_Definitive_Edition) 上的问题记录。

第三方代码及其许可证见 [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)。

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
   component apply them in its own next update, without a lookup. The same actions also dry you under an
   umbrella.

How wetness builds up in the rain and dries afterwards (1/60 per second) is the game's own logic, unchanged.

### How it works

The installed Steam exe has no symbols. Functions are found by byte signatures made from the legacy v1.0 exe
and its PDB (from the SDmodding project); they match uniquely in both builds. When one isn't found, that
feature stays off and the log says `MISSING`. Hook points:

- `Illusion::StageShader::LoadShader`, where the game hands bytecode from `data\shaders\shaders.temp.bin` to
  D3D11 and then reads it back with `D3DReflect`. Swapping in the patched copy there means no D3D11 hooks and
  nothing to conflict with ReShade. The patch recognizes the wet shaders by structure (they bind `texSpecular`
  and `cbSceneryInstance` and read its sweat and wetness) and recomputes the DXBC checksum, which D3D checks.
- `ApplyWetnessOrSweatTask::Begin/Update/End` and `CharacterLookComponent::Update`: point 2 above.

All the details (offsets, the shader's formula, how it was found) are in [CLAUDE.md](CLAUDE.md); the scripts
used to find them (shader unpacking, material parsing, RenderDoc capture analysis and offline previews) are in
[research/](research/README.md).

### Settings (`plugins\SDWet.ini`)

| Key | Default | Meaning |
|---|---|---|
| `[Wet] WetLook` | 1 | The shader patch (wetness you can see). |
| `[Wet] Shine` | 0.05 | x channel used where the specular map has no wet mask: soaked, glossiness rises by `Shine × 10`. 0 = the game as is; 0.1+ looks like plastic. |
| `[Wet] Gloss` | 0.10 | The same for z: extra wet gloss. |
| `[Wet] ActionWetness` | 1 | Let swimming, umbrellas and other actions set wetness. |
| `[Debug] Logging` | 1 | Write `SDWet.log`. |
| `[Debug] LogWetnessTracks` | 0 | Log every wetness action and the player's wetness afterwards. |

Restart the game after changing them.

### The log

- `shaders: wet/sweat pixel shader #N patched`: 20 lines at start-up.
- `wetness: applied a track to look ... registered as type 0xCC000001; the game looks for 0xCC000005`: on the
  first swim; fix 2 at work.
- `MISSING` or `not hooked`: a game function wasn't found and that feature is off.

### Requirements and compatibility

- Both released builds of Sleeping Dogs: Definitive Edition: the current Steam one (verified in game) and the
  legacy v1.0 (the signatures match it uniquely too), Windows 10/11 x64. No Windows-only services, so it should
  run under Wine/Proton/CrossOver (untested).
- Any ASI loader, e.g. [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) (`SDWet.zip`
  ships one as `dinput8.dll`).
- Works with ReShade (tested with ReShade 6.8.0 installed). Changes no game files and doesn't touch saves.

### Building and tests

Visual Studio 2022 (v143), Windows SDK 10.0.26100. The project has to sit in the modding workspace as
`mods\SDWet`, next to `reference\minhook` ([MinHook](https://github.com/TsudaKageyu/minhook) v1.3.4 source,
compiled in). From the workspace root, `.\tools\build.ps1 -Mod SDWet -Test` builds and runs `tests\`:

- `dxbc_test`: a shader with the same structure compiled with D3DCompile: checksum, recognition, corrupted input,
  and actual WARP renders with and without a wet mask, wet and dry.
- `game_shaders_test`: all 2237 shaders of the installed game: checksums verify, exactly the 28 wet
  permutations are patched, D3D11 accepts each (skipped without the game).
- `load_test`: loads the .asi outside the game: no crash, missing game functions reported.

### Credits

- [SDmodding](https://github.com/SDmodding): the legacy PDB, the SDK and the TheoryEngine headers (material and
  shader resource layouts).
- [MinHook](https://github.com/TsudaKageyu/minhook).
- [RenderDoc](https://renderdoc.org): frame captures and offline previews during the investigation.
- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader).
- The issue write-up on [PCGamingWiki](https://www.pcgamingwiki.com/wiki/Sleeping_Dogs:_Definitive_Edition).

Third-party code and its licenses: [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).

Not affiliated with Square Enix or United Front Games.
