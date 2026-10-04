# SDWet — wet clothes and skin for Sleeping Dogs: Definitive Edition

Fixes PCGamingWiki's "Wetness Bug on default and many other outfits": in the rain (and after swimming)
most clothes and skin don't look wet; only a few items (and hair) do, and scripted rainy missions look right.
Public at https://github.com/aUsernameWoW/sleeping-dogs-wet-officer-shen (created 2026-09-28; the name is the
user's choice).

Status (2026-09-28): both fixes verified in game by the user ("working as expected"): wet in the rain, drying
over ~60 s after it stops; wet right after climbing out of the water, drying the same way. Wet footprints
verified (at night). The umbrella works as designed after 4 test rounds (opening, walking only,
rain, indoor/outdoor sprint) with F7; since 2026-10-04 holding E opens/closes it and it is on by default
(`[Umbrella] Enabled`); with it open, E at a car, taxi or talk prompt closes it first (build-13, works); instant
close at taxis and in the water written the same day, untested. README.md (players) and ADVANCED.md are written; README's download link
(`releases/latest/download/SDWet.zip`) works once a CI prerelease is promoted to a full release. See
**Handoff** at the end for what's left.

Two independent bugs:
1. **Invisible wetness** (rain and swimming): the specular maps have no wet mask → `core/dxbc.cc` shader patch.
2. **Swimming never sets wetness**: the player's look component is registered in its sim object's component
   array as `0xCC000001` (CompositeLookComponent), but `ApplyWetnessOrSweatTask` looks for `0xCC000005`
   (CharacterLookComponent::_TypeUID), so its lookup returns null and it writes nothing (log, installed build:
   component #51 of 63, fixed table 49, flags 0x4001) → `core/wetness.cc`. `DisableSelfIlluminationTask`
   does the same lookup and is probably broken on the player too (not looked into).

Plus additions (the user's ideas): wet footprints for `WetFootprints` (25) seconds after swimming →
`core/footprints.cc` (2026-09-27; works per the user, tested at night only); not a bug fix, the original never
leaves prints after a swim. An umbrella Wei can open → `core/umbrella.cc` (see below).

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
  sits on 23 `GlobalActions\Swimming\...` nodes (every character), ~40 NIS cutscene nodes (Wei in Election,
  Big Hit, Amanda Story, the nightmares...) and `GlobalActions\UpperBodySpawn\inventoryItem\Umbrella\Cycle\DryMeOff!`,
  the open-umbrella carry cycle of pedestrians and thugs (PedestrianAI/PedestrianHangout/Thug behaviours open
  and close it in the rain). **The player has no umbrella action**: he can only hold one as a melee weapon
  (`Player\...\Melee\Umbrella` creates `object-physical-weapon-melee-umbrella`, dropped via
  `PropReactions\Drop\PlayerUmbrella`). Source: `reference\SDmodding\Files\ActionNodes\*.txt`, a dump of all
  action trees (tracks without values). The swim tracks carry sweat -1, wetness 1.0 (logged); the cutscene and
  umbrella values were never logged. `ApplyWetnessOrSweatTask` Begin 0x1403ff1d0 / Update 0x140401b70 copy them into the
  look component (+0xB8 / +0xBC), End 0x1404004f0 resets sweat to -1 if the track set it. The task keeps its
  ActionContext at +0x28 (sim object at context +0x10). Component lookups: `SimObjectGame::GetComponentOfTypeHK`
  searches `m_Components` (size +0x60, 16-byte holders {component, type UID} at +0x68) from
  `mComponentTableEntryCount` (+0x80) on, `SimObject::GetComponentOfType` from 0; a holder matches if the high
  7 bits are equal and it has all the wanted low bits. Neither finds the player's look (see bug 2 above).

## How the game does footprints (installed-build facts from the PDB/IDA; `research\footprints.py`)

- `CharacterEffectsComponent::HandleFootstep(this, foot)` (legacy 0x140533e90, installed 0x1405340f0), called
  from `FootStep{Left,Right}EffectTask::Begin` (locomotion animations; they find the component by fixed slot 38
  on the player, so it works, unlike the look lookup). In shallow water (`CharacterPhysicsComponent::IsInWater`,
  physics component at this+0xB0) it places `mWalkingThroughWaterEffect` (splash). Then for each ref in
  `mPhysVolumeRefs` (+0x108, RB tree of `PhysVolumeRef` {mActive +0x20, mForceInactive +0x21, mTimeInVolume,
  mTimeOutVolume, `PhysicsVolumeProperties*` +0x30}): the volume's `mFootStepEffect` if active, and its
  `mFootStepDecal_Left/Right`. Last `mFootstepOverride[foot]` (+0x1A4, `SetFootstepOverride`, script
  `set_footstep_override_effect(left, right)`, "none" = -1) unless -1 or equal to the decal just placed.
- `CharacterEffectsComponent::Update` fills the refs each frame from up to 5 phantom volumes the character is in,
  the ground's `PhysicsSurfaceProperties.mEffectProperty` (+0x6C; only `Water` and `Mud` name a volume), and for
  the player while `m_WeatherState` > 1 outdoors `PhysVol_WetSurface` (0xF18CE285, the only volume the exe
  names). A ref left behind counts `mTimeOutVolume` up and is dropped after `mFootStepDecalCountdown`: that's
  why blood/mud/puddle prints continue 5 s after stepping out.
- Volumes (`PhysicsProperties.perm.bin`): `Water` splash only; `PhysVol_WetSurface` (rain) `HK_PuddleSplash_01`
  only; `PhysVol_Blood` bloody prints (texture `FX_DECL_BLOOD_FOOTSTEP_D_01`, a **bare foot** whatever the
  shoes), 5 s; `Mud` muddy prints 5 s; `PhysVol_Puddle` splash + wet prints 5 s; `PhysVol_WetFootPrints` wet
  prints (`HK_WetFootPrintLeft/Right_Effect` 0x823906F4/0xD439BE30, texture `FX_DECL_MUDPRINT_01`, a shoe
  sole), 5 s, used only by the placed `PhantomVolume_2mSqr_WetFeet`; `PhysVol_OnFire` burning prints.
  `HK_WEI_BloodyFoot_Effect` exists in `Effects.perm.bin` but no file references it (cut content?).

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
- `core/footprints.cc` hooks `HandleFootstep` (23-byte prologue signature; `scan::Matches` then checks the
  instructions that use +0xB0, +0x28 and +0x1A4 at fixed offsets, same in both builds, and takes `IsInWater`
  from its call). The look hook reports each applied wetness track (`OnTrackApplied`: soaked if wetness >
  1 − `WetFootprints`/60) and each update (`OnLookUpdated`: game seconds since the last soaking track, current
  wetness). While a sim object is soaked, not in water and has no script override on that foot, the hook puts
  the wet print effect in `mFootstepOverride[foot]` for the call only and restores -1. Ends after
  `WetFootprints` game seconds (rain keeps wetness at 1) or when wetness drops below the threshold (a track
  setting it low); entries whose look stopped updating expire after 10 s. Logs soaked / first print / end with
  counts. Any soaking track counts, so a cutscene that soaks Wei would give prints after it too (untested).
- `core/umbrella.cc`: the umbrella, below. Its signatures are passed to `scan::FindUnique` literally so
  `tools\pdb.ps1 verify` checks them; offsets it relies on inside functions are checked with `scan::Matches`.
- `core/crash.cc` (copied from SDRadio): with `Logging`, a vectored handler logs the first access violations with
  a stack and writes `SDWet-crash-<n>.dmp`. The game's exit crash (execute at `...488C`, see the workspace
  CLAUDE.md) shows up there on every exit.
- `core/hash.hh`: `qStringHash32` / `qStringHashUpper32` as constexpr (effect IDs, node names, track classes).

## Umbrella (`core/umbrella.cc`, 2026-09-28 round 4 verified with F7; hold E 2026-10-04)

The user's idea: open the umbrella Wei holds (eventually by holding E). Wei can hold an umbrella only as a melee
weapon (`object-physical-weapon-melee-umbrella`, actor `LOP_Umbrella002`, the same prop pedestrians carry and drop
when scared); the prop's own tree has `Object\Animation\Opening/Closing`, `Object\OnInit\Opened`, `Object\Closed`.
Pedestrians and thugs open and carry theirs with upper body nodes in GlobalActions:
`...\UpperBodySpawn\inventoryItem\Umbrella\UmbrellaActions\Open\Open_Umbrella` (group `Pedestrian_Upright_Umbrella`,
End 1.333; a TargetPlayTrack at 0.433 s plays the prop's Opening on target type 17 = the right hand),
`...\Cycle\Male_Carry_Umbrella` (group `Pedestrian_Upright`, loops) and `...\Close\Close_Umbrella` (shakes the water
off; TargetPlayTrack at 1.6 s plays Closing), requested by their AI and run in a spawned upper body controller.

What it does now (ini `[Umbrella] Enabled`, default 1 since 2026-10-04; until then `Prototype`, default 0, which
published inis say explicitly, so the key was renamed: the user tests on a second machine with default inis and
didn't want to edit them; hold E or F7 open/close, F9 dump):
- **Hold E** (2026-10-04, the user's design: "长按 E"): E is the game's Action button, `UFG::ActionDef_Action`
  (an `InputActionDef`: `InputActionData*` per controller [5]; `mActionTrue` +0x4C while held, `mOnSeconds` +0x40).
  `ReadControllerInputTask::Update` (+0x718: `mov rdx, [rdi+rbx*8+ActionDef_Action]`, rdi = image base, the disp32
  is the RVA; then `mov r9d, gActionRequest_Action`) sets the Action request every frame while it's held, with the
  held time (×60, max 255) as its charge: ARS_ONESHOT/ARS_CHARGE in conditions are press/held. We read the input
  data, any controller, so remapping and gamepad Y work and masking the request doesn't hide it. 0.5 s held → toggle,
  once per press. E also counters, talks, enters vehicles and hires taxis (hold 1/3 s): the PlayerAI tree
  (`Player_behaviour.act`, AIActionTreeComponent = character slot 19, controller +0xD8) handles it in
  `ButtonHandlers\InteractHandler` (Prompts: AttackCounter, Socialize, Taxi, Vehicle under `Query(ValidLocomotionState)`;
  WaitForTap → TapActions) and a twin `0xAE51F4F2` (ChargeActions = hold to enter). A vehicle press walks Wei to
  the door at once (`InputPressed\InputHeld` spawns MoveDirectlyToInteractionPoint), release gets in. So while the
  umbrella is closed, every frame of the hold checks `GameUsesE`: Wei's own tree not playing `Locomotion`, the
  social target locked (the talk prompt; `TargetIsLockedCondition::Match` layout), or the PlayerAI controller
  playing `TapActions` / `InputPressed` / `Tap` → that press is the game's, logged, no toggle.
  `ActionController::IsPlaying(controller, &qStringHashUpper32(name), -1, true)` = what `IsPlayingCondition` calls
  (for characters with an AI tree it checks the root context's controller; it matches the last-segment IDs in
  `m_PlayingNodeUID` +0x94 (count +0x90) of the controller and its spawns, so a name like `Available` matches any
  handler's). Target types from the PDB: ATTACKING 38, INTERACTIVE_PROP 40, SOCIAL 50, TRANSIT 57 (only SOCIAL
  used: whether the others are set loosely, e.g. INTERACTIVE_PROP near any parked car, is unknown, and the
  outcome checks cover them).
- **E while open** (2026-10-04, after the first hold-E test): E is kept from the game while the umbrella is open or
  closing, and after closing until it's let go. Round 1 masked the Action request (`m_ActionRequestMask`): useless,
  E still got Wei into cars (the umbrella vanished open, a closed one came back after getting out) and taxis.
  `ActionRequestCondition::Match` reads the **AI controller's** `m_Intention` (`GetComponent<AICharacterController
  BaseComponent>`), `ReadControllerInputTask::Update` (a PlayerAI task; this +0x48 = that component, `Intention::
  operator=` into its +0x80 at the end) writes it, and the PlayerAI tree reads it before `AICharacterController
  Component::Update` ANDs the mask in (only Wei's own tree gets the masked copy, `SetIntentionOnActionTreeComponent`).
  Now `ReadControllerInputTask::Update` is hooked: around the original call, for the local player's task, the
  `ActionDef_Action` input data of all 5 controllers reads `mActionTrue` 0 (hidden) or 1 with our `mOnSeconds`
  (replayed), then is restored. E sets the Action and POI_Use requests there and nowhere else for gameplay (other
  readers of `ActionDef_Action`: `getSignalValue` FIS_Y_BUTTON for `InputSignalCondition`, ProgressionTracker,
  init). PC has no separate vehicle key: `gActionRequest_VehicleEnter` and `MultiAction_VehicleEnter` are unused.
- **Close, then get in** (the user's wish, "收伞+上车"): E pressed while open (or opening/closing) while the PlayerAI
  tree waits at a prompt node, `InteractHandler\Prompts\Vehicle\Default\Available`, `...\Vehicle\Boat\Available`,
  `...\Taxi\Available` or `...\Socialize\Available` (found by pointer: a controller of the AI tree, or of its
  running SpawnTasks, list `m_RunningSpawnTasksTmp` +0x50, list node +0x28 in the SpawnTask, its controller +0x120,
  whose `m_currentNode` is one of them), starts Close_Umbrella at once (at a taxi it's closed at once instead,
  `CloseNow`, and the press goes over the next frame: taxis drive off). When the prop is folded (≥ 2.2 s, prop not
  Opening/Opened; Close_Umbrella itself runs ~3 s) and the same prompt is still up, the press is handed to the game
  as a fresh one (mOnSeconds from 0: charge 0 = ARS_ONESHOT on the first frame): as long as the player held it
  (≥ 0.1 s; ≥ 0.6 s for a taxi hold ≥ 1/3 s), or until they let go if still held. Otherwise dropped and logged.
  The prompts (`extract.ps1 act node InteractHandler`): Socialize\Tap = Action ARS_ONESHOT → `Taunt` request; Vehicle
  \Default\Tap = Action ARS_CHARGE + `Locomotion` → InputHeld (pathfind to the door while held) / InputReleased →
  Request (≤ 1.5 m) → `EnterVehicle` (220) every frame; Taxi\Tap → InputPressed\Charge: at 0.333 s still held →
  InputHeld presses `EnterTaxi` (222) for 1/3 s, released earlier → the Vehicle InputReleased path (driver's seat).
  The counter prompt isn't replayed (no fighting under the umbrella).
- **Taxi entry is a teleport in the original**: `Vehicle\Interactions\Action\GetIn\Player\Taxi\Spawn` has
  `UIPromptAsPassenger` (spawns `MoveToPassengerSeat`: `TargetAttachTrack` to `C_SeatSync03` with a 1/3 s blend,
  plus the taxi ride) and `AsPassenger` (the real get-in animation `Content\GetIn\P1\Reg`), which is disabled by a
  `False` condition. `GetIn\HelperFunctions\DropMeleeEquipped` is disabled too, so a melee weapon stays in hand in
  the back seat; an open umbrella poked through the roof (the user's screenshot, 2026-10-04).
- F7 with an umbrella in hand plays Open_Umbrella, then Male_Carry_Umbrella, on Wei in an ActionController of our
  own, updated right after his (`ActionTreeComponent::update` hook, like a SpawnTask); the pedestrians'
  TargetPlayTrack opens his umbrella. F7 again plays Close_Umbrella and stops the controller after ~2 s.
- While open (the user's design) Wei only walks: a brisk walk (jog) while Shift is held, no sprint, no fighting,
  pickups, weapon changes, parkour or cover:
  - `TSCharacter::Mthd_allow_jog/allow_sprint` = bits 0/1 of `SimObjectCharacterPropertiesComponent::mBooleans`
    (+0xF0; character component slot 3). The `Sprint` request never reaches the action tree's intention
    (+0x218, mActionRequests +0xB0) while sprint is disallowed, so the brisk walk is Shift (no gamepad yet).
  - `Mthd_action_request_disable` = clear a bit of `AICharacterControllerComponent::m_ActionRequestMask`
    (+0x3D0, slot 21), which `AICharacterControllerComponent::Update` ANDs into the requests; the player has one
    too (input goes through the `PlayerAI` tree). Blocked: Attack, Attack2, RunningAttack, MidRangeAttack,
    StrikeRelease, Grab, Guard, Taunt, Pickup, Equip, EquipUP, Inventory, Weapon, WeaponMode, Freerun, Jump, Dive,
    UseCover, CoverToggle (indices by name: `Intention::GetActionRequest`; ~100 names, `gActionRequest_*` in the
    PDB).
  - Restored when it closes or leaves his hand, to the game's current wish: the two script methods are hooked
    (while restricted a call sees and sets the game's values, which are kept, then ours go back) and other
    changes to the two bits are adopted too; every change is logged (`player can [not] jog, ...`). Needed because
    the interiors' force walk (`InteriorTriggerProperties::mForceWalk` → script methods `start_force_walk` /
    `stop_force_walk` on PlayerOne) turns sprint off indoors: opened indoors and carried out, the indoor state
    used to come back. Force walk itself doesn't fit the umbrella: per its log strings it only stops jogging and
    sprinting and switches the camera to `FollowCameraWalkSlow`; combat stays.
- The prop's tree opens it whenever it rains (`OnInit` sits under an `IsRainingCondition`, next to it in
  `Data\Global\Act_Files.bin`, the archive of all action trees; `Match` = `TimeOfDayManager::m_WeatherState > 1`)
  and re-checks on entering `Closed` (a 0.1 s opportunity window), so in the rain it reopened right after
  closing. `IsRainingCondition::Match` is hooked: for the umbrella in Wei's hand only (context +0x10 = its sim
  object) it answers whether he holds it open. Our state follows the prop: an umbrella open while we think it's
  closed (picked up open) gets closed; one that closes while open ends our open state.

Test rounds (2026-09-28): 1. opening works and looks right, but the open umbrella stayed a usable weapon.
2. walking, Shift and the blocks work; the rain reopened it after closing, and F8 (then a prop-only key) closed
it behind our back, leaving sprint off. 3. F8 removed, rain check hooked: fixed; but sprint stayed off after it
was opened indoors (force walk, above). 4. script hooks: "working as expected"; the log shows it opened indoors
(`sprint 0 before`), `script allow_sprint set jog 1 sprint 1 while the umbrella is open` on walking out, and
`jog 1 sprint 1` restored on closing.
Hold E, 2026-10-04 (second machine, build-12): opening/closing by holding E works, no unexpected
refusals; but with it open, E still got Wei into a car (umbrella vanished, a closed one came back after getting
out) and a taxi (teleported in, umbrella open through the roof), and talking to a car park valet took the umbrella
away ("无伤大雅"). Fixed as above (input hidden at the source, close then hand the press over) in build-13:
"挺好的". Its round found: the valet still takes the umbrella after it's closed (the game's own; the user: leave
it); a taxi may drive off during Close_Umbrella (→ at a taxi prompt `CloseNow`: the prop jumps to its `Closed`
node, the layer stops, the press goes over the next frame); falling into the water with it open, Wei swam with it
open and couldn't swim fast (→ `CloseNow` when his tree plays `Swimming`, the only node of that name:
`GlobalActions\Swimming`, entered from `StartSwiming\Enter` or a boat jump's fall). Both untested: check
`closing it at once` and `Wei is in the water`.
Not done: closing for cutscenes (dropping the weapon ends it), staying dry under it (the look's rain wetness keeps
rising), gamepad brisk walk.

Action tree runtime (from the PDB; layouts in umbrella.cc's header comment):
- Each node path segment is `qStringHashUpper32`; `ActionNode::Find(ActionPath*, root)` walks from
  `ActionNode::smRoot` ("Global") by `FindChild`; an ActionPath is {count, qOffset64 to the IDs}, built by hand
  (`ActionPath::Append` allocates). Track class UIDs are `qStringHash32` of the class name.
- `ActionTreeComponent::update` sets `UEL::gCurrentParameters` to the object's UELComponent (slot 0) + 0x58, then
  `ActionController::Update(&mActionController /* +0xC0 */)`. Characters keep their ActionTreeComponent in
  component slot 7, props in 6; `eTARGET_TYPE_EQUIPPED` = 17 (right hand), `UFG::getTarget(sim, type)`.
  Component type UIDs are assigned at run time (-1 in the file), so use the slots.
- `TargetPlayTask::Begin` = how to make another object play a node: its controller `Play(node, false)` (or
  `PlayTracks`), then `updateTasksTimeBegin(0, false)`, with gCurrentParameters set to that object's.
- `SpawnTask::Begin` = how a sub-controller is made: copy the calling ActionContext (`operator=`), point
  context/controller at each other, parent context, opening branch = the node, tree type; if the node's tree
  root (`GetAbsoluteRoot`, vtable +0x98) differs from the caller's, `ActionTreeComponentBase::AllocateFor` +
  `ActionNodeRoot::Init`; then `Play(opening branch)`. `SpawnTask::Update` just calls `ActionController::Update`.
- Which bones an animation drives is the AnimationTrack's own `mWeightSetName`, not the controller.
- The player's own upper body layer is `Player\Inventory\MasterSpawn\SpawnActions\LocoUpperBody\Upperbody`
  (None/Gun/Rifle, `ArmState\...\1_handWeaponMelee`, `Bag` with `ShoppingBag` pedestrian anims).
- `reference\SDmodding\Files\ActionNodes\*.txt` dumps every tree (no conditions, few track values); `Chan.txt` is
  the player's tree despite its name.

## Tests

- `dxbc_test`: a stand-in shader compiled with D3DCompile; checksum, recognition, corrupted input,
  reflection counts, and WARP renders with/without a wet mask, wet and dry.
- `game_shaders_test`: all of the installed game's `shaders.temp.bin` (skips without the game): checksums,
  exactly the 28 `_WS` character permutations patched, each accepted by D3D11.
- `load_test`: loads the .asi outside the game: default ini, and every feature reports its functions missing.
- `umbrella_load_test`: an old ini (`[Umbrella] Prototype = 0`): still on, functions reported missing.
- `umbrella_off_test`: `[Umbrella] Enabled = 0`: off, nothing looked for.

## CI (`.github\`, copied from SDAtmos 2026-09-28)

`build.yml` builds with `-warnAsError` against MinHook at the pin in `reference.env` (MinHook only), runs
`tests\*_test.cc` (`game_shaders_test` passes without the game), packages `SDWet.zip` (Ultimate ASI Loader
pinned in `asi-loader.env`) and publishes each passing push to `main` as prerelease `build-<N>`;
`research/**` and `*.md` changes don't trigger it. `reference.yml` / `asi-loader.yml` open monthly PRs for new
MinHook / loader releases (they need the repo setting "Allow GitHub Actions to create and approve pull
requests"), Dependabot bumps the SHA-pinned actions. The `nexus` job and `nexus-release.yml` are skipped until
the repo has the `NEXUS_*` variables and `NEXUSMODS_API_KEY` (no Nexus page yet); see `mods\SDIMEFix\CLAUDE.md`
for how they work.

## Research tools (`research\`, see its README.md)

Everything the findings above came from, runnable again: `shaders.py` (unpack `shaders.temp.bin`, bindings,
cbuffer layouts, which permutations read Mask, patched variants), `dxbc.py`/`patch.py` (Python originals of
`core/dxbc.cc`; byte-identical output), `materials.py` (material → shader template), `specsurvey.py` (wet
masks in specular maps), `sigcheck.py` (signature uniqueness in both builds), `footprints.py` (the game's
footprint volumes, effects and decal textures), and `renderdoc.ps1` +
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
Footprints: climb out, walk and look back; the log's `footprints:` lines count prints and in-water steps.
Umbrella: F6 rain makes pedestrians carry umbrellas; hit or scare one and pick it up. Hold E (or F7) opens/closes,
F9 logs nodes, action request indices, the jog/sprint switches and the hold state with the game's current use of E.
Night scenes show the effect weakly (few highlights); judge `Shine`/`Gloss` in daylight or under street lights.

## Handoff: what's left (as of 2026-10-04)

- **Umbrella, next**: hold E works (2026-10-04, `[Umbrella] Enabled`, in README.md); close-then-get-in at car,
  taxi and talk prompts works (build-13); instant close at taxis and in the water is written but untested; then
  auto-close for cutscenes, staying dry under it,
  gamepad brisk walk. Still to do
  once it settles: move the action tree runtime and umbrella details into `docs/action-trees.md` (like SDRadio's
  `docs/radio-internals.md`) and keep a summary here (agreed with the user).
- **First full release**: promote a verified `build-<N>` prerelease (un-tick "Set as a pre-release") so the
  README's `releases/latest/download/SDWet.zip` works. Add screenshots (before/after in daylight) to README.md.
- **Nexus page** (optional, like the other mods): create it, then set the `NEXUS_*` variables and
  `NEXUSMODS_API_KEY` secret.
- **Wet footprints**: the user says they work as expected (2026-09-28, 3 swims, 32-92 prints each), but only
  tested at night; check in daylight on several surfaces (concrete, sand, grass) and whether 25 s is right.
- **Tune `Shine`/`Gloss`** in daylight with the user (defaults 0.05/0.10; 0.10/0.20 already looks plastic on
  the vest). Use `research\renderdoc.ps1 preview` on a daylight capture before asking for game restarts.
  Maybe also darken more (the shader darkens by `0.35 × w × (1 − gloss)`, so more gloss means less
  darkening); that would be a second patch on the `mad ..., l(0.35...)` instruction.
- **Clean up** when tuning is done: `steam_appid.txt` in the game folder (added for RenderDoc launches), the
  user's `plugins\SDWet.ini` has `LogWetnessTracks = 1` (theirs; leave it unless asked), captures in
  `build\research\captures\`.
- Not investigated: sweat (no sweat track fired in testing; the shader uses `max(wet, sweat)`), NPCs'
  swimming (likely the same registration bug), `DisableSelfIlluminationTask` (same lookup).
