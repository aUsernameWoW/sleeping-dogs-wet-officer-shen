# research — the tools SDWet was found and verified with

Scripts for picking the game's rendering apart. None of them ship; they need the modding workspace
(`tools\extract` for the archives) and the installed game (`%SDDE_DIR%`, default
`D:\SteamLibrary\steamapps\common\SleepingDogsDefinitiveEdition`). Everything they write goes to
`build\research\` (gitignored: it's game data).

Python 3.11+. Scripts that decode textures (`materials.py`, `specsurvey.py`) need Pillow: run them with
`tools\extract\build\venv\Scripts\python.exe` (`tools\extract.ps1` creates that venv on first use). Run them
from this folder (`cd research`), since they import each other.

## Shaders — `shaders.py`, `dxbc.py`, `patch.py`

| Command | What for |
|---|---|
| `python shaders.py unpack` | Every DXBC blob of `data\shaders\shaders.temp.bin` → `build\research\shaders\NAME.PSBIN` etc., `names.json` (DXBC checksum → names, including `+SDWet` for the patched ones) and `variants.json` + `variants\<tag>\` for RenderDoc previews. Run this first. |
| `python shaders.py maskreads [REGEX] [-v]` | Which `cbSceneryInstance.Mask` components each character pixel shader reads (`.yzw` = the `_WS` wet/sweat permutations). Uses `fxc /dumpbin` (Windows SDK). |
| `python shaders.py bindings [REGEX]` | Bound textures and cbuffers with their slots. |
| `python shaders.py cbuffer NAME cbSceneryInstance` | Member layout of a cbuffer. |
| `python shaders.py disasm NAME` | Disassembly. |
| `python dxbc.py [x.o ...]` | Checks the checksum implementation on all unpacked shaders; dumps instruction tokens of compiled snippets (how the inserted encodings were found: compile HLSL with `fxc /T ps_4_0 /Fo x.o /Fc x.asm` and compare). |
| `python patch.py [shine gloss]` | The Python original of `core/dxbc.cc`'s patch; lists what it patches (28 names, 20 unique blobs). |

The blobs keep their reflection data, so names like `texSpecular` and `cbSceneryInstance` are all there.
Permutation suffixes: `DR` deferred, `WS` wet/sweat, `OT` overlay texture, `EM` emissive, `MC`/`RI` unknown.

## Materials and textures — `materials.py`, `specsurvey.py`

| Command | What for |
|---|---|
| `materials.py dump "Data\Characters_New\Wei_Head.perm.bin"` | Every material of a file: shader template, textures, state blocks. |
| `materials.py scan [PATH-REGEX]` | Shader template of every character material (all 2042 use `HK_CHARACTER*`). |
| `specsurvey.py` | Which `*_S` specular maps have data in x (the wet mask): why most clothes look dry. |

Texture names come from `tools\extract.ps1 find/export` (e.g. `export '^T_VESTPUFFY_S$' out`).

## Frame captures — `renderdoc.ps1` + `renderdoc\*.py`

RenderDoc 1.46 (`winget install BaldurKarlsson.RenderDoc`). `shaders.py unpack` first.

```powershell
.\research\renderdoc.ps1 launch                     # game under RenderDoc; PrtScn or F12 captures a frame
.\research\renderdoc.ps1 report  <capture.rdc>      # per character draw: permutation, Mask (sweat/wetness), diffuse PNG
.\research\renderdoc.ps1 frame   <capture.rdc>      # the presented image
.\research\renderdoc.ps1 targets <capture.rdc> -Event N   # render targets at event N (G-buffer: albedo, gloss in rt1.a)
.\research\renderdoc.ps1 preview <capture.rdc>      # the frame as captured and with the de/default/strong shaders
```

- `launch` starts `sdhdship.exe` directly, which needs `steam_appid.txt` (`307690`) in the game folder; the
  script creates it. Steam must be running. ReShade, the ASI loader and the mods load normally.
- The analysis scripts run inside `qrenderdoc --python` and exit before its window opens. qrenderdoc's Python
  has no `__file__`; the scripts take everything from environment variables, which the wrapper sets.
- `targets` is the honest way to compare looks: the G-buffer doesn't depend on lighting. Find the event of the
  last character draw in `report`'s output.
- `preview` swaps shaders with `BuildTargetShader` + `ReplaceResource` on replay: tuning `Shine`/`Gloss`
  without starting the game. Add tags to `VARIANTS` in `shaders.py` and re-run `unpack`.

## Footprints — `footprints.py`

`footprints.py` (extract venv): every `PhysicsVolumeProperties` of `Data\Global\PhysicsProperties.perm.bin`
with its footstep effect, left/right decals, how long the decals continue after leaving, damage and fire; the
surfaces that count as a volume (`Water`, `Mud`); the decal textures. Where `core/footprints.cc`'s facts come
from (wet footprint decals exist, only puddles and placed volumes use them). The file isn't in the community
name lists; the script reads it by path. Its docstring has the qReflect layout.

## Signatures — `sigcheck.py`

`python sigcheck.py "Name=48 8B C4 ? ? 57"`: match count in the installed exe and in the legacy v1.0 exe
(`reference\SDmodding\game-itself`). Take the bytes from the legacy PDB's IDA database (IDA MCP `get_bytes`),
wildcard rel32/RIP displacements, and extend until both builds match once.
