#include "scan.hh"

#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "log.hh"

namespace scan
{
	struct Section
	{
		uint8_t* mBegin = nullptr;
		size_t mSize = 0;
		const uint8_t* mImage = nullptr;
		size_t mImageSize = 0;
	};

	static Section FindText()
	{
		// The main module, not whoever loaded us: the ASI loader runs inside dinput8.dll.
		auto* base = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
		auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
		auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
		IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
		for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
			if (std::memcmp(section->Name, ".text", 6) == 0) {
				return { base + section->VirtualAddress, section->Misc.VirtualSize, base, nt->OptionalHeader.SizeOfImage };
			}
		}
		return {};
	}

	static bool Parse(const char* pattern, std::vector<uint8_t>& bytes, std::vector<bool>& mask)
	{
		for (const char* p = pattern; *p;) {
			if (*p == ' ') {
				++p;
			}
			else if (*p == '?') {
				bytes.push_back(0);
				mask.push_back(false);
				while (*p == '?') {
					++p;
				}
			}
			else {
				char* end = nullptr;
				const unsigned long value = std::strtoul(p, &end, 16);
				if (end == p || value > 0xFF) {
					return false;
				}
				bytes.push_back(static_cast<uint8_t>(value));
				mask.push_back(true);
				p = end;
			}
		}
		return !bytes.empty() && mask.front();
	}

	// A `jmp rel32` at `at` that leaves the exe: MinHook's, to the relay it allocates outside the image.
	static bool JumpsOut(const Section& text, const uint8_t* at)
	{
		int32_t offset;
		std::memcpy(&offset, at + 1, sizeof(offset));
		const uint8_t* target = at + 5 + offset;
		return target < text.mImage || target >= text.mImage + text.mImageSize;
	}

	// Matches compared from byte `from` of the pattern on (a fixed byte); `found` is where the last one starts.
	static int Count(const Section& text, const std::vector<uint8_t>& bytes, const std::vector<bool>& mask, size_t from, bool hooked,
		uint8_t*& found)
	{
		int count = 0;
		const size_t length = bytes.size();
		const uint8_t* last = text.mBegin + text.mSize - length + from;
		for (uint8_t* p = text.mBegin + from; p <= last; ++p) {
			p = static_cast<uint8_t*>(std::memchr(p, bytes[from], static_cast<size_t>(last - p) + 1));
			if (!p) {
				break;
			}
			uint8_t* start = p - from;
			size_t i = from + 1;
			while (i < length && (!mask[i] || start[i] == bytes[i])) {
				++i;
			}
			if (i == length && (!hooked || JumpsOut(text, start))) {
				found = start;
				++count;
			}
		}
		return count;
	}

	// When only the first 5 bytes differ, logs what is there instead: a patch of some other kind, which one log then shows.
	static void LogPatchedStart(const char* name, const Section& text, const std::vector<uint8_t>& bytes, const std::vector<bool>& mask)
	{
		size_t from = 5;
		while (from < bytes.size() && !mask[from]) {
			++from;
		}
		uint8_t* found = nullptr;
		if (from == bytes.size() || Count(text, bytes, mask, from, false, found) != 1) {
			return;
		}
		char start[16 * 3] = {};
		const size_t shown = std::min<size_t>(16, bytes.size());
		for (size_t i = 0; i < shown; ++i) {
			std::snprintf(start + i * 3, 4, i + 1 < shown ? "%02X " : "%02X", found[i]);
		}
		const auto* base = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
		LOG("scan: %s: not found; one match without its first 5 bytes, at +0x%llX, which starts %s", name,
			static_cast<unsigned long long>(found - base), start);
	}

	uint8_t* FindUnique(const char* name, const char* pattern)
	{
		static const Section text = FindText();
		std::vector<uint8_t> bytes;
		std::vector<bool> mask;
		if (!text.mBegin || !Parse(pattern, bytes, mask)) {
			LOG("scan: %s: bad pattern or no .text", name);
			return nullptr;
		}

		uint8_t* found = nullptr;
		int count = Count(text, bytes, mask, 0, false, found);
		// Another mod may have hooked the function before we looked (plugins load in name order), and MinHook writes a
		// `jmp rel32` over the first 5 bytes. Take that jump in their place; hooking the function again chains (MinHook
		// copies the jump into our trampoline). Only a jump out of the exe counts: with the first 5 bytes gone a pattern
		// can also match where another function's tail jump (`jmp rel32` into the exe) sits right before the same prologue.
		const char* hooked = "";
		if (count == 0 && bytes.size() >= 12) {
			std::vector<uint8_t> jumpBytes = bytes;
			std::vector<bool> jumpMask = mask;
			jumpBytes[0] = 0xE9;
			std::fill(jumpMask.begin() + 1, jumpMask.begin() + 5, false);
			count = Count(text, jumpBytes, jumpMask, 0, true, found);
			hooked = " (it starts with a jump: hooked by another mod)";
			if (count == 0) {
				LogPatchedStart(name, text, bytes, mask);
			}
		}

		const auto* base = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
		if (count != 1) {
			LOG("scan: %s: %d matches, not using it", name, count);
			return nullptr;
		}
		LOG("scan: %s at +0x%llX%s", name, static_cast<unsigned long long>(found - base), hooked);
		return found;
	}

	bool Matches(const uint8_t* at, const char* pattern)
	{
		std::vector<uint8_t> bytes;
		std::vector<bool> mask;
		if (!at || !Parse(pattern, bytes, mask)) {
			return false;
		}
		for (size_t i = 0; i < bytes.size(); ++i) {
			if (mask[i] && at[i] != bytes[i]) {
				return false;
			}
		}
		return true;
	}

	void* RipTarget(const uint8_t* disp, int trailing)
	{
		int32_t offset;
		std::memcpy(&offset, disp, sizeof(offset));
		return const_cast<uint8_t*>(disp + 4 + trailing + offset);
	}
}
