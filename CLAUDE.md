# SDWet — wet clothes and skin for Sleeping Dogs: Definitive Edition

Fixes PCGamingWiki's "Wetness Bug on default and many other outfits": in the rain (and after swimming)
most clothes and skin don't look wet; only a few items (and hair) do, and scripted rainy missions look right.
Local-only so far; the planned public repo name is `sleeping-dogs-wet-officer-shen` (the user's choice).

Status (2026-09-26): both fixes verified in game by the user ("working as expected"): wet in the rain, drying
over ~60 s after it stops; wet right after climbing out of the water, drying the same way. README.md
(players) and ADVANCED.md are written; their download/issue links point at the planned repo and only work
once it's published. See **Handoff** at the end for what's left.

Two independent bugs:
1. **Invisible wetness** (rain and swimming): the specular maps have no wet mask → `core/dxbc.cc` shader patch.
2. **Swimming never sets wetness**: the player's look component is registered in its sim object's component
   array as `0xCC000001` (CompositeLookComponent), but `ApplyWetnessOrSweatTask` looks for `0xCC000005`
   (CharacterLookComponent::_TypeUID), so its lookup returns null and it writes nothing (log, installed build:
   component #51 of 63, fixed table 49, flags 0x4001) → `core/wetness.cc`. `DisableSelfIlluminationTask`
   does the same lookup and is probably broken on the player too (not looked into).

## How the game does wetness (legacy addresses, see the workspace CLAUDE.md for the builds)

- `UFG::CharacterLookComponent` (+0xB8 `mSweatLevel`, +0xBC `mWetnessLevel`). `Update` (0x14058d600, its
  `this` is the component + 0x48): while `TimeOfDayManager::m_WeatherState` > 1, the sky irradiance of
  `RenderWorld::msExposureMetering` > 0.2 and the character isn't in a vehicle, wetness rises by
  irradiance × (weather − 1) × dt × 0.1; otherwise it dries at dt / 60. Sweat and wetness go to the
  character's `cbSceneryInstance.Mask.y` / `.z` (`.w` is the charred amount).
- `OnDrawSkin` (0x1405870d0) sets the state param `texWetLookMap` while sweat or wetness > 0, and
  `Illusion::ShaderTemplate::SelectShader` (0x140095610) then picks the `_WS` permutation of the material's
  shader (`HK_CHARACTER_DR*_WS`, `HK_CHARACTERNIS*_WS`; 28 unique pixel shaders). Verified in RenderDoc:
  after F6 rain (SDAtmos) Mask.z = 0.999 and Wei's body draws use `_WS`.
- The `_WS` pixel shader: `w = max(wet, sweat)`, glossiness `g' = w × spec.x × 10 + spec.y`, then
  `gloss = sat(g' + 0.8 × w × (0.65 × sat(0.65 − g') + 2 × spec.z))`, diffuse darkened by
  `0.35 × w × (1 − gloss)`. `spec` = `texSpecular` (DXT1).
- **Why it looks dry**: nearly all clothing and skin specular maps have x = z = 0 (only y, the glossiness):
  1/62 tops, 1/46 pants (`P_SLACKS`), 2/34 shoes, `WEI_ARMS_S`, `WEI_HEAD_S` (lips only) vs 78/87 hair maps.
  Wet, those only get the weak fallback term. All 2042 character materials use the `HK_CHARACTER*` shaders
  (the `CHARACTER_*` / `UBERSHADER_*` uber shaders in `shaders.temp.bin` are used by no character).
- **Swimming**: `ApplyWetnessOrSweatTrack` (`mSweatLevel` +0x38, `mWetnessLevel` +0x3C, -1 = leave as is)
  sits on many `Swimming\...` action nodes and on `Umbrella\Cycle\DryMeOff!`; the swim tracks carry
  sweat -1, wetness 1.0. `ApplyWetnessOrSweatTask` Begin 0x1403ff1d0 / Update 0x140401b70 copy them into the
  look component (+0xB8 / +0xBC), End 0x1404004f0 resets sweat to -1 if the track set it. The task keeps its
  ActionContext at +0x28 (sim object at context +0x10). Component lookups: `SimObjectGame::GetComponentOfTypeHK`
  searches `m_Components` (size +0x60, 16-byte holders {component, type UID} at +0x68) from
  `mComponentTableEntryCount` (+0x80) on, `SimObject::GetComponentOfType` from 0; a holder matches if the high
  7 bits are equal and it has all the wanted low bits. Neither finds the player's look (see bug 2 above).

## What the mod does

- `core/shaders.cc` hooks `Illusion::StageShader::LoadShader` (0x140a1be00; `Create*Shader` + `D3DReflect`,
  keeps the bytecode pointer as `mRawShader`, so patched copies are never freed) and swaps pixel shaders
  for `dxbc::PatchWetShader`'s copy. Game-level on purpose: no D3D11 hooks, nothing to fight with ReShade.
- `core/dxbc.cc`: recognizes the wet shaders structurally (binds `texSpecular` and `cbSceneryInstance`,
  reads Mask.y and .z outside declarations) and inserts after the `texSpecular` sample
  `add/lt/movc`: where spec.x + spec.z < 0.004, use (`Shine`, `Gloss`) from `SDWet.ini`. Adds one temp,
  fixes chunk sizes, STAT counts and the DXBC checksum (MD5 variant; verified on all 2237 game shaders).
  Token encodings were taken from an fxc-compiled reference (the WDK's tokenized-format header isn't here).
- `core/wetness.cc` hooks `ApplyWetnessOrSweatTask` Begin/Update/End (whole-function signatures: their
  prologues match 18 other tasks) and `CharacterLookComponent::Update` (its `this` is the component + 0x48).
  The task hooks queue the track's values per sim object; the look's Update applies them to itself before
  running, so nothing is looked up by type and no component pointer outlives a frame. Queued values expire
  after 2 s. `ActionWetness` switches it; `LogWetnessTracks` (default 0) logs tracks and the wetness curve.
  The first track applied to each sim object is always logged with its component registration.

## Tests

- `dxbc_test`: a stand-in shader compiled with D3DCompile; checksum, recognition, corrupted input,
  reflection counts, and WARP renders with/without a wet mask, wet and dry.
- `game_shaders_test`: all of the installed game's `shaders.temp.bin` (skips without the game): checksums,
  exactly the 28 `_WS` character permutations patched, each accepted by D3D11.
- `load_test`: loads the .asi outside the game.

## Research tools (`research\`, see its README.md)

Everything the findings above came from, runnable again: `shaders.py` (unpack `shaders.temp.bin`, bindings,
cbuffer layouts, which permutations read Mask, patched variants), `dxbc.py`/`patch.py` (Python originals of
`core/dxbc.cc`; byte-identical output), `materials.py` (material → shader template), `specsurvey.py` (wet
masks in specular maps), `sigcheck.py` (signature uniqueness in both builds), and `renderdoc.ps1` +
`renderdoc\*.py` (launch the game under RenderDoc; per-draw permutation/wetness report, presented frame,
G-buffer targets, and **offline previews**: re-render a capture with the original / default / stronger
shaders, the fastest way to tune `Shine`/`Gloss`). Output goes to `build\research\` (gitignored game data).

How findings were made, for similar work: RenderDoc report on a wet frame (Mask.z per draw, permutation by
checksum) → G-buffer comparison dry vs wet (lighting-independent) → shader formula from `fxc /dumpbin` →
spec map survey → offline preview of candidate patches → in-game test. For CPU-side questions (why swimming
didn't wet), a diagnostic hook that logs the look component's wetness every frame and any change made outside
its Update found that the task never wrote it, then a one-time dump of the component array showed the type.

## Testing in game

The user tests and reports; `tools\build.ps1 -Mod SDWet -Test -Deploy` puts the build in place (not while
the game runs). Quick checks: F6 (SDAtmos' debug key) forces rain; wetness reaches 1 in ~12 s outdoors (not
in vehicles, not under cover, needs sky irradiance > 0.2) and dries in 60 s. For swimming, jump into the sea
anywhere along the harbour. `LogWetnessTracks = 1` in the user's ini logs the tracks and a wetness curve.
Night scenes show the effect weakly (few highlights); judge `Shine`/`Gloss` in daylight or under street lights.

## Handoff: what's left (as of 2026-09-26)

- **Tune `Shine`/`Gloss`** in daylight with the user (defaults 0.05/0.10; 0.10/0.20 already looks plastic on
  the vest). Use `research\renderdoc.ps1 preview` on a daylight capture before asking for game restarts.
  Maybe also darken more (the shader darkens by `0.35 × w × (1 − gloss)`, so more gloss means less
  darkening); that would be a second patch on the `mad ..., l(0.35...)` instruction.
- **CI**: copy `.github\` from SDAtmos (build.yml, reference.env with MinHook only, asi-loader.env,
  dependabot, reference.yml, asi-loader.yml, nexus-release.yml); `game_shaders_test` skips there.
- **Publish** as `aUsernameWoW/sleeping-dogs-wet-officer-shen` only when the user says so (outward-facing).
  Add screenshots (before/after in daylight) to README.md then.
- **Clean up** when tuning is done: `steam_appid.txt` in the game folder (added for RenderDoc launches), the
  user's `plugins\SDWet.ini` has `LogWetnessTracks = 1` (theirs; leave it unless asked), captures in
  `build\research\captures\`.
- Not investigated: sweat (no sweat track fired in testing; the shader uses `max(wet, sweat)`), NPCs'
  swimming (likely the same registration bug), `DisableSelfIlluminationTask` (same lookup).
