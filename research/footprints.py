"""The game's footprint setup: every PhysicsVolumeProperties with its footstep effect, decals and countdown.

python footprints.py            the volume table, the surfaces that point at a volume, the decal textures

Where core/footprints.cc's facts come from. CharacterEffectsComponent::HandleFootstep places, for each volume the
character is in, mFootStepEffect and mFootStepDecal_Left/_Right, and keeps placing the decals for
mFootStepDecalCountdown seconds after leaving. Volumes come from placed phantom volumes, the ground's
PhysicsSurfaceProperties (+0x6C: a volume name hash) and, for the player in the rain, PhysVol_WetSurface.

Data: Data\\Global\\PhysicsProperties.perm.bin (Global.big; not in the community name lists, read by path) is a
qReflect inventory. Objects: qReflectObject (0x50 bytes: vtable, mBaseNode.mUID at +8, mTypeUID at +0x38 =
qStringHash64 of the type name), then the fields; PhysicsVolumeProperties (0x80): mName count at +0x58,
mFootStepEffect +0x60, decal left +0x64, right +0x68, countdown +0x6C, damage/s +0x70, fire ignition +0x74,
hit record type +0x78. Object names sit in a name table earlier in the file: {uid, object offset, name offset}
with a name offset whose base varies by a few bytes, so the name is found by matching the stored length.
Effect IDs are qStringHashUpper32 of names in Data\\Global\\Effects.perm.bin; a decal effect refers (type
0xAE323146) to a decal definition that names its texture by hash. Run with tools\\extract's venv Python.
"""
import json
import os
import re
import struct

from common import use_extract

use_extract()
from extract import INDEX, Archives  # noqa: E402
from sdbig import hash_upper32  # noqa: E402

# UFG::qStringHash64: reflected CRC-64 (Jones, 0x95AC9329AC4BC9B5), init all ones (TheoryEngine quark/checksum.hh).
_CRC64 = []
for _i in range(256):
	_c = _i
	for _ in range(8):
		_c = (_c >> 1) ^ 0x95AC9329AC4BC9B5 if _c & 1 else _c >> 1
	_CRC64.append(_c)
assert _CRC64[1] == 0x7AD870C830358979


def hash64(s):
	h = 0xFFFFFFFFFFFFFFFF
	for ch in s.encode("latin-1"):
		h = (h >> 8) ^ _CRC64[(h ^ ch) & 0xFF]
	return h


DECAL_REF = 0xAE323146
NONE = 0xFFFFFFFF


def strings(data):
	"""qStringHashUpper32 -> every printable string in `data`."""
	return {hash_upper32(m.group().decode()): m.group().decode() for m in re.finditer(rb"[\x20-\x7e]{3,}", data)}


def objects(data, type_name):
	"""Offsets of the objects of one qReflect type (the inventory tables before them carry the same hash)."""
	key = struct.pack("<Q", hash64(type_name))
	offsets = [m.start() - 0x38 for m in re.finditer(re.escape(key), data)]
	uids = {o: struct.unpack_from("<Q", data, o + 8)[0] for o in offsets if o >= 0}
	# An object's uid also appears once in the name table; table entries of other kinds don't have a name count.
	return [o for o in offsets if o >= 0 and 0 < struct.unpack_from("<Q", data, o + 0x58)[0] < 200 and uids[o]]


def object_name(data, o):
	uid, = struct.unpack_from("<Q", data, o + 8)
	length, = struct.unpack_from("<Q", data, o + 0x58)
	p = data.find(struct.pack("<Q", uid))
	off, = struct.unpack_from("<Q", data, p + 0x10)
	for d in sorted(range(-0x20, 0x21), key=abs):
		q = p + off + d
		if 0 < q < len(data) - length and data[q - 1] == 0 and data[q + length - 1] == 0 and 0 not in data[q:q + length - 1]:
			return data[q:q + length - 1].decode("latin-1")
	return "?%016x" % uid


def decal_texture(fx, effect_id, textures):
	"""Texture of a decal effect: the effect's decal reference, then the texture hash in that definition."""
	at = fx.find(struct.pack("<I", effect_id))
	if at < 0:
		return "?"
	block = fx[at:at + 0x140]
	i = block.find(struct.pack("<I", DECAL_REF))
	if i < 0:
		return "? (no decal reference near the effect; mud and fire are laid out differently)"
	ref = block[i + 4:i + 8]
	for m in re.finditer(re.escape(ref), fx):
		for j in range(m.start(), m.start() + 0x100, 4):
			h, = struct.unpack_from("<I", fx, j)
			if h in textures:
				return textures[h]
	return "?"


def main():
	arcs = Archives()
	phys = arcs.read("Global", "Data\\Global\\PhysicsProperties.perm.bin")
	fx = arcs.read("Global", "Data\\Global\\Effects.perm.bin")
	names: dict[int, str] = strings(fx)
	names.update(strings(phys))
	textures: dict[int, str] = {}
	if os.path.exists(INDEX):
		textures = {hash_upper32(row[2]): row[2] for row in json.load(open(INDEX))}
	else:
		print("(no texture index: run tools\\extract.ps1 index for decal texture names)")

	def name(h: int) -> str:
		return "-" if h == NONE else names.get(h, "%08X" % h)

	print("%-26s %-8s %-30s %-28s %-28s %6s %6s %5s" % ("volume", "hash", "footstep effect", "left decal", "right decal",
		"after", "dmg/s", "fire"))
	decals = set()
	volumes = set()
	for o in objects(phys, "UFG::PhysicsVolumeProperties"):
		fx_id, left, right = struct.unpack_from("<III", phys, o + 0x60)
		countdown, damage, fire = struct.unpack_from("<fff", phys, o + 0x6C)
		n = object_name(phys, o)
		volumes.add(hash_upper32(n))
		print("%-26s %08X %-30s %-28s %-28s %5gs %6g %5g" % (n, hash_upper32(n), name(fx_id), name(left), name(right),
			countdown, damage, fire))
		decals.update(d for d in (left, right) if d != NONE)

	# PhysicsSurfaceProperties (0xA8): mEffectProperty +0x6C names the surface's effects ("Metal", "Wood", ...);
	# CharacterEffectsComponent::Update looks it up as a volume, which only a few names are.
	counts = {}
	key = struct.pack("<Q", hash64("UFG::PhysicsSurfaceProperties"))
	for m in re.finditer(re.escape(key), phys):
		volume, = struct.unpack_from("<I", phys, m.start() - 0x38 + 0x6C)
		if volume in volumes:
			counts[volume] = counts.get(volume, 0) + 1
	print("\nsurfaces whose mEffectProperty is a volume (standing on them = being in it):")
	for volume, count in sorted(counts.items(), key=lambda kv: name(kv[0])):
		print("  %-26s %d surface(s)" % (name(volume), count))

	print("\ndecal textures:")
	for d in sorted(decals, key=name):
		print("  %-30s %s" % (name(d), decal_texture(fx, d, textures)))


if __name__ == "__main__":
	main()
