"""DXBC container: chunks, checksum, SM4 token walking (the Python original of core/dxbc.cc)."""
import struct

def chunks(blob):
	n = struct.unpack_from("<I", blob, 28)[0]
	out = []
	for i in range(n):
		o = struct.unpack_from("<I", blob, 32 + 4 * i)[0]
		out.append((blob[o:o + 4], o, struct.unpack_from("<I", blob, o + 4)[0]))
	return out

# MD5 with DXBC's own finalization (bit count at the front of the last block, (bits >> 2) | 1 at its end).
_S = [7, 12, 17, 22] * 4 + [5, 9, 14, 20] * 4 + [4, 11, 16, 23] * 4 + [6, 10, 15, 21] * 4
import math
_K = [int(abs(math.sin(i + 1)) * 2**32) & 0xFFFFFFFF for i in range(64)]

def _rotl(x, c):
	return ((x << c) | (x >> (32 - c))) & 0xFFFFFFFF

def _transform(state, block):
	m = struct.unpack("<16I", block)
	a, b, c, d = state
	for i in range(64):
		if i < 16:
			f, g = (b & c) | (~b & d), i
		elif i < 32:
			f, g = (d & b) | (~d & c), (5 * i + 1) % 16
		elif i < 48:
			f, g = b ^ c ^ d, (3 * i + 5) % 16
		else:
			f, g = c ^ (b | (~d & 0xFFFFFFFF)), (7 * i) % 16
		f = (f + a + _K[i] + m[g]) & 0xFFFFFFFF
		a, d, c = d, c, b
		b = (b + _rotl(f, _S[i])) & 0xFFFFFFFF
	return [(x + y) & 0xFFFFFFFF for x, y in zip(state, (a, b, c, d))]

def checksum(blob):
	data = blob[20:]
	state = [0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476]
	full = len(data) // 64 * 64
	for o in range(0, full, 64):
		state = _transform(state, data[o:o + 64])
	rest = data[full:]
	bits = len(data) * 8
	if len(rest) >= 56:
		state = _transform(state, rest + b"\x80" + b"\0" * (63 - len(rest)))
		state = _transform(state, struct.pack("<I", bits) + b"\0" * 56 + struct.pack("<I", (bits >> 2) | 1))
	else:
		block = struct.pack("<I", bits) + rest + b"\x80"
		block += b"\0" * (60 - len(block)) + struct.pack("<I", (bits >> 2) | 1)
		state = _transform(state, block)
	return struct.pack("<4I", *state)

def instructions(shdr):
	"""Yields (offset in DWORDs, opcode, length, tokens) for each instruction of a SHDR/SHEX body."""
	toks = struct.unpack("<%dI" % (len(shdr) // 4), shdr)
	i = 2
	while i < len(toks):
		op = toks[i] & 0x7FF
		if op == 0x35:  # customdata: length is the next DWORD
			n = toks[i + 1]
		else:
			n = (toks[i] >> 24) & 0x7F
		yield i, op, n, toks[i:i + n]
		i += n

if __name__ == "__main__":
	# python dxbc.py [file.o ...]: checks the checksum of every unpacked game shader (shaders.py unpack), then
	# dumps the instruction tokens of the given compiled shaders. That's how the encodings SDWet inserts were
	# found: compile a snippet with fxc (/T ps_4_0 /Fo x.o /Fc x.asm) and read its tokens next to the asm.
	import os, sys
	from common import SHADERS
	names = sorted(os.listdir(SHADERS)) if os.path.isdir(SHADERS) else []
	bad = sum(checksum(b) != b[4:20] for b in (open(os.path.join(SHADERS, n), "rb").read() for n in names))
	print("checksum mismatches: %d / %d unpacked game shaders" % (bad, len(names)))
	for path in sys.argv[1:]:
		blob = open(path, "rb").read()
		print("%s: checksum %s" % (path, "ok" if checksum(blob) == blob[4:20] else "WRONG"))
		shdr = next(blob[o + 8:o + 8 + s] for c, o, s in chunks(blob) if c in (b"SHDR", b"SHEX"))
		for off, op, n, t in instructions(shdr):
			print("%3d op 0x%02X len %d  %s" % (off, op, n, " ".join("%08X" % x for x in t)))
