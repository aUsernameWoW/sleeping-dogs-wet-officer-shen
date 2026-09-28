# Sleeping Dogs: Definitive Edition — wet clothes and skin (SDWet)

[中文](#中文) | [English](#english)

## 中文

让《热血无赖：终极版》里的角色**淋雨和游泳后真的变湿**：

- 下雨时，衣服和皮肤会慢慢变湿，颜色变深、开始反光；雨停后大约一分钟内逐渐变干；
- 从水里爬上岸时，Wei 浑身湿透，然后同样慢慢变干；
- 上岸后的二十几秒里，走过的地方会留下湿脚印。

游戏本来就设计了这个效果，但在终极版里，大部分衣服和皮肤都看不出湿，游泳后也完全不湿。这个 mod 把它修好了。
游戏的其他画面不受影响。

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
3. 如果 Windows 弹出「替换或跳过文件」，说明游戏文件夹里已经有 `dinput8.dll` 了（你以前装过别的 mod，
   加载器已经在了），选「跳过该文件」。已有的 `plugins` 文件夹会自动合并，不用管。

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

用记事本打开 `plugins\SDWet.ini`，改 `Shine`（湿了以后的反光强度，默认 0.05，0 就是游戏原样），保存后重启
游戏。每一项都有中文说明。

**不想要湿脚印，或者想让它留久一点**

同样在 `plugins\SDWet.ini` 里，改 `WetFootprints`（上岸后留脚印的秒数，默认 25，0 就是不留），保存后重启游戏。

**`plugins` 里没有 `SDWet.log`**

说明 mod 没被加载：检查 `dinput8.dll` 是否和 `sdhdship.exe` 在同一层，杀毒软件有没有删掉它（ASI 加载器偶尔
会被误报，可以从隔离区还原并把游戏文件夹加入排除项）。如果第 3 步跳过了原有的 `dinput8.dll`，那个文件可能
不是 ASI 加载器，备份后换成压缩包里的。

**更新**

下载新的 `SDWet.zip`，只把里面的 `plugins` 文件夹拖进游戏文件夹，Windows 询问时选「替换目标中的文件」。
`SDWet.ini` 不在压缩包里，你的设置会保留。

**卸载**

删掉 `plugins` 里的 `SDWet.asi`、`SDWet.ini` 和 `SDWet.log`。如果 `plugins` 里已经没有其他 `.asi` 文件了，
`dinput8.dll` 也可以删掉。

**遇到问题怎么反馈**

在 [GitHub Issues](https://github.com/aUsernameWoW/sleeping-dogs-wet-officer-shen/issues) 里说明情况，并附上
`plugins\SDWet.log`。

与 Square Enix、United Front Games 均无关联。

## English

Makes characters in Sleeping Dogs: Definitive Edition **actually get wet** in the rain and after a swim:

- in the rain, clothes and skin slowly get wet: darker and glossy; after the rain stops they dry within about a
  minute;
- climbing out of the water, Wei is soaked, and dries the same way;
- for the first twenty-odd seconds out of the water, he leaves wet footprints.

The game was made with this effect, but in the Definitive Edition most clothes and skin never look wet, and
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

Not affiliated with Square Enix or United Front Games.
