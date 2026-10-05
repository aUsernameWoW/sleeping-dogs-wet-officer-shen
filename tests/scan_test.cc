// The signature scan (core/scan.cc) on bytes in this program's own .text: a function's pattern is found, and still
// found once a 5-byte jump is written over its start, which is how MinHook hooks a function (another mod may hook
// ours before we load), but not where a tail jump inside the program precedes the same bytes, nor when the start was
// patched some other way. argv[1] (the .asi) isn't used. Copied from SDTaxi.

#include "../core/log.cc"
#include "../core/scan.cc"

#include <cstdio>

// A made-up function: a common prologue, then bytes found nowhere else. Main writes it over Placeholder: data in a
// section of its own would land in a second ".text" (the linker keeps data and code apart), which the scan skips.
static const unsigned char kFunction[] = {
	0x48, 0x89, 0x5C, 0x24, 0x10, 0x56, 0x48, 0x83, 0xEC, 0x20, 0x0F, 0xB6, 0xDA, 0x48, 0x8B, 0xF1,
	0x5A, 0x7C, 0x3E, 0x91, 0xD2, 0x6B, 0xC3,
};

static volatile int gSink;

// Never called; only its bytes in .text are used (long enough for kFunction).
static __declspec(noinline) void Placeholder()
{
	for (int i = 0; i < 16; ++i) {
		gSink = gSink * 31 + i;
		gSink = gSink ^ (gSink >> 3);
	}
}

// Never called either: where the decoy goes, another function's tail jump followed by the hooked function's bytes.
static __declspec(noinline) void DecoyPlaceholder()
{
	for (int i = 0; i < 16; ++i) {
		gSink = gSink * 37 - i;
		gSink = gSink ^ (gSink >> 5);
	}
}

static unsigned char* gFunction = nullptr;

static constexpr char kPattern[] = "48 89 5C 24 10 56 48 83 EC 20 0F B6 DA 48 8B F1 5A 7C 3E 91 D2 6B";
static constexpr char kAbsent[] = "48 89 5C 24 10 56 48 83 EC 20 0F B6 DA 48 8B F1 5A 7C 3E 91 D2 6C";

static bool Check(bool ok, const char* what)
{
	std::printf("%s: %s\n", ok ? "ok" : "FAIL", what);
	return ok;
}

static bool Write(unsigned char* at, const unsigned char* bytes, size_t size)
{
	DWORD protect;
	if (!VirtualProtect(at, size, PAGE_EXECUTE_READWRITE, &protect)) {
		std::printf("FAIL: VirtualProtect error %lu\n", GetLastError());
		return false;
	}
	std::memcpy(at, bytes, size);
	VirtualProtect(at, size, protect, &protect);
	return true;
}

int main()
{
	gFunction = reinterpret_cast<unsigned char*>(&Placeholder);
	if (!Write(gFunction, kFunction, sizeof(kFunction))) {
		return 1;
	}
	// jmp rel32 to the made-up function (inside this program), then the made-up function's bytes after its first 5.
	auto* decoy = reinterpret_cast<unsigned char*>(&DecoyPlaceholder);
	unsigned char decoyBytes[sizeof(kFunction)];
	std::memcpy(decoyBytes, kFunction, sizeof(kFunction));
	decoyBytes[0] = 0xE9;
	const int32_t toFunction = static_cast<int32_t>(gFunction - (decoy + 5));
	std::memcpy(decoyBytes + 1, &toFunction, sizeof(toFunction));
	if (!Write(decoy, decoyBytes, sizeof(decoyBytes))) {
		return 1;
	}

	bool ok = Check(scan::FindUnique("function", kPattern) == gFunction, "the pattern is found");
	ok &= Check(!scan::FindUnique("absent", kAbsent), "a pattern that isn't there is not found");

	// What MH_EnableHook writes: jmp rel32 to a detour somewhere else.
	const unsigned char jump[] = { 0xE9, 0x12, 0x34, 0x56, 0x07 };
	if (!Write(gFunction, jump, sizeof(jump))) {
		return 1;
	}

	ok &= Check(scan::FindUnique("hooked function", kPattern) == gFunction,
		"the pattern is found after a hook jump was written over its start, and not at a tail jump into the program");
	ok &= Check(!scan::FindUnique("hooked, absent", kAbsent), "a pattern that isn't there is still not found");
	ok &= Check(scan::Matches(gFunction + 5, "56 48 83 EC 20"), "only the first 5 bytes were replaced");

	// Some other patch over the start (int3s): not a hook we can chain.
	const unsigned char patch[] = { 0xCC, 0xCC, 0xCC, 0xCC, 0xCC };
	if (!Write(gFunction, patch, sizeof(patch))) {
		return 1;
	}
	ok &= Check(!scan::FindUnique("patched function", kPattern), "a start patched some other way is not taken");

	std::printf("%s\n", ok ? "PASS" : "FAIL");
	return ok ? 0 : 1;
}
