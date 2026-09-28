#pragma once

#include <cstdint>

namespace hash
{
	// UFG::qStringHash32: MSB-first CRC-32 (0x04C11DB7), init 0xFFFFFFFF, no final xor. Track class names.
	// UFG::qStringHashUpper32: the same over the uppercased text. Effect IDs, action node names, symbols.
	constexpr uint32_t String32(const char* text, bool upper = false)
	{
		uint32_t hash = 0xFFFFFFFF;
		for (; *text; ++text) {
			const char c = upper && *text >= 'a' && *text <= 'z' ? static_cast<char>(*text - 'a' + 'A') : *text;
			hash ^= static_cast<uint32_t>(static_cast<uint8_t>(c)) << 24;
			for (int bit = 0; bit < 8; ++bit) {
				hash = hash & 0x80000000 ? (hash << 1) ^ 0x04C11DB7 : hash << 1;
			}
		}
		return hash;
	}

	constexpr uint32_t Upper32(const char* text)
	{
		return String32(text, true);
	}
}
