#include "dxbc.hh"

#include <cstring>

namespace dxbc
{
	static uint32_t Read32(const uint8_t* p)
	{
		uint32_t value;
		std::memcpy(&value, p, sizeof(value));
		return value;
	}

	static void Write32(uint8_t* p, uint32_t value)
	{
		std::memcpy(p, &value, sizeof(value));
	}

	// --- Checksum ---

	static constexpr uint32_t kMd5K[64] = {
		0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
		0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
		0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
		0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
		0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
		0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
		0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
		0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
	};
	static constexpr int kMd5S[16] = { 7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21 };

	static void Md5Block(uint32_t state[4], const uint8_t block[64])
	{
		uint32_t m[16];
		std::memcpy(m, block, sizeof(m));
		uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
		for (int i = 0; i < 64; ++i) {
			uint32_t f;
			int g;
			if (i < 16) {
				f = (b & c) | (~b & d);
				g = i;
			}
			else if (i < 32) {
				f = (d & b) | (~d & c);
				g = (5 * i + 1) % 16;
			}
			else if (i < 48) {
				f = b ^ c ^ d;
				g = (3 * i + 5) % 16;
			}
			else {
				f = c ^ (b | ~d);
				g = (7 * i) % 16;
			}
			f += a + kMd5K[i] + m[g];
			a = d;
			d = c;
			c = b;
			const int s = kMd5S[(i / 16) * 4 + i % 4];
			b += (f << s) | (f >> (32 - s));
		}
		state[0] += a;
		state[1] += b;
		state[2] += c;
		state[3] += d;
	}

	void Checksum(const uint8_t* blob, size_t size, uint8_t out[16])
	{
		// Plain MD5 over everything after the checksum field, except the final padding: the bit count goes
		// at the start of the last block and (bits >> 2) | 1 at its end.
		const uint8_t* data = blob + 20;
		const size_t length = size - 20;
		uint32_t state[4] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 };
		const size_t full = length / 64 * 64;
		for (size_t offset = 0; offset < full; offset += 64) {
			Md5Block(state, data + offset);
		}

		const size_t rest = length - full;
		const uint32_t bits = static_cast<uint32_t>(length * 8);
		uint8_t block[64] = {};
		if (rest >= 56) {
			std::memcpy(block, data + full, rest);
			block[rest] = 0x80;
			Md5Block(state, block);
			std::memset(block, 0, sizeof(block));
			Write32(block, bits);
		}
		else {
			Write32(block, bits);
			std::memcpy(block + 4, data + full, rest);
			block[4 + rest] = 0x80;
		}
		Write32(block + 60, (bits >> 2) | 1);
		Md5Block(state, block);
		std::memcpy(out, state, 16);
	}

	// --- Container ---

	struct Chunk
	{
		uint32_t mFourCC = 0;
		size_t mOffset = 0; // of the 8-byte chunk header
		size_t mSize = 0;   // of the body
	};

	static constexpr uint32_t FourCC(const char (&s)[5])
	{
		return uint32_t(uint8_t(s[0])) | uint32_t(uint8_t(s[1])) << 8 | uint32_t(uint8_t(s[2])) << 16 | uint32_t(uint8_t(s[3])) << 24;
	}

	static bool ReadChunks(const uint8_t* blob, size_t size, std::vector<Chunk>& chunks)
	{
		if (size < 32 || Read32(blob) != FourCC("DXBC") || Read32(blob + 24) != size) {
			return false;
		}
		const uint32_t count = Read32(blob + 28);
		if (32 + size_t(count) * 4 > size) {
			return false;
		}
		for (uint32_t i = 0; i < count; ++i) {
			const size_t offset = Read32(blob + 32 + 4 * i);
			if (offset + 8 > size) {
				return false;
			}
			const size_t body = Read32(blob + offset + 4);
			if (offset + 8 + body > size) {
				return false;
			}
			chunks.push_back({ Read32(blob + offset), offset, body });
		}
		return true;
	}

	static const Chunk* Find(const std::vector<Chunk>& chunks, uint32_t fourcc)
	{
		for (const Chunk& chunk : chunks) {
			if (chunk.mFourCC == fourcc) {
				return &chunk;
			}
		}
		return nullptr;
	}

	// Bind point of a named resource in the RDEF chunk (D3D_SIT_CBUFFER = 0, D3D_SIT_TEXTURE = 2), or -1.
	static int BindPoint(const uint8_t* rdef, size_t size, const char* name, uint32_t inputType)
	{
		if (size < 16) {
			return -1;
		}
		const uint32_t count = Read32(rdef + 8);
		const size_t offset = Read32(rdef + 12);
		if (offset + size_t(count) * 32 > size) {
			return -1;
		}
		const size_t nameLength = std::strlen(name);
		for (uint32_t i = 0; i < count; ++i) {
			const uint8_t* binding = rdef + offset + i * 32;
			const size_t nameOffset = Read32(binding);
			if (Read32(binding + 4) != inputType || nameOffset + nameLength + 1 > size) {
				continue;
			}
			if (std::memcmp(rdef + nameOffset, name, nameLength + 1) == 0) {
				return static_cast<int>(Read32(binding + 20));
			}
		}
		return -1;
	}

	// --- Shader Model 4 tokens ---

	enum : uint32_t
	{
		kOpAdd = 0x00,
		kOpLt = 0x31,
		kOpCustomData = 0x35,
		kOpMovc = 0x37,
		kOpSample = 0x45,  // .. kOpSampleB = 0x4A: sample, _c, _c_lz, _l, _d, _b
		kOpSampleB = 0x4A,
		kOpDclFirst = 0x58, // dcl_resource .. dcl_global_flags
		kOpDclTemps = 0x68,
		kOpDclLast = 0x6A,

		kTypeTemp = 0,
		kTypeImmediate32 = 4,
		kTypeImmediate64 = 5,
		kTypeResource = 7,
		kTypeConstantBuffer = 8,
	};

	struct Operand
	{
		uint32_t mToken = 0;
		uint32_t mIndex[3] = {};
		bool mIndexKnown = true; // false with relative addressing (x[r0.x + 2])
		uint32_t Type() const { return (mToken >> 12) & 0xFF; }
		uint32_t Dims() const { return (mToken >> 20) & 3; }
		uint32_t SelectionMode() const { return (mToken >> 2) & 3; }
	};

	// Parses the operand at toks[i]; returns the index after it, or 0 if it runs past `end`.
	static size_t ParseOperand(const uint32_t* toks, size_t i, size_t end, Operand& operand)
	{
		if (i >= end) {
			return 0;
		}
		operand.mToken = toks[i++];
		for (bool extended = (operand.mToken & 0x80000000) != 0; extended; ++i) {
			if (i >= end) {
				return 0;
			}
			extended = (toks[i] & 0x80000000) != 0;
		}
		const bool fourComponents = (operand.mToken & 3) == 2;
		if (operand.Type() == kTypeImmediate32) {
			i += fourComponents ? 4 : 1;
		}
		else if (operand.Type() == kTypeImmediate64) {
			i += fourComponents ? 8 : 2;
		}
		for (uint32_t d = 0; d < operand.Dims(); ++d) {
			switch ((operand.mToken >> (22 + 3 * d)) & 7) {
			case 0: // immediate32
				if (i >= end) {
					return 0;
				}
				operand.mIndex[d] = toks[i++];
				break;
			case 1: // immediate64
				i += 2;
				operand.mIndexKnown = false;
				break;
			case 2: // relative
			case 3: // immediate32 + relative
			case 4: // immediate64 + relative
			{
				const uint32_t representation = (operand.mToken >> (22 + 3 * d)) & 7;
				i += representation == 3 ? 1 : representation == 4 ? 2 : 0;
				Operand relative;
				i = ParseOperand(toks, i, end, relative);
				if (!i) {
					return 0;
				}
				operand.mIndexKnown = false;
				break;
			}
			default:
				return 0;
			}
		}
		return i <= end ? i : 0;
	}

	struct Instruction
	{
		size_t mOffset = 0; // in tokens, from the start of the SHDR body
		size_t mLength = 0;
		uint32_t mOpcode = 0;
		size_t mFirstOperand = 0;
	};

	static bool ReadInstructions(const uint32_t* toks, size_t count, std::vector<Instruction>& out)
	{
		for (size_t i = 2; i < count;) {
			Instruction inst;
			inst.mOffset = i;
			inst.mOpcode = toks[i] & 0x7FF;
			if (inst.mOpcode == kOpCustomData) {
				if (i + 1 >= count) {
					return false;
				}
				inst.mLength = toks[i + 1];
			}
			else {
				inst.mLength = (toks[i] >> 24) & 0x7F;
			}
			if (inst.mLength == 0 || i + inst.mLength > count) {
				return false;
			}
			size_t first = i + 1;
			for (bool extended = (toks[i] & 0x80000000) != 0 && inst.mOpcode != kOpCustomData; extended; ++first) {
				if (first >= i + inst.mLength) {
					return false;
				}
				extended = (toks[first] & 0x80000000) != 0;
			}
			inst.mFirstOperand = first;
			out.push_back(inst);
			i += inst.mLength;
		}
		return true;
	}

	static uint32_t FloatBits(float value)
	{
		uint32_t bits;
		std::memcpy(&bits, &value, sizeof(bits));
		return bits;
	}

	Result PatchWetShader(const uint8_t* blob, size_t size, const WetDefaults& defaults, std::vector<uint8_t>& out, std::string& why)
	{
		std::vector<Chunk> chunks;
		if (!ReadChunks(blob, size, chunks)) {
			why = "not a DXBC container";
			return Result::NotWet;
		}
		const Chunk* rdef = Find(chunks, FourCC("RDEF"));
		const Chunk* code = Find(chunks, FourCC("SHDR"));
		if (!code) {
			code = Find(chunks, FourCC("SHEX"));
		}
		if (!rdef || !code || code->mSize % 4 != 0 || code->mSize < 8) {
			why = "no RDEF or shader code";
			return Result::NotWet;
		}

		const uint8_t* rdefBody = blob + rdef->mOffset + 8;
		const int specular = BindPoint(rdefBody, rdef->mSize, "texSpecular", 2);
		const int scenery = BindPoint(rdefBody, rdef->mSize, "cbSceneryInstance", 0);
		if (specular < 0 || scenery < 0) {
			why = "no texSpecular / cbSceneryInstance";
			return Result::NotWet;
		}

		const size_t count = code->mSize / 4;
		std::vector<uint32_t> toks(count);
		std::memcpy(toks.data(), blob + code->mOffset + 8, code->mSize);
		// Version token, then the length of the whole program in tokens (the chunk may be padded).
		if (toks[1] < 2 || toks[1] > count) {
			why = "bad program length";
			return Result::NotWet;
		}
		toks.resize(toks[1]);

		std::vector<Instruction> insts;
		if (!ReadInstructions(toks.data(), toks.size(), insts)) {
			why = "unparsable instruction stream";
			return Result::NotWet;
		}

		// The _WS permutations start with max(cbSceneryInstance.Mask.z, .y): wetness and sweat.
		bool readsSweat = false, readsWetness = false;
		for (const Instruction& inst : insts) {
			if ((inst.mOpcode >= kOpDclFirst && inst.mOpcode <= kOpDclLast) || inst.mOpcode == kOpCustomData) {
				continue; // declarations: dcl_constantbuffer's second index is the buffer size, not a register
			}
			const size_t end = inst.mOffset + inst.mLength;
			for (size_t i = inst.mFirstOperand; i && i < end;) {
				Operand operand;
				i = ParseOperand(toks.data(), i, end, operand);
				if (!i || operand.Type() != kTypeConstantBuffer || operand.Dims() != 2 || !operand.mIndexKnown ||
					operand.mIndex[0] != uint32_t(scenery) || operand.mIndex[1] != 2) {
					continue;
				}
				for (int k = 0; k < 4; ++k) {
					uint32_t component;
					if (operand.SelectionMode() == 1) { // swizzle
						component = (operand.mToken >> (4 + 2 * k)) & 3;
					}
					else if (operand.SelectionMode() == 2 && k == 0) { // select one
						component = (operand.mToken >> 4) & 3;
					}
					else {
						break;
					}
					readsSweat |= component == 1;
					readsWetness |= component == 2;
				}
			}
		}
		if (!readsSweat || !readsWetness) {
			why = "doesn't read cbSceneryInstance.Mask.yz";
			return Result::NotWet;
		}

		size_t tempsOffset = 0;
		for (const Instruction& inst : insts) {
			if (inst.mOpcode == kOpDclTemps && inst.mLength == 2) {
				tempsOffset = inst.mOffset + 1;
			}
		}
		if (!tempsOffset) {
			why = "no dcl_temps";
			return Result::Failed;
		}
		const uint32_t temp = toks[tempsOffset]; // a new register, rN with N = the old count

		// After each sample from texSpecular into rX.xz:
		//   add  rT.x, rX.x, rX.z
		//   lt   rT.x, l(0.004), rT.x
		//   movc rX.xz, rT.xxxx, rX.xxzx, l(shine, 0, gloss, 0)
		struct Insert
		{
			size_t mAt;
			uint32_t mRegister;
		};
		std::vector<Insert> inserts;
		for (const Instruction& inst : insts) {
			if (inst.mOpcode < kOpSample || inst.mOpcode > kOpSampleB) {
				continue;
			}
			const size_t end = inst.mOffset + inst.mLength;
			Operand dest, address, resource;
			size_t i = ParseOperand(toks.data(), inst.mFirstOperand, end, dest);
			i = i ? ParseOperand(toks.data(), i, end, address) : 0;
			i = i ? ParseOperand(toks.data(), i, end, resource) : 0;
			if (!i) {
				why = "unparsable sample instruction";
				return Result::Failed;
			}
			if (resource.Type() != kTypeResource || resource.Dims() != 1 || resource.mIndex[0] != uint32_t(specular)) {
				continue;
			}
			const uint32_t mask = (dest.mToken >> 4) & 0xF;
			if (dest.Type() != kTypeTemp || dest.Dims() != 1 || !dest.mIndexKnown || dest.SelectionMode() != 0 || (mask & 5) != 5) {
				why = "texSpecular isn't sampled into .x and .z of a temp";
				return Result::Failed;
			}
			inserts.push_back({ end, dest.mIndex[0] });
		}
		if (inserts.empty()) {
			why = "no texSpecular sample";
			return Result::Failed;
		}

		std::vector<uint32_t> patched;
		patched.reserve(toks.size() + inserts.size() * 26);
		size_t copied = 0;
		for (const Insert& insert : inserts) {
			patched.insert(patched.end(), toks.begin() + copied, toks.begin() + insert.mAt);
			copied = insert.mAt;
			const uint32_t x = insert.mRegister;
			const uint32_t words[] = {
				0x07000000, 0x00100012, temp, 0x0010000A, x, 0x0010002A, x,
				0x07000031, 0x00100012, temp, 0x00004001, FloatBits(0.004f), 0x0010000A, temp,
				0x0C000037, 0x00100052, x, 0x00100006, temp, 0x00100206, x,
				0x00004002, FloatBits(defaults.mShine), 0, FloatBits(defaults.mGloss), 0,
			};
			patched.insert(patched.end(), std::begin(words), std::end(words));
		}
		patched.insert(patched.end(), toks.begin() + copied, toks.end());
		patched[tempsOffset] = temp + 1;
		patched[1] = static_cast<uint32_t>(patched.size());

		// Rebuild the container around the longer code chunk; chunks after it move.
		const size_t newCodeSize = patched.size() * 4;
		const size_t delta = newCodeSize - code->mSize;
		out.assign(blob, blob + code->mOffset + 4);
		out.resize(out.size() + 4 + newCodeSize);
		Write32(out.data() + code->mOffset + 4, static_cast<uint32_t>(newCodeSize));
		std::memcpy(out.data() + code->mOffset + 8, patched.data(), newCodeSize);
		out.insert(out.end(), blob + code->mOffset + 8 + code->mSize, blob + size);
		for (size_t i = 0; i < chunks.size(); ++i) {
			if (chunks[i].mOffset > code->mOffset) {
				Write32(out.data() + 32 + 4 * i, static_cast<uint32_t>(chunks[i].mOffset + delta));
			}
		}
		Write32(out.data() + 24, static_cast<uint32_t>(out.size()));

		// D3DReflect reports these; keep them honest.
		if (const Chunk* stat = Find(chunks, FourCC("STAT")); stat && stat->mSize >= 8) {
			uint8_t* body = out.data() + stat->mOffset + (stat->mOffset > code->mOffset ? delta : 0) + 8;
			Write32(body, Read32(body) + 3 * static_cast<uint32_t>(inserts.size()));
			Write32(body + 4, Read32(body + 4) + 1);
		}

		Checksum(out.data(), out.size(), out.data() + 4);
		why = "texSpecular sampled into r" + std::to_string(inserts.front().mRegister) + ", new temp r" + std::to_string(temp);
		return Result::Patched;
	}
}
