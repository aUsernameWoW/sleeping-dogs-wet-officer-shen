"""Which shader template each character material uses (Illusion.Material chunks in the archives' .perm.bin).

python materials.py dump "Data\\Characters_New\\Wei_Head.perm.bin"   every material of one file with its params
python materials.py scan [PATH-REGEX]                                 all materials of matching files -> counts by
                                                                      shader (default: Characters); materials.json

Run with tools\\extract's venv Python. Layout (TheoryEngine illusion/material.hh, verified on the DE data): the
chunk (0xF5F8516F) holds a qResourceData (name at +0x34), mNumParams at +0x70, then from +0x80 params of 0x38
bytes: state name hash +0, state type hash +4, resource name hash +0x28, resource type hash +0x30. State
names use qStringHash32 (case-sensitive), resource names qStringHashUpper32. The iShader param names the
shader *template* (HK_CHARACTER); the permutation (HK_CHARACTER_DR_WS) is picked at draw time.
"""
import collections
import json
import os
import re
import struct
import sys

from common import GAME, OUT, use_extract

use_extract()
from extract import Archives, all_archives, load_names  # noqa: E402
from sdbig import _CRC, extract, hash_upper32  # noqa: E402
from textures import iter_chunks  # noqa: E402

CHUNK_MATERIAL = 0xF5F8516F


def hash32(s):
	h = 0xFFFFFFFF
	for ch in s.encode("latin-1"):
		h = ((h << 8) & 0xFFFFFFFF) ^ _CRC[((h >> 24) ^ ch) & 0xFF]
	return h


def shader_names():
	"""Hash -> name for every shader template: each permutation name and its prefixes."""
	names = {}
	temp = open(os.path.join(GAME, "data", "shaders", "shaders.temp.bin"), "rb").read()
	for n in set(re.findall(rb"([A-Za-z0-9_]+)\.[VPCGHD]SBIN", temp)):
		parts = n.decode().split("_")
		for i in range(1, len(parts) + 1):
			names[hash_upper32("_".join(parts[:i]))] = "_".join(parts[:i])
	return names


PARAMS = {hash32(n): n for n in ["iShader", "iTexture", "iRasterState", "iAlphaState", "sbDepthBiasSortLayer", "sbSpecularLook",
	"sbTextureAnim", "texDiffuse", "texBump", "texSpecular", "texDamage", "texDiffuseBlend", "texBlendMask", "texDiffuse2",
	"texBump2", "texSpecular2", "texEmission", "cbUberParams", "sbCharacterLook", "texFadeDitherMask", "Illusion.Shader",
	"Illusion.Texture", "Illusion.RasterState", "Illusion.AlphaState", "Illusion.StateBlock"]}


def parse(data):
	for uid, p, _ in iter_chunks(data):
		if uid != CHUNK_MATERIAL:
			continue
		name = data[p + 0x34:p + 0x58].split(b"\0")[0].decode("latin-1")
		params = []
		for i in range(struct.unpack_from("<I", data, p + 0x70)[0]):
			q = p + 0x80 + i * 0x38
			state, stype = struct.unpack_from("<II", data, q)
			params.append((PARAMS.get(state, "%08X" % state), PARAMS.get(stype, "%08X" % stype),
				struct.unpack_from("<I", data, q + 0x28)[0], PARAMS.get(struct.unpack_from("<I", data, q + 0x30)[0], "?")))
		yield name, params


def main():
	shaders = shader_names()
	cmd = sys.argv[1] if len(sys.argv) > 1 else ""
	if cmd == "dump":
		arcs = Archives()
		for arc in all_archives():
			data = arcs.read(arc, sys.argv[2])
			if data:
				for name, params in parse(data):
					print(arc, name)
					for state, stype, res, rtype in params:
						print("    %-20s %-14s %08X %-18s %s" % (state, stype, res, shaders.get(res, ""), rtype))
				return
		print("not found:", sys.argv[2])
	elif cmd == "scan":
		names, arcs, pat, rows = load_names(), Archives(), re.compile(sys.argv[2] if len(sys.argv) > 2 else "Characters", re.I), []
		for arc in all_archives():
			for uid, entry in arcs.index(arc).items():
				path = names.get(uid)
				if not path or not path.lower().endswith(".perm.bin") or not pat.search(path):
					continue
				try:
					data = extract(os.path.join(GAME, arc + ".big"), entry)
				except ValueError:
					continue
				for name, params in parse(data):
					shader = next((shaders.get(res, "?%08X" % res) for state, _, res, _ in params if state == "iShader"), None)
					rows.append((arc, path, name, shader))
		json.dump(rows, open(os.path.join(OUT, "materials.json"), "w"), indent=0)
		print(len(rows), "materials ->", os.path.join(OUT, "materials.json"))
		for shader, n in collections.Counter(r[3] for r in rows).most_common():
			print("%6d  %s" % (n, shader))
	else:
		print(__doc__)


if __name__ == "__main__":
	main()
