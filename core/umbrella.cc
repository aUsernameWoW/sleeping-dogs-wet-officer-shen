#include "umbrella.hh"

#include <Windows.h>

#include <MinHook.h>

#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <malloc.h>

#include "config.hh"
#include "hash.hh"
#include "log.hh"
#include "scan.hh"

namespace umbrella
{
	// Layouts (same in both builds; the offsets used by code are checked against the game's own instructions in
	// Install):
	// - SimObject: m_Flags (u16) +0x4C, component holders {component*, type UID} of 16 bytes: count +0x60, array
	//   +0x68. Characters (flag 0x4000) keep their ActionTreeComponent in slot 7, props (0x2000) in slot 6
	//   (TargetPlayTask::Begin); both keep the UELComponent in slot 0, and UEL::gCurrentParameters must point at
	//   its parameters (+0x58) while their action tree runs (ActionTreeComponent::update).
	// - ActionTreeComponent: m_pSimObject +0x28, mpActionContext +0xB8, mActionController +0xC0.
	// - ActionController (0x118): m_currentNode +0x10, m_Context +0x18, m_ActionNodePlayTime +0x20, mKeepAlive
	//   +0x25, m_BankTracksEnabled +0x27, m_OnEnterExitCallbacksEnabled +0x28.
	// - ActionContext (0xD8): m_OpeningBranch +0x18, mActionController +0x20, mParentContext +0x28,
	//   m_ActionTreeType (u16) +0x40, mActionTreeComponentBase[4] +0x48.
	// - ActionNode: GetAbsoluteRoot in vtable slot +0x98, mID +0x28, mTracks (qOffset64 to a TrackGroup) +0x40.
	//   ActionNodeRoot::mActionTreeType (s8) +0xEC. TrackGroup: track count +0x10, qOffset64 +0x18 to an array of
	//   qOffset64s, one per track. ITrack: class name UID (qStringHash32) +0x10, begin/end +0x30/+0x34.
	//   AnimationTrack: mAnimation +0x38, mPlayPriority +0x54, mWeightSetName +0x58, mAnimationName +0x5C.
	//   TargetPlayTrack: node reference +0x38, target type +0x78.
	static constexpr size_t kContextSize = 0xD8;
	static constexpr size_t kControllerSize = 0x118;
	static constexpr int kTargetEquipped = 17; // eTARGET_TYPE_EQUIPPED: the right hand (TSCharacter::Mthd_unequip)
	static constexpr int kActionTreeTypeAction = 1;

	using AtcUpdateFn = void(__fastcall*)(void* atc, float delta);
	using FindFn = void*(__fastcall*)(const void* path, void* root);
	using PlayFn = void(__fastcall*)(void* controller, void* node, bool force);
	using ControllerUpdateFn = void(__fastcall*)(void* controller, float delta);
	using StopFn = void(__fastcall*)(void* controller);
	using TimeBeginFn = bool(__fastcall*)(void* controller, float time, bool flag);
	using CtorFn = void*(__fastcall*)(void* self);
	using AssignFn = void*(__fastcall*)(void* self, const void* other);
	using AllocateForFn = bool(__fastcall*)(void* base, void* root);
	using RootInitFn = void(__fastcall*)(void* root, void* context);
	using GetTargetFn = void*(__fastcall*)(void* sim, int type);
	using RootFn = void*(__fastcall*)(void* node);

	static AtcUpdateFn gAtcUpdate = nullptr;
	static FindFn gFind = nullptr;
	static PlayFn gPlay = nullptr;
	static ControllerUpdateFn gControllerUpdate = nullptr;
	static StopFn gStop = nullptr;
	static TimeBeginFn gTimeBegin = nullptr;
	static CtorFn gNewController = nullptr;
	static CtorFn gNewContext = nullptr;
	static AssignFn gAssignContext = nullptr;
	static AllocateForFn gAllocateFor = nullptr;
	static RootInitFn gRootInit = nullptr;
	static GetTargetFn gGetTarget = nullptr;
	static void** gLocalPlayer = nullptr;     // UFG::gSim.mpLocalPlayer.m_pPointer
	static const void** gParameters = nullptr; // UEL::gCurrentParameters

	template <typename T>
	static T Read(const void* p, size_t offset)
	{
		T value;
		std::memcpy(&value, static_cast<const uint8_t*>(p) + offset, sizeof(value));
		return value;
	}

	template <typename T>
	static void Write(void* p, size_t offset, T value)
	{
		std::memcpy(static_cast<uint8_t*>(p) + offset, &value, sizeof(value));
	}

	// The pedestrians' umbrella (GlobalActions\UpperBodySpawn\inventoryItem\Umbrella, run in a spawned upper
	// body controller) and the umbrella prop's own tree (LOP_Umbrella002; the weapon is the same actor).
	enum NodeIndex { kOpen, kCarry, kClose, kPropOpening, kPropClosing, kPropOpened, kPropClosed, kNodeCount };
	struct Node
	{
		const char* mName;
		const char* mPath;
		void* mNode = nullptr;
	};
	static Node gNodes[kNodeCount] = {
		{ "Open_Umbrella", "\\Global\\GlobalActions\\UpperBodySpawn\\inventoryItem\\Umbrella\\UmbrellaActions\\Open\\Open_Umbrella" },
		{ "Male_Carry_Umbrella", "\\Global\\GlobalActions\\UpperBodySpawn\\inventoryItem\\Umbrella\\Cycle\\Male_Carry_Umbrella" },
		{ "Close_Umbrella", "\\Global\\GlobalActions\\UpperBodySpawn\\inventoryItem\\Umbrella\\UmbrellaActions\\Close\\Close_Umbrella" },
		{ "prop Opening", "\\Global\\LOP_Umbrella002\\Object\\Animation\\Opening" },
		{ "prop Closing", "\\Global\\LOP_Umbrella002\\Object\\Animation\\Closing" },
		{ "prop Opened", "\\Global\\LOP_Umbrella002\\Object\\OnInit\\Opened" },
		{ "prop Closed", "\\Global\\LOP_Umbrella002\\Object\\Closed" },
	};
	static bool gNodesResolved = false;

	static uint32_t NodeId(const void* node)
	{
		return node ? Read<uint32_t>(node, 0x28) : 0;
	}

	static const char* NodeName(const void* node)
	{
		for (const Node& known : gNodes) {
			if (node && known.mNode == node) {
				return known.mName;
			}
		}
		return node ? "other" : "none";
	}

	static void* Root(void* node)
	{
		return node ? (*reinterpret_cast<RootFn**>(node))[0x98 / 8](node) : nullptr;
	}

	// ActionNode::Find with an ActionPath built here: {count, qOffset64 to the IDs}, one qStringHashUpper32 per
	// segment, the first ("Global") standing for ActionNode::smRoot. ActionPath::Append would allocate.
	static void* FindNode(const char* path)
	{
		uint32_t ids[24];
		int count = 0;
		char segment[128];
		for (const char* p = path; *p && count < 24;) {
			while (*p == '\\') {
				++p;
			}
			size_t n = 0;
			while (*p && *p != '\\' && n + 1 < sizeof(segment)) {
				segment[n++] = *p++;
			}
			segment[n] = 0;
			if (n) {
				ids[count++] = hash::Upper32(segment);
			}
		}
		struct
		{
			int32_t mCount;
			int32_t mPad;
			int64_t mOffset;
		} actionPath{ count, 0, 0 };
		actionPath.mOffset = reinterpret_cast<const char*>(ids) - reinterpret_cast<const char*>(&actionPath.mOffset);
		return gFind(&actionPath, nullptr);
	}

	static const char* TrackClass(uint32_t uid)
	{
		static constexpr const char* kClasses[] = { "AnimationTrack", "TargetPlayTrack", "SequenceTrack", "SpawnTrack", "OpportunityTrack",
			"LookAtTrack", "ApplyWetnessOrSweatTrack", "AnimationBankReferenceTrack", "TargetAttachTrack", "TargetDetachTrack" };
		for (const char* name : kClasses) {
			if (hash::String32(name) == uid) {
				return name;
			}
		}
		return "?";
	}

	// Its tracks, and for animations whether the animation is there (bound when its bank is loaded).
	static void LogNode(const Node& known)
	{
		void* node = known.mNode;
		if (!node) {
			LOG("umbrella:   %s: NOT FOUND (%s)", known.mName, known.mPath);
			return;
		}
		void* root = Root(node);
		LOG("umbrella:   %s: node %p id %08X, tree root %p (type %d)", known.mName, node, NodeId(node), root, root ? Read<int8_t>(root, 0xEC) : -1);
		const int64_t groupOffset = Read<int64_t>(node, 0x40);
		if (!groupOffset) {
			return;
		}
		const uint8_t* group = static_cast<const uint8_t*>(node) + 0x40 + groupOffset;
		const int count = Read<int32_t>(group, 0x10) & 0x7FFFFFFF;
		const uint8_t* entries = group + 0x18 + Read<int64_t>(group, 0x18);
		for (int i = 0; i < count && i < 16; ++i) {
			const int64_t trackOffset = Read<int64_t>(entries, i * 8);
			if (!trackOffset) {
				continue;
			}
			const uint8_t* track = entries + i * 8 + trackOffset;
			const uint32_t uid = Read<uint32_t>(track, 0x10);
			const char* cls = TrackClass(uid);
			const float begin = Read<float>(track, 0x30);
			const float end = Read<float>(track, 0x34);
			if (uid == hash::String32("AnimationTrack")) {
				LOG("umbrella:     %s %.3f..%.3f: animation %p (name %08X), weight set %08X, priority %d, blend mode %u", cls, begin, end,
					Read<const void*>(track, 0x38), Read<uint32_t>(track, 0x5C), Read<uint32_t>(track, 0x58), Read<int32_t>(track, 0x54),
					Read<uint8_t>(track, 0x61));
			}
			else if (uid == hash::String32("TargetPlayTrack")) {
				const uint8_t* reference = track + 0x38 + Read<int64_t>(track, 0x38);
				const int64_t nodeOffset = Read<int64_t>(reference, 0x48);
				const void* target = nodeOffset ? reference + 0x48 + nodeOffset : nullptr;
				LOG("umbrella:     %s %.3f..%.3f: target type %d, plays node %08X (%s)", cls, begin, end, Read<int32_t>(track, 0x78),
					NodeId(target), NodeName(target));
			}
			else {
				LOG("umbrella:     %s (%08X) %.3f..%.3f", cls, uid, begin, end);
			}
		}
	}

	// Trees load with what uses them (the umbrella's with an umbrella), so missing nodes are looked for again.
	static void ResolveNodes()
	{
		if (gNodesResolved) {
			return;
		}
		bool found = false;
		bool missing = false;
		for (Node& known : gNodes) {
			if (!known.mNode) {
				known.mNode = FindNode(known.mPath);
				found = found || known.mNode;
				missing = missing || !known.mNode;
			}
		}
		gNodesResolved = !missing;
		if (found || gNodesResolved) {
			LOG("umbrella: nodes:");
			for (const Node& known : gNodes) {
				LogNode(known);
			}
		}
	}

	static void* Component(void* sim, int slot)
	{
		const uint8_t* holders = Read<const uint8_t*>(sim, 0x68);
		return holders && static_cast<uint32_t>(slot) < Read<uint32_t>(sim, 0x60) ? Read<void*>(holders, slot * 16) : nullptr;
	}

	// UEL::gCurrentParameters = the object's parameters while its action tree code runs, as the game does.
	class Parameters
	{
	public:
		explicit Parameters(void* sim) : mSaved(*gParameters)
		{
			if (void* uel = Component(sim, 0)) {
				*gParameters = static_cast<uint8_t*>(uel) + 0x58;
			}
		}
		~Parameters() { *gParameters = mSaved; }
		Parameters(const Parameters&) = delete;
		Parameters& operator=(const Parameters&) = delete;

	private:
		const void* mSaved;
	};

	struct Prop
	{
		void* mSim = nullptr;
		uint8_t* mAtc = nullptr;
		const char* mFile = nullptr;
		void* Current() const { return Read<void*>(mAtc, 0xC0 + 0x10); }
		bool IsOpen() const
		{
			const void* node = Current();
			return node && (node == gNodes[kPropOpening].mNode || node == gNodes[kPropOpened].mNode);
		}
	};

	static bool ContainsNoCase(const char* text, const char* word)
	{
		for (; text && *text; ++text) {
			size_t i = 0;
			while (word[i] && text[i] && std::tolower(static_cast<unsigned char>(text[i])) == word[i]) {
				++i;
			}
			if (!word[i]) {
				return true;
			}
		}
		return false;
	}

	// What Wei holds in his right hand, if it is an umbrella: its action tree is the umbrella prop's.
	static bool Umbrella(void* player, const uint8_t* playerAtc, Prop& prop, bool log)
	{
		void* sim = gGetTarget(player, kTargetEquipped);
		if (!sim) {
			if (log) {
				LOG("umbrella: nothing in the right hand");
			}
			return false;
		}
		const uint16_t flags = Read<uint16_t>(sim, 0x4C);
		const int slot = (flags & 0xC000) ? 7 : (flags & 0x2000) ? 6 : -1;
		uint8_t* atc = slot >= 0 ? static_cast<uint8_t*>(Component(sim, slot)) : nullptr;
		if (!atc || Read<void*>(atc, 0x28) != sim) {
			if (log) {
				LOG("umbrella: right hand holds %p (flags 0x%04X), no action tree component in slot %d (%p)", sim, flags, slot, atc);
			}
			return false;
		}
		prop.mSim = sim;
		prop.mAtc = atc;
		prop.mFile = Read<const char*>(atc, 0xB0);
		void* current = prop.Current();
		const bool sameTree = current && gNodes[kPropOpening].mNode && Root(current) == Root(gNodes[kPropOpening].mNode);
		const bool named = ContainsNoCase(prop.mFile, "umbrella");
		if (log) {
			LOG("umbrella: right hand holds %p (flags 0x%04X), action tree %p (vtable %s the player's) \"%s\", current node %08X (%s)%s", sim, flags,
				atc, Read<void*>(atc, 0) == Read<void*>(playerAtc, 0) ? "=" : "!=", prop.mFile ? prop.mFile : "?", NodeId(current), NodeName(current),
				sameTree || named ? ": an umbrella" : ": not an umbrella");
		}
		return sameTree || named;
	}

	static void PlayOnProp(const Prop& prop, NodeIndex index)
	{
		void* node = gNodes[index].mNode;
		if (!node) {
			LOG("umbrella: %s missing, the prop stays as it is", gNodes[index].mName);
			return;
		}
		// As TargetPlayTask::Begin does.
		Parameters parameters(prop.mSim);
		uint8_t* controller = prop.mAtc + 0xC0;
		gPlay(controller, node, false);
		gTimeBegin(controller, 0.0f, false);
		LOG("umbrella: prop plays %s (now %s)", gNodes[index].mName, NodeName(prop.Current()));
	}

	// Our upper body layer: an ActionController with a copy of the player's context, updated right after the
	// player's own, like a SpawnTask's.
	struct Layer
	{
		uint8_t* mContext = nullptr;
		uint8_t* mController = nullptr;
		const void* mSim = nullptr;
	};
	static Layer gLayer;

	static bool Ready(uint8_t* playerAtc)
	{
		void* sim = Read<void*>(playerAtc, 0x28);
		void* playerContext = Read<void*>(playerAtc, 0xB8);
		if (!playerContext) {
			LOG("umbrella: the player has no action context");
			return false;
		}
		if (gLayer.mController && gLayer.mSim == sim) {
			return true;
		}
		// A new player object (the first use, or after a load): new storage; the old one may still be referenced.
		gLayer.mContext = static_cast<uint8_t*>(_aligned_malloc(kContextSize, 16));
		gLayer.mController = static_cast<uint8_t*>(_aligned_malloc(kControllerSize, 16));
		if (!gLayer.mContext || !gLayer.mController) {
			gLayer = {};
			return false;
		}
		std::memset(gLayer.mContext, 0, kContextSize);
		std::memset(gLayer.mController, 0, kControllerSize);
		gNewContext(gLayer.mContext);
		gNewController(gLayer.mController);
		gAssignContext(gLayer.mContext, playerContext);
		const uint8_t* playerController = playerAtc + 0xC0;
		Write<uint8_t>(gLayer.mController, 0x27, Read<uint8_t>(playerController, 0x27) != 0);
		Write<uint8_t>(gLayer.mController, 0x28, Read<uint8_t>(playerController, 0x28) != 0);
		Write<uint8_t>(gLayer.mController, 0x25, 1); // keep alive
		Write<void*>(gLayer.mController, 0x18, gLayer.mContext);
		Write<void*>(gLayer.mContext, 0x20, gLayer.mController);
		Write<void*>(gLayer.mContext, 0x28, playerContext);
		Write<uint16_t>(gLayer.mContext, 0x40, kActionTreeTypeAction);
		gLayer.mSim = sim;
		LOG("umbrella: upper body layer for player %p: context %p, controller %p (player's context %p)", sim, gLayer.mContext, gLayer.mController,
			playerContext);
		return true;
	}

	static void PlayOnLayer(uint8_t* playerAtc, NodeIndex index)
	{
		void* node = gNodes[index].mNode;
		if (!node) {
			LOG("umbrella: %s missing, Wei doesn't move", gNodes[index].mName);
			return;
		}
		void* sim = Read<void*>(playerAtc, 0x28);
		void* playerContext = Read<void*>(playerAtc, 0xB8);
		Write<void*>(gLayer.mContext, 0x18, node); // opening branch
		// A node from another tree needs that tree's memory for this character (SpawnTask::Begin).
		void* root = Root(node);
		void* playerRoot = Root(Read<void*>(playerContext, 0x18));
		if (root && root != playerRoot) {
			const int type = Read<int8_t>(root, 0xEC);
			void* base = type >= 0 && type < 4 ? Read<void*>(gLayer.mContext, 0x48 + type * 8) : nullptr;
			if (!base) {
				base = Read<void*>(gLayer.mContext, 0x50);
			}
			if (base && gAllocateFor(base, root)) {
				gRootInit(root, gLayer.mContext);
				LOG("umbrella: allocated tree %p (type %d) for the player", root, type);
			}
		}
		Parameters parameters(sim);
		gPlay(gLayer.mController, node, false);
		gTimeBegin(gLayer.mController, 0.0f, false);
		const void* current = Read<void*>(gLayer.mController, 0x10);
		LOG("umbrella: Wei plays %s (layer now at %08X %s)", gNodes[index].mName, NodeId(current), NodeName(current));
	}

	// Under an open umbrella Wei walks: a brisk walk (jog) only while the sprint input is held, no sprint, no
	// fighting, weapon changes or parkour. All through the game's own switches, as scripts set them, restored when
	// the umbrella closes:
	// - SimObjectCharacterPropertiesComponent (character slot 3) mBooleans +0xF0: bit 0 jog allowed, bit 1 sprint
	//   allowed (TSCharacter::Mthd_allow_jog / allow_sprint).
	// - AICharacterControllerComponent (slot 21; the player's input goes through the PlayerAI tree and this too):
	//   m_ActionRequestMask (9 x u64) +0x3D0, ANDed into the requests every update (Mthd_action_request_disable
	//   clears bits). Request indices by name: Intention::GetActionRequest.
	// - What the action tree got this frame: ActionTreeComponent::m_Intention +0x218, mActionRequests +0xB0.
	using GetActionRequestFn = bool(__fastcall*)(const char* name, uint32_t* index);
	static GetActionRequestFn gGetActionRequest = nullptr;

	static constexpr const char* kBlockedRequests[] = { "Attack", "Attack2", "RunningAttack", "MidRangeAttack", "StrikeRelease", "Grab", "Guard",
		"Taunt", "Pickup", "Equip", "EquipUP", "Inventory", "Weapon", "WeaponMode", "Freerun", "Jump", "Dive", "UseCover", "CoverToggle" };
	static constexpr size_t kBlockedCount = sizeof(kBlockedRequests) / sizeof(kBlockedRequests[0]);
	static uint32_t gBlocked[kBlockedCount];
	static size_t gBlockedFound = 0;
	static uint32_t gSprintRequest = UINT32_MAX;
	static bool gRequestsResolved = false;

	struct Restrictions
	{
		const void* mSim = nullptr;
		uint8_t* mProperties = nullptr;
		uint8_t* mController = nullptr;
		uint64_t mBooleans = 0;
		uint64_t mMask[9] = {};
		int mBrisk = -1; // last jog state written
	};
	static Restrictions gRestrictions;
	static void NoteOurWrite();

	static bool Bit(const uint8_t* bits, uint32_t index)
	{
		return index < 9 * 64 && ((Read<uint64_t>(bits, (index >> 6) * 8) >> (index & 63)) & 1) != 0;
	}

	static void ResolveRequests()
	{
		if (gRequestsResolved) {
			return;
		}
		gRequestsResolved = true;
		char list[512] = {};
		for (const char* name : kBlockedRequests) {
			uint32_t index = UINT32_MAX;
			if (gGetActionRequest(name, &index) && index < 9 * 64) {
				gBlocked[gBlockedFound++] = index;
			}
			const size_t used = std::strlen(list);
			std::snprintf(list + used, sizeof(list) - used, "%s%s=%d", used ? " " : "", name, index < 9 * 64 ? static_cast<int>(index) : -1);
		}
		if (!gGetActionRequest("Sprint", &gSprintRequest) || gSprintRequest >= 9 * 64) {
			gSprintRequest = UINT32_MAX;
		}
		LOG("umbrella: action requests: %s, Sprint=%d", list, gSprintRequest != UINT32_MAX ? static_cast<int>(gSprintRequest) : -1);
	}

	static void Restrict(void* player)
	{
		if (gRestrictions.mSim) {
			return;
		}
		ResolveRequests();
		Restrictions r;
		r.mSim = player;
		r.mProperties = static_cast<uint8_t*>(Component(player, 3));
		r.mController = static_cast<uint8_t*>(Component(player, 21));
		if (r.mProperties && Read<void*>(r.mProperties, 0x28) != player) {
			r.mProperties = nullptr;
		}
		if (r.mController && Read<void*>(r.mController, 0x28) != player) {
			r.mController = nullptr;
		}
		if (r.mProperties) {
			r.mBooleans = Read<uint64_t>(r.mProperties, 0xF0);
			Write<uint64_t>(r.mProperties, 0xF0, r.mBooleans & ~3ull);
			NoteOurWrite();
		}
		if (r.mController) {
			std::memcpy(r.mMask, r.mController + 0x3D0, sizeof(r.mMask));
		}
		gRestrictions = r;
		LOG("umbrella: walking only: properties %p (jog %d, sprint %d before), controller %p, %zu requests blocked", r.mProperties,
			static_cast<int>(r.mBooleans & 1), static_cast<int>((r.mBooleans >> 1) & 1), r.mController, gBlockedFound);
	}

	// Every frame while restricted: requests blocked (again, in case a script enabled them), jog only while the
	// sprint input is held.
	static void KeepRestricted(const uint8_t* playerAtc, bool focus)
	{
		Restrictions& r = gRestrictions;
		if (!r.mSim) {
			return;
		}
		if (r.mController) {
			for (size_t i = 0; i < gBlockedFound; ++i) {
				const size_t word = (gBlocked[i] >> 6) * 8;
				Write<uint64_t>(r.mController, 0x3D0 + word, Read<uint64_t>(r.mController, 0x3D0 + word) & ~(1ull << (gBlocked[i] & 63)));
			}
		}
		if (r.mProperties) {
			const bool sprintRequest = Bit(playerAtc + 0x218 + 0xB0, gSprintRequest);
			const bool shift = focus && (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
			const bool brisk = (r.mBooleans & 1) && (sprintRequest || shift);
			const uint64_t booleans = Read<uint64_t>(r.mProperties, 0xF0) & ~3ull;
			Write<uint64_t>(r.mProperties, 0xF0, booleans | (brisk ? 1 : 0));
			NoteOurWrite();
			if (static_cast<int>(brisk) != r.mBrisk) {
				r.mBrisk = brisk;
				LOG("umbrella: %s (sprint request %d, shift %d)", brisk ? "brisk walk" : "slow walk", sprintRequest, shift);
			}
		}
	}

	// Scripts switch jog and sprint too (TSCharacter::Mthd_allow_jog/allow_sprint: the interiors' force walk,
	// missions). Such a call, and any other change, is the game's own wish: while the umbrella is open it becomes
	// what is restored when it closes. (Otherwise, opened indoors and carried out, the indoor "no sprint" would be
	// restored.) Every change to the player's two switches is logged.
	using ScriptMethodFn = void(__fastcall*)(void* self, void* scope, void** result);
	static ScriptMethodFn gAllowJog = nullptr;
	static ScriptMethodFn gAllowSprint = nullptr;
	static uint64_t gSeenBits = UINT64_MAX; // the player's jog/sprint bits after our last write or look

	static uint8_t* PlayerProperties()
	{
		void* player = gLocalPlayer ? *gLocalPlayer : nullptr;
		uint8_t* properties = player ? static_cast<uint8_t*>(Component(player, 3)) : nullptr;
		return properties && Read<void*>(properties, 0x28) == player ? properties : nullptr;
	}

	// Changes since we last looked were made by the game: log them, and while restricted keep them as its wish.
	static void NoteGameChanges(const char* by)
	{
		uint8_t* properties = PlayerProperties();
		if (!properties) {
			return;
		}
		const uint64_t bits = Read<uint64_t>(properties, 0xF0) & 3;
		if (bits == gSeenBits) {
			return;
		}
		Restrictions& r = gRestrictions;
		if (r.mSim && r.mProperties == properties && gSeenBits != UINT64_MAX) {
			const uint64_t changed = bits ^ gSeenBits;
			r.mBooleans = (r.mBooleans & ~changed) | (bits & changed);
			LOG("umbrella: %s set jog %d sprint %d while the umbrella is open; restored when it closes", by, static_cast<int>(bits & 1),
				static_cast<int>(bits >> 1));
		}
		else {
			LOG("umbrella: player can %sjog, %ssprint (%s)", bits & 1 ? "" : "not ", bits & 2 ? "" : "not ", gSeenBits == UINT64_MAX ? "first look" : by);
		}
		gSeenBits = bits;
	}

	static void NoteOurWrite()
	{
		if (uint8_t* properties = PlayerProperties()) {
			gSeenBits = Read<uint64_t>(properties, 0xF0) & 3;
		}
	}

	static void ScriptAllow(ScriptMethodFn original, const char* name, void* self, void* scope, void** result)
	{
		Restrictions& r = gRestrictions;
		uint8_t* properties = r.mSim ? r.mProperties : nullptr;
		uint64_t ours = 0;
		if (properties) {
			// Let the script see and set the game's own values, then take them and put ours back.
			ours = Read<uint64_t>(properties, 0xF0) & 3;
			Write<uint64_t>(properties, 0xF0, (Read<uint64_t>(properties, 0xF0) & ~3ull) | (r.mBooleans & 3));
			gSeenBits = r.mBooleans & 3;
		}
		original(self, scope, result);
		NoteGameChanges(name);
		if (properties) {
			Write<uint64_t>(properties, 0xF0, (Read<uint64_t>(properties, 0xF0) & ~3ull) | ours);
			NoteOurWrite();
		}
	}

	static void __fastcall AllowJogHook(void* self, void* scope, void** result)
	{
		ScriptAllow(gAllowJog, "script allow_jog", self, scope, result);
	}

	static void __fastcall AllowSprintHook(void* self, void* scope, void** result)
	{
		ScriptAllow(gAllowSprint, "script allow_sprint", self, scope, result);
	}

	static void Unrestrict()
	{
		Restrictions& r = gRestrictions;
		if (!r.mSim) {
			return;
		}
		if (r.mProperties) {
			Write<uint64_t>(r.mProperties, 0xF0, (Read<uint64_t>(r.mProperties, 0xF0) & ~3ull) | (r.mBooleans & 3));
			NoteOurWrite();
		}
		if (r.mController) {
			for (size_t i = 0; i < gBlockedFound; ++i) {
				const size_t word = (gBlocked[i] >> 6) * 8;
				const uint64_t bit = 1ull << (gBlocked[i] & 63);
				const uint64_t saved = r.mMask[word / 8] & bit;
				Write<uint64_t>(r.mController, 0x3D0 + word, (Read<uint64_t>(r.mController, 0x3D0 + word) & ~bit) | saved);
			}
		}
		LOG("umbrella: blocked requests restored, jog %d sprint %d (the game's current wish)", static_cast<int>(r.mBooleans & 1),
			static_cast<int>((r.mBooleans >> 1) & 1));
		r = {};
	}

	enum class State { Closed, Opening, Open, Closing };
	static State gState = State::Closed;
	static float gTime = 0.0f;
	static float gSinceStatus = 0.0f;
	static bool gPropChecked = false;
	static bool gKeyWasDown[2] = {};

	// The umbrella prop's tree opens it whenever it rains (OnInit, under an IsRainingCondition, which
	// pedestrians' umbrellas rely on) and re-checks that on entering Closed, so in the rain Wei's umbrella
	// reopened right after closing. For the umbrella in his hand the check answers whether he holds it open.
	using IsRainingFn = bool(__fastcall*)(void* condition, void* context);
	static IsRainingFn gIsRaining = nullptr;
	static std::atomic<void*> gHeldUmbrella{ nullptr };
	static std::atomic<bool> gWantOpen{ false };
	static std::atomic<int> gRainAnswers{ 0 };

	static bool __fastcall IsRainingHook(void* condition, void* context)
	{
		void* held = gHeldUmbrella.load(std::memory_order_relaxed);
		// ActionContext::mSimObject.m_pPointer
		if (held && context && Read<void*>(context, 0x10) == held) {
			const bool open = gWantOpen.load(std::memory_order_relaxed);
			if (gRainAnswers.fetch_add(1, std::memory_order_relaxed) < 5) {
				LOG("umbrella: rain check for the umbrella in hand: %s (raining: %d)", open ? "yes" : "no", gIsRaining(condition, context));
			}
			return open;
		}
		return gIsRaining(condition, context);
	}

	static bool Pressed(int key, bool& wasDown)
	{
		const bool down = (GetAsyncKeyState(key) & 0x8000) != 0;
		const bool pressed = down && !wasDown;
		wasDown = down;
		return pressed;
	}

	static bool GameHasFocus()
	{
		DWORD pid = 0;
		GetWindowThreadProcessId(GetForegroundWindow(), &pid);
		return pid == GetCurrentProcessId();
	}

	static void Dump(void* player, uint8_t* playerAtc)
	{
		const uint8_t* controller = playerAtc + 0xC0;
		const void* context = Read<void*>(playerAtc, 0xB8);
		const void* current = Read<void*>(controller, 0x10);
		const void* opening = context ? Read<void*>(context, 0x18) : nullptr;
		LOG("umbrella: F9: player %p (flags 0x%04X), action tree %p \"%s\", node %08X, opening branch %08X, state %d, layer %p at %08X (%s)", player,
			Read<uint16_t>(player, 0x4C), playerAtc, Read<const char*>(playerAtc, 0xB0) ? Read<const char*>(playerAtc, 0xB0) : "?", NodeId(current),
			NodeId(opening), static_cast<int>(gState), gLayer.mController,
			gLayer.mController ? NodeId(Read<void*>(gLayer.mController, 0x10)) : 0,
			gLayer.mController ? NodeName(Read<void*>(gLayer.mController, 0x10)) : "-");
		Prop prop;
		Umbrella(player, playerAtc, prop, true);
		gNodesResolved = false;
		ResolveNodes();
		LOG("umbrella: nodes:");
		for (const Node& known : gNodes) {
			LogNode(known);
		}
		gRequestsResolved = false;
		gBlockedFound = 0;
		ResolveRequests();
		const uint8_t* properties = static_cast<const uint8_t*>(Component(player, 3));
		const uint8_t* aiController = static_cast<const uint8_t*>(Component(player, 21));
		LOG("umbrella: properties %p (booleans 0x%llX), AI controller %p (mask word 0 0x%016llX), restricted %d", properties,
			properties ? static_cast<unsigned long long>(Read<uint64_t>(properties, 0xF0)) : 0ull, aiController,
			aiController ? static_cast<unsigned long long>(Read<uint64_t>(aiController, 0x3D0)) : 0ull, gRestrictions.mSim != nullptr);
	}

	static void Stop(const char* why)
	{
		if (gLayer.mController) {
			gStop(gLayer.mController);
		}
		gState = State::Closed;
		LOG("umbrella: closed (%s)", why);
		Unrestrict();
	}

	static void Tick(uint8_t* playerAtc, float delta)
	{
		NoteGameChanges("the game");
		void* player = Read<void*>(playerAtc, 0x28);
		const bool focus = GameHasFocus();
		const bool toggle = Pressed(VK_F7, gKeyWasDown[0]) && focus;
		const bool dump = Pressed(VK_F9, gKeyWasDown[1]) && focus;

		if (dump) {
			Dump(player, playerAtc);
		}
		if (gState != State::Closed && player != gLayer.mSim) {
			// Another player object (a load): the old layer's tasks may point at the old one, and the old
			// components may be gone; leave them alone.
			gState = State::Closed;
			gLayer = {};
			gRestrictions = {};
			LOG("umbrella: the player changed, layer dropped");
		}

		// Which umbrella Wei holds, every frame: IsRainingHook answers for it.
		Prop prop;
		const bool hasUmbrella = Umbrella(player, playerAtc, prop, toggle);
		void* held = hasUmbrella ? prop.mSim : nullptr;
		if (void* previous = gHeldUmbrella.exchange(held); previous != held) {
			if (held) {
				LOG("umbrella: umbrella in hand: %p", held);
				ResolveNodes();
			}
			else {
				LOG("umbrella: umbrella %p no longer in hand", previous);
			}
		}
		if (toggle) {
			ResolveNodes();
			if (gState == State::Closed && hasUmbrella && Ready(playerAtc)) {
				Restrict(player);
				PlayOnLayer(playerAtc, kOpen);
				gState = State::Opening;
				gTime = gSinceStatus = 0.0f;
				gPropChecked = false;
			}
			else if (gState == State::Opening || gState == State::Open) {
				PlayOnLayer(playerAtc, kClose);
				gState = State::Closing;
				gTime = gSinceStatus = 0.0f;
				gPropChecked = false;
			}
		}
		const bool wantOpen = gState == State::Opening || gState == State::Open;
		if (gWantOpen.exchange(wantOpen) != wantOpen) {
			gRainAnswers = 3; // log the next two answers
		}

		// The umbrella is open exactly while Wei holds it open: close one that is open anyway (picked up open,
		// or opened by the rain before), and end our open state if it closed.
		void* propNode = hasUmbrella ? prop.Current() : nullptr;
		if (gState == State::Closed) {
			if (propNode && propNode == gNodes[kPropOpened].mNode) {
				LOG("umbrella: the umbrella in hand is open by itself, closing it");
				PlayOnProp(prop, kPropClosing);
			}
			return;
		}
		if (!hasUmbrella) {
			Stop("no umbrella in hand any more");
			return;
		}
		if (gState == State::Open && propNode && (propNode == gNodes[kPropClosing].mNode || propNode == gNodes[kPropClosed].mNode)) {
			Stop("the umbrella closed by itself");
			return;
		}

		KeepRestricted(playerAtc, focus);
		gTime += delta;
		gSinceStatus += delta;
		// The pedestrians' Open_Umbrella/Close_Umbrella tell the umbrella to open at 0.433 s / close at 1.6 s
		// through a TargetPlayTrack; if that target isn't Wei's umbrella, open or close it ourselves.
		if (gState == State::Opening && !gPropChecked && gTime >= 0.6f) {
			gPropChecked = true;
			LOG("umbrella: %.2f s: prop at %08X (%s)", gTime, NodeId(prop.Current()), NodeName(prop.Current()));
			if (!prop.IsOpen()) {
				PlayOnProp(prop, kPropOpening);
			}
		}
		if (gState == State::Closing && !gPropChecked && gTime >= 1.8f) {
			gPropChecked = true;
			LOG("umbrella: %.2f s: prop at %08X (%s)", gTime, NodeId(prop.Current()), NodeName(prop.Current()));
			if (prop.IsOpen()) {
				PlayOnProp(prop, kPropClosing);
			}
		}
		if (gState == State::Opening && gTime >= 1.3f) {
			PlayOnLayer(playerAtc, kCarry);
			gState = State::Open;
		}

		{
			Parameters parameters(player);
			gControllerUpdate(gLayer.mController, delta);
		}
		const void* current = Read<void*>(gLayer.mController, 0x10);
		if (gSinceStatus >= 1.0f) {
			gSinceStatus = 0.0f;
			LOG("umbrella: %.1f s: layer at %08X (%s), %.2f s in; prop at %08X (%s); Wei's own node %08X", gTime, NodeId(current), NodeName(current),
				Read<float>(gLayer.mController, 0x20), NodeId(prop.Current()), NodeName(prop.Current()), NodeId(Read<void*>(playerAtc, 0xC0 + 0x10)));
		}
		if (gState == State::Closing && gTime >= 1.9f && (!current || gTime >= 3.0f)) {
			Stop("closing done");
		}
	}

	static void __fastcall AtcUpdateHook(void* atc, float delta)
	{
		gAtcUpdate(atc, delta);
		if (Read<void*>(atc, 0x28) == *gLocalPlayer && *gLocalPlayer) {
			Tick(static_cast<uint8_t*>(atc), delta);
		}
	}

	void Install()
	{
		if (!gConfig.mUmbrellaPrototype) {
			LOG("umbrella: prototype off");
			return;
		}
		uint8_t* update = scan::FindUnique("ActionTreeComponent::update",
			"40 57 48 83 EC 30 48 8B F9 48 8B 49 28 0F 29 74 24 20 0F 28 F1 48 85 C9 74 10");
		uint8_t* subjectPosition = scan::FindUnique("RoadNetworkVisibleArea::GetSubjectPosition",
			"48 89 5C 24 08 57 48 83 EC 20 48 8B 3D ? ? ? ? 48 8B DA");
		bool ok = update && subjectPosition &&
			// mov rsi, UEL::gCurrentParameters; add rax, 58h (UELComponent parameters); lea rcx, [rdi+0C0h] (controller)
			scan::Matches(update + 0x34, "48 8B 35") && scan::Matches(update + 0xA0, "48 83 C0 58 48 89 05") &&
			scan::Matches(update + 0xAB, "48 8D 8F C0 00 00 00") &&
			// mov rdi, UFG::gSim.mpLocalPlayer
			scan::Matches(subjectPosition + 0xA, "48 8B 3D");
		// Literal patterns straight into FindUnique, so tools\pdb.ps1 verify checks them.
		gFind = reinterpret_cast<FindFn>(scan::FindUnique("ActionNode::Find", "48 89 5C 24 20 41 56 48 83 EC 20 8B 19 4C 8B F1"));
		gPlay = reinterpret_cast<PlayFn>(scan::FindUnique("ActionController::Play", "48 85 D2 0F 84 ? ? ? ? 48 89 5C 24 18 48 89 7C 24 20 41 56"));
		gControllerUpdate = reinterpret_cast<ControllerUpdateFn>(scan::FindUnique("ActionController::Update",
			"40 55 53 48 8D 6C 24 B1 48 81 EC 88 00 00 00 80 3D ? ? ? ? 00"));
		gStop = reinterpret_cast<StopFn>(scan::FindUnique("ActionController::Stop", "40 53 48 83 EC 20 48 8B D9 E8 ? ? ? ? 33 C0 48 89 43 38"));
		gTimeBegin = reinterpret_cast<TimeBeginFn>(scan::FindUnique("ActionController::updateTasksTimeBegin",
			"48 8B C4 48 89 58 18 48 89 70 20 41 54 41 56 41 57 48 83 EC 70"));
		gNewController = reinterpret_cast<CtorFn>(scan::FindUnique("ActionController::ActionController",
			"48 89 4C 24 08 48 83 EC 18 48 C7 04 24 FE FF FF FF 48 8D 05 ? ? ? ? 48 89 01 33 D2 48 89 51 08 48 8D 05 ? ? ? ? 48 89 01 48 89 51 10 "
			"48 89 51 18 89 51 20"));
		gNewContext = reinterpret_cast<CtorFn>(scan::FindUnique("ActionContext::ActionContext",
			"48 89 4C 24 08 57 48 83 EC 30 48 C7 44 24 20 FE FF FF FF 48 89 5C 24 50 48 8B D9 48 89 4C 24 48 48 89 09 48 89 49 08 33 FF 48 89 79 10 "
			"48 89 79 18"));
		gAssignContext = reinterpret_cast<AssignFn>(scan::FindUnique("ActionContext::operator=",
			"48 89 5C 24 08 57 48 83 EC 20 48 8B FA 48 8B D9 48 3B CA 0F 84 ? ? ? ? 0F B7 52 40"));
		gAllocateFor = reinterpret_cast<AllocateForFn>(scan::FindUnique("ActionTreeComponentBase::AllocateFor",
			"40 57 48 83 EC 30 48 C7 44 24 20 FE FF FF FF 48 89 5C 24 40 48 89 74 24 50 48 8B DA"));
		gRootInit = reinterpret_cast<RootInitFn>(scan::FindUnique("ActionNodeRoot::Init",
			"48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 20 48 8D A9 88 00 00 00"));
		gGetTarget = reinterpret_cast<GetTargetFn>(scan::FindUnique("UFG::getTarget", "48 89 5C 24 08 57 48 83 EC 20 8B FA 48 8B D9 83 FA 01 75 0E"));
		gGetActionRequest = reinterpret_cast<GetActionRequestFn>(scan::FindUnique("Intention::GetActionRequest",
			"48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20 41 56 48 83 EC 20 8B 05"));
		uint8_t* isRaining = scan::FindUnique("IsRainingCondition::Match", "48 83 EC 28 E8 ? ? ? ? 48 8B C8 48 85 C0 74 16 F3 0F 10 05");
		uint8_t* allowJog = scan::FindUnique("TSCharacter::Mthd_allow_jog",
			"40 53 48 83 EC 20 48 8B 89 D0 00 00 00 48 8B DA 48 85 C9 74 3E 0F B7 41 4C 66 C1 E8 0E A8 01 74 32 E8 ? ? ? ? 48 85 C0 74 28 "
			"48 8B 4B 60 48 8B 11 48 8B 4A 08 48 83 79 20 00 76 0E 48 83 88 F0 00 00 00 01");
		uint8_t* allowSprint = scan::FindUnique("TSCharacter::Mthd_allow_sprint",
			"40 53 48 83 EC 20 48 8B 89 D0 00 00 00 48 8B DA 48 85 C9 74 3E 0F B7 41 4C 66 C1 E8 0E A8 01 74 32 E8 ? ? ? ? 48 85 C0 74 28 "
			"48 8B 4B 60 48 8B 11 48 8B 4A 08 48 83 79 20 00 76 0E 48 83 88 F0 00 00 00 02");
		ok = ok && gFind && gPlay && gControllerUpdate && gStop && gTimeBegin && gNewController && gNewContext && gAssignContext && gAllocateFor &&
			gRootInit && gGetTarget && gGetActionRequest && isRaining && allowJog && allowSprint;
		if (!ok) {
			LOG("umbrella: game functions MISSING or not as expected, prototype off");
			return;
		}
		gParameters = static_cast<const void**>(scan::RipTarget(update + 0x37));
		gLocalPlayer = static_cast<void**>(scan::RipTarget(subjectPosition + 0xD));

		// The rain check first, so the update never runs without it.
		if (MH_CreateHook(isRaining, reinterpret_cast<void*>(&IsRainingHook), reinterpret_cast<void**>(&gIsRaining)) != MH_OK ||
			MH_EnableHook(isRaining) != MH_OK ||
			MH_CreateHook(allowJog, reinterpret_cast<void*>(&AllowJogHook), reinterpret_cast<void**>(&gAllowJog)) != MH_OK ||
			MH_EnableHook(allowJog) != MH_OK ||
			MH_CreateHook(allowSprint, reinterpret_cast<void*>(&AllowSprintHook), reinterpret_cast<void**>(&gAllowSprint)) != MH_OK ||
			MH_EnableHook(allowSprint) != MH_OK ||
			MH_CreateHook(update, reinterpret_cast<void*>(&AtcUpdateHook), reinterpret_cast<void**>(&gAtcUpdate)) != MH_OK ||
			MH_EnableHook(update) != MH_OK) {
			LOG("umbrella: hooking the rain check, allow_jog/allow_sprint or ActionTreeComponent::update failed, prototype off");
			return;
		}
		LOG("umbrella: prototype hooked: with an umbrella in hand F7 opens/closes it, F9 logs the state");
	}
}
