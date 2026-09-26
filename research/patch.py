"""Python original of SDWet's shader patch (core/dxbc.cc, which is what ships): in the wet/sweat character
pixel shaders, fall back to a default wet mask (texSpecular .x/.z) where the specular map leaves both empty.
Both produce identical bytes; shaders.py uses this one to make variants for RenderDoc previews.

python patch.py [shine gloss]: patches every unpacked pixel shader and lists the ones it changed."""
import os, struct, sys
from dxbc import chunks, checksum, instructions

OP_SAMPLE = {0x45, 0x46, 0x47, 0x48, 0x49, 0x4A}  # sample, _c, _c_lz, _l, _d, _b
OP_DCL_TEMPS = 0x68
OPERAND_TYPE = lambda t: (t >> 12) & 0xFF
TYPE_TEMP, TYPE_RESOURCE, TYPE_CB = 0, 7, 8

def f32(x):
	return struct.unpack("<I", struct.pack("<f", x))[0]

def rdef_bindings(blob):
	r = next(blob[o + 8:o + 8 + s] for c, o, s in chunks(blob) if c == b"RDEF")
	nbind, bindo = struct.unpack_from("<II", r, 8)
	out = {}
	for i in range(nbind):
		nm, typ, _, _, _, point = struct.unpack_from("<6I", r, bindo + i * 32)
		out[(r[nm:r.index(b"\0", nm)].decode(), typ)] = point
	return out

def operands(toks):
	"""Splits an instruction's operand tokens: yields (token, [index values], [immediates])."""
	i = 1
	if toks[0] & 0x80000000:  # extended opcode tokens
		while toks[i] & 0x80000000:
			i += 1
		i += 1
	while i < len(toks):
		t = toks[i]; i += 1
		ext = t & 0x80000000
		if ext:
			i += 1
		dim = (t >> 20) & 3
		imms = []
		if OPERAND_TYPE(t) in (4,):  # immediate32
			n = 4 if (t & 3) == 2 else 1
			imms = list(toks[i:i + n]); i += n
		idx = []
		for d in range(dim):
			rep = (t >> (22 + 3 * d)) & 7
			if rep != 0:
				return  # relative indexing: not expected in these shaders
			idx.append(toks[i]); i += 1
		yield t, idx, imms

OP_DCL_CONSTANT_BUFFER = 0x59

def mask_reads(toks, cb):
	"""Components of cbSceneryInstance.Mask (register 2) an instruction reads."""
	found = set()
	if (toks[0] & 0x7FF) == OP_DCL_CONSTANT_BUFFER:  # its second index is the buffer size, not a register
		return found
	for t, idx, _ in operands(toks):
		if OPERAND_TYPE(t) == TYPE_CB and idx == [cb, 2]:
			mode = (t >> 2) & 3
			if mode == 1:
				found |= {(t >> (4 + 2 * k)) & 3 for k in range(4)}
			elif mode == 2:
				found.add((t >> 4) & 3)
	return found

def patch(blob, wet_r, wet_b, eps=0.004):
	b = rdef_bindings(blob)
	spec = b.get(("texSpecular", 2))
	cb = b.get(("cbSceneryInstance", 0))
	if spec is None or cb is None:
		return None, "no texSpecular/cbSceneryInstance"
	ch = {c: (o, s) for c, o, s in chunks(blob)}
	code = b"SHDR" if b"SHDR" in ch else b"SHEX"
	o, s = ch[code]
	shdr = blob[o + 8:o + 8 + s]
	insts = list(instructions(shdr))
	reads = set()
	for _, _, _, t in insts:
		reads |= mask_reads(t, cb)
	if not {1, 2} <= reads:  # sweat (.y) and wetness (.z): max(wet, sweat) in the _WS permutations
		return None, "does not read cbSceneryInstance.Mask.yz"
	temps = next((t for off, op, n, t in insts if op == OP_DCL_TEMPS), None)
	if temps is None:
		return None, "no dcl_temps"
	tmp = temps[1]
	toks = list(struct.unpack("<%dI" % (len(shdr) // 4), shdr))
	inserts = []
	for off, op, n, t in insts:
		if op not in OP_SAMPLE:
			continue
		ops = list(operands(t))
		dest, res = ops[0], ops[2]
		if OPERAND_TYPE(res[0]) != TYPE_RESOURCE or res[1] != [spec]:
			continue
		if OPERAND_TYPE(dest[0]) != TYPE_TEMP or (dest[0] >> 2) & 3 != 0 or (dest[0] >> 4) & 5 != 5:
			return None, "texSpecular sample doesn't write .x and .z of a temp"
		x = dest[1][0]
		inserts.append((off + n, [
			0x07000000, 0x00100012, tmp, 0x0010000A, x, 0x0010002A, x,                  # add rT.x, rX.x, rX.z
			0x07000031, 0x00100012, tmp, 0x00004001, f32(eps), 0x0010000A, tmp,         # lt rT.x, l(eps), rT.x
			0x0C000037, 0x00100052, x, 0x00100006, tmp, 0x00100206, x,                  # movc rX.xz, rT.xxxx, rX.xxzx,
			0x00004002, f32(wet_r), 0, f32(wet_b), 0,                                   #      l(r, 0, b, 0)
		]))
	if not inserts:
		return None, "no texSpecular sample"
	for pos, words in sorted(inserts, reverse=True):
		toks[pos:pos] = words
	for off, op, n, t in insts:
		if op == OP_DCL_TEMPS:
			toks[off + 1] = tmp + 1
	toks[1] = len(toks)
	new_code = struct.pack("<%dI" % len(toks), *toks)
	delta = len(new_code) - len(shdr)

	out = bytearray(blob[:o + 4]) + struct.pack("<I", len(new_code)) + new_code + blob[o + 8 + s:]
	count = struct.unpack_from("<I", out, 28)[0]
	for i in range(count):
		co = struct.unpack_from("<I", out, 32 + 4 * i)[0]
		if co > o:
			struct.pack_into("<I", out, 32 + 4 * i, co + delta)
	struct.pack_into("<I", out, 24, len(out))
	so = next((co for c, co, _ in chunks(bytes(out)) if c == b"STAT"), None)
	if so is not None:
		ic, tc = struct.unpack_from("<II", out, so + 8)
		struct.pack_into("<II", out, so + 8, ic + 3 * len(inserts), tc + 1)
	out[4:20] = checksum(bytes(out))
	return bytes(out), "patched %d sample(s), temp r%d" % (len(inserts), tmp)

if __name__ == "__main__":
	from common import SHADERS
	shine, gloss = (float(a) for a in sys.argv[1:3]) if len(sys.argv) >= 3 else (0.05, 0.10)
	hits = 0
	for n in sorted(os.listdir(SHADERS)):
		if not n.endswith(".PSBIN"):
			continue
		new, why = patch(open(os.path.join(SHADERS, n), "rb").read(), shine, gloss)
		if new:
			hits += 1
			print(n, why)
		elif "does not read" not in why and "no texSpecular" not in why:
			print("  skip", n, why)
	print(hits, "patched")
