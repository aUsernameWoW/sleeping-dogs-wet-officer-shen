"""The game's shaders (data\\shaders\\shaders.temp.bin): unpack, inspect, make patched variants.

python shaders.py unpack            every DXBC blob -> build\\research\\shaders\\<NAME>.<V|P|C|G|H|D>SBIN, plus
                                    names.json (checksum -> names, including SDWet's default patch) and
                                    variants.json (for renderdoc\\preview.py)
python shaders.py bindings [REGEX]  bound textures/cbuffers of matching shaders (default: HK_CHARACTER pixel)
python shaders.py cbuffer NAME CB   member layout of cbuffer CB in shader NAME (e.g. cbSceneryInstance)
python shaders.py maskreads [REGEX] which cbSceneryInstance.Mask components each character pixel shader reads
python shaders.py disasm NAME       fxc /dumpbin disassembly

shaders.temp.bin is a chunk file of Illusion.ShaderBinary resources; each is a header ending in the shader's
name (e.g. HK_CHARACTER_DR_WS.PSBIN) followed by the raw DXBC blob, so scanning for "DXBC" is enough. The
blobs keep their reflection data (RDEF), so resource and cbuffer names are all there.
"""
import collections
import json
import os
import re
import struct
import subprocess
import sys

from common import FXC, GAME, OUT, SHADERS
from dxbc import chunks
from patch import patch

# RenderDoc preview variants: tag -> (Shine, Gloss); "de" is the unpatched original.
VARIANTS = {"default": (0.05, 0.10), "strong": (0.10, 0.20)}

# Name parts that select a permutation of a shader template (the game picks one at draw time from the state).
# WS = wet/sweat, DR = deferred, OT = overlay texture (texDiffuseBlend), EM = emissive; the rest are texture
# layers of the CHARACTER_/UBERSHADER_ templates, which no character material uses.
PERMUTATION_FLAGS = {"DR", "MC", "OT", "WS", "RI", "EM", "NM", "NM2", "SM", "SM2", "GM", "DTM", "DM2"}


def unpack():
	data = open(os.path.join(GAME, "data", "shaders", "shaders.temp.bin"), "rb").read()
	os.makedirs(SHADERS, exist_ok=True)
	names = collections.defaultdict(list)
	count = 0
	for m in re.finditer(b"DXBC", data):
		at = m.start()
		size = struct.unpack_from("<I", data, at + 24)[0]
		if size < 32 or at + size > len(data):
			continue
		found = re.findall(rb"([A-Za-z0-9_\.]+\.[VPCGHD]SBIN)", data[max(0, at - 128):at])
		name = found[-1].decode() if found else "unnamed_%x" % at
		blob = data[at:at + size]
		open(os.path.join(SHADERS, name), "wb").write(blob)
		names[blob[4:20].hex()].append(name)
		count += 1

	table = {k: "|".join(v) for k, v in names.items()}
	variants = {}
	for tag in ["de", *VARIANTS]:
		os.makedirs(os.path.join(OUT, "variants", tag), exist_ok=True)
	for name in sorted(os.listdir(SHADERS)):
		if not name.endswith(".PSBIN"):
			continue
		blob = open(os.path.join(SHADERS, name), "rb").read()
		paths = {"de": os.path.join(OUT, "variants", "de", name)}
		open(paths["de"], "wb").write(blob)
		for tag, (shine, gloss) in VARIANTS.items():
			new, _ = patch(blob, shine, gloss)
			if not new:
				break
			paths[tag] = os.path.join(OUT, "variants", tag, name)
			open(paths[tag], "wb").write(new)
			if tag == "default":
				table.setdefault(new[4:20].hex(), table[blob[4:20].hex()].replace(".PSBIN", "+SDWet.PSBIN"))
				variants[new[4:20].hex()] = paths
		else:
			variants[blob[4:20].hex()] = paths
	json.dump(table, open(os.path.join(OUT, "names.json"), "w"), indent=0)
	json.dump({"variants": ["de", *VARIANTS], "map": variants}, open(os.path.join(OUT, "variants.json"), "w"), indent=1)
	print("%d shaders (%d unique) -> %s; %d wet shaders with variants %s" % (count, len(names), SHADERS, len(variants) // 2,
		", ".join(["de", *VARIANTS])))


# --- RDEF ---

RTYPE = {0: "cbuffer", 1: "tbuffer", 2: "texture", 3: "sampler"}


def cstr(b, o):
	return b[o:b.index(b"\0", o)].decode("latin1")


def rdef_chunk(blob):
	return next(blob[o + 8:o + 8 + s] for c, o, s in chunks(blob) if c == b"RDEF")


def bindings(blob):
	"""[(name, kind, bind point)] of the bound resources."""
	r = rdef_chunk(blob)
	nbind, bindo = struct.unpack_from("<II", r, 8)
	out = []
	for i in range(nbind):
		nm, typ, _, _, _, point = struct.unpack_from("<6I", r, bindo + i * 32)
		out.append((cstr(r, nm), RTYPE.get(typ, str(typ)), point))
	return out


def members(blob, cbname):
	"""Leaf members of cbuffer `cbname`: [(path, byte offset)]."""
	r = rdef_chunk(blob)
	ncb, cbo = struct.unpack_from("<II", r, 0)
	out = []

	def walk(typo, base, prefix):
		# Type: class, type, rows, columns, elements, member count (u16 each), member array offset (u32).
		nmem = struct.unpack_from("<H", r, typo + 10)[0]
		memo = struct.unpack_from("<I", r, typo + 12)[0]
		if nmem == 0:
			out.append((prefix, base))
			return
		for k in range(nmem):
			mn, mt, mo = struct.unpack_from("<3I", r, memo + k * 12)
			walk(mt, base + mo, "%s.%s" % (prefix, cstr(r, mn)))

	for i in range(ncb):
		nm, nvar, varo = struct.unpack_from("<3I", r, cbo + i * 24)
		if cstr(r, nm) == cbname:
			for j in range(nvar):
				vn, start, _, _, typo = struct.unpack_from("<5I", r, varo + j * 40)
				walk(typo, start, cstr(r, vn))
	return out


def disasm(name):
	asm_dir = os.path.join(OUT, "asm")
	os.makedirs(asm_dir, exist_ok=True)
	out = os.path.join(asm_dir, name + ".asm")
	if not os.path.exists(out):
		subprocess.run([FXC, "/nologo", "/dumpbin", "/Fc", out, os.path.join(SHADERS, name)], check=True, stdout=subprocess.DEVNULL)
	return open(out, encoding="latin1").read()


def matching(regex):
	pat = re.compile(regex)
	return [n for n in sorted(os.listdir(SHADERS)) if pat.fullmatch(n)]


def main():
	cmd = sys.argv[1] if len(sys.argv) > 1 else "help"
	args = sys.argv[2:]
	if cmd == "unpack":
		unpack()
	elif cmd == "bindings":
		for name in matching(args[0] if args else r"HK_CHARACTER.*\.PSBIN"):
			blob = open(os.path.join(SHADERS, name), "rb").read()
			print(name)
			for kind in ("texture", "cbuffer"):
				print("  %-8s %s" % (kind, " ".join("%s:%d" % (n, p) for n, k, p in bindings(blob) if k == kind)))
	elif cmd == "cbuffer":
		blob = open(os.path.join(SHADERS, args[0]), "rb").read()
		for path, offset in members(blob, args[1]):
			print("%-40s +%-3d c%d.%s" % (path, offset, offset // 16, "xyzw"[offset % 16 // 4]))
	elif cmd == "maskreads":
		# Components of cbSceneryInstance register 2 (Mask: .y sweat, .z wetness, .w charred) read by the code.
		families = collections.Counter()
		for name in matching(args[0] if args else r"(HK_)?CHARACTER.*\.PSBIN|HK_HAIR.*\.PSBIN|HK_SKINSS.*\.PSBIN"):
			slot = next((p for n, k, p in bindings(open(os.path.join(SHADERS, name), "rb").read()) if n == "cbSceneryInstance" and k == "cbuffer"), None)
			if slot is None:
				continue
			code = disasm(name).split("dcl_temps", 1)[-1]
			comps = set()
			for swz in re.findall(r"\bcb%d\[2\]\.?([xyzw]*)" % slot, code):
				comps |= set(swz or "xyzw")
			reads = "".join(c for c in "xyzw" if c in comps) or "-"
			family = "_".join(p for p in name.split(".")[0].split("_") if p not in PERMUTATION_FLAGS)
			families[(family, reads)] += 1
			if "-v" in args:
				print("%-44s Mask.%s" % (name, reads))
		for (family, reads), n in sorted(families.items()):
			print("%-24s Mask.%-5s %d permutation(s)" % (family, reads, n))
	elif cmd == "disasm":
		print(disasm(args[0]))
	else:
		print(__doc__)


if __name__ == "__main__":
	main()
