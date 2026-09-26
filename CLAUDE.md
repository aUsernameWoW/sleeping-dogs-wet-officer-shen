# SDWet — wet clothes and skin for Sleeping Dogs: Definitive Edition

Fixes PCGamingWiki's "Wetness Bug on default and many other outfits": in the rain (and after swimming)
most clothes and skin don't look wet; only a few items (and hair) do, and scripted rainy missions look right.
Local-only so far; the planned public repo name is `sleeping-dogs-wet-officer-shen` (the user's choice).

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
- **Swimming**: after climbing out of the water Mask.z stayed 0 (capture). `ApplyWetnessOrSweatTrack`
  (`mSweatLevel` +0x38, `mWetnessLevel` +0x3C, -1 = leave as is; `ApplyWetnessOrSweatTask::Begin`
  0x1403ff1d0 writes them, `End` resets sweat to -1) sits on many `Swimming\...` action nodes and on
  `Umbrella\Cycle\DryMeOff!`. Which values the DE data has is what the `LogWetnessTracks` hook finds out.

## What the mod does

- `core/shaders.cc` hooks `Illusion::StageShader::LoadShader` (0x140a1be00; `Create*Shader` + `D3DReflect`,
  keeps the bytecode pointer as `mRawShader`, so patched copies are never freed) and swaps pixel shaders
  for `dxbc::PatchWetShader`'s copy. Game-level on purpose: no D3D11 hooks, nothing to fight with ReShade.
- `core/dxbc.cc`: recognizes the wet shaders structurally (binds `texSpecular` and `cbSceneryInstance`,
  reads Mask.y and .z outside declarations) and inserts after the `texSpecular` sample
  `add/lt/movc`: where spec.x + spec.z < 0.004, use (`Shine`, `Gloss`) from `SDWet.ini`. Adds one temp,
  fixes chunk sizes, STAT counts and the DXBC checksum (MD5 variant; verified on all 2237 game shaders).
  Token encodings were taken from an fxc-compiled reference (the WDK's tokenized-format header isn't here).
- `core/wetness.cc` logs every `ApplyWetnessOrSweatTask::Begin` (whole-function signature: its prologue
  matches 18 other tasks).

## Tests

- `dxbc_test`: a stand-in shader compiled with D3DCompile; checksum, recognition, corrupted input,
  reflection counts, and WARP renders with/without a wet mask, wet and dry.
- `game_shaders_test`: all of the installed game's `shaders.temp.bin` (skips without the game): checksums,
  exactly the 28 `_WS` character permutations patched, each accepted by D3D11.
- `load_test`: loads the .asi outside the game.

## Tooling used for the investigation (session scratchpad, not in the repo yet)

qrenderdoc `--python` scripts (character draws with their permutation and Mask, G-buffer export, replaying
a capture with patched shaders per variant), a shader unpacker (DXBC blobs are stored raw in
`shaders.temp.bin` after an `Illusion.ShaderBinary` header), a Material chunk parser (`0xF5F8516F`, params of
0x38 bytes: state name/type hashes, resource name/type at +0x28/+0x30), and a specular-map channel survey.
Promote them into `tools\` if they're needed again. RenderDoc launches need `steam_appid.txt` (307690) in the
game folder, which was added for this and should be removed when done.
