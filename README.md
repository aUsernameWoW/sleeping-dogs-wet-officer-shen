# Sleeping Dogs: Definitive Edition — wet clothes and skin (SDWet)

> [!IMPORTANT]
> **关于这个 mod**：它完全是用 Claude Code 里的 Claude Fable 和 Opus vibe coding 写出来的，几乎没有经过审查，请当作实验性质的 mod 使用，发现异常请反馈。作为一个长期缺少 mod 而自己下场做 mod 的普通玩家，我在 vibe 的过程中收获了许多快乐；如果你也有想实现的灵感，不妨也试着 vibe 一下。由于这些代码都是 vibe 出来的，所以我不会以我的 mod 盈利，也不接受捐助。如果你喜欢我的作品，请考虑向[致谢](#致谢)中提到的开源项目和作者捐赠，祝你游玩愉快！
>
> **About this mod**: it was fully vibe-coded with Claude Fable and Opus in Claude Code, with little review, so treat
> it as experimental and please report anything unusual. I'm just an ordinary player who went a long time without
> mods for this game and finally started making them myself. Vibe coding them has been a lot of fun; if you have an
> idea of your own, it might be for you too. Since all this code is vibe-coded, I won't make money from my mods and
> don't accept donations. If you like my work, please consider donating to the open-source projects and authors
> listed in the [Credits](#credits) instead. Have fun!

[中文](#中文) | [English](#english)

## 中文

让《热血无赖：终极版》里的角色**淋雨和游泳后真的变湿**：

- 下雨时，衣服和皮肤会慢慢变湿，颜色变深、开始反光；雨停后大约一分钟内逐渐变干；
- 从水里爬上岸时，Wei 浑身湿透，然后同样慢慢变干；
- 上岸后的二十几秒里，走过的地方会留下湿脚印；
- 下雨天路人受了惊吓会扔下手里的伞，捡起来以后**长按 E** 就能撑开，再长按 E 收起。撑着伞只能走路；撑着伞在车门边、出租车旁或能搭话的人面前按 E，Wei 会先收伞再上车（打车、搭话）；下水时伞会自动收起。

湿身效果游戏本来就设计了，但在终极版里，大部分衣服和皮肤都看不出湿，游泳后也完全不湿。这个 mod 把它修好了。游戏的其他画面不受影响。

状态：**早期版本**。在作者的电脑上测试正常。

> 适用于**任何版本**的《热血无赖：终极版》，Windows 10/11 64 位。想了解原理、自己编译或调参数，请看
> [ADVANCED.md](ADVANCED.md)。

### 安装（大约三分钟）

**第 1 步：下载**

点这里下载 **[SDWet.zip](https://github.com/aUsernameWoW/sleeping-dogs-wet-officer-shen/releases/latest/download/SDWet.zip)**。

压缩包里只有这些：

```text
dinput8.dll                  ← Ultimate ASI Loader：让游戏加载 mod 的“加载器”
plugins\
    SDWet.asi                ← mod 本体
    SDWet-THIRD-PARTY-NOTICES.md
```

**第 2 步：打开游戏文件夹**

1. 打开 Steam，进入「库」。
2. 在左侧列表里右键点「Sleeping Dogs: Definitive Edition」→「管理」→「浏览本地文件」。
3. 弹出来的就是游戏文件夹，里面有 `sdhdship.exe`（如果电脑不显示扩展名，就是一个叫 `sdhdship` 的程序）。

**第 3 步：把文件放进去**

1. 双击打开下载的 `SDWet.zip`。
2. 选中里面的 `dinput8.dll` 和 `plugins` 文件夹，一起拖进游戏文件夹。
3. 如果 Windows 弹出「替换或跳过文件」，说明游戏文件夹里已经有 `dinput8.dll` 了（你以前装过别的 mod，加载器已经在了），选「跳过该文件」。已有的 `plugins` 文件夹会自动合并，不用管。

放好后，游戏文件夹里应该是这样（只列出相关的部分）：

```text
SleepingDogsDefinitiveEdition\
    sdhdship.exe
    dinput8.dll
    plugins\
        SDWet.asi
```

注意 `dinput8.dll` 要和 `sdhdship.exe` 在同一层，不要多套一层文件夹。

**第 4 步：启动游戏**

照常从 Steam 启动游戏。`plugins` 里多出 `SDWet.ini` 和 `SDWet.log` 两个文件，就说明 mod 已经加载。

想马上看效果：跳进海里游几秒，再爬上岸，走几步回头看看。

### 常见问题

**觉得太亮或者不够湿**

用记事本打开 `plugins\SDWet.ini`，改 `Shine`（湿了以后的反光强度，默认 0.05，0 就是游戏原样），保存后重启游戏。每一项都有中文说明。

**不想要湿脚印，或者想让它留久一点**

同样在 `plugins\SDWet.ini` 里，改 `WetFootprints`（上岸后留脚印的秒数，默认 25，0 就是不留），保存后重启游戏。

**不想要撑伞**

在 `plugins\SDWet.ini` 的 `[Umbrella]` 下面写上 `Enabled = 0`，保存后重启游戏。

**`plugins` 里没有 `SDWet.log`**

说明 mod 没被加载：检查 `dinput8.dll` 是否和 `sdhdship.exe` 在同一层，杀毒软件有没有删掉它（ASI 加载器偶尔会被误报，可以从隔离区还原并把游戏文件夹加入排除项）。如果第 3 步跳过了原有的 `dinput8.dll`，那个文件可能不是 ASI 加载器，备份后换成压缩包里的。

**更新**

下载新的 `SDWet.zip`，只把里面的 `plugins` 文件夹拖进游戏文件夹，Windows 询问时选「替换目标中的文件」。`SDWet.ini` 不在压缩包里，你的设置会保留。

**卸载**

删掉 `plugins` 里的 `SDWet.asi`、`SDWet.ini` 和 `SDWet.log`。如果 `plugins` 里已经没有其他 `.asi` 文件了，`dinput8.dll` 也可以删掉。

**遇到问题怎么反馈**

在 [GitHub Issues](https://github.com/aUsernameWoW/sleeping-dogs-wet-officer-shen/issues) 里说明情况，并附上
`plugins\SDWet.log`。

### 致谢

这个 mod 用到或参考了下面这些人和项目的成果，在此致谢。

**研究资料**

- [PCGamingWiki](https://www.pcgamingwiki.com/wiki/Sleeping_Dogs:_Definitive_Edition)：上面记录了终极版的湿身问题（“Wetness Bug on default and many other outfits”）。
- [SDmodding](https://github.com/SDmodding)，几乎全部出自 [sneakyevil](https://github.com/sneakyevil) 一人之手。这个 mod 用到了：
  - SDmodding 随 [SDK](https://github.com/SDmodding/SDK) 发布的 [Visual Studio 2022 项目模板](https://github.com/SDmodding/SDK/releases/tag/vs2022)：这个 mod 的 Visual Studio 工程源自这个模板，编译设置和以 `dllmain.cc` 为起点的源文件结构都来自它；
  - SDmodding 分享的游戏 v1.0 版 exe 和调试符号（PDB，Steam 首发版自带）：函数特征码和游戏的数据结构都是从这里查到的；
  - [SDK](https://github.com/SDmodding/SDK) 和 [TheoryEngine](https://github.com/SDmodding/TheoryEngine)：材质与着色器资源的结构，以及游戏的字符串哈希；SDK 里的游戏输入（E 键对应的 `ActionDef_Action` 及其数据结构）和目标类型列表：长按 E 撑伞；
  - [Files](https://github.com/SDmodding/Files) 里导出的动作树（ActionNodes）：撑伞；
  - [BigFileSystem](https://github.com/SDmodding/BigFileSystem)、[TheoryEngine](https://github.com/SDmodding/TheoryEngine)，以及 sneakyevil 的 [SD-BigFileExplorer](https://github.com/sneakyevil/SD-BigFileExplorer) 和 [Ekey](https://github.com/Ekey) 的 SDDEUnpacker 里的文件名列表：读取游戏资源包（`.big`）的工具是照着它们写的，研究脚本（`research/`）用它读取游戏的材质、着色器和贴图。

**mod 里包含的代码**（许可证全文见 [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)）

- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader)（ThirteenAG）：压缩包里的 `dinput8.dll`，让游戏加载 mod。它本身还包含 MinHook、[miniz](https://github.com/richgel999/miniz)（Rich Geldreich 等）和 [praydog](https://github.com/praydog) 的 FunctionHookMinHook。
- [MinHook](https://github.com/TsudaKageyu/minhook)（Tsuda Kageyu，内含 Vyacheslav Patkov 的 Hacker Disassembler Engine）：mod 靠它接入游戏。

**工具**

- [RenderDoc](https://renderdoc.org)（Baldur Karlsson 等）：抓帧分析和离线预览。
- Microsoft 的 HLSL 编译器（fxc、D3DCompiler）和 WARP：编译对照用的着色器、运行测试。
- [Python](https://www.python.org) 和 [Pillow](https://python-pillow.org)：研究脚本。
- [IDA Pro](https://hex-rays.com/ida-pro)（Hex-Rays）和 [ida-pro-mcp](https://github.com/mrexodia/ida-pro-mcp)（mrexodia）：分析游戏程序。
- [Claude Code](https://claude.com/claude-code)（Anthropic）：这个 mod 完全是用 Claude Fable 和 Opus vibe coding 写出来的，代码、文档和逆向分析都出自 Claude，几乎没有经过人工审查。

**游戏与商标**

《热血无赖：终极版》（Sleeping Dogs: Definitive Edition）由 United Front Games 开发、Square Enix 发行，游戏及其内容的版权归 Square Enix 所有。

与 Square Enix、United Front Games 均无关联。

## English

Makes characters in Sleeping Dogs: Definitive Edition **actually get wet** in the rain and after a swim:

- in the rain, clothes and skin slowly get wet: darker and glossy; after the rain stops they dry within about a
  minute;
- climbing out of the water, Wei is soaked, and dries the same way;
- for the first twenty-odd seconds out of the water, he leaves wet footprints;
- in the rain, scared pedestrians drop their umbrellas: pick one up and **hold E** to open it, hold E again to close
  it. Under it, Wei walks; press E at a car door, a taxi or someone to talk to and he closes it first, then gets in
  (hires the taxi, talks); in the water it closes by itself.

The game was made with the wet look, but in the Definitive Edition most clothes and skin never look wet, and
swimming doesn't make anyone wet at all. This mod fixes that. Nothing else in the game's look changes.

Status: **early**. Works on the author's PC.

> Works with **any version** of Sleeping Dogs: Definitive Edition, Windows 10/11 64-bit. How it works,
> building it and all settings: [ADVANCED.md](ADVANCED.md).

### Install (about three minutes)

**Step 1: download**

Download **[SDWet.zip](https://github.com/aUsernameWoW/sleeping-dogs-wet-officer-shen/releases/latest/download/SDWet.zip)**.

It only contains:

```text
dinput8.dll                  ← Ultimate ASI Loader: what makes the game load mods
plugins\
    SDWet.asi                ← the mod
    SDWet-THIRD-PARTY-NOTICES.md
```

**Step 2: open the game folder**

1. Open Steam and go to your Library.
2. Right-click "Sleeping Dogs: Definitive Edition" → Manage → Browse local files.
3. That's the game folder; it contains `sdhdship.exe` (or `sdhdship`, if file extensions are hidden).

**Step 3: put the files in**

1. Open the downloaded `SDWet.zip`.
2. Select `dinput8.dll` and the `plugins` folder and drag both into the game folder.
3. If Windows asks whether to replace or skip a file, the game folder already has a `dinput8.dll` (you've
   installed a mod before and the loader is there): choose "Skip this file". An existing `plugins` folder is
   merged automatically.

Afterwards the game folder should look like this (only the relevant part):

```text
SleepingDogsDefinitiveEdition\
    sdhdship.exe
    dinput8.dll
    plugins\
        SDWet.asi
```

`dinput8.dll` has to be next to `sdhdship.exe`, not in a subfolder.

**Step 4: start the game**

Start the game from Steam as usual. When `SDWet.ini` and `SDWet.log` appear in `plugins`, the mod is loaded.

To see it right away: jump into the sea, swim for a few seconds, climb out, walk a few steps and look back.

### FAQ

**Too shiny, or not wet enough**

Open `plugins\SDWet.ini` in Notepad and change `Shine` (how much wet clothes and skin shine; default 0.05, 0 is
the game as is), save and restart the game. Every setting is explained in the file.

**No wet footprints wanted, or longer ones**

In the same `plugins\SDWet.ini`, change `WetFootprints` (seconds of footprints after climbing out; default 25,
0 = none), save and restart the game.

**No umbrella wanted**

In `plugins\SDWet.ini`, write `Enabled = 0` under `[Umbrella]`, save and restart the game.

**There's no `SDWet.log` in `plugins`**

The mod wasn't loaded: check that `dinput8.dll` is next to `sdhdship.exe` and that your antivirus didn't remove
it (ASI loaders are sometimes flagged; restore it from quarantine and exclude the game folder). If you skipped
an existing `dinput8.dll` in step 3, that file may not be an ASI loader: back it up and use the one from the zip.

**Updating**

Download the new `SDWet.zip` and drag only its `plugins` folder into the game folder; choose "Replace the files
in the destination". `SDWet.ini` isn't in the zip, so your settings stay.

**Uninstalling**

Delete `SDWet.asi`, `SDWet.ini` and `SDWet.log` from `plugins`. If there are no other `.asi` files left in
`plugins`, you can delete `dinput8.dll` too.

**Reporting a problem**

Describe it in [GitHub Issues](https://github.com/aUsernameWoW/sleeping-dogs-wet-officer-shen/issues) and attach
`plugins\SDWet.log`.

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
